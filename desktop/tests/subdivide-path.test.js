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

const near = (actual, expected, epsilon = 1e-7) => {
  assert.equal(actual.length, expected.length);
  actual.forEach((value, index) =>
    assert.ok(Math.abs(value - expected[index]) <= epsilon, `${actual} != ${expected}`),
  );
};
const pointless = (path) =>
  path.contours.map(({ id, points, ...contour }) => ({
    ...contour,
    points: points.map(({ id: pointId, ...point }) => point),
  }));
function scene(raw = 'M0 0L100 0') {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    subdivide = node('Subdivide Path'),
    fill = node('Fill'),
    output = node('Output');
  source.rawPath = raw;
  layer.nodes = [source, subdivide, fill, output];
  layer.output = output.id;
  connect(layer, source.id, subdivide.id, 'path');
  connect(layer, subdivide.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, source, subdivide };
}
function sampledPath(path) {
  const contour = Array.isArray(path.contours) ? path.contours[0] : path;
  const scope = new paper.PaperScope();
  scope.setup(new scope.Size(1200, 800));
  const item = new scope.Path({ insert: false, closed: contour.closed });
  for (const point of contour.points)
    item.add(new scope.Segment(point.position, point.handleA, point.handleB));
  return { scope, item };
}

test('Subdivide Path splits straight edges into per-edge anchors and keeps endpoints and zero handles', () => {
  const { layer, source, subdivide } = scene('M0 0L90 0L90 45');
  subdivide.params.segments.value = 3;
  const before = JSON.stringify(source),
    input = evaluate(layer).run(source.id).contours[0],
    result = evaluate(layer).run(subdivide.id).contours[0];
  const { scope, item } = sampledPath({ contours: [input] });
  const expected = [input.points[0].position];
  for (const curve of item.curves)
    for (let part = 0; part < 3; part++)
      expected.push(curve.getPart(part / 3, (part + 1) / 3).values.slice(6, 8));
  assert.equal(result.points.length, 7);
  assert.equal(result.closed, false);
  assert.deepEqual(
    result.points.map((point) => point.position),
    expected,
  );
  near(result.points[0].position, input.points[0].position);
  near(result.points.at(-1).position, input.points.at(-1).position);
  assert.ok(
    result.points.every((point) =>
      [...point.handleA, ...point.handleB].every((value) => value === 0),
    ),
  );
  assert.equal(JSON.stringify(source), before);
  item.remove();
  scope.project.clear();
});

test('Subdivide Path keeps open endpoint handles and handles empty and zero-length contours explicitly', () => {
  const empty = scene('');
  assert.deepEqual(evaluate(empty.layer).run(empty.subdivide.id).contours, []);

  const degenerate = scene('M10 20L10 20');
  degenerate.subdivide.params.segments.value = 3;
  const repeated = evaluate(degenerate.layer).run(degenerate.subdivide.id).contours[0];
  assert.equal(repeated.points.length, 4);
  assert.ok(
    repeated.points.every((point) =>
      [...point.position, ...point.handleA, ...point.handleB].every(Number.isFinite),
    ),
  );
  assert.ok(repeated.points.every((point) => point.position[0] === 10 && point.position[1] === 20));

  const doc = document(),
    layer = doc.layers[0],
    source = node('Curve'),
    subdivide = node('Subdivide Path');
  source.params.closed.value = false;
  source.points[0].handleA.value = [2, 3];
  source.points.at(-1).handleB.value = [4, 5];
  layer.nodes = [source, subdivide];
  connect(layer, source.id, subdivide.id, 'path');
  const result = evaluate(layer).run(subdivide.id).contours[0];
  near(result.points[0].handleA, [2, 3]);
  near(result.points.at(-1).handleB, [4, 5]);
});

