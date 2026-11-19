import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  CommandSession,
} from '../src/core.js';
function fixture() {
  const doc = document(),
    l = doc.layers[0];
  const a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    blend = node('Blend'),
    out = node('Output');
  a.params.center.value = [10, 10];
  b.params.center.value = [110, 10];
  blend.operation = 'distance';
  blend.params.distance.value = 30;
  l.nodes = [a, b, fa, fb, blend, out];
  l.output = out.id;
  connect(l, a.id, fa.id, 'path');
  connect(l, b.id, fb.id, 'path');
  connect(l, fa.id, blend.id, 'a');
  connect(l, fb.id, blend.id, 'b');
  connect(l, blend.id, out.id, 'path');
  return { doc, l, a, b, blend };
}
const result = (doc) => evaluate(doc.layers[0], {}, doc.imageAssets, doc);
test('distance equalizes intervals, keeps endpoints and survives codec/SVG', () => {
  const { doc, blend } = fixture();
  const r = result(doc);
  assert.equal(r.error, null);
  assert.equal(r.path.children.length, 5);
  const xs = r.path.children.map((c) => c.contours[0].points[0].position[0]);
  assert.deepEqual(
    xs.map((x) => x - xs[0]),
    [0, 25, 50, 75, 100],
  );
  assert.equal(exportSVG(deserialize(serialize(doc))), exportSVG(doc));
  assert.equal(blend.params.steps.value, 5);
});
test('distance rejects invalid values, scaling and overflow, allows coincident endpoints', () => {
  for (const d of [0, -1, Infinity, NaN, 0.1]) {
    const { doc, blend } = fixture();
    blend.params.distance.value = d;
    assert.ok(result(doc).error);
  }
  const { doc, b } = fixture();
  b.params.center.value = [10, 10];
  assert.equal(result(doc).path.children.length, 2);
  b.params.size.value = [200, 40];
  assert.match(result(doc).error, /identical translated/);
});
test('legacy Blend without operation/distance remains Steps', () => {
  const { doc, blend } = fixture();
  blend.operation = 'subtract';
  delete blend.params.distance;
  blend.params.steps.value = 2;
  const reopened = deserialize(serialize(doc));
  assert.equal(result(reopened).error, null);
  assert.equal(result(reopened).path.children.length, 4);
});
test('distance edits use existing History and expansion', () => {
  const { doc, l, blend } = fixture();
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  send('set-parameter', { nodeId: blend.id, key: 'distance', value: 50 });
  assert.equal(result(session.snapshot()).path.children.length, 3);
  send('undo');
  assert.equal(result(session.snapshot()).path.children.length, 5);
  send('redo');
  assert.equal(result(session.snapshot()).path.children.length, 3);
  const before = exportSVG(session.snapshot());
  send('expand-blend', { nodeId: blend.id });
  assert.equal(result(session.snapshot()).error, null);
  assert.equal(exportSVG(session.snapshot()), before);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), before);
});

test('diagonal translation, large/small coordinates, and exact interval ceiling', () => {
  for (const [start, delta, d, n] of [
    [[0, 0], [60, 80], 25, 5],
    [[1e5, 1e5], [100, 0], 1, 101],
    [[0, 0], [0.001953125, 0], 0.0009765625, 3],
    [[0, 0], [255, 0], 1, 256],
  ]) {
    const { doc, a, b, blend } = fixture();
    a.params.center.value = start;
    b.params.center.value = start.map((x, i) => x + delta[i]);
    blend.params.distance.value = d;
    const r = result(doc);
    assert.equal(r.error, null);
    assert.equal(r.path.children.length, n);
  }
});
test('changed source diagnoses without fallback and explicit Steps recovers', () => {
  const { doc, b, blend } = fixture();
  b.params.size.value = [200, 40];
  assert.match(result(doc).error, /identical translated/);
  blend.operation = 'steps';
  assert.equal(result(doc).error, null);
  assert.equal(result(doc).path.children.length, 7);
});
test('same anchors with different Bezier handles are not translation-equivalent', async () => {
  const { translatedContourDistance } = await import('../src/blend-geometry.js');
  const c = {
    closed: true,
    points: [
      [0, 0],
      [10, 0],
      [0, 10],
    ].map((position) => ({ position, handleA: [0, 0], handleB: [0, 0] })),
  };
  const b = structuredClone(c);
  b.points[0].handleB = [1, 0];
  assert.throws(() => translatedContourDistance(c, b), /identical translated/);
  b.points[0].handleB = [0, 0];
  b.closed = false;
  assert.throws(() => translatedContourDistance(c, b), /identical translated/);
});

