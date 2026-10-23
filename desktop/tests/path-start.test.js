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

const near = (actual, expected, epsilon = 1e-5) => {
  assert.equal(actual.length, expected.length);
  actual.forEach((value, index) =>
    assert.ok(Math.abs(value - expected[index]) <= epsilon, `${actual} != ${expected}`),
  );
};

function scene(raw = 'M0 0L100 0L100 100L0 100Z') {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    start = node('Set Path Start'),
    fill = node('Fill'),
    output = node('Output');
  source.rawPath = raw;
  layer.nodes = [source, start, fill, output];
  layer.output = output.id;
  connect(layer, source.id, start.id, 'path');
  connect(layer, start.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, source, start };
}

function cubicSegments(contour) {
  const count = contour.closed ? contour.points.length : Math.max(0, contour.points.length - 1);
  return Array.from({ length: count }, (_, index) => {
    const a = contour.points[index],
      b = contour.points[(index + 1) % contour.points.length];
    return [
      [...a.position],
      a.position.map((value, axis) => value + a.handleB[axis]),
      b.position.map((value, axis) => value + b.handleA[axis]),
      [...b.position],
    ];
  });
}

function area(contour) {
  return (
    contour.points.reduce((sum, point, index, points) => {
      const next = points[(index + 1) % points.length];
      return sum + point.position[0] * next.position[1] - next.position[0] * point.position[1];
    }, 0) / 2
  );
}

test('Set Path Start rotates a closed polygon seam by per-contour arc length without reversing winding', () => {
  const { layer, source, start } = scene(),
    original = evaluate(layer).run(source.id).contours[0],
    sourceBefore = JSON.stringify(source);
  start.params.start.value = 0.25;
  const result = evaluate(layer).run(start.id).contours[0];

  assert.equal(result.closed, true);
  assert.deepEqual(
    result.points.map(({ position }) => position),
    [
      [100, 0],
      [100, 100],
      [0, 100],
      [0, 0],
    ],
  );
  assert.equal(area(result), area(original));
  assert.deepEqual(cubicSegments(result), [
    cubicSegments(original)[1],
    cubicSegments(original)[2],
    cubicSegments(original)[3],
    cubicSegments(original)[0],
  ]);
  assert.notEqual(result.id, original.id);
  assert.equal(JSON.stringify(source), sourceBefore);
});

test('Set Path Start splits one cubic exactly and rejoins the tail/head at the new seam', () => {
  const { layer, source, start } = scene('M0 0C0 100 100 100 100 0L100 100L0 100Z'),
    original = evaluate(layer).run(source.id).contours[0],
    scope = new paper.PaperScope();
  scope.setup(new scope.Size(1200, 800));
  start.params.start.value = 0.13;
  const result = evaluate(layer).run(start.id).contours[0];
  const path = new scope.Path({
      segments: original.points.map(
        (point) => new scope.Segment(point.position, point.handleA, point.handleB),
      ),
      closed: true,
      insert: false,
    }),
    target = path.length * start.params.start.value;
  let cursor = 0,
    edgeIndex = -1,
    time = 0;
  for (let index = 0; index < path.curves.length; index++) {
    const curve = path.curves[index];
    if (target < cursor + curve.length) {
      edgeIndex = index;
      time = curve.getTimeAt(target - cursor);
      break;
    }
    cursor += curve.length;
  }
  assert.equal(edgeIndex, 0, 'fixture must split the first cubic edge');
  assert.ok(time > 0 && time < 1);
  const expected = [
    path.curves[edgeIndex].getPart(time, 1).values,
    ...Array.from(
      { length: original.points.length - 1 },
      (_, offset) => path.curves[(edgeIndex + 1 + offset) % path.curves.length].values,
    ),
    path.curves[edgeIndex].getPart(0, time).values,
  ];
  const actual = cubicSegments(result).map((segment) => segment.flat());
  assert.equal(result.closed, true);
  assert.equal(result.points.length, original.points.length + 1);
  assert.equal(actual.length, expected.length);
  actual.forEach((values, index) => near(values, expected[index]));
  near(cubicSegments(result).at(-1).at(-1), result.points[0].position);
  path.remove();
});

test('Set Path Start avoids a duplicate knot at an exact vertex and preserves open/degenerate contours', () => {
  const { layer, source, start } = scene('M0 0L100 0L100 100L0 100Z M5 10L25 10 M50 50L50 50Z'),
    original = evaluate(layer).run(source.id),
    sourceBefore = JSON.stringify(source);
  start.params.start.value = 0.25;
  const result = evaluate(layer).run(start.id);
  assert.equal(result.contours.length, original.contours.length);
  assert.equal(result.contours[0].closed, true);
  assert.equal(result.contours[0].points.length, original.contours[0].points.length);
  assert.deepEqual(result.contours[0].points[0].position, [100, 0]);
  assert.equal(result.contours[1].closed, false);
  assert.deepEqual(
    result.contours[1].points.map(({ position, handleA, handleB }) => ({
      position,
      handleA,
      handleB,
    })),
    original.contours[1].points.map(({ position, handleA, handleB }) => ({
      position,
      handleA,
      handleB,
    })),
  );
  assert.equal(result.contours[2].closed, true);
  assert.deepEqual(
    result.contours[2].points,
    original.contours[2].points.map((point, index) => ({
      ...point,
      id: result.contours[2].points[index].id,
    })),
  );
  assert.equal(JSON.stringify(source), sourceBefore);
});

