import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import {
  document,
  node,
  evaluate,
  serialize,
  deserialize,
  outputType,
  connect,
  setNodeBypassed,
  CommandSession,
  enableRemapPoint,
} from '../src/core.js';
import { luminanceInstancesGeometry, selectLuminanceCenters } from '../src/luminance-instances.js';
import { prepareLuminanceMask } from '../src/luminance-mask.js';

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Linear Gradient Fill'),
    motif = node('Imported Path'),
    effect = node('Luminance Instances'),
    paint = node('Fill'),
    out = node('Output');
  shape.params.center.value = [32, 16];
  shape.params.size.value = [64, 32];
  fill.params.path.source = shape.id;
  fill.params.start.value = [0, 16];
  fill.params.end.value = [64, 16];
  fill.params.startColor.value = { hex: '#ffffff', alpha: 1 };
  fill.params.endColor.value = { hex: '#000000', alpha: 1 };
  motif.rawPath = 'M-5 -2 C-3 -4 3 -4 5 0 L-5 2Z';
  effect.params.path.source = fill.id;
  effect.params.motif.source = motif.id;
  effect.params.pitch.value = 16;
  paint.params.path.source = effect.id;
  out.params.path.source = paint.id;
  layer.nodes = [shape, fill, motif, effect, paint, out];
  layer.output = out.id;
  return { doc, layer, shape, fill, motif, effect, paint, out };
}
const run = (f) => evaluate(f.layer, {}, f.doc.imageAssets).run(f.effect.id);
const geometry = {
  contours: [
    {
      id: 'c',
      closed: false,
      group: 'source',
      points: [{ id: 'p', position: [2, 0], handleA: [-1, 0], handleB: [0, 2], tag: 'kept' }],
    },
  ],
};
const opts = {
  minScale: 0,
  maxScale: 2,
  minRotation: 0,
  maxRotation: 90,
  pivot: [0, 0],
  prefix: 'test',
};

test('helper linearly transforms positions/relative cubic handles; zero scale omitted; IDs/groups deterministic', () => {
  const cells = [0, 0.5, 1].map((darkness, i) => ({ x: 10 + i * 10, y: 10, darkness }));
  const before = structuredClone(geometry),
    result = luminanceInstancesGeometry(geometry, cells, opts);
  assert.equal(result.contours.length, 2);
  assert.deepEqual(geometry, before);
  const p = result.contours[1].points[0];
  assert.ok(Math.abs(p.position[0] - 30) < 1e-12);
  assert.ok(Math.abs(p.position[1] - 14) < 1e-12);
  assert.ok(Math.abs(p.handleA[1] + 2) < 1e-12);
  assert.ok(Math.abs(p.handleB[0] + 4) < 1e-12);
  assert.equal(p.tag, 'kept');
  assert.equal(result.contours[1].group, 'test:i1:g0');
  assert.deepEqual(result, luminanceInstancesGeometry(geometry, cells, opts));
});
test('helper rejects invalid cells, endpoints, finite overflow and instance/group/point excess atomically', () => {
  for (const option of [
    { minScale: -1 },
    { maxScale: 101 },
    { minRotation: Infinity },
    { maxRotation: 361 },
  ])
    assert.throws(() => luminanceInstancesGeometry(geometry, [], { ...opts, ...option }));
  assert.throws(() => luminanceInstancesGeometry(geometry, [{ x: 0, y: 0, darkness: NaN }], opts));
  assert.throws(
    () =>
      luminanceInstancesGeometry(
        geometry,
        Array.from({ length: 257 }, () => ({ x: 0, y: 0, darkness: 1 })),
        opts,
      ),
    /instance budget/,
  );
  const grouped = structuredClone(geometry);
  grouped.contours.push({
    ...structuredClone(grouped.contours[0]),
    id: 'd',
    group: 'other',
    points: [{ ...structuredClone(grouped.contours[0].points[0]), id: 'q' }],
  });
  assert.throws(
    () =>
      luminanceInstancesGeometry(
        grouped,
        Array.from({ length: 129 }, () => ({ x: 0, y: 0, darkness: 1 })),
        opts,
      ),
    /geometry budget/,
  );
  const huge = structuredClone(geometry);
  huge.contours[0].points[0].position = [1e308, 0];
  assert.throws(
    () =>
      luminanceInstancesGeometry(huge, [{ x: 0, y: 0, darkness: 1 }], {
        ...opts,
        maxRotation: 0,
        maxScale: 100,
      }),
    /finite/,
  );
});
test('nonzero mask holes, boundary centers, partial image cells and bounded mask work', () => {
  const f = fixture();
  f.motif.rawPath = 'M0 0H64V64H0Z M16 16V48H48V16Z';
  const path = evaluate(f.layer).run(f.motif.id),
    mask = prepareLuminanceMask(path);
  const cells = [
    { x: 8, y: 8 },
    { x: 32, y: 32 },
    { x: 0, y: 8 },
    { x: -8, y: 8 },
  ];
  assert.deepEqual(selectLuminanceCenters(cells, mask), [cells[0], cells[2]]);
  assert.deepEqual(selectLuminanceCenters(cells, null, { position: [0, 0], size: [16, 16] }), [
    cells[0],
    cells[2],
  ]);
  assert.throws(
    () =>
      selectLuminanceCenters(Array(4096).fill(cells[0]), { polygons: [Array(512).fill([0, 0])] }),
    /work budget/,
  );
});
test('core returns unpainted editable cubic Path, useful varying geometry and exact cold reopen', () => {
  const f = fixture(),
    result = run(f);
  assert.equal(outputType(f.effect), 'Path');
  assert.equal(result.fill, undefined);
  assert.equal(result.stroke, undefined);
  assert.equal(result.contours.length, 8);
  assert.ok(result.contours[0].points.some((p) => p.handleB.some(Boolean)));
  const widths = result.contours.map((c) =>
    Math.hypot(
      c.points[1].position[0] - c.points[0].position[0],
      c.points[1].position[1] - c.points[0].position[1],
    ),
  );
  assert.ok(widths[3] > widths[0]);
  const restored = deserialize(serialize(f.doc));
  assert.equal(restored.schemaVersion, 20);
  assert.deepEqual(evaluate(restored.layers[0]).run(f.effect.id), result);
  const cold = execFileSync(
    process.execPath,
    [
      '--input-type=module',
      '-e',
      `import {deserialize,evaluate} from './src/core.js'; import fs from 'node:fs'; const d=deserialize(fs.readFileSync(0,'utf8')); console.log(JSON.stringify(evaluate(d.layers[0]).run(${JSON.stringify(f.effect.id)})));`,
    ],
    { input: serialize(f.doc), encoding: 'utf8' },
  );
  assert.deepEqual(JSON.parse(cold), result);
});
test('bypass passes motif unchanged and ignores source; incomplete setup is a safe wiring state', () => {
  const f = fixture();
  setNodeBypassed(f.layer, f.effect.id, true);
  assert.deepEqual(run(f), evaluate(f.layer).run(f.motif.id));
  f.effect.params.path.source = null;
  assert.deepEqual(run(f), evaluate(f.layer).run(f.motif.id));
  setNodeBypassed(f.layer, f.effect.id, false);
  assert.throws(() => run(f), /Connect both/);
});
test('unsupported source and motif sockets reject; empty source is empty; zero-scale is empty', () => {
  const f = fixture();
  assert.throws(() => connect(f.layer, f.fill.id, f.effect.id, 'motif'), /Cannot connect/);
  f.shape.type = 'Imported Path';
  f.shape.params = {};
  f.shape.rawPath = 'M0 0L10 0L10 10';
  assert.throws(() => run(f), /closed/);
  f.shape.rawPath = '';
  assert.deepEqual(run(f).contours, []);
  const z = fixture();
  z.effect.params.minScale.value = 0;
  z.effect.params.maxScale.value = 0;
  assert.deepEqual(run(z).contours, []);
});
test('only new node promotes schema; history preserves exact geometry and over-budget edits roll back', () => {
  const old = document(),
    saved = serialize(old);
  assert.equal(serialize(deserialize(saved)), saved);
  const f = fixture(),
    session = new CommandSession(f.doc),
    dispatch = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: f.layer.id, ...payload } });
  const before = serialize(session.snapshot());
  assert.throws(
    () => dispatch('set-parameter', { nodeId: f.effect.id, key: 'pitch', value: 1 }),
    /budget|limit/,
  );
  assert.equal(serialize(session.snapshot()), before);
  dispatch('set-parameter', { nodeId: f.effect.id, key: 'maxRotation', value: -90 });
  const after = serialize(session.snapshot());
  assert.notEqual(after, before);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), after);
});

