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
    round = node('Corner Round'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [shape, round, fill, out];
  l.output = out.id;
  connect(l, shape.id, round.id, 'path');
  connect(l, round.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  return { doc, l, shape, round };
}
test('Corner Round graph is editable, serializable, exportable and bypassable', () => {
  const { doc, l, shape, round } = scene();
  const before = JSON.stringify(shape);
  const r = evaluate(l);
  assert.equal(r.error, null);
  assert.ok(r.run(round.id).contours[0].points.length > 4);
  assert.equal(r.run(round.id).unstable, true);
  assert.match(exportSVG(doc), /<path /);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).path, r.path);
  assert.equal(JSON.stringify(shape), before);
  round.bypassed = true;
  assert.deepEqual(evaluate(l).run(round.id), evaluate(l).run(shape.id));
});
test('Corner Round radius uses atomic common commands and respects history / locks', () => {
  const { doc, l, round } = scene();
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: round.id, ...payload },
    });
  const original = session.snapshot();
  send('set-parameter', { key: 'radius', value: 20 });
  const after = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), original);
  send('redo');
  assert.deepEqual(session.snapshot(), after);
  assert.throws(() => send('set-parameter', { key: 'radius', value: -1 }));
  assert.deepEqual(session.snapshot(), after);
  send('set-parameter-lock', { key: 'radius', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'radius', value: 3 }), /lock/i);
  assert.deepEqual(session.snapshot(), locked);
});
test('curved input can be connected through common commands and preserves its cubic', () => {
  const { doc, l, round } = scene();
  const curve = node('Curve');
  curve.points[0].handleB.value = [30, 0];
  l.nodes.push(curve);
  const session = new CommandSession(doc);
  session.dispatch({
    ...session.state,
    type: 'connect',
    payload: { layerId: l.id, sourceId: curve.id, nodeId: round.id, key: 'path' },
  });
  const result = evaluate(session.snapshot().layers[0]);
  assert.equal(result.error, null);
  const geometry = (path) =>
    path.contours.map((c) =>
      c.points.map(({ position, handleA, handleB }) => ({ position, handleA, handleB })),
    );
  assert.deepEqual(
    geometry(result.run(round.id))[0].slice(0, 2),
    geometry(result.run(curve.id))[0].slice(0, 2),
  );
});