test('Set Path Start clones endpoint identities, preserves groups and has deterministic unstable output', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    second = node('Imported Path'),
    join = node('Join Geometry'),
    start = node('Set Path Start');
  source.rawPath = 'M0 0L100 0L100 100L0 100Z';
  second.rawPath = 'M200 0L300 0L300 100L200 100Z';
  layer.nodes = [source, second, join, start];
  connect(layer, source.id, join.id, 'a');
  connect(layer, second.id, join.id, 'b');
  connect(layer, join.id, start.id, 'path');
  const original = evaluate(layer).run(join.id);
  start.params.start.value = 0;
  const zero = evaluate(layer).run(start.id),
    again = evaluate(layer).run(start.id);
  assert.deepEqual(zero, again);
  assert.equal(zero.unstable, true);
  assert.ok(zero.fingerprint);
  assert.notEqual(zero.contours[0].id, original.contours[0].id);
  assert.deepEqual(
    zero.contours.map(({ group }) => group),
    original.contours.map(({ group }) => group),
  );
  for (const end of [0, 1]) {
    start.params.start.value = end;
    const result = evaluate(layer).run(start.id);
    assert.deepEqual(
      result.contours.map(({ closed, points }) => ({
        closed,
        points: points.map(({ position, handleA, handleB }) => ({ position, handleA, handleB })),
      })),
      original.contours.map(({ closed, points }) => ({
        closed,
        points: points.map(({ position, handleA, handleB }) => ({ position, handleA, handleB })),
      })),
    );
  }
  start.params.start.value = 0.25;
  const moved = evaluate(layer).run(start.id);
  assert.deepEqual(
    moved.contours.map((contour) => contour.group),
    original.contours.map((contour) => contour.group),
  );
  assert.deepEqual(
    moved.contours.map((contour) => contour.points[0].position),
    [
      [100, 0],
      [300, 0],
    ],
  );
});

test('Set Path Start locates tiny cubic seams with normalized arc-length lookup', () => {
  const { layer, start } = scene('M0 0C0 1e-12 1e-12 1e-12 1e-12 0L1e-12 1e-12Z');
  start.params.start.value = 0.1;
  const result = evaluate(layer).run(start.id).contours[0];
  assert.equal(result.closed, true);
  assert.ok(result.points.length > 3);
  assert.ok(
    result.points.every((point) =>
      [...point.position, ...point.handleA, ...point.handleB].every(Number.isFinite),
    ),
  );
});

test('Set Path Start retains the source Text editing restriction', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = opentype.parse(
      bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    ),
    doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    start = node('Set Path Start');
  layer.nodes = [text, start];
  connect(layer, text.id, start.id, 'path');
  const result = evaluate(layer, { default: font }).run(start.id);
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.match(
    result.characterOccurrences[0].reason,
    /Canvas character editing is unavailable through Set Path Start; edit the source Text/,
  );
});

test('Set Path Start rejects input over the bounded contour budget', () => {
  const { layer, start } = scene(Array(10001).fill('M0 0Z').join(' '));
  assert.throws(() => evaluate(layer).run(start.id), /geometry budget/);
});

test('Set Path Start supports bypass, Session history, locks, JSON roundtrip, and SVG export', () => {
  const { doc, layer, source, start } = scene('M0 0C0 100 100 100 100 0Z'),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: start.id, ...payload },
      }),
    before = serialize(session.snapshot());
  send('set-parameter', { key: 'start', value: 0.37 });
  const configured = serialize(session.snapshot()),
    result = evaluate(session.snapshot().layers[0]).run(start.id);
  assert.notEqual(configured, before);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), configured);
  send('set-node-bypassed', { bypassed: true });
  assert.deepEqual(
    evaluate(session.snapshot().layers[0]).run(start.id),
    evaluate(session.snapshot().layers[0]).run(source.id),
  );
  send('undo');
  send('set-parameter-lock', { key: 'start', locked: true });
  const locked = serialize(session.snapshot());
  assert.throws(() => send('set-parameter', { key: 'start', value: 0.5 }), /lock/i);
  assert.equal(serialize(session.snapshot()), locked);
  const reopened = deserialize(serialize(session.snapshot()));
  assert.deepEqual(evaluate(reopened.layers[0]).run(start.id), result);
  assert.match(exportSVG(session.snapshot()), /<path /);
});