test('generated points remain editable and changes suspend retained remaps by provenance', () => {
  const f = fixture(),
    initial = run(f),
    remap = node('Path Remap');
  f.layer.nodes.push(remap);
  connect(f.layer, f.effect.id, remap.id, 'path');
  enableRemapPoint(remap, initial, initial.contours[0].points[0].id);
  assert.equal(evaluate(f.layer).run(remap.id).remapSuspended, undefined);
  f.effect.params.maxRotation.value = -90;
  assert.equal(evaluate(f.layer).run(remap.id).remapSuspended, true);
  assert.notEqual(run(f).fingerprint, initial.fingerprint);
});

test('driven endpoints and locked controls reject transactionally; dense motifs retain output point cap', () => {
  const f = fixture(),
    driver = node('Value');
  driver.params.value.value = 1;
  f.layer.nodes.push(driver);
  connect(f.layer, driver.id, f.effect.id, 'maxScale');
  const session = new CommandSession(f.doc);
  const dispatch = (type, payload) =>
    session.dispatch({ ...session.state, type, payload: { layerId: f.layer.id, ...payload } });
  const before = serialize(session.snapshot());
  assert.throws(
    () => dispatch('set-parameter', { nodeId: driver.id, key: 'value', value: 101 }),
    /scale endpoints/,
  );
  assert.equal(serialize(session.snapshot()), before);
  dispatch('set-parameter-lock', { nodeId: f.effect.id, key: 'minRotation', locked: true });
  const locked = serialize(session.snapshot());
  assert.throws(
    () => dispatch('set-parameter', { nodeId: f.effect.id, key: 'minRotation', value: 30 }),
    /locked/,
  );
  assert.equal(serialize(session.snapshot()), locked);
  const dense = structuredClone(geometry);
  dense.contours[0].points = Array.from({ length: 401 }, (_, i) => ({
    ...structuredClone(geometry.contours[0].points[0]),
    id: `p${i}`,
  }));
  assert.throws(
    () =>
      luminanceInstancesGeometry(
        dense,
        Array.from({ length: 256 }, () => ({ x: 0, y: 0, darkness: 1 })),
        opts,
      ),
    /geometry budget/,
  );
});
