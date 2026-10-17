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
    effect = node('Pucker & Bloat'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [shape, effect, fill, out];
  l.output = out.id;
  connect(l, shape.id, effect.id, 'path');
  connect(l, effect.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  effect.params.amount.value = 50;
  return { doc, l, shape, effect };
}
test('Pucker & Bloat uses source anchor bounds, creates curves and preserves editable graph', () => {
  const { doc, l, shape, effect } = scene(),
    before = JSON.stringify(shape),
    r = evaluate(l),
    p = r.run(effect.id);
  assert.equal(r.error, null);
  assert.ok(p.contours[0].points.some((p) => p.handleA.some((v) => v !== 0)));
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(effect.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  effect.bypassed = true;
  assert.deepEqual(evaluate(l).run(effect.id), evaluate(l).run(shape.id));
});
test('Pucker & Bloat common controls reject invalid amounts, preserve history and manual center', () => {
  const { doc, l, effect } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: effect.id, ...payload },
    });
  const before = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'amount', value: 101 }));
  assert.deepEqual(session.snapshot(), before);
  send('set-parameter', { key: 'amount', value: -50 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter', { key: 'autoCenter', value: false });
  send('set-parameter', { key: 'center', value: [460, 250] });
  assert.deepEqual(evaluate(session.snapshot().layers[0]).path, evaluate(changed.layers[0]).path);
  send('set-parameter-lock', { key: 'amount', locked: true });
  assert.throws(() => send('set-parameter', { key: 'amount', value: 0 }), /lock/i);
});
