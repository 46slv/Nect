import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import paper from 'paper/dist/paper-core.js';
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

const near = (actual, expected, epsilon = 1e-6) =>
  actual.forEach((value, index) =>
    assert.ok(Math.abs(value - expected[index]) <= epsilon, `${actual} != ${expected}`),
  );

function scene(raw = 'M0 0L100 0') {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    trim = node('Trim Path'),
    fill = node('Fill'),
    output = node('Output');
  source.rawPath = raw;
  layer.nodes = [source, trim, fill, output];
  layer.output = output.id;
  connect(layer, source.id, trim.id, 'path');
  connect(layer, trim.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, source, trim };
}

function pathWithoutIds(path) {
  return path.contours.map(({ id, points, ...contour }) => ({
    ...contour,
    points: points.map(({ id: pointId, ...point }) => point),
  }));
}

test('Trim Path selects an open line interval by per-contour arc-length fractions', () => {
  const { layer, source, trim } = scene(),
    sourceBefore = JSON.stringify(source);
  trim.params.start.value = 0.25;
  trim.params.end.value = 0.75;
  const result = evaluate(layer).run(trim.id),
    contour = result.contours[0];
  assert.equal(contour.closed, false);
  assert.equal(contour.points.length, 2);
  near(contour.points[0].position, [25, 0]);
  near(contour.points[1].position, [75, 0]);
  assert.deepEqual(contour.points[0].handleA, [0, 0]);
  assert.deepEqual(contour.points[1].handleB, [0, 0]);
  assert.equal(contour.points[0].handleB[1], 0);
  assert.equal(contour.points[1].handleA[1], 0);
  assert.notEqual(contour.id, evaluate(layer).run(source.id).contours[0].id);
  assert.equal(JSON.stringify(source), sourceBefore);
});

test('Trim Path retains exact cubic subcurves and relative endpoint handles', () => {
  const { layer, trim } = scene('M0 0C0 100 100 100 100 0');
  trim.params.start.value = 0.25;
  trim.params.end.value = 0.75;
  const result = evaluate(layer).run(trim.id).contours[0],
    scope = new paper.PaperScope();
  scope.setup(new scope.Size(1200, 800));
  const referencePath = new scope.Path({
      pathData: 'M0 0C0 100 100 100 100 0',
      insert: false,
    }),
    curve = referencePath.curves[0],
    t0 = curve.getTimeAt(curve.length * 0.25),
    t1 = curve.getTimeAt(curve.length * 0.75),
    values = curve.getPart(t0, t1).values,
    expectedStart = values.slice(0, 2),
    expectedStartOut = [values[2] - values[0], values[3] - values[1]],
    expectedEnd = values.slice(6, 8),
    expectedEndIn = [values[4] - values[6], values[5] - values[7]];
  near(result.points[0].position, expectedStart);
  near(result.points[0].handleB, expectedStartOut);
  near(result.points[1].position, expectedEnd);
  near(result.points[1].handleA, expectedEndIn);
  referencePath.remove();
});

test('Trim Path joins segment slices at an exact source vertex without duplicate knots', () => {
  const { layer, trim } = scene('M0 0L100 0L100 100');
  trim.params.start.value = 0.5;
  trim.params.end.value = 0.75;
  const points = evaluate(layer).run(trim.id).contours[0].points;
  assert.equal(points.length, 2);
  near(points[0].position, [100, 0]);
  near(points[1].position, [100, 50]);
  assert.deepEqual(points[0].handleA, [0, 0]);
  assert.deepEqual(points[0].handleB, [0, 0]);
  assert.deepEqual(points[1].handleB, [0, 0]);
  assert.deepEqual(points[1].handleA, [0, -25]);
});

test('Trim Path preserves full closed geometry and makes a closing-edge partial open', () => {
  const { layer, source, trim } = scene('M0 0L100 0L100 100L0 100Z'),
    sourcePath = evaluate(layer).run(source.id);
  const full = evaluate(layer).run(trim.id);
  assert.equal(full.contours[0].closed, true);
  assert.deepEqual(pathWithoutIds(full), pathWithoutIds(sourcePath));
  assert.notEqual(full.contours[0].points[0].id, sourcePath.contours[0].points[0].id);

  trim.params.start.value = 0.75;
  trim.params.end.value = 1;
  const partial = evaluate(layer).run(trim.id).contours[0];
  assert.equal(partial.closed, false);
  assert.equal(partial.points.length, 2);
  near(partial.points[0].position, [0, 100]);
  near(partial.points[1].position, [0, 0]);
});

