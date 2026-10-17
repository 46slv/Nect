import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {
  document,
  node,
  point,
  connect,
  evaluate,
  pathBounds,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
  enableRemapPoint,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';

const fontBytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
const fonts = {
  default: parseFont(
    fontBytes.buffer.slice(fontBytes.byteOffset, fontBytes.byteOffset + fontBytes.byteLength),
  ),
};

function addTargetPoints(target, positions, handles = []) {
  target.points = positions.map(([x, y], index) => {
    const p = point(x, y);
    p.handleA.value = handles[index] || [0, 0];
    p.handleB.value = handles[index] || [0, 0];
    return p;
  });
  target.params.closed.value = false;
}

function scene({
  positions = [
    [100, 70],
    [5, 12],
  ],
  targetHandles = [],
} = {}) {
  const doc = document(),
    layer = doc.layers[0],
    donut = node('Imported Path'),
    extra = node('Circle'),
    source = node('Join Geometry'),
    targets = node('Curve'),
    effect = node('Instance on Points'),
    fill = node('Fill'),
    output = node('Output');
  donut.rawPath = 'M0 0H20V20H0Z M5 5V15H15V5Z';
  extra.params.center.value = [40, 10];
  extra.params.radius.value = 5;
  addTargetPoints(targets, positions, targetHandles);
  layer.nodes = [donut, extra, source, targets, effect, fill, output];
  layer.output = output.id;
  fill.params.color.value = { hex: '#123456', alpha: 0.5 };
  connect(layer, donut.id, source.id, 'a');
  connect(layer, extra.id, source.id, 'b');
  connect(layer, source.id, effect.id, 'path');
  connect(layer, targets.id, effect.id, 'points');
  connect(layer, effect.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, donut, source, targets, effect, fill, output };
}

function signedArea(contour) {
  const points = contour.points;
  let area = 0;
  for (let index = 0; index < points.length; index++) {
    const a = points[index].position,
      b = points[(index + 1) % points.length].position;
    area += a[0] * b[1] - b[0] * a[1];
  }
  return area / 2;
}

test('centers the source bounds at every target anchor, keeps handles, and creates deterministic duplicate copies', () => {
  const f = scene({
      positions: [
        [100, 70],
        [100, 70],
        [-5, 12],
      ],
      targetHandles: [
        [1000, -20],
        [-30, 400],
        [200, 300],
      ],
    }),
    before = JSON.stringify(f.source),
    sourcePath = evaluate(f.layer).run(f.source.id),
    targets = evaluate(f.layer).run(f.targets.id),
    bounds = pathBounds(sourcePath),
    pivot = [bounds[0][1], bounds[1][1]],
    path = evaluate(f.layer).run(f.effect.id);

  assert.equal(
    path.contours.length,
    sourcePath.contours.length * targets.contours[0].points.length,
  );
  const prefix = path.contours[0].id.split(':i0:c')[0],
    groupIndices = new Map();
  for (const [instanceIndex, targetPoint] of targets.contours[0].points.entries()) {
    const copies = path.contours.slice(
      instanceIndex * sourcePath.contours.length,
      (instanceIndex + 1) * sourcePath.contours.length,
    );
    const delta = [targetPoint.position[0] - pivot[0], targetPoint.position[1] - pivot[1]];
    sourcePath.contours.forEach((sourceContour, contourIndex) => {
      const copy = copies[contourIndex];
      const key = sourceContour.group ?? '';
      if (!groupIndices.has(key)) groupIndices.set(key, groupIndices.size);
      assert.equal(copy.closed, sourceContour.closed);
      assert.equal(copy.id, `${prefix}:i${instanceIndex}:c${contourIndex}`);
      assert.equal(copy.group, `${prefix}:i${instanceIndex}:g${groupIndices.get(key)}`);
      sourceContour.points.forEach((sourcePoint, pointIndex) => {
        const actual = copy.points[pointIndex];
        assert.deepEqual(actual.position, [
          sourcePoint.position[0] + delta[0],
          sourcePoint.position[1] + delta[1],
        ]);
        assert.deepEqual(actual.handleA, sourcePoint.handleA);
        assert.deepEqual(actual.handleB, sourcePoint.handleB);
      });
    });
  }
  assert.equal(
    new Set(path.contours.slice(0, sourcePath.contours.length).map((c) => c.group)).size,
    2,
  );
  assert.equal(
    new Set(
      path.contours
        .slice(sourcePath.contours.length, sourcePath.contours.length * 2)
        .map((c) => c.group),
    ).size,
    2,
  );
  assert.notEqual(path.contours[0].id, path.contours[sourcePath.contours.length].id);
  assert.deepEqual(path, evaluate(f.layer).run(f.effect.id));
  assert.equal(JSON.stringify(f.source), before, 'evaluation must not mutate the source graph');
  assert.equal(path.unstable, true);
  assert.ok(path.fingerprint);
  const fingerprint = path.fingerprint;
  f.targets.points[0].handleA.value[0] += 1;
  const afterTargetHandleEdit = evaluate(f.layer).run(f.effect.id);
  assert.equal(
    afterTargetHandleEdit.fingerprint,
    fingerprint,
    'ignored target handles do not affect provenance',
  );
  assert.deepEqual(afterTargetHandleEdit.contours, path.contours);
});

test('upstream source geometry changes deterministically suspend retained Path Remap edits', () => {
  const f = scene(),
    initial = evaluate(f.layer).run(f.effect.id),
    remap = node('Path Remap');
  f.layer.nodes.push(remap);
  connect(f.layer, f.effect.id, remap.id, 'path');
  enableRemapPoint(remap, initial, initial.contours[0].points[0].id);
  const oldFingerprint = initial.fingerprint;
  f.donut.rawPath = 'M0 0H30V20H0Z M8 5V15H22V5Z';
  const result = evaluate(f.layer).run(remap.id);
  assert.notEqual(result.fingerprint, oldFingerprint);
  assert.equal(result.unstable, true);
  assert.equal(result.remapSuspended, true);
});

test('retains source hole and group membership as independently alpha-painted SVG paths', () => {
  const f = scene(),
    source = evaluate(f.layer).run(f.source.id),
    path = evaluate(f.layer).run(f.effect.id),
    copySize = source.contours.length,
    firstCopy = path.contours.slice(0, copySize),
    secondCopy = path.contours.slice(copySize, copySize * 2),
    svg = exportSVG(f.doc);

  assert.equal(source.contours.length, 3);
  assert.equal(firstCopy[0].group, firstCopy[1].group, 'the two donut contours stay in one group');
  assert.notEqual(firstCopy[0].group, firstCopy[2].group, 'the joined source group stays separate');
  assert.notEqual(firstCopy[0].group, secondCopy[0].group, 'each instance has its own group');
  assert.equal(Math.sign(signedArea(firstCopy[0])), -Math.sign(signedArea(firstCopy[1])));
  assert.equal((svg.match(/<path /g) || []).length, 4);
  assert.equal((svg.match(/fill-opacity="0\.5"/g) || []).length, 4);
  assert.equal((svg.match(/fill-rule="nonzero"/g) || []).length, 4);
});

test('ignores target contour groups while stamping each authored point in order', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Circle'),
    targetA = node('Curve'),
    targetB = node('Curve'),
    targetGroups = node('Join Geometry'),
    effect = node('Instance on Points');
  addTargetPoints(targetA, [[80, 30]], [[700, 10]]);
  addTargetPoints(targetB, [[10, -20]], [[-300, 90]]);
  layer.nodes = [source, targetA, targetB, targetGroups, effect];
  connect(layer, targetA.id, targetGroups.id, 'a');
  connect(layer, targetB.id, targetGroups.id, 'b');
  connect(layer, source.id, effect.id, 'path');
  connect(layer, targetGroups.id, effect.id, 'points');
  const targets = evaluate(layer).run(targetGroups.id),
    path = evaluate(layer).run(effect.id),
    outputAnchors = path.contours.map((contour) => {
      const bounds = pathBounds({ contours: [contour] });
      return [bounds[0][1], bounds[1][1]];
    });
  assert.notEqual(targets.contours[0].group, targets.contours[1].group);
  assert.deepEqual(
    outputAnchors,
    targets.contours.map((contour) => contour.points[0].position),
  );
  assert.notEqual(path.contours[0].group, path.contours[1].group);
  assert.ok(
    path.contours.every(
      (contour) => !targets.contours.some((target) => target.group === contour.group),
    ),
  );
});

