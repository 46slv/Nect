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
  activeParameters,
} from '../src/core.js';
import { toPaper } from '../src/paper-runtime.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
const near = (a, b) => a.forEach((x, i) => assert.ok(Math.abs(x - b[i]) < 1e-6, `${a} != ${b}`));
function fixture(raw = 'M10 10L110 10') {
  const doc = document(),
    l = doc.layers[0],
    a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    spine = node('Imported Path'),
    blend = node('Blend'),
    out = node('Output');
  a.params.center.value = [10, 10];
  b.params.center.value = [110, 10];
  spine.rawPath = raw;
  blend.operation = 'spine-steps';
  blend.params.steps.value = 1;
  l.nodes = [a, b, fa, fb, spine, blend, out];
  l.output = out.id;
  for (const [from, to, key] of [
    [a, fa, 'path'],
    [b, fb, 'path'],
    [fa, blend, 'a'],
    [fb, blend, 'b'],
    [spine, blend, 'spine'],
    [blend, out, 'path'],
  ])
    connect(l, from.id, to.id, key);
  return { doc, l, a, b, spine, blend };
}
const result = (doc) => evaluate(doc.layers[0], {}, doc.imageAssets, doc);
const center = (c) => {
  const item = toPaper({ contours: [c] });
  try {
    return [item.bounds.center.x, item.bounds.center.y];
  } finally {
    item.remove();
  }
};
test('straight matching spine agrees with Steps and preserves authored inputs', () => {
  const { doc, blend } = fixture();
  const before = JSON.stringify(doc),
    spined = exportSVG(doc);
  assert.equal(JSON.stringify(doc), before);
  blend.operation = 'steps';
  assert.equal(exportSVG(doc), spined);
  assert.equal(
    activeParameters(blend).some(([k]) => k === 'spine'),
    false,
  );
});
test('cubic spine places endpoints and intermediate with fixed handles and roundtrips', () => {
  const { doc, blend } = fixture('M0 0C0 100 100 100 100 0');
  const r = result(doc);
  assert.equal(r.error, null);
  r.path.children.forEach((c, i) =>
    near(
      center(c.contours[0]),
      [
        [0, 0],
        [50, 75],
        [100, 0],
      ][i],
    ),
  );
  const points = r.path.children.map((c) => c.contours[0].points);
  assert.deepEqual(
    points[0].map((p) => [p.handleA, p.handleB]),
    points[1].map((p) => [p.handleA, p.handleB]),
  );
  assert.equal(exportSVG(deserialize(serialize(doc))), exportSVG(doc));
  blend.params.steps.value = 0;
  assert.equal(result(doc).path.children.length, 2);
});
test('invalid and missing spines diagnose; explicit mode switch keeps dormant wire', () => {
  for (const raw of ['M0 0L1 1Z', 'M0 0L0 0', 'M0 0L10 0M20 0L30 0']) {
    const { doc } = fixture(raw);
    assert.ok(result(doc).error, raw);
  }
  const { doc, l, blend, spine } = fixture();
  l.nodes = l.nodes.filter((n) => n.id !== spine.id);
  assert.ok(result(doc).error);
  blend.operation = 'distance';
  assert.equal(result(doc).error, null);
  assert.ok(blend.params.spine.source);
  blend.operation = 'spine-steps';
  assert.ok(result(doc).error);
});
test('Session edits, Undo/Redo, expansion and SVG retain spined result', () => {
  const { doc, l, blend } = fixture('M0 0C0 100 100 100 100 0');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const original = exportSVG(session.snapshot());
  send('set-parameter', { nodeId: blend.id, key: 'steps', value: 3 });
  assert.equal(result(session.snapshot()).path.children.length, 5);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), original);
  send('redo');
  const before = exportSVG(session.snapshot());
  send('expand-blend', { nodeId: blend.id });
  assert.equal(exportSVG(session.snapshot()), before);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), before);
});
test('morph placement uses interpolated endpoint pivots, not intermediate bounds recentering', () => {
  const { doc, a, b, blend } = fixture('M0 0L100 100');
  a.params.size.value = [20, 40];
  b.params.size.value = [80, 10];
  const spineResult = result(doc).path;
  blend.operation = 'steps';
  const straight = result(doc).path;
  for (let i = 0; i < 3; i++) {
    const t = i / 2;
    const offset = [100 * t - (10 + 100 * t), 100 * t - 10];
    spineResult.children[i].contours[0].points.forEach((p, j) =>
      near(
        p.position,
        straight.children[i].contours[0].points[j].position.map((x, k) => x + offset[k]),
      ),
    );
  }
});

