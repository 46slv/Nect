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
    smooth = node('Smooth Path'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [shape, resample, roughen, smooth, fill, out];
  l.output = out.id;
  connect(l, shape.id, resample.id, 'path');
  connect(l, resample.id, roughen.id, 'path');
  connect(l, roughen.id, smooth.id, 'path');
  connect(l, smooth.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  return { doc, l, shape, roughen, smooth };
}
const energy = (points) =>
  points.reduce(
    (sum, p, i) =>
      sum +
      [0, 1].reduce(
        (v, k) =>
          v +
          (points[(i + points.length - 1) % points.length].position[k] -
            2 * p.position[k] +
            points[(i + 1) % points.length].position[k]) **
            2,
        0,
      ),
    0,
  );
test('Resample Roughen Smooth chain reduces point-angle variation and roundtrips', () => {
  const { doc, l, shape, roughen, smooth } = scene(),
    before = JSON.stringify(shape),
    result = evaluate(l);
  assert.equal(result.error, null);
  const source = result.run(roughen.id),
    p = result.run(smooth.id);
  assert.equal(p.contours[0].points.length, 32);
  assert.ok(energy(p.contours[0].points) < energy(source.contours[0].points));
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(smooth.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  smooth.bypassed = true;
  assert.deepEqual(evaluate(l).run(smooth.id), evaluate(l).run(roughen.id));
});
test('Smooth controls preserve atomic commands, undo redo and locks', () => {
  const { doc, l, smooth } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: smooth.id, ...payload },
    });
  const before = session.snapshot();
  for (const [key, value] of [
    ['factor', -0.1],
    ['factor', 1.1],
    ['iterations', 0],
    ['iterations', 51],
    ['iterations', 1.5],
  ]) {
    assert.throws(() => send('set-parameter', { key, value }));
    assert.deepEqual(session.snapshot(), before);
  }
  send('set-parameter', { key: 'factor', value: 0.25 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'factor', locked: true });
  assert.throws(() => send('set-parameter', { key: 'factor', value: 0.5 }), /lock/i);
});