test('empty inputs and missing sockets follow ordinary typed wiring semantics', () => {
  const doc = document(),
    layer = doc.layers[0],
    emptySource = node('Curve'),
    targets = node('Curve'),
    effect = node('Instance on Points');
  emptySource.points = [];
  addTargetPoints(targets, [[10, 10]]);
  layer.nodes = [emptySource, targets, effect];
  connect(layer, emptySource.id, effect.id, 'path');
  connect(layer, targets.id, effect.id, 'points');
  const emptyFromSource = evaluate(layer).run(effect.id);
  assert.deepEqual(emptyFromSource.contours, []);

  const noAnchors = scene({ positions: [] });
  assert.deepEqual(evaluate(noAnchors.layer).run(noAnchors.effect.id).contours, []);

  const bare = document(),
    unconnected = node('Instance on Points');
  bare.layers[0].nodes.push(unconnected);
  assert.throws(
    () => evaluate(bare.layers[0]).run(unconnected.id),
    /Connect both Source and Points Path inputs/,
  );

  const partialDoc = document(),
    partialLayer = partialDoc.layers[0],
    badPoints = node('Circle'),
    partial = node('Instance on Points');
  badPoints.params.radius.expression = 'not valid';
  partialLayer.nodes = [badPoints, partial];
  connect(partialLayer, badPoints.id, partial.id, 'points');
  assert.throws(
    () => evaluate(partialLayer).run(partial.id),
    /Connect both Source and Points Path inputs/,
    'a missing Source input stays an ordinary wiring prompt without evaluating Points',
  );
});

