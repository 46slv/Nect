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
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-6, `${a} != ${b}`));
function scene(raw = 'M0 0L100 0') {
  const doc = document(),
    l = doc.layers[0],
    shape = node('Imported Path'),
    dash = node('Dash Path'),
    stroke = node('Stroke'),
    out = node('Output');
  shape.rawPath = raw;
  l.nodes = [shape, dash, stroke, out];
  l.output = out.id;
  connect(l, shape.id, dash.id, 'path');
  connect(l, dash.id, stroke.id, 'path');
  connect(l, stroke.id, out.id, 'path');
  dash.params.dashLength.value = 20;
  dash.params.gapLength.value = 10;
  return { doc, l, shape, dash, stroke };
}
test('Dash Path creates actual open dash contours at measured distances and roundtrips', () => {
  const { doc, l, shape, dash } = scene(),
    before = JSON.stringify(shape),
    r = evaluate(l),
    p = r.run(dash.id);
  assert.equal(r.error, null);
  assert.equal(p.contours.length, 4);
  [
    [0, 20],
    [30, 50],
    [60, 80],
    [90, 100],
  ].forEach(([a, b], i) => {
    near(p.contours[i].points[0].position, [a, 0]);
    near(p.contours[i].points.at(-1).position, [b, 0]);
    assert.equal(p.contours[i].closed, false);
  });
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(dash.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  dash.bypassed = true;
  assert.deepEqual(evaluate(l).run(dash.id), evaluate(l).run(shape.id));
});
test('Dash Path preserves cubic pieces and joins a dash across a closed seam', () => {
  const f = scene('M0 0C0 100 100 100 100 0');
  f.dash.params.dashLength.value = 40;
  f.dash.params.gapLength.value = 20;
  assert.ok(
    evaluate(f.l)
      .run(f.dash.id)
      .contours[0].points.some(
        (p) => p.handleA.some((v) => v !== 0) || p.handleB.some((v) => v !== 0),
      ),
  );
  f.shape.rawPath = 'M0 0L100 0L100 100L0 100Z';
  f.dash.params.dashLength.value = 300;
  f.dash.params.gapLength.value = 100;
  f.dash.params.offset.value = 50;
  const p = evaluate(f.l).run(f.dash.id);
  assert.equal(p.contours.length, 1);
  assert.equal(p.contours[0].closed, false);
  near(p.contours[0].points[0].position, [0, 50]);
  near(p.contours[0].points.at(-1).position, [50, 100]);
  f.dash.params.gapLength.value = 0;
  assert.deepEqual(evaluate(f.l).run(f.dash.id).contours, evaluate(f.l).run(f.shape.id).contours);
});
test('Dash Path validates disconnected settings and geometry budgets with atomic history', () => {
  const { doc, l, dash } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: dash.id, ...payload },
    });
  const before = session.snapshot();
  for (const [key, value] of [
    ['dashLength', 0],
    ['gapLength', -1],
    ['offset', Infinity],
  ]) {
    assert.throws(() => send('set-parameter', { key, value }));
    assert.deepEqual(session.snapshot(), before);
  }
  send('set-parameter', { key: 'offset', value: 5 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter', { key: 'gapLength', value: 0.0001 });
  const bounded = session.snapshot();
  assert.throws(
    () => send('set-parameter', { key: 'dashLength', value: 0.0001 }),
    /512|budget|segments|interval/i,
  );
  assert.deepEqual(session.snapshot(), bounded);
});
