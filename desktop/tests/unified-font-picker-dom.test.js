import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { document as newDocument, node, serialize } from '../src/core.js';
import { makeFontAsset } from '../src/fonts.js';

// Synthetic native responses prove UI/routing and history, not real installed
// Adobe/Morisawa availability, font licensing, font fidelity or WebView2 behavior.
const exactFace = {
  family: 'Synthetic native fixture',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const fileBytes = async (relative) => {
  const bytes = await fs.readFile(new URL(relative, import.meta.url));
  return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
};
const deferred = () => {
  let resolve;
  const promise = new Promise((done) => {
    resolve = done;
  });
  return { promise, resolve };
};
function preparedOutline(request) {
  const modern = request.axes !== undefined || request.writingMode !== undefined;
  return {
    ...request,
    engine: modern ? 'directwrite-v2' : 'directwrite-v1',
    ...(modern ? { axes: [], advanceHeight: request.size * 4 } : {}),
    ascent: request.size * 0.8,
    advanceWidth: request.size * 2,
    lineCount: request.text.split('\n').length,
    glyphCount: [...request.text].length,
    points: [
      [0, 0],
      [20, 0],
      [20, 10],
      [0, 10],
    ],
    types: [0, 1, 1, 129],
  };
}

test('one real app Font picker preserves editable Text, native intent and atomic history across interrupted flows', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'Event', 'localStorage'])
    globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return this;
    }
  };
  const matrix = {
    a: 1,
    b: 0,
    c: 0,
    d: 1,
    e: 0,
    f: 0,
    inverse() {
      return this;
    },
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: () => fileBytes('../public/fonts/DejaVuSans.ttf'),
  });
  t.after(() => win.happyDOM.abort());

  const calls = [];
  let byteReads = 0;
  let shape = async (request) => preparedOutline(request);
  globalThis.unifiedNativeFaces = [exactFace];
  globalThis.unifiedShape = (request) => {
    calls.push(structuredClone(request));
    return shape(request);
  };
  globalThis.unifiedReadBytes = () => {
    byteReads++;
    throw Error('Normal Font selection must not copy installed font bytes');
  };
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source.replace("import './style.css';", '').replace(
    /import \{[^}]*\} from '.\/storage.js';/,
    `import { DocumentLifecycle } from '${new URL('../src/document-lifecycle.js', import.meta.url)}';
const desktop = true, releaseProject = async () => {}, writeProject = async () => null,
  saveFile = async () => true, openFile = async () => null, openSVGSource = async () => null,
  chooseUnsaved = async () => 'cancel', registerClose = async () => {},
  beginRecovery = async () => 'unified-font-test', writeRecovery = async () => Date.now(),
  listRecovery = async () => [], readRecovery = async () => '', acknowledgeRecovery = async () => {},
  listSystemFonts = async () => [['system-byte-only', 'Byte-only discovery fixture', null,
    { family: 'Discovery fixture', subfamily: 'Regular', weightClass: 400,
      localizedNames: ['合成ディスカバリー', 'ＭＳ 合成'] }]],
  readSystemFont = () => globalThis.unifiedReadBytes(),
  listOsOutlineFonts = async () => [], listDesktopFontFaces = async () => globalThis.unifiedNativeFaces,
  listDesktopFontAxes = async () => [], shapeDesktopText = (request) => globalThis.unifiedShape(request),
  outlineOsText = async () => { throw Error('Legacy name-based native fallback must not run'); };`,
  );
  source = resolveMainRelativeImports(source);
  source +=
    '\nwindow.unifiedFontDebug = () => ({ past: history.past.length, bundled: !!bundledFont, pending: osFontCatalogPending, refresh: () => refreshOsFontCatalog() });';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));

  const q = (selector) => document.querySelector(selector);
  const tick = () => new Promise((done) => setTimeout(done, 20));
  const until = async (predicate, message = 'font UI operation') => {
    const deadline = Date.now() + 6000;
    while (!predicate()) {
      assert.ok(Date.now() < deadline, `Timed out: ${message}`);
      await new Promise((done) => setTimeout(done, 10));
    }
  };
  await until(() => win.nect && win.unifiedFontDebug().bundled, 'bundled font initialization');
  await tick();
  const asset = makeFontAsset(
    await fileBytes('./fixtures/fonts/NectJPFixture-Regular.otf'),
    'fixture.otf',
  );
  const snapshot = () => JSON.stringify(win.nect.document);
  const past = () => win.unifiedFontDebug().past;
  const selectText = (id) => {
    const head = q(`.graph .node[data-id="${id}"] .node-head`);
    assert.ok(head, 'real graph Text node exists');
    head.dispatchEvent(
      new win.PointerEvent('pointerdown', { bubbles: true, pointerId: 1, button: 0 }),
    );
    document.dispatchEvent(
      new win.PointerEvent('pointerup', { bubbles: true, pointerId: 1, button: 0 }),
    );
  };
  const setup = (patch = {}, includeAsset = false) => {
    const doc = newDocument(),
      text = node('Text'),
      output = node('Output');
    Object.assign(text, { text: 'ABC', ...patch });
    output.params.path.source = text.id;
    doc.layers[0].nodes = [text, output];
    doc.layers[0].output = output.id;
    if (includeAsset) doc.fontAssets = { [asset.id]: asset.asset };
    if (text.fontFeatures) doc.schemaVersion = Math.max(doc.schemaVersion, 38);
    win.nect.load(serialize(doc));
    selectText(text.id);
    return {
      text,
      doc,
      live: () => win.nect.document.layers[0].nodes.find((n) => n.id === text.id),
    };
  };
  const open = async () => {
    const control = q('.inspector-body [data-change-font]');
    assert.ok(control, 'normal Font entry exists');
    control.dispatchEvent(new win.PointerEvent('pointerdown', { bubbles: true, button: 0 }));
    await until(() => q('dialog[aria-label="Font"]'), 'single Font picker');
    await tick();
    return q('dialog[aria-label="Font"]');
  };
  const search = async (picker, value) => {
    const input = picker.querySelector('input[type="search"]');
    input.value = value;
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
    await tick();
    return [...picker.querySelectorAll('.picker-option')];
  };
  const choose = async (picker, value) => {
    const rows = await search(picker, value);
    const row = rows.find((candidate) => candidate.getAttribute('aria-disabled') !== 'true');
    assert.ok(row, `enabled exact choice for ${value}`);
    row.click();
    await tick();
  };
  const apply = (picker) => picker.querySelector('.picker-apply').click();
  const cancel = (picker) => picker.querySelector('.picker-cancel').click();
  const escape = (picker) =>
    picker.dispatchEvent(
      new win.KeyboardEvent('keydown', {
        key: 'Escape',
        bubbles: true,
        cancelable: true,
      }),
    );

  await t.test(
    'an open Font picker updates after catalog completion without committing',
    async () => {
      const f = setup(),
        before = snapshot(),
        commandBefore = structuredClone(win.nect.commandState),
        lateFace = { ...exactFace, family: 'Delayed native fixture' },
        wait = deferred();
      globalThis.unifiedNativeFaces = wait.promise;
      const refresh = win.unifiedFontDebug().refresh();
      await until(() => win.unifiedFontDebug().pending, 'pending catalog');
      let picker = await open();
      await search(picker, lateFace.family);
      const input = picker.querySelector('input[type="search"]');
      input.focus();
      input.setSelectionRange(2, 5);
      assert.equal(picker.querySelectorAll('.picker-option').length, 0);
      wait.resolve([exactFace, lateFace]);
      await refresh;
      await tick();
      let rows = [...picker.querySelectorAll('.picker-option')];
      assert.equal(rows.length, 1, 'new native face appears without reopening');
      assert.equal(rows[0].getAttribute('aria-disabled'), 'false');
      assert.equal(rows[0].getAttribute('aria-selected'), 'false', 'refresh does not select a row');
      assert.equal(picker.querySelector('.picker-apply').disabled, true);
      assert.notEqual(picker.querySelector('.picker-status').textContent, 'No matches');
      assert.equal(input.value, lateFace.family);
      assert.equal(document.activeElement, input);
      assert.equal(input.selectionStart, 2);
      assert.equal(input.selectionEnd, 5);
      assert.equal(snapshot(), before);
      assert.equal(past(), 0);
      assert.deepEqual(win.nect.commandState, commandBefore);
      assert.equal(f.live().osFont, undefined);
      cancel(picker);

      const selectedWait = deferred();
      globalThis.unifiedNativeFaces = selectedWait.promise;
      const selectedRefresh = win.unifiedFontDebug().refresh();
      await until(() => win.unifiedFontDebug().pending, 'second pending catalog');
      picker = await open();
      await choose(picker, exactFace.family);
      const selectedRow = picker.querySelector('.picker-option[aria-selected="true"]'),
        selected = selectedRow.dataset.id,
        selectedFamily = selectedRow.closest('.picker-family');
      const focusedFamily = selectedFamily.dataset.fontFamily;
      selectedFamily.focus();
      selectedWait.resolve([exactFace, lateFace, { ...exactFace, name: 'Bold', weight: 700 }]);
      await selectedRefresh;
      await tick();
      assert.equal(
        picker.querySelector('.picker-option[aria-selected="true"]').dataset.id,
        selected,
      );
      assert.equal(document.activeElement.dataset.fontFamily, focusedFamily);
      assert.equal(snapshot(), before);
      assert.equal(past(), 0);
      cancel(picker);

      const filterWait = deferred(),
        filteredFace = { ...exactFace, family: 'Discovery fixture' };
      globalThis.unifiedNativeFaces = filterWait.promise;
      const filterRefresh = win.unifiedFontDebug().refresh();
      await until(() => win.unifiedFontDebug().pending, 'catalog with active filters');
      picker = await open();
      const familyFilter = picker.querySelector('[aria-label="Font family"]'),
        weightFilter = picker.querySelector('[aria-label="Available weight"]');
      familyFilter.value = `family:${filteredFace.family}`;
      familyFilter.dispatchEvent(new win.Event('change', { bubbles: true }));
      weightFilter.value = '400';
      weightFilter.dispatchEvent(new win.Event('change', { bubbles: true }));
      weightFilter.focus();
      filterWait.resolve([exactFace, lateFace, filteredFace]);
      await filterRefresh;
      await tick();
      assert.equal(familyFilter.value, `family:${filteredFace.family}`);
      assert.equal(weightFilter.value, '400');
      assert.equal(document.activeElement, weightFilter);
      assert.equal(
        picker.querySelectorAll('.picker-option[aria-disabled="false"]').length,
        1,
        'a newly discovered source is expanded for the active family filter',
      );
      assert.equal(snapshot(), before);
      assert.equal(past(), 0);
      cancel(picker);

      const imeWait = deferred(),
        imeFace = { ...exactFace, family: 'Composing native fixture' };
      globalThis.unifiedNativeFaces = imeWait.promise;
      const imeRefresh = win.unifiedFontDebug().refresh();
      await until(() => win.unifiedFontDebug().pending, 'catalog during composition');
      picker = await open();
      await search(picker, imeFace.family);
      const imeInput = picker.querySelector('input[type="search"]');
      imeInput.focus();
      imeInput.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
      imeWait.resolve([exactFace, lateFace, imeFace]);
      await imeRefresh;
      assert.equal(
        picker.querySelectorAll('.picker-option').length,
        0,
        'do not rebuild during IME',
      );
      imeInput.dispatchEvent(new win.CompositionEvent('compositionend', { bubbles: true }));
      await tick();
      assert.equal(picker.querySelectorAll('.picker-option').length, 1);
      assert.equal(document.activeElement, imeInput);
      assert.equal(snapshot(), before);
      cancel(picker);

      for (const composing of [false, true]) {
        const removingWait = deferred(),
          shapingWait = deferred();
        let pendingShape;
        globalThis.unifiedNativeFaces = removingWait.promise;
        const removingRefresh = win.unifiedFontDebug().refresh();
        shape = (request) => {
          pendingShape = request;
          return shapingWait.promise;
        };
        await until(() => win.unifiedFontDebug().pending, 'catalog before native preparation');
        picker = await open();
        await choose(picker, exactFace.family);
        apply(picker);
        await until(() => pendingShape, 'native preparation before catalog removal');
        const removingInput = picker.querySelector('input[type="search"]');
        if (composing) {
          removingInput.focus();
          removingInput.dispatchEvent(
            new win.CompositionEvent('compositionstart', { bubbles: true }),
          );
        }
        removingWait.resolve([lateFace]);
        await removingRefresh;
        await tick();
        assert.equal(picker.querySelector('.picker-apply').disabled, true);
        shapingWait.resolve(preparedOutline(pendingShape));
        await tick();
        assert.equal(snapshot(), before, 'removed choice cannot commit a late native result');
        assert.equal(past(), 0);
        if (composing)
          removingInput.dispatchEvent(
            new win.CompositionEvent('compositionend', { bubbles: true }),
          );
        cancel(picker);
        globalThis.unifiedNativeFaces = [exactFace, lateFace];
        await win.unifiedFontDebug().refresh();
      }
      shape = async (request) => preparedOutline(request);
      calls.length = 0;

      const canceledWait = deferred();
      globalThis.unifiedNativeFaces = canceledWait.promise;
      const canceledRefresh = win.unifiedFontDebug().refresh();
      await until(() => win.unifiedFontDebug().pending, 'catalog before cancel');
      picker = await open();
      cancel(picker);
      canceledWait.resolve([exactFace, lateFace]);
      await canceledRefresh;
      await tick();
      assert.equal(
        q('dialog[aria-label="Font"]'),
        null,
        'late completion does not reopen a canceled picker',
      );
      assert.equal(snapshot(), before);
      assert.equal(past(), 0);
      globalThis.unifiedNativeFaces = [exactFace];
      await win.unifiedFontDebug().refresh();
    },
  );

  let f = setup(),
    before = snapshot();
  assert.equal(document.querySelectorAll('.inspector-body [data-change-font]').length, 1);
  assert.equal(
    document.querySelectorAll(`.graph .node[data-id="${f.text.id}"] [data-change-font]`).length,
    1,
  );
  assert.equal(document.querySelectorAll('[data-os-text-font], [data-os-text-outline]').length, 0);
  assert.doesNotMatch(
    q('.inspector-body .text-controls').textContent,
    /Adobe Fonts|Choose OS font/,
  );
  assert.ok(q('.inspector-body [data-load-font]').closest('.text-font-advanced'));
  assert.ok(q('.inspector-body [data-load-variable-font]').closest('.text-font-advanced'));
  assert.ok(q('.inspector-body [data-bake-text-outline]').closest('.text-actions'));

  let picker = await open();
  const family = () =>
    [...picker.querySelectorAll('.picker-family')].find(
      (g) => g.getAttribute('aria-label') === exactFace.family,
    );
  assert.equal(picker.querySelector('.picker-list').getAttribute('role'), 'tree');
  assert.equal(family().getAttribute('aria-expanded'), 'false');
  assert.ok(
    picker.querySelector('.picker-family[aria-expanded="true"]'),
    'selected family starts open',
  );
  for (const [key, expanded] of [
    ['Enter', 'true'],
    [' ', 'false'],
    ['ArrowRight', 'true'],
    ['ArrowLeft', 'false'],
  ]) {
    family().focus();
    family().dispatchEvent(
      new win.KeyboardEvent('keydown', { key, bubbles: true, cancelable: true }),
    );
    assert.equal(family().getAttribute('aria-expanded'), expanded);
  }
  assert.equal(snapshot(), before);

  let rows = await search(picker, '合成ディスカバリー');
  assert.equal(rows.length, 1, 'actual supplied localized alias remains searchable');
  assert.equal(rows[0].getAttribute('aria-disabled'), 'true');
  assert.match(
    rows[0].textContent,
    /Font file import is required; use Advanced for a file you may embed/,
  );
  rows[0].click();
  assert.equal(picker.querySelector('.picker-apply').disabled, true);
  assert.equal(byteReads, 0);
  assert.equal(snapshot(), before);
  cancel(picker);

  picker = await open();
  await choose(picker, exactFace.family);
  assert.equal(calls.length, 0, 'native preparation is explicit Apply, not browsing');
  apply(picker);
  await until(() => !q('dialog[aria-label="Font"]'), 'native adoption');
  assert.equal(f.live().type, 'Text');
  assert.deepEqual(f.live().osFont.face, exactFace);
  assert.equal(
    f.live().osFont.outline.ligatures,
    false,
    'Legacy static effective liga-off is preserved',
  );
  assert.equal(calls.at(-1).ligatures, false);
  assert.equal(past(), 1);
  assert.equal(byteReads, 0);
  assert.equal(Object.keys(win.nect.document.fontAssets || {}).length, 0);
  const native = snapshot(),
    nativeSVG = win.nect.export();
  q('[data-action="undo"]').click();
  assert.equal(snapshot(), before, 'one Undo exactly restores editable static Text');
  q('[data-action="redo"]').click();
  assert.equal(snapshot(), native);
  assert.equal(win.nect.export(), nativeSVG);
  assert.equal(document.querySelectorAll('.inspector-body [data-change-font]').length, 1);

  picker = await open();
  await choose(picker, 'DejaVu Sans');
  apply(picker);
  await until(() => !q('dialog[aria-label="Font"]'), 'atomic native-to-embedded replacement');
  assert.equal(f.live().type, 'Text');
  assert.equal(f.live().osFont, undefined);
  assert.equal(f.live().font, 'default');
  assert.equal(past(), 2);
  const embedded = snapshot();
  q('[data-action="undo"]').click();
  assert.equal(snapshot(), native, 'one Undo exactly restores saved native snapshot');
  q('[data-action="redo"]').click();
  assert.equal(snapshot(), embedded);

  f = setup();
  before = snapshot();
  let wait = deferred(),
    pendingRequest;
  shape = (request) => {
    pendingRequest = request;
    return wait.promise;
  };
  picker = await open();
  await choose(picker, exactFace.family);
  apply(picker);
  await until(() => !!pendingRequest, 'pending native request');
  const callCount = calls.length;
  apply(picker);
  assert.equal(calls.length, callCount, 'repeated Apply does not duplicate preparation');
  escape(picker);
  await until(() => !q('dialog[aria-label="Font"]'), 'Escape dismissal');
  wait.resolve(preparedOutline(pendingRequest));
  await tick();
  assert.equal(snapshot(), before);
  assert.equal(past(), 0);

  f = setup({}, true);
  before = snapshot();
  wait = deferred();
  pendingRequest = null;
  shape = (request) => {
    pendingRequest = request;
    return wait.promise;
  };
  picker = await open();
  await choose(picker, exactFace.family);
  apply(picker);
  await until(() => !!pendingRequest, 'older native preparation');
  await choose(picker, asset.asset.name);
  wait.resolve(preparedOutline(pendingRequest));
  await tick();
  assert.equal(snapshot(), before, 'older native completion never commits a newer selection');
  apply(picker);
  await until(() => !q('dialog[aria-label="Font"]'), 'newer embedded choice');
  assert.equal(f.live().font, asset.id);
  assert.equal(f.live().osFont, undefined);
  assert.equal(past(), 1);

  f = setup();
  wait = deferred();
  pendingRequest = null;
  shape = (request) => {
    pendingRequest = request;
    return wait.promise;
  };
  picker = await open();
  await choose(picker, exactFace.family);
  apply(picker);
  await until(() => !!pendingRequest, 'stale revision preparation');
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-text-content',
    payload: {
      layerId: f.doc.layers[0].id,
      nodeId: f.text.id,
      text: 'Newer edit',
    },
  });
  const newer = snapshot();
  wait.resolve(preparedOutline(pendingRequest));
  await tick();
  assert.equal(snapshot(), newer, 'late native response cannot overwrite a newer authored edit');
  assert.equal(f.live().osFont, undefined);
  assert.equal(past(), 1);
  if (q('dialog[aria-label="Font"]')) cancel(q('dialog[aria-label="Font"]'));

  f = setup({ fontFeatures: { liga: false } });
  before = snapshot();
  shape = async (request) => preparedOutline(request);
  const callsBeforeUnsupported = calls.length;
  picker = await open();
  rows = await search(picker, exactFace.family);
  assert.equal(rows.length, 1);
  assert.equal(rows[0].getAttribute('aria-disabled'), 'true');
  assert.match(rows[0].textContent, /cannot preserve explicit OpenType settings/);
  assert.equal(calls.length, callsBeforeUnsupported);
  cancel(picker);
  assert.equal(snapshot(), before, 'unsupported choice preserves every explicit override');

  f = setup({ text: '日本語\n世界', writingMode: 'vertical-rl', font: asset.id }, true);
  before = snapshot();
  const original = structuredClone(f.live());
  picker = await open();
  await choose(picker, exactFace.family);
  apply(picker);
  await until(() => !q('dialog[aria-label="Font"]'), 'multiline vertical native choice');
  assert.equal(f.live().type, 'Text');
  assert.equal(f.live().text, original.text);
  assert.equal(f.live().writingMode, 'vertical-rl');
  assert.deepEqual(f.live().params, original.params);
  assert.equal(calls.at(-1).writingMode, 'vertical-rl');
  assert.equal(calls.at(-1).ligatures, false);
  assert.equal(past(), 1);
  q('[data-action="undo"]').click();
  assert.equal(snapshot(), before);
  assert.equal(byteReads, 0, 'none of the normal choices read or embed installed font bytes');
});