test('Subdivide Path retains exact cubic shape and tangent continuity at every inserted anchor', () => {
  const { layer, source, subdivide } = scene('M0 0C0 120 100 120 100 0');
  subdivide.params.segments.value = 4;
  const input = evaluate(layer).run(source.id).contours[0],
    result = evaluate(layer).run(subdivide.id).contours[0],
    { scope, item: original } = sampledPath(input),
    { item: split } = sampledPath(result),
    originalCurve = original.curves[0];
  assert.equal(result.points.length, 5);
  assert.equal(split.curves.length, 4);
  for (let part = 0; part < 4; part++)
    for (let sample = 0; sample <= 8; sample++) {
      const t = sample / 8,
        actual = split.curves[part].getPointAtTime(t),
        expected = originalCurve.getPointAtTime((part + t) / 4);
      near([actual.x, actual.y], [expected.x, expected.y], 2e-7);
    }
  for (let index = 1; index < result.points.length - 1; index++) {
    const anchor = result.points[index];
    near([-anchor.handleA[0], -anchor.handleA[1]], anchor.handleB, 2e-7);
    assert.ok(Math.hypot(...anchor.handleA) > 0);
    assert.ok(Math.hypot(...anchor.handleB) > 0);
  }
  assert.equal(result.points[0].id, `${result.id}:p0`);
  original.remove();
  split.remove();
  scope.project.clear();
});

test('Subdivide Path preserves a closed seam without duplicating its endpoint or losing curve handles', () => {
  const { layer, subdivide } = scene('M0 0C0 30 30 30 30 0L30 -30Z');
  subdivide.params.segments.value = 2;
  const result = evaluate(layer).run(subdivide.id).contours[0];
  assert.equal(result.closed, true);
  assert.equal(result.points.length, 6);
  assert.notDeepEqual(result.points[0].position, result.points.at(-1).position);
  assert.ok(
    result.points.every((point) =>
      [...point.position, ...point.handleA, ...point.handleB].every(Number.isFinite),
    ),
  );
  assert.ok(result.points.slice(0, 2).some((point) => Math.hypot(...point.handleB) > 0));
  const { scope, item } = sampledPath(result);
  assert.equal(item.curves.length, 6);
  near([item.curves.at(-1).getPointAtTime(1).x, item.curves.at(-1).getPointAtTime(1).y], [0, 0]);
  item.remove();
  scope.project.clear();
});

test('Subdivide Path retains Join Geometry contour groups', () => {
  const doc = document(),
    layer = doc.layers[0],
    first = node('Imported Path'),
    second = node('Imported Path'),
    join = node('Join Geometry'),
    subdivide = node('Subdivide Path');
  first.rawPath = 'M0 0L20 0L20 20Z';
  second.rawPath = 'M40 0L60 0L60 20Z';
  layer.nodes = [first, second, join, subdivide];
  connect(layer, first.id, join.id, 'a');
  connect(layer, second.id, join.id, 'b');
  connect(layer, join.id, subdivide.id, 'path');
  const input = evaluate(layer).run(join.id),
    result = evaluate(layer).run(subdivide.id);
  assert.equal(result.contours.length, input.contours.length);
  assert.deepEqual(
    result.contours.map((contour) => contour.group),
    input.contours.map((c) => c.group),
  );
  assert.ok(result.contours.every((contour) => contour.points.length === 6));
});

test('Subdivide Path with one segment is a geometry-preserving clone with deterministic fresh IDs', () => {
  const { layer, source, subdivide } = scene('M0 0C0 100 100 100 100 0 M200 0L240 0');
  subdivide.params.segments.value = 1;
  const sourcePath = evaluate(layer).run(source.id),
    result = evaluate(layer).run(subdivide.id),
    again = evaluate(layer).run(subdivide.id);
  assert.deepEqual(pointless(result), pointless(sourcePath));
  assert.notEqual(result.contours[0].id, sourcePath.contours[0].id);
  assert.ok(
    result.contours.every((contour, index) =>
      contour.points.every(
        (point, pointIndex) => point.id !== sourcePath.contours[index].points[pointIndex].id,
      ),
    ),
  );
  assert.equal(result.unstable, true);
  assert.ok(result.fingerprint);
  assert.deepEqual(result, again);
});