test('bypass skips Points evaluation; Session history and locks remain atomic', () => {
  const f = scene();
  f.targets.params.closed.expression = 'not a valid expression';
  const session = new CommandSession(f.doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: f.layer.id, ...payload },
      });
  send('set-node-bypassed', { nodeId: f.effect.id, bypassed: true });
  assert.deepEqual(
    evaluate(session.snapshot().layers[0]).run(f.effect.id),
    evaluate(f.layer).run(f.source.id),
  );
  send('undo');
  assert.throws(
    () => evaluate(session.snapshot().layers[0]).run(f.effect.id),
    /expression|Expected|Unsupported/i,
  );
  send('redo');
  assert.deepEqual(
    evaluate(session.snapshot().layers[0]).run(f.effect.id),
    evaluate(f.layer).run(f.source.id),
  );
  send('undo');

  send('set-parameter-lock', { nodeId: f.effect.id, key: 'points', locked: true });
  const before = serialize(session.snapshot());
  assert.throws(() => send('disconnect', { nodeId: f.effect.id, key: 'points' }), /lock/i);
  assert.equal(serialize(session.snapshot()), before);
  send('undo');
  assert.ok(
    !session.snapshot().layers[0].nodes.find((n) => n.id === f.effect.id).params.points.locked,
  );
});

test('JSON roundtrip preserves both inputs and direct Text character editing is blocked upstream', () => {
  const f = scene(),
    serialized = serialize(f.doc),
    reopened = deserialize(serialized),
    path = evaluate(reopened.layers[0]).run(f.effect.id);
  assert.deepEqual(path, evaluate(f.layer).run(f.effect.id));
  assert.deepEqual(
    reopened.layers[0].nodes.find((n) => n.id === f.effect.id).params,
    f.effect.params,
  );

  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    targets = node('Curve'),
    effect = node('Instance on Points');
  addTargetPoints(targets, [[80, 50]]);
  layer.nodes = [text, targets, effect];
  connect(layer, text.id, effect.id, 'path');
  connect(layer, targets.id, effect.id, 'points');
  const output = evaluate(layer, fonts).run(effect.id);
  assert.ok(
    output.characterOccurrences.some((occurrence) =>
      occurrence.reason?.includes('unavailable through Instance on Points'),
    ),
  );
});

test('target and materialized geometry caps reject before commit and roll back the failed command', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Circle'),
    targets = node('Curve'),
    effect = node('Instance on Points'),
    output = node('Output');
  addTargetPoints(
    targets,
    Array.from({ length: 257 }, (_, i) => [i, 0]),
  );
  layer.nodes = [source, targets, effect, output];
  layer.output = output.id;
  connect(layer, effect.id, output.id, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, ...payload },
      });
  send('connect', { sourceId: source.id, nodeId: effect.id, key: 'path' });
  const before = serialize(session.snapshot());
  assert.throws(
    () => send('connect', { sourceId: targets.id, nodeId: effect.id, key: 'points' }),
    /instance budget/,
  );
  assert.equal(serialize(session.snapshot()), before);

  const large = document(),
    largeLayer = large.layers[0],
    manySourcePoints = node('Curve'),
    manyTargets = node('Curve'),
    manyEffect = node('Instance on Points'),
    manyOutput = node('Output');
  manySourcePoints.points = Array.from({ length: 401 }, (_, i) => point(i, 0));
  addTargetPoints(
    manyTargets,
    Array.from({ length: 256 }, (_, i) => [i, 1]),
  );
  largeLayer.nodes = [manySourcePoints, manyTargets, manyEffect, manyOutput];
  largeLayer.output = manyOutput.id;
  connect(largeLayer, manyEffect.id, manyOutput.id, 'path');
  const largeSession = new CommandSession(large),
    sendLarge = (type, payload = {}) =>
      largeSession.dispatch({
        ...largeSession.state,
        type,
        payload: { layerId: largeLayer.id, ...payload },
      });
  sendLarge('connect', { sourceId: manySourcePoints.id, nodeId: manyEffect.id, key: 'path' });
  const largeBefore = serialize(largeSession.snapshot());
  assert.throws(
    () => sendLarge('connect', { sourceId: manyTargets.id, nodeId: manyEffect.id, key: 'points' }),
    /geometry budget/,
  );
  assert.equal(serialize(largeSession.snapshot()), largeBefore);
});
