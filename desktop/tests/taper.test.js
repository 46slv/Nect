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
    shape = node('Imported Path'),
    taper = node('Taper'),
    fill = node('Fill'),
    out = node('Output');
  shape.rawPath = 'M0 0L100 0L100 100L0 100Z';
  taper.params.amount.value = 50;
  l.nodes = [shape, taper, fill, out];
  l.output = out.id;
  connect(l, shape.id, taper.id, 'path');
  connect(l, taper.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  return { doc, l, shape, taper };
}
test('Taper maps a rectangle across common bounds, preserves source and editable export', () => {
  const { doc, l, shape, taper } = scene(),
    before = JSON.stringify(shape),
    r = evaluate(l),
    p = r.run(taper.id);
  assert.equal(r.error, null);
  assert.deepEqual(
    p.contours[0].points.map((p) => p.position),
    [
      [0, 25],
      [100, -25],
      [100, 125],
      [0, 75],
    ],
  );
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(taper.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  taper.bypassed = true;
  assert.deepEqual(evaluate(l).run(taper.id), evaluate(l).run(shape.id));
});
test('Taper amount and axis use atomic Session commands, history and locks', () => {
  const { doc, l, taper } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: taper.id, ...payload },
    });
  const before = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'amount', value: 51 }));
  assert.deepEqual(session.snapshot(), before);
  send('set-operation', { operation: 'vertical' });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'amount', locked: true });
  assert.throws(() => send('set-parameter', { key: 'amount', value: 0 }), /lock/i);
});
