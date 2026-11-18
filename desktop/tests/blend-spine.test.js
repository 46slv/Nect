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
  const output = evaluateBlend('spine-steps', (key) => ({ a, b, spine, steps: 3 })[key], {
    nodeId: 'blocked-spine',
    groupAppearance: paintedGroup,
  });
  assert.equal(output.children.length, 5);
  for (const child of output.children)
    assert.deepEqual(child.exportBlockers, ['source-blocker', 'spine-blocker']);
  assert.equal(JSON.stringify([a, b, spine]), before);
});
