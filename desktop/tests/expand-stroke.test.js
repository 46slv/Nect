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
  enableRemapPoint,
  point as curvePoint,
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
function openStrokeScene() {
  const doc = document(),
    l = doc.layers[0],
    curve = node('Curve'),
    stroke = node('Stroke'),
    expand = node('Expand Stroke'),
    fill = node('Fill'),
    out = node('Output');
  curve.params.closed.value = false;
  curve.points = [curvePoint(100, 100), curvePoint(300, 100)];
  l.nodes = [curve, stroke, expand, fill, out];
  l.output = out.id;
  connect(l, curve.id, stroke.id, 'path');
  connect(l, stroke.id, expand.id, 'path');
  connect(l, expand.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  stroke.params.width.value = 10;
  return { doc, l, curve, stroke, expand, fill };
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

test('a failed curved Expand Stroke edit rolls back through the ordinary command path', () => {
  const doc = document();
  const l = doc.layers[0];
  const curve = node('Curve');
  const stroke = node('Stroke');
  const expand = node('Expand Stroke');
  const fill = node('Fill');
  const out = node('Output');
  curve.params.closed.value = false;
  curve.points = [curvePoint(0, 0), curvePoint(20, 20)];
  curve.points[0].handleB.value = [0, 10];
  curve.points[1].handleA.value = [-10, 0];
  l.nodes = [curve, stroke, expand, fill, out];
  l.output = out.id;
  connect(l, curve.id, stroke.id, 'path');
  connect(l, stroke.id, expand.id, 'path');
  connect(l, expand.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  stroke.params.width.value = 4;
  const session = new CommandSession(doc);
  const before = session.snapshot();
  const evaluated = evaluate(before.layers[0]);
  assert.equal(evaluated.error, null);
  assert.ok(evaluated.run(expand.id).contours.length > 0);
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId: l.id, nodeId: stroke.id, key: 'width', value: 100000 },
      }),
    /safe integer envelope/i,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('legacy constant-width Expand Stroke backfills schema 16 without changing fingerprints or remap edits', () => {
  const { doc, l, expand, fill } = scene(),
    remap = node('Path Remap');
  l.nodes.push(remap);
  connect(l, expand.id, remap.id, 'path');
  connect(l, remap.id, fill.id, 'path');
  const baseline = evaluate(l),
    expanded = baseline.run(expand.id),
    target = expanded.contours[0].points[0];
  enableRemapPoint(remap, expanded, target.id).params.position.value = [1, 0];
  assert.equal(evaluate(l).run(remap.id).remapSuspended, undefined);

  const legacy = structuredClone(doc);
  legacy.schemaVersion = 15;
  delete legacy.layers[0].nodes.find((n) => n.id === expand.id).params.startWidthMultiplier;
  delete legacy.layers[0].nodes.find((n) => n.id === expand.id).params.endWidthMultiplier;
  const loaded = deserialize(serialize(legacy));
  const loadedExpand = loaded.layers[0].nodes.find((n) => n.id === expand.id);
  assert.equal(loaded.schemaVersion, 16);
  assert.equal(loadedExpand.params.startWidthMultiplier.value, 1);
  assert.equal(loadedExpand.params.endWidthMultiplier.value, 1);
  const loadedEvaluation = evaluate(loaded.layers[0]);
  assert.deepEqual(loadedEvaluation.run(expand.id), expanded);
  assert.equal(loadedEvaluation.run(remap.id).remapSuspended, undefined);
});

test('open-path width multipliers change geometry and retain undo, redo, and serialized values', () => {
  const { doc, l, expand } = openStrokeScene(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: expand.id, ...payload },
    });
  const before = session.snapshot(),
    beforePath = evaluate(before.layers[0]).run(expand.id);
  send('set-parameter', { key: 'startWidthMultiplier', value: 0.5 });
  const midpoint = session.snapshot();
  send('set-parameter', { key: 'endWidthMultiplier', value: 2 });
  const after = session.snapshot(),
    afterPath = evaluate(after.layers[0]).run(expand.id);
  assert.notDeepEqual(afterPath.contours, beforePath.contours);
  assert.equal(afterPath.fingerprint === beforePath.fingerprint, false);
  assert.equal(
    after.layers[0].nodes.find((n) => n.id === expand.id).params.startWidthMultiplier.value,
    0.5,
  );
  assert.equal(
    after.layers[0].nodes.find((n) => n.id === expand.id).params.endWidthMultiplier.value,
    2,
  );
  assert.equal(after.schemaVersion, 16);
  assert.deepEqual(deserialize(serialize(after)), after);
  send('undo');
  assert.deepEqual(session.snapshot(), midpoint);
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  send('redo');
  assert.deepEqual(session.snapshot(), after);
});

test('Expand Stroke rejects unequal closed-path widths, invalid driven values, and edits while locked atomically', () => {
  const { doc, l, expand } = scene(),
    source = node('Math');
  source.params.a.value = 5;
  l.nodes.push(source);
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: expand.id, ...payload },
    });
  const before = session.snapshot();
  assert.throws(
    () => send('set-parameter', { key: 'startWidthMultiplier', value: 2 }),
    /open contours only/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.throws(() => send('set-parameter', { key: 'endWidthMultiplier', value: 4.01 }), /0 to 4/);
  assert.deepEqual(session.snapshot(), before);
  assert.throws(
    () => send('set-parameter-expression', { key: 'startWidthMultiplier', expression: '5' }),
    /between 0 and 4/i,
  );
  assert.deepEqual(session.snapshot(), before);
  assert.throws(
    () =>
      send('connect', {
        nodeId: expand.id,
        key: 'startWidthMultiplier',
        sourceId: source.id,
      }),
    /between 0 and 4/i,
  );
  assert.deepEqual(session.snapshot(), before);
  send('set-parameter-lock', { key: 'startWidthMultiplier', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'startWidthMultiplier', value: 2 }), /locked/i);
  assert.deepEqual(session.snapshot(), locked);
});
