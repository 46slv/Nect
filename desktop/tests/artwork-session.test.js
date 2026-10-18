import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, blankDocument, serialize } from '../src/core.js';
const png =
  'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';
const fixture = () => {
  const session = new CommandSession(blankDocument()),
    send = (type, payload) => session.dispatch({ ...session.state, type, payload });
  return { session, send };
};

test('Text creation direction is a shared atomic command and preserves existing authored Text', () => {
  const { session: s, send } = fixture();
  const first = send('create-artwork', { artworkType: 'Text', at: [40, 50] });
  send('set-text-content', { layerId: first.layerId, nodeId: first.nodeId, text: 'Existing Text' });
  const existing = s.snapshot().layers.at(-1);
  for (const writingMode of ['horizontal-tb', 'vertical-rl']) {
    const before = serialize(s.snapshot()),
      revision = s.state.expectedRevision;
    send('create-artwork', { artworkType: 'Text', at: [120, 90], writingMode });
    const created = serialize(s.snapshot()),
      text = s.snapshot().layers.at(-1).nodes[0];
    assert.equal(text.writingMode, writingMode);
    assert.deepEqual(text.params.position.value, [120, 90]);
    assert.deepEqual(
      s.snapshot().layers.find((l) => l.id === first.layerId),
      existing,
    );
    assert.equal(s.state.expectedRevision, revision + 1);
    send('undo');
    assert.equal(serialize(s.snapshot()), before);
    send('redo');
    assert.equal(serialize(s.snapshot()), created);
  }
  send('undo');
  const before = serialize(s.snapshot()),
    state = s.state,
    future = s.historyView.future.length;
  for (const payload of [
    { artworkType: 'Text', writingMode: 'sideways' },
    { artworkType: 'Circle', writingMode: 'vertical-rl' },
  ]) {
    assert.throws(() => send('create-artwork', { at: [0, 0], ...payload }), /writing mode/);
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
    assert.equal(s.historyView.future.length, future);
  }
});

test('creation/duplication/order/deletion share stable identities and exact one-step History', () => {
  const { session: s, send } = fixture(),
    original = serialize(s.snapshot());
  const first = send('create-artwork', { artworkType: 'Rectangle', at: [80, 90] });
  const created = serialize(s.snapshot());
  assert.deepEqual(s.snapshot().layers.at(-1).nodes[0].params.center.value, [80, 90]);
  send('undo');
  assert.equal(serialize(s.snapshot()), original);
  send('redo');
  assert.equal(serialize(s.snapshot()), created);
  const copied = send('duplicate-artworks', { layerIds: [first.layerId] }).copies[0];
  assert.notEqual(copied.layerId, first.layerId);
  const duplicate = s.snapshot().layers.find((l) => l.id === copied.layerId),
    source = s.snapshot().layers.find((l) => l.id === first.layerId);
  assert.equal(
    new Set([...source.nodes, ...duplicate.nodes].map((n) => n.id)).size,
    source.nodes.length + duplicate.nodes.length,
  );
  const boundary = s.state.expectedRevision;
  assert.equal(send('order-layer', { layerId: copied.layerId, direction: 'forward' }).moved, false);
  assert.equal(s.state.expectedRevision, boundary);
  send('order-layer', { layerId: copied.layerId, direction: 'backward' });
  const beforeDelete = serialize(s.snapshot());
  send('delete-layer', { layerId: first.layerId });
  assert.ok(s.snapshot().layers.some((l) => l.id === copied.layerId));
  assert.ok(!s.snapshot().layers.some((l) => l.id === first.layerId));
  send('undo');
  assert.equal(serialize(s.snapshot()), beforeDelete);
});

test('late invalid batch, locked deletion and stale creation preserve state, revision and redo', () => {
  const { session: s, send } = fixture();
  const shape = send('create-artwork', { artworkType: 'Circle', at: [20, 30] });
  const blank = send('create-blank-layer');
  const before = serialize(s.snapshot()),
    state = s.state;
  for (const [type, payload] of [
    ['duplicate-artworks', { layerIds: [shape.layerId, blank.layerId] }],
    ['duplicate-artworks', { layerIds: [shape.layerId, shape.layerId] }],
    ['create-artwork', { artworkType: 'unsupported', at: [0, 0] }],
    ['create-artwork', { artworkType: 'Circle', at: [NaN, 0] }],
    ['delete-layer', { layerId: 'missing' }],
  ]) {
    assert.throws(() => send(type, payload));
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
  }
  const locked = s.snapshot();
  locked.layers[0].locked = true;
  const other = new CommandSession(locked),
    old = serialize(other.snapshot());
  assert.throws(
    () =>
      other.dispatch({
        ...other.state,
        type: 'delete-layer',
        payload: { layerId: locked.layers[0].id },
      }),
    /locked/,
  );
  assert.equal(serialize(other.snapshot()), old);
  // An independent new composition is permitted while an existing one is locked.
  other.dispatch({ ...other.state, type: 'create-blank-layer' });
  send('undo');
  const redo = s.historyView.future.length;
  assert.throws(() => s.dispatch({ ...state, type: 'create-blank-layer' }), /STALE_REVISION/);
  assert.equal(s.historyView.future.length, redo);
});

test('Image adoption derives dimensions from PNG bytes and commits asset/reference together', () => {
  const { session: s, send } = fixture(),
    before = serialize(s.snapshot());
  const result = send('adopt-image', { name: 'red.png', png });
  const doc = s.snapshot(),
    image = doc.layers.find((l) => l.id === result.layerId).nodes[0];
  assert.equal(image.asset, result.assetId);
  assert.deepEqual(image.params.size.value, [1, 1]);
  assert.equal(doc.imageAssets[result.assetId].width, 1);
  const after = serialize(doc),
    state = s.state;
  assert.throws(() => send('adopt-image', { name: 'bad.png', png: png.slice(0, -8) }));
  assert.equal(serialize(s.snapshot()), after);
  assert.deepEqual(s.state, state);
  send('undo');
  assert.equal(serialize(s.snapshot()), before);
  send('redo');
  assert.equal(serialize(s.snapshot()), after);
  assert.throws(
    () => s.dispatch({ ...state, type: 'adopt-image', payload: { name: 'late.png', png } }),
    /STALE_REVISION/,
  );
  assert.equal(serialize(s.snapshot()), after);
});
