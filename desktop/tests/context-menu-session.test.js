import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, blankDocument, blankLayer, node, serialize } from '../src/core.js';

test('delete-layers is atomic, keeps survivors and assets, and restores with one Undo/Redo', () => {
  const doc = blankDocument();
  doc.layers.push(blankLayer('B'), blankLayer('C'));
  const s = new CommandSession(doc);
  const send = (type, payload) => s.dispatch({ ...s.state, type, payload });
  const before = serialize(s.snapshot()),
    past = s.historyView.past.length;
  send('delete-layers', { layerIds: [doc.layers[2].id, doc.layers[0].id] });
  assert.deepEqual(
    s.snapshot().layers.map((l) => l.id),
    [doc.layers[1].id],
  );
  assert.equal(s.historyView.past.length, past + 1);
  const after = serialize(s.snapshot());
  send('undo');
  assert.equal(serialize(s.snapshot()), before);
  send('redo');
  assert.equal(serialize(s.snapshot()), after);
  send('delete-layers', { layerIds: [doc.layers[1].id] });
  assert.equal(s.snapshot().layers.length, 1);
  assert.equal(s.snapshot().layers[0].nodes.length, 1);
  assert.notEqual(s.snapshot().layers[0].id, doc.layers[1].id);
});

test('invalid, locked and stale multi-layer deletion preserve document, revision and redo', () => {
  const doc = blankDocument();
  doc.layers.push(blankLayer('Locked'));
  doc.layers[1].locked = true;
  const s = new CommandSession(doc);
  const before = serialize(s.snapshot()),
    state = s.state;
  for (const ids of [
    [],
    [doc.layers[0].id, 'missing'],
    [doc.layers[0].id, doc.layers[0].id],
    doc.layers.map((l) => l.id),
  ]) {
    assert.throws(() =>
      s.dispatch({ ...s.state, type: 'delete-layers', payload: { layerIds: ids } }),
    );
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
    assert.equal(s.historyView.past.length, 0);
  }
});

test('node deletion retains locked nodes and Outputs in a mixed selection', () => {
  const doc = blankDocument(),
    l = doc.layers[0],
    a = node('Circle'),
    b = node('Rectangle');
  a.locked = true;
  l.nodes.push(a, b);
  const s = new CommandSession(doc),
    before = serialize(s.snapshot());
  const result = s.dispatch({
    ...s.state,
    type: 'delete-nodes',
    payload: { layerId: l.id, nodeIds: l.nodes.map((n) => n.id) },
  });
  assert.deepEqual(new Set(result.retainedNodeIds), new Set([l.output, a.id]));
  assert.deepEqual(result.deletedNodeIds, [b.id]);
  s.dispatch({ ...s.state, type: 'undo' });
  assert.equal(serialize(s.snapshot()), before);
  assert.throws(
    () => s.dispatch({ ...s.state, type: 'delete-node', payload: { layerId: l.id, nodeId: a.id } }),
    /locked/,
  );
});