test('curved translated contours accept reversed winding and shifted seam', async () => {
  const { translatedContourDistance } = await import('../src/blend-geometry.js');
  const { evaluateBlend } = await import('../src/blend-node-binding.js');
  const { paintedGroup } = await import('../src/composition-reference.js');
  const contour = {
    closed: true,
    points: [
      [0, 0],
      [12, 0],
      [8, 9],
      [0, 5],
    ].map((position, i) => ({
      id: `p${i}`,
      position,
      handleA: [-1, i / 4],
      handleB: [2, i === 0 ? 0 : -i / 4],
    })),
  };
  const translated = structuredClone(contour);
  translated.points = translated.points
    .map((p) => ({
      ...p,
      position: [p.position[0] + 30, p.position[1] + 40],
      handleA: p.handleB,
      handleB: p.handleA,
    }))
    .reverse();
  translated.points.push(translated.points.shift());
  assert.equal(translatedContourDistance(contour, translated), 50);
  const a = { contours: [contour], fill: { hex: '#000000', alpha: 1 } };
  const b = { contours: [translated], fill: { hex: '#ffffff', alpha: 1 } };
  const original = JSON.stringify([a, b]);
  const output = evaluateBlend('distance', (key) => ({ a, b, distance: 25 })[key], {
    nodeId: 'curved',
    groupAppearance: paintedGroup,
  });
  assert.equal(output.children.length, 3);
  const middle = output.children[1].contours[0];
  assert.deepEqual(
    middle.points.map((p) => p.position),
    contour.points.map((p) => [p.position[0] + 15, p.position[1] + 20]),
  );
  assert.deepEqual(
    middle.points.map((p) => [p.handleA, p.handleB]),
    contour.points.map((p) => [p.handleA, p.handleB]),
  );
  assert.equal(JSON.stringify([a, b]), original);
});

test('legacy Boolean tokens evaluate directly and through typed interfaces without mutation', async () => {
  const { operationOptions } = await import('../src/core.js');
  const { evaluateBlend } = await import('../src/blend-node-binding.js');
  const { paintedGroup } = await import('../src/composition-reference.js');
  assert.deepEqual(operationOptions.Blend, [
    'steps',
    'smooth-color',
    'distance',
    'spine-distance',
    'spine-steps',
    'spine-tangent-steps',
  ]);
  const { doc, blend } = fixture();
  blend.operation = 'steps';
  const expected = exportSVG(doc);
  for (const operation of operationOptions.Boolean) {
    blend.operation = operation;
    const before = JSON.stringify(doc);
    assert.equal(result(doc).error, null);
    assert.equal(exportSVG(doc), expected);
    assert.equal(JSON.stringify(doc), before);
    const a = {
      fill: { hex: '#000000', alpha: 1 },
      contours: [
        {
          closed: true,
          points: [
            [0, 0],
            [10, 0],
            [0, 10],
          ].map((position) => ({ position, handleA: [0, 0], handleB: [0, 0] })),
        },
      ],
    };
    const b = structuredClone(a);
    b.contours[0].points.forEach((p) => (p.position[0] += 20));
    const output = evaluateBlend(operation, (key) => ({ a, b, steps: 1 })[key], {
      nodeId: 'legacy',
      groupAppearance: paintedGroup,
    });
    assert.equal(output.children.length, 3);
  }
});
