import test from 'node:test';
import assert from 'node:assert/strict';
import { createFontChoiceController } from '../src/font-choice-controller.js';
import { fontPickerItems } from '../src/font-choice-ui.js';

const face = { family: 'Family', name: 'Regular', weight: 400, style: 0,
  stretch: 5, format: 1, color: false };
const text = (patch = {}) => ({ type: 'Text', text: 'ABC',
  params: { size: { value: 100 }, letterSpacing: { value: 0 }, lineSpacing: { value: 1.2 } },
  ...patch });
const deferred = () => { let resolve; const promise = new Promise((r) => { resolve = r; });
  return { promise, resolve }; };
const choice = (source, id = 'opaque', extra = {}) =>
  fontPickerItems({ [source]: [{ id, label: 'Family Regular', ...extra }] }, text())[0];
function fixture(source = text(), adapters = {}) {
  const commands = [], previews = [], statuses = [], applied = [];
  let current = true, gestureId = 0, historyEntries = 0;
  const owner = {
    state: { sessionId: 'session', expectedRevision: 3 },
    hasGesture: false, gestureState: null,
    dispatch(command) {
      commands.push(command);
      if (command.type === 'begin-gesture') {
        this.hasGesture = true; this.gestureState = { gestureId: ++gestureId };
      } else if (command.type === 'cancel-gesture') this.hasGesture = false;
      else if (command.type === 'commit-gesture' ||
        !['update-gesture'].includes(command.type)) { this.hasGesture = false; historyEntries++; }
      return this.gestureState || { changed: true };
    },
    previewSnapshot: () => ({ document: { preview: true } }),
  };
  const controller = createFontChoiceController({ owner, envelope: { ...owner.state },
    layerId: 'layer', nodeId: 'node', source, isCurrent: () => current,
    readInstalled: async () => new ArrayBuffer(1),
    prepareWindows: async (_source, exactFace, { settings }) => ({ face: exactFace, ligatures: settings.ligatures }),
    onPreview: (p) => previews.push(p), onStatus: (...s) => statuses.push(s),
    onApplied: (r) => applied.push(r), ...adapters,
  });
  return { controller, owner, commands, previews, statuses, applied,
    stale: () => { current = false; }, history: () => historyEntries };
}

test('static document browsing previews but Apply admits exactly one history change', async () => {
  for (const source of ['embedded']) {
    const f = fixture(), row = choice(source);
    await f.controller.select(row);
    assert.equal(f.history(), 0);
    assert.deepEqual(f.previews.at(-1), { preview: true });
    assert.equal(await f.controller.apply(row), true);
    assert.equal(f.history(), 1);
    assert.equal(await f.controller.apply(row), false);
    const update = f.commands.find((c) => c.type === 'update-gesture').payload.update;
    if (source === 'embedded') assert.equal(update.font, 'opaque');
    else assert.ok(update.bytes instanceof ArrayBuffer);
  }
});

test('OS to document replacement is atomic: no clear/adopt or intermediate history', async () => {
  for (const source of ['embedded']) {
    const original = text({ osFont: { outline: { ligatures: false }, face } }), before = structuredClone(original);
    const f = fixture(original), row = choice(source);
    await f.controller.select(row);
    assert.equal(f.commands.length, 0);
    assert.equal(await f.controller.apply(row), true);
    assert.deepEqual(f.commands.map((c) => c.type), ['replace-os-text-font']);
    assert.equal(f.history(), 1);
    const payload = f.commands[0].payload;
    if (source === 'embedded') assert.equal(payload.fontId, 'opaque');
    else assert.ok(payload.bytes instanceof ArrayBuffer);
    assert.deepEqual(original, before);
  }
});

test('Cancel/Escape during native preparation restores only our preview and never commits', async () => {
  const wait = deferred(), f = fixture(text(), { prepareWindows: () => wait.promise });
  const row = choice('windows', 'native', { face });
  await f.controller.select(row); const pending = f.controller.apply(row);
  await Promise.resolve(); f.controller.cancel(); wait.resolve({ face, ligatures: false });
  assert.equal(await pending, false);
  assert.equal(f.history(), 0); assert.equal(f.commands.length, 0);
  const g = fixture(); await g.controller.select(choice('embedded'));
  g.controller.cancel(); g.controller.cancel();
  assert.deepEqual(g.commands.map((c) => c.type), ['begin-gesture', 'update-gesture', 'cancel-gesture']);
  assert.equal(g.history(), 0); assert.equal(g.previews.at(-1), null);
});

