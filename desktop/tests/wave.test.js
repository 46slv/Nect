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
function scene() {
  const doc = document(),
    l = doc.layers[0],
    shape = node('Imported Path'),
    resample = node('Resample Path'),
    wave = node('Wave Points'),
    stroke = node('Stroke'),
    out = node('Output');
  shape.rawPath = 'M0 0L100 0';
  resample.params.count.value = 5;
  wave.params.amplitude.value = 10;
  wave.params.wavelength.value = 100;
  l.nodes = [shape, resample, wave, stroke, out];
  l.output = out.id;
  connect(l, shape.id, resample.id, 'path');
  connect(l, resample.id, wave.id, 'path');
  connect(l, wave.id, stroke.id, 'path');
  connect(l, stroke.id, out.id, 'path');
  return { doc, l, shape, resample, wave };
}
test('Resample then Wave Points produces a static sine polyline with editable source', () => {
  const { doc, l, shape, resample, wave } = scene(),
    before = JSON.stringify(shape),
    r = evaluate(l),
    p = r.run(wave.id);
  assert.equal(r.error, null);
  [
    [0, 0],
    [25, 10],
    [50, 0],
    [75, -10],
    [100, 0],
  ].forEach((v, i) => near(p.contours[0].points[i].position, v));
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(wave.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  wave.bypassed = true;
  assert.deepEqual(evaluate(l).run(wave.id), evaluate(l).run(resample.id));
});
test('Wave common parameters and operation are atomic, undoable and locked', () => {
  const { doc, l, wave } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: wave.id, ...payload },
    });
  const before = session.snapshot();
  for (const [key, value] of [
    ['wavelength', 0],
    ['amplitude', 100001],
    ['phase', 361],
  ]) {
    assert.throws(() => send('set-parameter', { key, value }));
    assert.deepEqual(session.snapshot(), before);
  }
  send('set-operation', { operation: 'horizontal' });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'phase', locked: true });
  assert.throws(() => send('set-parameter', { key: 'phase', value: 90 }), /lock/i);
});

test('Wave geometry precision failures roll back a valid numeric edit', () => {
  const { doc, l, shape, wave } = scene();
  shape.rawPath = 'M1000000000 0L1000000100 0';
  const session = new CommandSession(doc);
  const before = session.snapshot();
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId: l.id, nodeId: wave.id, key: 'wavelength', value: 0.0001 },
      }),
    /precision/i,
  );
  assert.deepEqual(session.snapshot(), before);
});
