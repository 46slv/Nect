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
    roughen = node('Roughen'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [shape, resample, roughen, fill, out];
  l.output = out.id;
  connect(l, shape.id, resample.id, 'path');
  connect(l, resample.id, roughen.id, 'path');
  connect(l, roughen.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  return { doc, l, shape, resample, roughen };
}
test('Resample then Roughen produces deterministic editable exported geometry', () => {
  const { doc, l, shape, resample, roughen } = scene();
  const before = JSON.stringify(shape),
    result = evaluate(l),
    p = result.run(roughen.id);
  assert.equal(result.error, null);
  assert.equal(p.contours[0].points.length, 32);
  assert.notDeepEqual(
    p.contours[0].points.map((p) => p.position),
    result.run(resample.id).contours[0].points.map((p) => p.position),
  );
  assert.deepEqual(evaluate(l).run(roughen.id), p);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(roughen.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  roughen.bypassed = true;
  assert.deepEqual(evaluate(l).run(roughen.id), evaluate(l).run(resample.id));
});
test('Roughen commands reject invalid amount and seed atomically, preserving undo/redo and locks', () => {
  const { doc, l, roughen } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: roughen.id, ...payload },
    });
  const before = session.snapshot();
  for (const [key, value] of [
    ['amount', -1],
    ['amount', 100001],
    ['seed', 1.5],
    ['seed', -1],
    ['seed', 2147483648],
  ]) {
    assert.throws(() => send('set-parameter', { key, value }));
    assert.deepEqual(session.snapshot(), before);
  }
  send('set-parameter', { key: 'seed', value: 99 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'amount', locked: true });
  assert.throws(() => send('set-parameter', { key: 'amount', value: 1 }), /lock/i);
});
