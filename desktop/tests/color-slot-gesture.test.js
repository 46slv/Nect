import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, serialize, deserialize } from '../src/core.js';

function fixture() {
  const s = new CommandSession(document()),
    send = (type, payload = {}) => s.dispatch({ ...s.state, type, payload });
  const ids = ['One', 'Two'].map((name) =>
    send('create-color-slot', { name, value: { hex: '#123456', alpha: 0.25 } }).slotId,
  );
  const updates = (hex = '#abcdef') =>
    ids.map((slotId) => ({ slotId, value: { hex, alpha: 0.25 } }));
  return { s, send, ids, updates };
}
const saved = (s) => ({
  document: serialize(s.snapshot()),
  state: s.state,
  past: structuredClone(s.historyView.past),
  future: structuredClone(s.historyView.future),
});

test('Color preview is detached and live while one commit creates exactly one Undo entry', () => {
  const f = fixture(),
    before = saved(f.s),
    target = { kind: 'color-slots', slotIds: [...f.ids] };
  let token = f.send('begin-gesture', { target });
  target.slotIds.reverse(); // Caller mutation cannot retarget the frozen draft.
  assert.equal(serialize(f.s.previewSnapshot(token.gestureId).document), before.document);
  for (const hex of ['#654321', '#abcdef']) {
    token = f.send('update-gesture', { ...token, update: { updates: f.updates(hex) } });
    assert.deepEqual(saved(f.s), before);
    const preview = f.s.previewSnapshot(token.gestureId).document;
    assert.ok(preview.colorSlots.every((slot) => slot.value.hex === hex));
    assert.ok(preview.colorSlots.every((slot) => slot.value.alpha === 0.25));
    preview.colorSlots[0].value.hex = '#000000';
    assert.equal(f.s.previewSnapshot(token.gestureId).document.colorSlots[0].value.hex, hex);
  }
  f.send('commit-gesture', token);
  assert.equal(f.s.hasGesture, false);
  assert.equal(f.s.historyView.past.length, before.past.length + 1);
  const after = serialize(f.s.snapshot());
  assert.equal(serialize(deserialize(after)), after);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before.document);
  f.send('redo');
  assert.equal(serialize(f.s.snapshot()), after);
});

test('opening, canceling and returning to the baseline do not mutate schema or history', () => {
  const f = fixture(),
    before = saved(f.s);
  let token = f.send('begin-gesture', { target: { kind: 'color-slots', slotIds: f.ids } });
  f.send('commit-gesture', token);
  assert.deepEqual(saved(f.s), before);
  token = f.send('begin-gesture', { target: { kind: 'color-slots', slotIds: f.ids } });
  token = f.send('update-gesture', { ...token, update: { updates: f.updates() } });
  f.send('cancel-gesture', token);
  assert.deepEqual(saved(f.s), before);
  token = f.send('begin-gesture', { target: { kind: 'color-slots', slotIds: f.ids } });
  token = f.send('update-gesture', { ...token, update: { updates: f.updates() } });
  token = f.send('update-gesture', { ...token, update: { updates: f.updates('#123456') } });
  f.send('commit-gesture', token);
  assert.deepEqual(saved(f.s), before);
});

test('unknown, duplicate, unbounded or expanded targets fail before opening a gesture', () => {
  const f = fixture(),
    before = saved(f.s);
  for (const target of [
    { kind: 'color-slots', slotIds: [] },
    { kind: 'color-slots', slotIds: ['missing'] },
    { kind: 'color-slots', slotIds: [f.ids[0], f.ids[0]] },
    { kind: 'color-slots', slotIds: Array(257).fill(f.ids[0]) },
    { kind: 'color-slots', slotIds: f.ids, command: 'delete-node' },
  ]) {
    assert.throws(() => f.send('begin-gesture', { target }));
    assert.equal(f.s.hasGesture, false);
    assert.deepEqual(saved(f.s), before);
  }
});

test('invalid updates preserve both the previous preview and committed state atomically', () => {
  const f = fixture(),
    before = saved(f.s),
    token = f.send('begin-gesture', { target: { kind: 'color-slots', slotIds: f.ids } });
  const preview = f.s.previewSnapshot(token.gestureId);
  for (const update of [
    { updates: [] },
    { updates: [f.updates()[0]] },
    { updates: [f.updates()[0], f.updates()[0]] },
    { updates: [f.updates()[0], { slotId: 'missing', value: { hex: '#abcdef', alpha: 1 } }] },
    { updates: [f.updates()[0], { slotId: f.ids[1], value: { hex: '#abcdef', alpha: 2 } }] },
    { updates: f.updates(), unrelated: true },
    { updates: [{ ...f.updates()[0], command: 'delete-node' }, f.updates()[1]] },
    { updates: f.updates(), design: { version: 99 } },
  ]) {
    assert.throws(() => f.send('update-gesture', { ...token, update }));
    assert.deepEqual(f.s.previewSnapshot(token.gestureId), preview);
    assert.deepEqual(saved(f.s), before);
  }
  f.send('cancel-gesture', token);
});

test('stale preview tokens and unrelated commands cannot change a Color gesture', () => {
  const f = fixture(),
    token = f.send('begin-gesture', { target: { kind: 'color-slots', slotIds: f.ids } }),
    next = f.send('update-gesture', { ...token, update: { updates: f.updates() } }),
    before = saved(f.s),
    preview = f.s.previewSnapshot(next.gestureId);
  assert.throws(() => f.send('commit-gesture', token), /STALE_GESTURE/);
  assert.throws(
    () => f.send('update-gesture', { ...token, update: { updates: f.updates() } }),
    /STALE_GESTURE/,
  );
  assert.throws(
    () => f.send('set-color-slot', { slotId: f.ids[0], value: { hex: '#000000', alpha: 1 } }),
    /GESTURE_ACTIVE/,
  );
  assert.deepEqual(saved(f.s), before);
  assert.deepEqual(f.s.previewSnapshot(next.gestureId), preview);
  f.send('cancel-gesture', next);
});

test('validated recipe and Keep identities preview and commit with literal alpha together', () => {
  const f = fixture(),
    before = serialize(f.s.snapshot()),
    design = {
      version: 1,
      profile: 'balanced',
      colorfulness: 0.5,
      areaAware: false,
      lockedSlotIds: [f.ids[0]],
    },
    values = f.updates();
  values[0].value = { hex: '#123456', alpha: 0.25 };
  values[1].value.alpha = 0;
  let token = f.send('begin-gesture', { target: { kind: 'color-slots', slotIds: f.ids } });
  token = f.send('update-gesture', { ...token, update: { updates: values, design } });
  const preview = f.s.previewSnapshot(token.gestureId).document;
  assert.deepEqual(preview.colorDesign, design);
  assert.deepEqual(preview.colorSlots[0].value, values[0].value);
  assert.equal(preview.colorSlots[1].value.alpha, 0);
  assert.equal(serialize(f.s.snapshot()), before);
  f.send('commit-gesture', token);
  assert.deepEqual(f.s.snapshot().colorDesign, design);
  assert.equal(f.s.snapshot().colorSlots[1].value.alpha, 0);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
});
