import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
} from '../src/core.js';
function scene() {
  const doc = document(),
    l = doc.layers[0],
    shape = node('Rectangle'),
    resample = node('Resample Path'),
    simplify = node('Simplify Path'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [shape, resample, simplify, fill, out];
  l.output = out.id;
  connect(l, shape.id, resample.id, 'path');
  connect(l, resample.id, simplify.id, 'path');
  connect(l, simplify.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  return { doc, l, shape, resample, simplify };
}
test('Resample to Simplify reduces redundant points and persists editable graph', () => {
  const { doc, l, shape, resample, simplify } = scene(),
    before = JSON.stringify(shape),
    r = evaluate(l);
  assert.equal(r.error, null);
  const p = r.run(simplify.id);
  assert.ok(p.contours[0].points.length < r.run(resample.id).contours[0].points.length);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(simplify.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  simplify.bypassed = true;
  assert.deepEqual(evaluate(l).run(simplify.id), evaluate(l).run(resample.id));
});
test('Simplify tolerance rejects invalid edits and supports history and locks', () => {
  const { doc, l, simplify } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: simplify.id, ...payload },
    });
  const before = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'tolerance', value: -1 }));
  assert.deepEqual(session.snapshot(), before);
  send('set-parameter', { key: 'tolerance', value: 2 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'tolerance', locked: true });
  assert.throws(() => send('set-parameter', { key: 'tolerance', value: 3 }), /lock/i);
});
