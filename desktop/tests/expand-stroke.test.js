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
  pathArea,
  bypassContract,
  outputType,
} from '../src/core.js';
function scene() {
  const doc = document(),
    l = doc.layers[0],
    shape = node('Rectangle'),
    stroke = node('Stroke'),
    expand = node('Expand Stroke'),
    fill = node('Fill'),
    out = node('Output');
  l.nodes = [shape, stroke, expand, fill, out];
  l.output = out.id;
  connect(l, shape.id, stroke.id, 'path');
  connect(l, stroke.id, expand.id, 'path');
  connect(l, expand.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  stroke.params.width.value = 4;
  return { doc, l, shape, stroke, expand, fill };
}
test('Expand Stroke converts centered closed stroke to geometric band, retaining holes and JSON', () => {
  const { doc, l, shape, stroke, expand } = scene(),
    before = JSON.stringify(shape),
    r = evaluate(l);
  assert.equal(r.error, null);
  const p = r.run(expand.id);
  assert.equal(outputType(expand), 'Path');
  assert.equal(bypassContract(expand).supported, false);
  assert.equal(p.stroke, undefined);
  assert.equal(p.fill, undefined);
  assert.ok(Math.abs(pathArea(p) - 3200) < 0.05);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(expand.id), p);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(shape), before);
});
test('Expanded outline feeds Boolean geometry inputs', () => {
  const { l, expand, fill } = scene(),
    clip = node('Rectangle'),
    boolean = node('Boolean');
  clip.params.center.value = [460, 250];
  clip.params.size.value = [400, 400];
  boolean.operation = 'intersect';
  l.nodes.push(clip, boolean);
  connect(l, expand.id, boolean.id, 'a');
  connect(l, clip.id, boolean.id, 'b');
  connect(l, boolean.id, fill.id, 'path');
  assert.equal(evaluate(l).error, null);
  assert.ok(pathArea(evaluate(l).run(boolean.id)) > 0);
});
test('Stroke changes remain editable and invalid dash expansion rolls back atomically', () => {
  const { doc, l, stroke } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: stroke.id, ...payload },
    });
  const before = session.snapshot();
  send('set-parameter', { key: 'width', value: 8 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  assert.throws(() => send('set-node-settings', { settings: { strokeDash: [3, 2] } }), /dash/i);
  assert.deepEqual(session.snapshot(), changed);
});
