import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  History,
  blankDocument,
  bypassContract,
  connect,
  deserialize,
  enableRemapPoint,
  evaluate,
  node,
  outputType,
  serialize,
  setNodeBypassed,
  socketCompatible,
  socketCompatibleFor,
  translateRemapPoints,
  validate,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';

const fontData = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const fonts = {
  default: parseFont(
    fontData.buffer.slice(fontData.byteOffset, fontData.byteOffset + fontData.byteLength),
  ),
};

function curveFixture(paintType = 'Fill') {
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = node('Curve'),
    join = node('Join Geometry'),
    paint = node(paintType),
    converter = node('Convert to Path'),
    output = layer.nodes[0];
  curve.points[0].position.value = [0, 0];
  curve.points[0].handleB.value = [20, 0];
  curve.points[1].position.value = [40, 0];
  curve.points[1].handleA.value = [-5, 12];
  curve.points[1].handleB.value = [0, 15];
  curve.points[2].position.value = [20, 30];
  curve.points[2].handleA.value = [-12, -5];
  layer.nodes = [curve, join, paint, converter, output];
  layer.output = output.id;
  connect(layer, curve.id, join.id, 'a');
  connect(layer, curve.id, join.id, 'b');
  connect(layer, join.id, paint.id, 'path');
  connect(layer, paint.id, converter.id, 'path');
  connect(layer, converter.id, output.id, 'path');
  return { doc, layer, curve, join, paint, converter, output };
}

test('Path inputs keep Convert to Path identity behavior and old documents stay at their old schema', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    converter = node('Convert to Path'),
    output = layer.nodes[0];
  layer.nodes = [shape, converter, output];
  layer.output = output.id;
  connect(layer, shape.id, converter.id, 'path');
  connect(layer, converter.id, output.id, 'path');
  const source = evaluate(layer).run(shape.id),
    result = evaluate(layer).run(converter.id);
  assert.equal(socketCompatible('Path', 'Path'), true);
  assert.equal(socketCompatibleFor(shape, converter, 'path'), true);
  assert.equal(outputType(converter), 'Path');
  assert.deepEqual(result, source);
  assert.equal(validate(doc).schemaVersion, 2);
  doc.schemaVersion = 17;
  assert.equal(validate(doc).schemaVersion, 17, 'legacy Path identity edge is not promoted');
});

test('Fill, Stroke and gradients convert to unpainted geometry without mutating their source', () => {
  for (const paintType of ['Fill', 'Stroke', 'Linear Gradient Fill', 'Radial Gradient Fill']) {
    const f = curveFixture(paintType),
      before = evaluate(f.layer),
      sourceBefore = structuredClone(before.run(f.paint.id)),
      converted = before.run(f.converter.id);
    assert.equal(before.error, null, paintType);
    assert.deepEqual(
      converted.contours,
      sourceBefore.contours,
      `${paintType} keeps exact contour data`,
    );
    assert.ok(
      converted.contours.some((contour) => contour.group),
      `${paintType} keeps groups`,
    );
    assert.ok(
      converted.contours.some((contour) =>
        contour.points.some((point) => point.handleA.some(Boolean) || point.handleB.some(Boolean)),
      ),
      `${paintType} keeps Bézier handles`,
    );
    assert.equal(Object.hasOwn(converted, 'fill'), false, `${paintType} fill removed`);
    assert.equal(Object.hasOwn(converted, 'stroke'), false, `${paintType} stroke removed`);
    assert.match(converted.fingerprint, /^sha256:/, `${paintType} gets a geometry fingerprint`);
    assert.deepEqual(
      evaluate(f.layer).run(f.converter.id),
      converted,
      `${paintType} fingerprint is deterministic`,
    );
    assert.deepEqual(
      evaluate(f.layer).run(f.paint.id),
      sourceBefore,
      `${paintType} source is untouched`,
    );
    validate(f.doc);
    assert.equal(f.doc.schemaVersion, 18, `${paintType} edge records schema 18`);
    assert.equal(deserialize(serialize(f.doc)).schemaVersion, 18);
  }
});