test('rapid selections and repeated Apply cannot commit an older native response', async () => {
  const first = deferred(), second = deferred();
  const faceA = { ...face, name: 'First' }, faceB = { ...face, name: 'Second' };
  const f = fixture(text(), { prepareWindows: (_source, exactFace) =>
    exactFace.name === 'First' ? first.promise : second.promise });
  const a = choice('windows', 'first', { face: faceA }), b = choice('windows', 'second', { face: faceB });
  await f.controller.select(a); const pa = f.controller.apply(a); await Promise.resolve();
  await f.controller.select(b);
  first.resolve({ face: faceA, ligatures: false }); assert.equal(await pa, false);
  assert.equal(await f.controller.apply(a), false);
  const pb = f.controller.apply(b), duplicate = f.controller.apply(b);
  await Promise.resolve(); second.resolve({ face: faceB, ligatures: false });
  assert.deepEqual(await Promise.all([pb, duplicate]), [true, false]);
  assert.equal(f.history(), 1);
  assert.deepEqual(f.commands[0].payload.outline.face, faceB);
});

test('native route uses exact face and refuses mismatched or canceled preparation', async () => {
  const row = choice('windows', 'native', { face });
  const good = fixture(); await good.controller.select(row);
  assert.equal(await good.controller.apply(row), true);
  assert.deepEqual(good.commands.map((c) => c.type), ['set-os-font-text']);
  const bad = fixture(text(), { prepareWindows: async () => ({ face: { ...face, format: 2 } }) });
  await bad.controller.select(row); assert.equal(await bad.controller.apply(row), false);
  assert.equal(bad.commands.length, 0); assert.match(bad.statuses.at(-1)[0], /did not match/);
  const wait = deferred(), canceled = fixture(text(), { prepareWindows: () => wait.promise });
  await canceled.controller.select(row); const applying = canceled.controller.apply(row);
  await Promise.resolve(); canceled.controller.cancel(); wait.resolve({ face, ligatures: false });
  assert.equal(await applying, false); assert.equal(canceled.commands.length, 0);
});

test('selection/session/revision changes during async work never target the newer document', async () => {
  for (const invalidate of [(f) => f.stale(), (f) => { f.owner.state.expectedRevision++; },
    (f) => { f.owner.state.sessionId = 'new-session'; }]) {
    const wait = deferred(), f = fixture(text(), { prepareWindows: () => wait.promise });
    const row = choice('windows', 'native', { face });
    await f.controller.select(row); const pending = f.controller.apply(row); await Promise.resolve();
    invalidate(f); wait.resolve({ face, ligatures: false });
    assert.equal(await pending, false); assert.equal(f.commands.length, 0);
  }
});

test('unsupported intent keeps editable Text and every override untouched', async () => {
  const original = text({ fontFeatures: { liga: true }, characterPositions: { styles: { c1: {} } } });
  const before = structuredClone(original), f = fixture(original), row = choice('windows', 'native', { face });
  await f.controller.select(row); assert.equal(await f.controller.apply(row), false);
  assert.equal(f.commands.length, 0); assert.deepEqual(original, before);
  assert.match(f.statuses.at(-1)[0], /OpenType/);
});

test('prepared variable route commits once and cancels late tickets after dismissal', async () => {
  const row = choice('embedded', 'var', { variable: true });
  let commits = 0, cancels = 0;
  const ticket = { commit() { commits++; return {}; }, cancel() { cancels++; } };
  const f = fixture(text(), { prepareVariable: async (command, options) => {
    assert.equal(command.type, 'set-variable-font'); assert.equal(options.choice.id, 'var');
    return ticket;
  } });
  await f.controller.select(row); assert.equal(await f.controller.apply(row), true);
  assert.equal(commits, 1); assert.equal(cancels, 0);
  const wait = deferred(), g = fixture(text(), { prepareVariable: () => wait.promise });
  await g.controller.select(row); const applying = g.controller.apply(row);
  await Promise.resolve(); g.controller.cancel(); wait.resolve(ticket); await applying;
  assert.equal(commits, 1); assert.equal(cancels, 1);
});

test('character Inherit stays in its fixed range and no-choice Apply is harmless', async () => {
  const f = fixture(text(), { characterIds: ['c1', 'c2'] });
  assert.equal(await f.controller.apply(), false);
  const row = { key: 'inherit-default-font', choice: { source: 'embedded', id: '', label: 'Inherit' } };
  await f.controller.select(row); assert.equal(await f.controller.apply(row), true);
  assert.deepEqual(f.commands[0].payload.target.characterIds, ['c1', 'c2']);
  assert.equal(f.commands[1].payload.update.font, '');
});

