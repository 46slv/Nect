import test from 'node:test';
import assert from 'node:assert/strict';
import { document, node, CommandSession, serialize, deserialize } from '../src/core.js';
test('disconnected modifier fallback ranges are rejected by commands and serialized documents', () => {
  for (const [type, key, value] of [
    ['Resample Path', 'count', 1.5],
    ['Resample Path', 'count', 1025],
    ['Roughen', 'amount', -1],
    ['Roughen', 'seed', -1],
    ['Smooth Path', 'factor', 2],
    ['Smooth Path', 'iterations', 0],
    ['Trim Path', 'start', 2],
    ['Corner Round', 'radius', -1],
    ['Wave Points', 'wavelength', 0],
    ['Wave Points', 'phase', 361],
    ['Subdivide Path', 'segments', 0],
    ['Pucker & Bloat', 'amount', 101],
    ['Simplify Path', 'tolerance', -1],
    ['Dash Path', 'dashLength', 0],
    ['Dash Path', 'gapLength', -1],
  ]) {
    const doc = document(),
      l = doc.layers[0],
      n = node(type);
    l.nodes.push(n);
    const session = new CommandSession(doc),
      before = session.snapshot();
    assert.throws(
      () =>
        session.dispatch({
          ...session.state,
          type: 'set-parameter',
          payload: { layerId: l.id, nodeId: n.id, key, value },
        }),
      undefined,
      `${type}.${key}`,
    );
    assert.deepEqual(session.snapshot(), before);
    n.params[key].value = value;
    assert.throws(() => deserialize(serialize(doc)), undefined, `${type} invalid file`);
  }
});
