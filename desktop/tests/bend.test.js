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

function scene(rawPath = 'M0 0L100 0L100 100L0 100Z') {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Imported Path'),
    bend = node('Bend Points'),
    fill = node('Fill'),
    output = node('Output');
  shape.rawPath = rawPath;
  bend.params.angle.value = 45;
  layer.nodes = [shape, bend, fill, output];
  layer.output = output.id;
  connect(layer, shape.id, bend.id, 'path');
  connect(layer, bend.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, shape, bend };
}

test('Bend Points evaluates Path geometry, bypasses, and survives JSON/SVG export', () => {
  const { doc, layer, shape, bend } = scene(),
    before = JSON.stringify(shape),
    evaluation = evaluate(layer),
    result = evaluation.run(bend.id);
  assert.equal(evaluation.error, null);
  assert.equal(result.contours[0].closed, true);
  assert.notDeepEqual(
    result.contours[0].points.map((item) => item.position),
    evaluate(layer)
      .run(shape.id)
      .contours[0].points.map((item) => item.position),
  );
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(bend.id), result);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
  bend.bypassed = true;
  assert.deepEqual(evaluate(layer).run(bend.id), evaluate(layer).run(shape.id));
});

test('Bend Points angle and axis use atomic Session history and locks', () => {
  const { doc, layer, bend } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: bend.id, ...payload },
    });
  const before = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'angle', value: 181 }), /from -180 to 180/);
  assert.deepEqual(session.snapshot(), before);
  send('set-operation', { operation: 'vertical' });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'angle', locked: true });
  assert.throws(() => send('set-parameter', { key: 'angle', value: 0 }), /lock/i);
});

test('Bend Points geometry fold failure rolls back the authored edit', () => {
  const { doc, layer, bend } = scene(),
    session = new CommandSession(doc),
    before = session.snapshot();
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId: layer.id, nodeId: bend.id, key: 'angle', value: 180 },
      }),
    /fold/,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('Bend Points rejects non-finite angles before evaluation', () => {
  const { layer, bend } = scene();
  bend.params.angle.value = NaN;
  const evaluation = evaluate(layer);
  assert.match(evaluation.error || '', /finite Scalar/);
});