test('source recolor leaves converted geometry and its fingerprint unchanged', () => {
  const f = curveFixture('Fill'),
    before = evaluate(f.layer),
    geometry = structuredClone(before.run(f.converter.id)),
    originalSource = structuredClone(before.run(f.paint.id));
  f.paint.params.color.value = { hex: '#2aa887', alpha: 0.35 };
  const after = evaluate(f.layer),
    painted = after.run(f.paint.id),
    converted = after.run(f.converter.id);
  assert.notDeepEqual(painted.fill, originalSource.fill);
  assert.deepEqual(converted.contours, geometry.contours);
  assert.equal(converted.fingerprint, geometry.fingerprint);
  assert.equal(Object.hasOwn(converted, 'fill'), false);
  assert.equal(Object.hasOwn(converted, 'stroke'), false);
});

test('all luminance outputs convert as PaintedPath geometry', () => {
  for (const type of ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither']) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node('Rectangle'),
      paint = node('Fill'),
      luminance = node(type),
      converter = node('Convert to Path'),
      output = layer.nodes[0];
    shape.params.center.value = [16, 16];
    shape.params.size.value = [32, 32];
    paint.params.color.value = { hex: '#000000', alpha: 1 };
    layer.nodes = [shape, paint, luminance, converter, output];
    layer.output = output.id;
    connect(layer, shape.id, paint.id, 'path');
    connect(layer, paint.id, luminance.id, 'path');
    connect(layer, luminance.id, converter.id, 'path');
    connect(layer, converter.id, output.id, 'path');
    const result = evaluate(layer),
      painted = result.run(luminance.id),
      converted = result.run(converter.id);
    assert.equal(result.error, null, type);
    assert.ok(painted.contours.length, `${type} fixture produces geometry`);
    assert.deepEqual(converted.contours, painted.contours, `${type} contours are retained`);
    assert.equal(Object.hasOwn(converted, 'fill'), false, `${type} fill is dropped`);
    assert.equal(Object.hasOwn(converted, 'stroke'), false, `${type} stroke is dropped`);
    assert.match(converted.fingerprint, /^sha256:/, `${type} geometry has a fingerprint`);
    validate(doc);
    assert.equal(
      doc.schemaVersion,
      type === 'Error Diffusion Dither' ? 19 : 18,
      `${type} conversion schema`,
    );
  }
});

test('PaintedPath acceptance is only for Convert to Path; direct Image and other Path consumers stay strict', () => {
  const f = curveFixture('Fill');
  assert.equal(socketCompatibleFor(f.paint, f.converter, 'path'), true);
  assert.equal(socketCompatibleFor(f.paint, node('Transform'), 'path'), false);
  assert.equal(socketCompatibleFor(f.paint, node('Path Remap'), 'path'), false);
  assert.equal(socketCompatibleFor(f.paint, node('Text to Path'), 'path'), false);
  assert.equal(socketCompatibleFor(f.paint, node('Boolean'), 'a'), false);
  for (const [type, key] of [
    ['Transform', 'path'],
    ['Path Remap', 'path'],
    ['Text to Path', 'path'],
    ['Boolean', 'a'],
  ]) {
    const target = node(type);
    f.layer.nodes.push(target);
    assert.throws(
      () => connect(f.layer, f.paint.id, target.id, key),
      /Cannot connect PaintedPath to Path/,
    );
  }
  const image = node('Image');
  f.layer.nodes.push(image);
  assert.equal(socketCompatibleFor(image, f.converter, 'path'), false);
  const oldSource = f.converter.params.path.source;
  assert.throws(
    () => connect(f.layer, image.id, f.converter.id, 'path'),
    /Cannot connect Image to Path/,
  );
  assert.equal(
    f.converter.params.path.source,
    oldSource,
    'failed Image edge leaves saved wire intact',
  );
});

