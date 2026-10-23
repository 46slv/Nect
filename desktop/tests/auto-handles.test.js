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
    source = node('Imported Path'),
    effect = node('Auto Handles'),
    stroke = node('Stroke'),
    out = node('Output');
  source.rawPath = 'M0 100L50 0L100 100';
  l.nodes = [source, effect, stroke, out];
  l.output = out.id;
  connect(l, source.id, effect.id, 'path');
  connect(l, effect.id, stroke.id, 'path');
  connect(l, stroke.id, out.id, 'path');
  return { doc, l, source, effect };
}
test('Auto Handles preserves anchors and creates smooth derived handles with editable export', () => {
  const { doc, l, source, effect } = scene(),
    before = JSON.stringify(source),
    r = evaluate(l),
    p = r.run(effect.id);
  assert.equal(r.error, null);
  assert.deepEqual(
    p.contours[0].points.map((p) => p.position),
    [
      [0, 100],
      [50, 0],
      [100, 100],
    ],
  );
  const middle = p.contours[0].points[1];
  assert.ok(middle.handleA[0] < 0 && middle.handleB[0] > 0);
  assert.equal(middle.handleA[1], 0);
  assert.equal(middle.handleB[1], 0);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(effect.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(source), before);
  effect.params.strength.value = 0;
  assert.ok(
    evaluate(l)
      .run(effect.id)
      .contours[0].points.every(
        (p) => p.handleA.every((x) => x === 0) && p.handleB.every((x) => x === 0),
      ),
  );
  effect.bypassed = true;
  assert.deepEqual(evaluate(l).run(effect.id), evaluate(l).run(source.id));
});
test('Auto Handles strength uses atomic Session history and locks', () => {
  const { doc, l, effect } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: effect.id, ...payload },
    });
  const before = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'strength', value: 1.1 }));
  assert.deepEqual(session.snapshot(), before);
  send('set-parameter', { key: 'strength', value: 0.5 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'strength', locked: true });
  assert.throws(() => send('set-parameter', { key: 'strength', value: 0 }), /lock/i);
});