test('byte-readable installed choices require explicit Advanced import even on variable Text', async () => {
  let reads = 0, preparations = 0;
  const f = fixture(text({ variation: { fontId: 'old-variable', axes: {} } }), {
    readInstalled: async () => { reads++; return new ArrayBuffer(1); },
    prepareVariable: async () => { preparations++; return { commit() {}, cancel() {} }; },
  });
  const row = choice('installed', 'exact-installed-token');
  await f.controller.select(row); assert.equal(await f.controller.apply(row), false);
  assert.equal(reads, 0); assert.equal(preparations, 0); assert.equal(f.commands.length, 0);
  assert.match(f.statuses.at(-1)[0], /Font file import is required; use Advanced/);
});

test('missing or invalid variable preparation never falls back to a raw command', async () => {
  for (const ticket of [undefined, null, {}, { commit() {} }, { cancel() {} }]) {
    const f = fixture(text(), { prepareVariable: async () => ticket });
    const row = choice('embedded', 'var', { variable: true });
    await f.controller.select(row); assert.equal(await f.controller.apply(row), false);
    assert.equal(f.commands.length, 0); assert.equal(f.history(), 0);
    assert.match(f.statuses.at(-1)[0], /preparation did not finish/);
  }
});

test('installed discovery never reads bytes or falls back to a colliding document asset ID', async () => {
  for (const source of [text(), text({ osFont: { face, outline: { ligatures: false } } }),
    text({ variation: { fontId: 'old-variable', axes: {} } })])
    for (const bytes of [undefined, null, new ArrayBuffer(0), new Uint8Array(4)]) {
      let reads = 0; const f = fixture(source, { readInstalled: async () => { reads++; return bytes; } });
      const row = choice('installed', 'colliding-document-asset-id');
      await f.controller.select(row); assert.equal(await f.controller.apply(row), false);
      assert.equal(f.commands.length, 0); assert.equal(f.history(), 0);
      assert.equal(reads, 0); assert.match(f.statuses.at(-1)[0], /Font file import is required/);
    }
});

test('late native preparation cannot clear or commit a newer static preview', async () => {
  const wait = deferred(), f = fixture(text(), { prepareWindows: () => wait.promise });
  const native = choice('windows', 'native', { face }), embedded = choice('embedded', 'newest');
  await f.controller.select(native); const applying = f.controller.apply(native);
  await Promise.resolve(); await f.controller.select(embedded);
  const preview = f.previews.at(-1);
  wait.resolve({ face, ligatures: false }); assert.equal(await applying, false);
  assert.equal(f.previews.at(-1), preview);
  assert.equal(f.commands.some((c) => c.type === 'set-os-font-text'), false);
  assert.equal(await f.controller.apply(embedded), true);
  assert.equal(f.history(), 1);
});

test('native and variable preparation rejection remain non-mutating and explain failure', async () => {
  for (const adapters of [
    { prepareWindows: async () => { throw Error('Font removed'); } },
    { prepareVariable: async () => { throw Error('Preparation refused'); } },
  ]) {
    const variable = !!adapters.prepareVariable;
    const f = fixture(text(), adapters), row = choice(variable ? 'embedded' : 'windows', 'opaque',
      variable ? { variable: true } : { face });
    await f.controller.select(row); assert.equal(await f.controller.apply(row), false);
    assert.equal(f.commands.length, 0); assert.equal(f.history(), 0);
    assert.match(f.statuses.at(-1)[0], /Font removed|Preparation refused/);
  }
});

test('native font changes preserve effective liga-off defaults and explicit saved native intent', async () => {
  for (const original of [text(), text({ text: '日本語\n世界', writingMode: 'vertical-rl' }),
    text({ osFont: { version: 2, face, outline: { ligatures: false } } }),
    text({ osFont: { version: 2, face, outline: { ligatures: true } } })]) {
    const expected = original.osFont?.outline.ligatures ?? false;
    const before = structuredClone(original);
    const f = fixture(original, { prepareWindows: async (source, exactFace, { settings }) => {
      assert.deepEqual(source, before); assert.equal(settings.ligatures, expected);
      return { face: exactFace, ligatures: settings.ligatures };
    } });
    const row = choice('windows', 'native', { face });
    await f.controller.select(row); assert.equal(await f.controller.apply(row), true);
    assert.equal(f.commands[0].payload.settings.ligatures, expected);
    assert.deepEqual(original, before);
  }
  const f = fixture(text(), { prepareWindows: async () => ({ face, ligatures: true }) });
  const row = choice('windows', 'native', { face });
  await f.controller.select(row); assert.equal(await f.controller.apply(row), false);
  assert.equal(f.commands.length, 0); assert.match(f.statuses.at(-1)[0], /did not preserve standard ligatures/);
});