test('asymmetric morph retains its virtual pivot rather than recentering actual bounds', async () => {
  const { evaluateBlend } = await import('../src/blend-node-binding.js');
  const { paintedGroup } = await import('../src/composition-reference.js');
  const shape = (positions) => ({
    fill: { hex: '#000000', alpha: 1 },
    contours: [
      {
        closed: true,
        points: positions.map((position) => ({ position, handleA: [0, 0], handleB: [0, 0] })),
      },
    ],
  });
  const a = shape([
      [0, 0],
      [20, 0],
      [0, 10],
    ]),
    b = shape([
      [100, 0],
      [120, 50],
      [130, 10],
      [110, 30],
    ]);
  const spine = {
    contours: [
      {
        closed: false,
        points: [
          [0, 0],
          [100, 100],
        ].map((position) => ({ position, handleA: [0, 0], handleB: [0, 0] })),
      },
    ],
  };
  const run = (operation) =>
    evaluateBlend(operation, (key) => ({ a, b, spine, steps: 1 })[key], {
      nodeId: 'asymmetric',
      groupAppearance: paintedGroup,
    });
  const straight = run('steps'),
    placed = run('spine-steps');
  const ca = center(a.contours[0]),
    cb = center(b.contours[0]);
  const delta = [50, 50].map((x, j) => x - (ca[j] + cb[j]) / 2);
  placed.children[1].contours[0].points.forEach((p, i) =>
    near(
      p.position,
      straight.children[1].contours[0].points[i].position.map((x, j) => x + delta[j]),
    ),
  );
  assert.notDeepEqual(center(placed.children[1].contours[0]), [50, 50]);
});

test('spine helper rejects nonfinite controls and bounded input overflow', async () => {
  const { blendSpineOffsets } = await import('../src/blend-spine.js');
  const p = (position) => ({ position, handleA: [0, 0], handleB: [0, 0] });
  const shape = {
    closed: true,
    points: [
      [0, 0],
      [10, 0],
      [0, 10],
    ].map(p),
  };
  for (const points of [
    [p([0, 0]), p([Infinity, 0])],
    Array.from({ length: 513 }, (_, i) => p([i, 0])),
  ])
    assert.throws(
      () => blendSpineOffsets({ contours: [{ closed: false, points }] }, shape, shape, 3),
      /open finite path/,
    );
});

test('spine export blockers propagate to every generated child', async () => {
  const { evaluateBlend } = await import('../src/blend-node-binding.js');
  const { paintedGroup } = await import('../src/composition-reference.js');
  const point = (position) => ({ position, handleA: [0, 0], handleB: [0, 0] });
  const a = {
    fill: { hex: '#000000', alpha: 1 },
    contours: [
      {
        closed: true,
        points: [
          [0, 0],
          [10, 0],
          [0, 10],
        ].map(point),
      },
    ],
    exportBlockers: ['source-blocker'],
  };
  const b = structuredClone(a);
  const spine = {
    contours: [
      {
        closed: false,
        points: [
          [0, 0],
          [100, 100],
        ].map(point),
      },
    ],
    exportBlockers: ['spine-blocker', 'source-blocker'],
  };
  const before = JSON.stringify([a, b, spine]);
  for (const operation of ['spine-steps', 'spine-distance']) {
    const output = evaluateBlend(
      operation,
      (key) => ({ a, b, spine, steps: 3, distance: 40 })[key],
      {
        nodeId: 'blocked-spine',
        groupAppearance: paintedGroup,
      },
    );
    assert.equal(output.children.length, 5);
    for (const child of output.children)
      assert.deepEqual(child.exportBlockers, ['source-blocker', 'spine-blocker']);
  }
  const many = structuredClone(a);
  many.contours[0].points = Array.from({ length: 512 }, (_, i) => point([i, 0]));
  assert.throws(
    () =>
      evaluateBlend('spine-distance', (key) => ({ a: many, b: many, spine, distance: 0.56 })[key], {
        nodeId: 'spine-budget',
        groupAppearance: paintedGroup,
      }),
    /geometry budget/,
  );
  assert.equal(JSON.stringify([a, b, spine]), before);
});