test('Convert to Path preserves Text edit provenance and composes with Remap, Transform and repaint', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    paint = node('Fill'),
    converter = node('Convert to Path'),
    remap = node('Path Remap'),
    transform = node('Transform'),
    repaint = node('Fill'),
    output = layer.nodes[0];
  text.text = 'Nect';
  layer.nodes = [text, paint, converter, remap, transform, repaint, output];
  layer.output = output.id;
  connect(layer, text.id, paint.id, 'path');
  connect(layer, paint.id, converter.id, 'path');
  connect(layer, converter.id, remap.id, 'path');
  connect(layer, remap.id, transform.id, 'path');
  transform.params.offset.value = [13, 7];
  connect(layer, transform.id, repaint.id, 'path');
  repaint.params.color.value = { hex: '#008c72', alpha: 1 };
  connect(layer, repaint.id, output.id, 'path');

  const initial = evaluate(layer, fonts),
    painted = initial.run(paint.id),
    converted = initial.run(converter.id),
    point = converted.contours[0].points[0];
  assert.equal(initial.error, null);
  assert.deepEqual(converted.characterOccurrences, painted.characterOccurrences);
  assert.deepEqual(converted.contours, painted.contours);
  enableRemapPoint(remap, converted, point.id);
  translateRemapPoints(remap, converted, [point.id], [4, -2]);
  const afterRemap = evaluate(layer, fonts),
    remappedPoint = afterRemap
      .run(remap.id)
      .contours[0].points.find((item) => item.id === point.id),
    final = afterRemap.path;
  assert.equal(afterRemap.error, null);
  assert.deepEqual(remappedPoint.position, [point.position[0] + 4, point.position[1] - 2]);
  assert.deepEqual(final.fill, repaint.params.color.value);
  assert.notDeepEqual(final.contours[0].points[0].position, remappedPoint.position);
  assert.equal(Object.hasOwn(afterRemap.run(converter.id), 'fill'), false);
  assert.equal(Object.hasOwn(afterRemap.run(converter.id), 'stroke'), false);
});

test('locks, bypass refusal, failed-edge rollback, history and schema-18 round-trip are atomic', () => {
  const f = curveFixture('Fill'),
    layer = f.layer,
    converter = f.converter,
    original = converter.params.path.source;
  converter.params.path.locked = true;
  assert.throws(() => connect(layer, f.curve.id, converter.id, 'path'), /locked/);
  assert.equal(converter.params.path.source, original);
  converter.params.path.locked = false;

  assert.equal(bypassContract(converter, layer).supported, false);
  assert.throws(() => setNodeBypassed(layer, converter.id, true), /cannot be bypassed/);
  assert.equal(converter.bypassed, undefined);

  const invalidBypass = structuredClone(f.doc),
    invalidConverter = invalidBypass.layers[0].nodes.find((n) => n.id === converter.id);
  invalidBypass.schemaVersion = 10;
  invalidConverter.bypassed = true;
  assert.throws(() => validate(invalidBypass), /Cannot bypass Convert to Path/);

  const oldSchema17 = structuredClone(f.doc);
  oldSchema17.schemaVersion = 17;
  assert.equal(validate(oldSchema17).schemaVersion, 18);

  const bypassed = curveFixture('Fill');
  connect(bypassed.layer, bypassed.join.id, bypassed.converter.id, 'path');
  setNodeBypassed(bypassed.layer, bypassed.converter.id, true);
  assert.equal(evaluate(bypassed.layer).error, null, 'Path-source bypass still works');
  assert.throws(
    () => connect(bypassed.layer, bypassed.paint.id, bypassed.converter.id, 'path'),
    /while bypassed/,
  );
  assert.equal(
    bypassed.converter.params.path.source,
    bypassed.join.id,
    'painted edge does not replace bypass input',
  );

  const historyDoc = structuredClone(f.doc),
    historyLayer = historyDoc.layers[0],
    historyConverter = historyLayer.nodes.find((n) => n.id === converter.id);
  connect(historyLayer, f.join.id, historyConverter.id, 'path');
  historyDoc.schemaVersion = 17;
  validate(historyDoc);
  const history = new History(historyDoc),
    before = serialize(history.doc);
  history.apply((draft) => {
    const targetLayer = draft.layers[0];
    connect(targetLayer, f.paint.id, f.converter.id, 'path');
    validate(draft);
  });
  const saved = serialize(history.doc);
  assert.equal(history.doc.schemaVersion, 18);
  assert.equal(deserialize(saved).schemaVersion, 18);
  history.undo();
  assert.equal(serialize(history.doc), before);
  history.redo();
  assert.equal(serialize(history.doc), saved);
});
