import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { CommandSession, blankDocument, serialize, deserialize } from '../src/core.js';
import { readSVGSource } from '../src/svg-tree.js';
const Parser = new Window().DOMParser;
const source = () =>
  readSVGSource(
    '<svg width="40" height="20"><rect width="40" height="20" fill="#2468ab"/></svg>',
    Parser,
  );
const png =
  'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';
const provenance = () => ({
  version: 1,
  mode: 'embedded',
  rootId: 'owned',
  relativePath: 'part.svg',
  sourceLocation: 'owned/part.svg',
  sha256: 'a'.repeat(64),
  author: '',
  source: '',
  license: '',
});
function setup() {
  const s = new CommandSession(blankDocument());
  const send = (type, payload = {}) => s.dispatch({ ...s.state, type, payload });
  return { s, send, layerId: s.snapshot().layers[0].id };
}
test('SVG graph placement separates graph-card coordinates from artwork coordinates', () => {
  const f = setup();
  const before = serialize(f.s.snapshot());
  const result = f.send('insert-svg-asset', {
    route: 'graph',
    layerId: f.layerId,
    source: source(),
    name: 'Part',
    provenance: provenance(),
    nodePosition: [310, 220],
    at: [480, 270],
    anchor: 'center',
  });
  const after = f.s.snapshot();
  const node = after.layers
    .find((l) => l.id === f.layerId)
    .nodes.find((n) => n.id === result.nodeId);
  assert.deepEqual(node.position, [310, 220]);
  assert.deepEqual(node.params.position.value, [460, 260]);
  assert.equal(result.joinId, null);
  assert.deepEqual(deserialize(serialize(after)), after);
  f.send('undo');
  assert.equal(serialize(f.s.snapshot()), before);
  f.send('redo');
  assert.deepEqual(f.s.snapshot(), after);
});
test('SVG origin placement remains backward compatible and invalid anchors are atomic', () => {
  const f = setup();
  const result = f.send('insert-svg-asset', {
    route: 'layer',
    source: source(),
    name: 'Part',
    provenance: provenance(),
    at: [50, 70],
  });
  const node = f.s
    .snapshot()
    .layers.find((l) => l.id === result.layerId)
    .nodes.find((n) => n.id === result.nodeId);
  assert.deepEqual(node.params.position.value, [50, 70]);
  const before = serialize(f.s.snapshot()),
    state = f.s.state;
  for (const bad of [
    { anchor: 'guess' },
    { nodePosition: [NaN, 20] },
    { nodePosition: [100001, 20] },
  ]) {
    assert.throws(() =>
      f.send('insert-svg-asset', {
        route: 'layer',
        source: source(),
        name: 'Part',
        provenance: provenance(),
        ...bad,
      }),
    );
    assert.equal(serialize(f.s.snapshot()), before);
    assert.deepEqual(f.s.state, state);
  }
});
test('image graph placement is disconnected, independent and shares one Undo', () => {
  const f = setup();
  const before = f.s.snapshot();
  const output = before.layers[0].output;
  const result = f.send('adopt-image', {
    route: 'graph',
    layerId: f.layerId,
    png,
    name: 'Part.png',
    provenance: { ...provenance(), relativePath: 'Part.png' },
    nodePosition: [200, 300],
    at: [50, 60],
    anchor: 'center',
  });
  const after = f.s.snapshot(),
    layer = after.layers[0],
    node = layer.nodes.find((n) => n.id === result.nodeId);
  assert.equal(after.layers.length, before.layers.length);
  assert.equal(layer.output, output);
  assert.deepEqual(
    layer.nodes.filter((n) => n.id !== node.id),
    before.layers[0].nodes,
  );
  assert.equal(node.type, 'Image');
  assert.deepEqual(node.position, [200, 300]);
  assert.deepEqual(node.params.position.value, [49.5, 59.5]);
  assert.ok(after.imageAssets[result.assetId]);
  assert.deepEqual(deserialize(serialize(after)), after);
  f.send('undo');
  assert.deepEqual(f.s.snapshot(), before);
  f.send('redo');
  assert.deepEqual(f.s.snapshot(), after);
});
test('image canvas placement supports explicit coordinates without changing old default', () => {
  const f = setup();
  const result = f.send('adopt-image', { png, name: 'Part.png', at: [100, 80] });
  const node = f.s
    .snapshot()
    .layers.find((l) => l.id === result.layerId)
    .nodes.find((n) => n.id === result.nodeId);
  assert.deepEqual(node.params.position.value, [100, 80]);
  const before = serialize(f.s.snapshot()),
    state = f.s.state;
  for (const bad of [
    { route: 'unknown' },
    { route: 'graph', layerId: 'missing' },
    { at: [Infinity, 0] },
    { nodePosition: [1] },
    { anchor: 'unknown' },
    { route: 'graph', layerId: f.layerId, joinBeforeOut: true },
  ]) {
    assert.throws(() => f.send('adopt-image', { png, name: 'Part.png', ...bad }));
    assert.equal(serialize(f.s.snapshot()), before);
    assert.deepEqual(f.s.state, state);
  }
});