test('Spine Distance samples equal arc intervals including endpoints and preserves morphing', () => {
  const { doc, a, b, blend } = fixture('M0 0C0 100 100 100 100 0');
  a.params.size.value = [20, 40];
  b.params.size.value = [80, 10];
  blend.operation = 'spine-distance';
  blend.params.distance.value = 80;
  blend.params.steps.value = -1; // Dormant Steps has no effect.
  assert.deepEqual(
    activeParameters(blend).map(([key]) => key),
    ['a', 'b', 'distance', 'spine'],
  );
  const before = serialize(doc);
  const output = result(doc);
  assert.equal(output.error, null);
  assert.equal(output.path.children.length, 4); // L=200, ceil(200/80)=3, not chord length.
  near(center(output.path.children[0].contours[0]), [0, 0]);
  near(center(output.path.children.at(-1).contours[0]), [100, 0]);
  assert.equal(serialize(doc), before);
  const spined = exportSVG(doc);
  assert.equal(exportSVG(deserialize(before)), spined);
  const project = migrateLegacyDocumentToCompositionProject(doc);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(project)), project);

  blend.operation = 'spine-steps';
  blend.params.steps.value = 2;
  assert.equal(exportSVG(doc), spined); // Same arc samples, fixed orientation and paint.
  blend.operation = 'distance';
  assert.ok(result(doc).error); // Unequal shapes are only accepted along the spine.
});

test('Spine Distance bounds interval count, diagnoses bad input and permits fixed corner samples', () => {
  const { doc, spine, blend } = fixture('M0 0L255 0');
  blend.operation = 'spine-distance';
  blend.params.distance.value = 1;
  assert.equal(result(doc).path.children.length, 256);
  for (const [distance, regex] of [
    [0, /Spine Distance must be positive and finite/],
    [-1, /Spine Distance must be positive and finite/],
    [Number.MIN_VALUE, /Spine Distance exceeds 255 intervals/],
  ]) {
    blend.params.distance.value = distance;
    assert.match(result(doc).error, regex);
  }
  blend.params.distance.value = 100;
  spine.rawPath = 'M0 0L10 0L10 10';
  assert.equal(result(doc).path.children.length, 2);
  blend.params.distance.value = 11;
  const corner = result(doc);
  assert.equal(corner.error, null);
  assert.equal(corner.path.children.length, 3);
  near(center(corner.path.children[1].contours[0]), [10, 0]);
  for (const invalid of ['M0 0L1 1Z', 'M0 0L0 0', 'M0 0L10 0M20 0L30 0']) {
    spine.rawPath = invalid;
    assert.ok(result(doc).error, invalid);
  }
  blend.params.spine.source = null;
  assert.match(result(doc).error, /Spine Distance requires one open finite path/);
});

test('Spine Distance edit Undo/Redo and Expand preserve expected geometry and SVG', () => {
  const { doc, l, blend } = fixture('M0 0C0 100 100 100 100 0');
  blend.operation = 'spine-distance';
  blend.params.distance.value = 80;
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: l.id, ...payload } });
  const original = exportSVG(session.snapshot());
  send('set-parameter', { nodeId: blend.id, key: 'distance', value: 0 });
  assert.match(result(session.snapshot()).error, /Spine Distance must be positive and finite/);
  const invalid = session.snapshot();
  assert.throws(() => send('expand-blend', { nodeId: blend.id }), /Spine Distance/);
  assert.deepEqual(session.snapshot(), invalid);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), original);
  send('set-parameter', { nodeId: blend.id, key: 'distance', value: 45 });
  assert.equal(result(session.snapshot()).path.children.length, 6);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), original);
  send('redo');
  const edited = exportSVG(session.snapshot());
  assert.notEqual(edited, original);
  send('expand-blend', { nodeId: blend.id });
  assert.equal(exportSVG(session.snapshot()), edited);
  send('undo');
  assert.equal(exportSVG(session.snapshot()), edited);
});