test('Subdivide Path JSON, SVG, bypass and source graph immutability', () => {
  const { doc, layer, source, subdivide } = scene('M0 0C0 80 80 80 80 0'),
    originalSource = JSON.stringify(source);
  subdivide.params.segments.value = 3;
  const result = evaluate(layer).run(subdivide.id),
    reopened = deserialize(serialize(doc));
  assert.deepEqual(evaluate(reopened.layers[0]).run(subdivide.id), result);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(JSON.stringify(source), originalSource);
  subdivide.bypassed = true;
  assert.deepEqual(evaluate(layer).run(subdivide.id), evaluate(layer).run(source.id));
});

test('Subdivide Path validates disconnected input and resolved segments, and rejects output over budget', () => {
  const disconnected = document(),
    loose = node('Subdivide Path');
  disconnected.layers[0].nodes.push(loose);
  assert.throws(() => evaluate(disconnected.layers[0]).run(loose.id), /Connect a Path input/);

  const connected = scene(),
    scalar = node('Value');
  scalar.params.value.value = 2.5;
  connected.layer.nodes.push(scalar);
  connect(connected.layer, scalar.id, connected.subdivide.id, 'segments');
  assert.throws(
    () => evaluate(connected.layer).run(connected.subdivide.id),
    /integer from 1 to 32/,
  );

  const large = scene('M0 0' + Array.from({ length: 3125 }, (_, i) => `L${i + 1} 0`).join(''));
  large.subdivide.params.segments.value = 32;
  assert.throws(() => evaluate(large.layer).run(large.subdivide.id), /geometry budget/);

  const transactional = scene(
      'M0 0' + Array.from({ length: 3125 }, (_, i) => `L${i + 1} 0`).join(''),
    ),
    session = new CommandSession(transactional.doc),
    before = session.snapshot();
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: {
          layerId: transactional.layer.id,
          nodeId: transactional.subdivide.id,
          key: 'segments',
          value: 32,
        },
      }),
    /geometry budget/,
  );
  assert.deepEqual(session.snapshot(), before);
});

test('Subdivide Path range validation is atomic and follows common history and locks', () => {
  const { doc, layer, subdivide } = scene(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: subdivide.id, ...payload },
      }),
    before = session.snapshot();
  for (const value of [0, 1.5, 33]) {
    assert.throws(() => send('set-parameter', { key: 'segments', value }), /integer from 1 to 32/);
    assert.deepEqual(session.snapshot(), before);
  }
  const malformed = structuredClone(doc);
  malformed.layers[0].nodes.find(
    (candidate) => candidate.id === subdivide.id,
  ).params.segments.value = 1.5;
  assert.throws(() => deserialize(serialize(malformed)), /integer from 1 to 32/);

  send('set-parameter', { key: 'segments', value: 4 });
  const valid = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), valid);
  send('set-parameter-lock', { key: 'segments', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'segments', value: 8 }), /lock/i);
  assert.deepEqual(session.snapshot(), locked);
  const lockedDocument = structuredClone(doc);
  lockedDocument.layers[0].locked = true;
  const lockedSession = new CommandSession(lockedDocument);
  assert.throws(
    () =>
      lockedSession.dispatch({
        ...lockedSession.state,
        type: 'set-parameter',
        payload: { layerId: layer.id, nodeId: subdivide.id, key: 'segments', value: 3 },
      }),
    /composition is locked/i,
  );
});

test('Subdivide Path disables direct canvas character edits but keeps upstream Text provenance', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = opentype.parse(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    ),
    doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    subdivide = node('Subdivide Path');
  layer.nodes = [text, subdivide];
  connect(layer, text.id, subdivide.id, 'path');
  const path = evaluate(layer, { default: font }).run(subdivide.id);
  assert.equal(path.characterOccurrences.length, 1);
  assert.equal(path.characterOccurrences[0].source, text.id);
  assert.match(
    path.characterOccurrences[0].reason,
    /Canvas character editing is unavailable through Subdivide Path/,
  );
  assert.equal(
    evaluate(layer, { default: font }).run(text.id).characterOccurrences[0].reason,
    null,
  );
});