test('Trim Path keeps finite degenerate contours and start equal to end is empty', () => {
  const { layer, trim } = scene('M10 20L10 20');
  trim.params.start.value = 0.2;
  trim.params.end.value = 0.8;
  let result = evaluate(layer).run(trim.id);
  assert.equal(result.contours.length, 1);
  assert.equal(result.contours[0].closed, false);
  assert.ok(
    result.contours[0].points.every((point) =>
      [...point.position, ...point.handleA, ...point.handleB].every(Number.isFinite),
    ),
  );
  trim.params.end.value = 0.2;
  result = evaluate(layer).run(trim.id);
  assert.deepEqual(result.contours, []);
});

test('Trim Path preserves contour groups across independent multi-contour trims', () => {
  const doc = document(),
    layer = doc.layers[0],
    first = node('Imported Path'),
    second = node('Imported Path'),
    join = node('Join Geometry'),
    trim = node('Trim Path');
  first.rawPath = 'M0 0L100 0';
  second.rawPath = 'M0 20L200 20';
  layer.nodes = [first, second, join, trim];
  connect(layer, first.id, join.id, 'a');
  connect(layer, second.id, join.id, 'b');
  connect(layer, join.id, trim.id, 'path');
  trim.params.start.value = 0.25;
  trim.params.end.value = 0.75;
  const joined = evaluate(layer).run(join.id),
    result = evaluate(layer).run(trim.id);
  assert.equal(result.contours.length, 2);
  assert.deepEqual(
    result.contours.map((contour) => contour.group),
    joined.contours.map((contour) => contour.group),
  );
  near(result.contours[0].points[0].position, [25, 0]);
  near(result.contours[0].points[1].position, [75, 0]);
  near(result.contours[1].points[0].position, [50, 20]);
  near(result.contours[1].points[1].position, [150, 20]);
});

test('Trim Path JSON, SVG, bypass, and unstable remap fingerprint contracts', () => {
  const { doc, layer, source, trim } = scene('M0 0C0 100 100 100 100 0'),
    originalSource = evaluate(layer).run(source.id);
  trim.params.start.value = 0.2;
  trim.params.end.value = 0.8;
  const result = evaluate(layer).run(trim.id),
    reopened = deserialize(serialize(doc));
  assert.deepEqual(evaluate(reopened.layers[0]).run(trim.id), result);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(result.unstable, true);
  assert.ok(result.fingerprint);
  trim.bypassed = true;
  assert.deepEqual(evaluate(layer).run(trim.id), originalSource);
});

test('Trim Path rejects invalid ranges atomically and respects parameter, history, and layer locks', () => {
  const { doc, layer, trim } = scene(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: trim.id, ...payload },
      }),
    before = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'start', value: -0.1 }), /0 to 1/);
  assert.throws(() => send('set-parameter', { key: 'end', value: 1.1 }), /0 to 1/);
  assert.deepEqual(session.snapshot(), before);

  send('set-parameter', { key: 'start', value: 0.75 });
  const valid = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'end', value: 0.25 }), /must not exceed/);
  assert.deepEqual(session.snapshot(), valid);
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), valid);

  send('set-parameter-lock', { key: 'start', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'start', value: 0.5 }), /lock/i);
  assert.deepEqual(session.snapshot(), locked);

  const lockedDocument = structuredClone(doc);
  lockedDocument.layers[0].locked = true;
  const lockedSession = new CommandSession(lockedDocument);
  assert.throws(
    () =>
      lockedSession.dispatch({
        ...lockedSession.state,
        type: 'set-parameter',
        payload: { layerId: layer.id, nodeId: trim.id, key: 'start', value: 0.25 },
      }),
    /composition is locked/i,
  );
});

test('Trim Path resolves tiny-scale curves without Paper getTimeAt absolute-tolerance collapse', () => {
  const { layer, trim } = scene('M0 0C0 1e-13 1e-13 1e-13 1e-13 0');
  trim.params.start.value = 0.25;
  trim.params.end.value = 0.75;
  const points = evaluate(layer).run(trim.id).contours[0].points;
  assert.equal(points.length, 2);
  assert.ok(points[0].position[1] > 0);
  assert.ok(points[1].position[1] > 0);
  assert.ok(
    points.every((point) =>
      [...point.position, ...point.handleA, ...point.handleB].every(Number.isFinite),
    ),
  );
});

test('Trim Path marks direct canvas character editing unavailable while retaining Text provenance', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = opentype.parse(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    ),
    doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    trim = node('Trim Path');
  layer.nodes = [text, trim];
  connect(layer, text.id, trim.id, 'path');
  const path = evaluate(layer, { default: font }).run(trim.id);
  assert.equal(path.characterOccurrences.length, 1);
  assert.match(
    path.characterOccurrences[0].reason,
    /Canvas character editing is unavailable through Trim Path/,
  );
  assert.equal(path.characterOccurrences[0].source, text.id);
});
