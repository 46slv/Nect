import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  evaluate,
  serialize,
  deserialize,
  CommandSession,
  exportSVG,
  pathBounds,
  connect,
} from '../src/core.js';
import { instancePointsGeometry } from '../src/instance-points-geometry.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    paint = node('Fill'),
    points = node('Luminance to Points'),
    motif = node('Curve'),
    instances = node('Instance on Points'),
    fill = node('Fill'),
    out = node('Output');
  shape.params.center.value = [32, 32];
  shape.params.size.value = [64, 64];
  paint.params.path.source = shape.id;
  paint.params.color.value = { hex: '#000000', alpha: 1 };
  points.params.path.source = paint.id;
  points.params.pitch.value = 16;
  // An asymmetric authored cubic motif demonstrates arbitrary shapes and handles.
  motif.points[0].position.value = [-10, -4];
  motif.points[1].position.value = [10, -4];
  motif.points[2].position.value = [2, 8];
  motif.points[0].handleB.value = [3, 2];
  instances.params.path.source = motif.id;
  instances.params.points.source = points.id;
  fill.params.path.source = instances.id;
  fill.params.color.value = { hex: '#000000', alpha: 1 };
  out.params.path.source = fill.id;
  layer.nodes = [shape, paint, points, motif, instances, fill, out];
  layer.output = out.id;
  return { doc, layer, shape, paint, points, motif, instances, fill };
}
test('Luminance to Points publishes stable center IDs and independent linear scale/rotation attributes', () => {
  const { layer, points, paint } = fixture(),
    run = () => evaluate(layer).run(points.id);
  const full = run();
  assert.equal(full.contours.length, 16);
  const pt = full.contours[0].points[0];
  assert.deepEqual(pt.position, [8, 8]);
  assert.deepEqual(pt.instanceTransform, { scale: 1, rotation: 0 });
  points.params.maxScale.value = 2;
  points.params.maxRotation.value = 45;
  paint.params.color.value.alpha = 0.5;
  const half = run();
  assert.equal(half.contours[0].points[0].id, pt.id);
  assert.deepEqual(half.contours[0].points[0].instanceTransform, { scale: 1, rotation: 22.5 });
  points.params.pitch.value = 32;
  const sparse = run();
  assert.equal(sparse.contours.length, 4);
  assert.deepEqual(sparse.contours[0].points[0].instanceTransform, { scale: 1, rotation: 22.5 });
});
test('Instance on Points consumes size and rotation on arbitrary cubic Path motifs without cell clipping', () => {
  const { layer, points, instances, motif } = fixture();
  points.params.maxScale.value = 2;
  points.params.maxRotation.value = 30;
  const evaluation = evaluate(layer),
    targets = evaluation.run(points.id),
    source = evaluation.run(motif.id),
    actual = evaluation.run(instances.id);
  const bounds = pathBounds(source),
    pivot = [bounds[0][1], bounds[1][1]];
  const expected = instancePointsGeometry(source, targets, {
    pivot,
    prefix: 'expected',
    transforms: targets.contours.flatMap((c) => c.points.map((p) => p.instanceTransform)),
  });
  const geometry = (p) =>
    p.contours.map((c) => c.points.map((p) => [p.position, p.handleA, p.handleB]));
  assert.deepEqual(geometry(actual), geometry(expected));
  const firstBounds = pathBounds({ contours: [actual.contours[0]] });
  assert.ok(firstBounds[0][2] - firstBounds[0][0] > 16, 'motif exceeds cell span');
  assert.ok(
    actual.contours[0].points.some((p) => p.handleB.some((v) => v !== 0)),
    'cubic handles retained',
  );
  assert.equal(actual.contours.length, 16);
});
test('point attributes validate before materialization; ordinary Path targets keep original translation arithmetic', () => {
  const { layer, points, motif } = fixture(),
    e = evaluate(layer),
    source = e.run(motif.id),
    targets = e.run(points.id);
  const bounds = pathBounds(source),
    pivot = [bounds[0][1], bounds[1][1]];
  targets.contours[0].points[0].instanceTransform.scale = Infinity;
  assert.throws(() => instancePointsGeometry(source, targets, { pivot }), /per-point/);
  delete targets.contours[0].points[0].instanceTransform;
  for (const c of targets.contours) for (const p of c.points) delete p.instanceTransform;
  const plain = instancePointsGeometry(source, targets, { pivot });
  assert.equal(
    plain.contours[0].points[0].position[0],
    source.contours[0].points[0].position[0] + 8 - pivot[0],
  );
});
test('saved procedural points recompute identically; edits undo and excessive density rejects atomically', () => {
  const { doc, layer, points, shape } = fixture(),
    session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  const before = serialize(session.snapshot()),
    svg = exportSVG(session.snapshot());
  send('set-parameter', { nodeId: points.id, key: 'maxScale', value: 2 });
  const resized = serialize(session.snapshot());
  send('set-parameter', { nodeId: points.id, key: 'pitch', value: 12 });
  const denser = serialize(session.snapshot());
  assert.notEqual(exportSVG(session.snapshot()), svg);
  const reopened = deserialize(denser);
  assert.equal(reopened.schemaVersion, 36);
  assert.equal(exportSVG(reopened), exportSVG(session.snapshot()));
  assert.deepEqual(
    evaluate(reopened.layers[0]).run(points.id),
    evaluate(session.snapshot().layers[0]).run(points.id),
  );
  send('undo');
  assert.equal(serialize(session.snapshot()), resized);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  send('redo');
  assert.equal(serialize(session.snapshot()), denser);
  for (const payload of [
    { nodeId: points.id, key: 'pitch', value: 1 },
    { nodeId: points.id, key: 'maxScale', value: 101 },
    { nodeId: shape.id, key: 'size', value: [2048, 2048] },
  ])
    assert.throws(() => send('set-parameter', payload));
  assert.equal(serialize(session.snapshot()), denser);
  send('set-parameter-lock', { nodeId: points.id, key: 'maxScale', locked: true });
  assert.throws(
    () => send('set-parameter', { nodeId: points.id, key: 'maxScale', value: 3 }),
    /locked/,
  );
});
test('transparent PNG samples are white before Invert and source alpha remains untouched', () => {
  const { doc, layer, points, paint } = fixture(),
    image = node('Image');
  image.asset = 'image-synthetic';
  image.params.position.value = [0, 0];
  image.params.size.value = [64, 64];
  layer.nodes.unshift(image);
  connect(layer, image.id, points.id, 'path');
  for (const alpha of [0, 128, 255]) {
    doc.imageAssets = {
      'image-synthetic': {
        name: 'synthetic.png',
        mime: 'image/png',
        width: 1,
        height: 1,
        data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([0, 0, 0, alpha]))),
      },
    };
    const before = serialize(doc),
      result = evaluate(layer, {}, doc.imageAssets).run(points.id);
    assert.equal(serialize(doc), before);
    assert.equal(result.contours.length, alpha === 0 ? 0 : 16);
    if (alpha)
      assert.ok(
        Math.abs(result.contours[0].points[0].instanceTransform.scale - alpha / 255) < 1e-12,
      );
  }
  points.params.invert.value = true;
  assert.equal(evaluate(layer, {}, doc.imageAssets).run(points.id).contours.length, 0);
  assert.equal(paint.params.color.value.alpha, 1);
});
