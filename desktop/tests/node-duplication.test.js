import test from 'node:test';
import assert from 'node:assert/strict';
import { adjacentNodeOffset, intersects } from '../src/layout.js';
import { CommandSession, blankDocument, node, serialize } from '../src/core.js';

test('adjacent placement uses world measurements and skips crowded cards', () => {
  const nodes = [node('Circle', 100, 100), node('Curve', 450, 150), node('Fill', 1000, 100)];
  const sizes = Object.fromEntries(
    nodes.map((n, i) => [n.id, { width: 300, height: i === 1 ? 700 : 250 }]),
  );
  const offset = adjacentNodeOffset(nodes, [nodes[0].id, nodes[1].id], sizes);
  assert.deepEqual(offset, [1224, 0]);
  for (const n of nodes.slice(0, 2)) {
    const copy = { x: n.position[0] + offset[0], y: n.position[1], ...sizes[n.id] };
    for (const other of nodes)
      assert.equal(
        intersects(copy, { x: other.position[0], y: other.position[1], ...sizes[other.id] }),
        false,
      );
  }
  assert.deepEqual(adjacentNodeOffset([nodes[0]], [nodes[0].id], sizes), [324, 0]);
});

test('Session offset retains relative layout, external wires and one-step undo/redo', () => {
  const doc = blankDocument(),
    l = doc.layers[0];
  const source = node('Circle', 20, 40),
    fill = node('Fill', 400, 90),
    stroke = node('Stroke', 800, 150);
  fill.params.path.source = source.id;
  stroke.params.path.source = fill.id;
  l.nodes.push(source, fill, stroke);
  const session = new CommandSession(doc);
  const send = (type, payload) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const before = serialize(session.snapshot());
  const result = send('duplicate-nodes', { nodeIds: [stroke.id, fill.id], offset: [950, 0] });
  const copies = session.snapshot().layers[0].nodes.filter((n) => result.nodeIds.includes(n.id));
  assert.deepEqual(
    copies.map((n) => n.position),
    [
      [1350, 90],
      [1750, 150],
    ],
  );
  assert.equal(copies[0].params.path.source, source.id);
  assert.equal(copies[1].params.path.source, copies[0].id);
  const after = serialize(session.snapshot());
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), after);
  for (const offset of [[Infinity, 0], [1], 'bad', [0, NaN]]) {
    const revision = session.state.expectedRevision;
    assert.throws(() => send('duplicate-nodes', { nodeIds: [source.id], offset }));
    assert.equal(serialize(session.snapshot()), after);
    assert.equal(session.state.expectedRevision, revision);
  }
  assert.throws(() =>
    send('duplicate-nodes', { nodeIds: [source.id], offset: [1, 2], position: [3, 4] }),
  );
  assert.equal(serialize(session.snapshot()), after);
});
