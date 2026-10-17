import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  bypassContract,
  connect,
  deserialize,
  document,
  evaluate,
  node,
  pathArea,
  serialize,
  setNodeBypassed,
  validate,
} from '../src/core.js';

function fixture({
  color = '#000000',
  pitch = 16,
  rawPath,
  strokeOnly = false,
  image = false,
} = {}) {
  const doc = document(),
    layer = doc.layers[0],
    source = node(image ? 'Image' : rawPath !== undefined ? 'Imported Path' : 'Rectangle'),
    fill = node(strokeOnly ? 'Stroke' : 'Fill'),
    effect = node('Line Screen'),
    out = node('Output');
  if (rawPath !== undefined) source.rawPath = rawPath;
  if (!image && rawPath === undefined) {
    source.params.center.value = [16, 8];
    source.params.size.value = [32, 16];
  }
  if (strokeOnly) {
    source.type = 'Curve';
    source.params = structuredClone(node('Curve').params);
    source.params.closed.value = false;
    source.points = structuredClone(node('Curve').points);
  }
  fill.params.path.source = source.id;
  if (!strokeOnly) fill.params.color.value = { hex: color, alpha: 1 };
  effect.params.path.source = fill.id;
  effect.params.pitch.value = pitch;
  out.params.path.source = effect.id;
  layer.nodes = [source, fill, effect, out];
  layer.output = out.id;
  return { doc, layer, source, fill, effect, out };
}

const run = (layer, id) => evaluate(layer).run(id);
const area = (path) => (path.contours.length ? pathArea(path) : 0);

test('Line Screen defaults to horizontal, returns opaque black geometry, and maps tone area to darkness', () => {
  const white = fixture({ color: '#ffffff' }),
    black = fixture(),
    gray = fixture({ color: '#bcbcbc' });
  assert.equal(white.effect.operation, 'horizontal');
  assert.equal(run(white.layer, white.effect.id).contours.length, 0);

  const dark = run(black.layer, black.effect.id);
  assert.equal(dark.fill.hex, '#000000');
  assert.equal(dark.fill.alpha, 1);
  assert.equal(dark.stroke, undefined);
  assert.ok(Math.abs(area(dark) - 32 * 16) < 0.001, 'black fills each sampled cell');

  const medium = run(gray.layer, gray.effect.id),
    expectedDarkness = 1 - ((188 / 255 + 0.055) / 1.055) ** 2.4;
  assert.ok(Math.abs(area(medium) - 32 * 16 * expectedDarkness) < 0.01);
  assert.ok(medium.unstable, 'derived geometry is marked unstable for downstream remaps');
  assert.deepEqual(run(gray.layer, gray.effect.id), medium, 'cold evaluation stays deterministic');
});

test('horizontal and vertical operations swap strip direction while preserving darkness area', () => {
  const horizontal = fixture({ color: '#bcbcbc' }),
    verticalDoc = structuredClone(horizontal.doc),
    verticalLayer = verticalDoc.layers[0],
    vertical = verticalLayer.nodes.find((item) => item.id === horizontal.effect.id);
  vertical.operation = 'vertical';

  const h = run(horizontal.layer, horizontal.effect.id),
    v = run(verticalLayer, vertical.id);
  assert.notDeepEqual(h.contours, v.contours, 'orientation changes the actual geometry');
  assert.ok(Math.abs(area(h) - area(v)) < 0.001);
});

test('Line Screen clips black marks to the compound source mask and keeps valid empty paths empty', () => {
  const donut = fixture({ rawPath: 'M0 0H64V64H0Z M16 16V48H48V16Z' });
  donut.effect.params.pitch.value = 16;
  const result = run(donut.layer, donut.effect.id);
  assert.ok(Math.abs(area(result) - (64 * 64 - 32 * 32)) < 0.001);
  assert.equal(result.contours.length, 2, 'the interior hole remains empty');

  const empty = fixture({ rawPath: '' }),
    emptyResult = run(empty.layer, empty.effect.id);
  assert.deepEqual(emptyResult.contours, []);
  assert.deepEqual(emptyResult.fill, { hex: '#000000', alpha: 1 });
});

test('Line Screen keeps the existing filled, ungrouped, closed vector source restrictions', () => {
  const strokeOnly = fixture({ strokeOnly: true });
  assert.throws(
    () => run(strokeOnly.layer, strokeOnly.effect.id),
    /stroke-only inputs are unsupported/,
  );

  const open = fixture();
  open.source.type = 'Curve';
  open.source.params = structuredClone(node('Curve').params);
  open.source.params.closed.value = false;
  open.source.points = [
    {
      id: 'line-a',
      position: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleA: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleB: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleType: 'corner',
    },
    {
      id: 'line-b',
      position: { type: 'Vector2', value: [32, 0], expression: '', locked: false },
      handleA: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleB: { type: 'Vector2', value: [0, 0], expression: '', locked: false },
      handleType: 'corner',
    },
  ];
  assert.throws(() => run(open.layer, open.effect.id), { code: 'LUMINANCE_MASK_OPEN_PATH' });

  const grouped = fixture(),
    join = node('Join Geometry');
  join.params.a.source = grouped.source.id;
  join.params.b.source = grouped.source.id;
  grouped.layer.nodes.splice(grouped.layer.nodes.indexOf(grouped.fill), 0, join);
  grouped.fill.params.path.source = join.id;
  assert.throws(() => run(grouped.layer, grouped.effect.id), {
    code: 'LUMINANCE_MASK_UNSUPPORTED',
  });
});

test('Line Screen operations and schema-15 serialization round-trip with bypass and shared history', () => {
  const f = fixture({ color: '#bcbcbc' });
  validate(f.doc);
  assert.equal(f.doc.schemaVersion, 15);
  assert.deepEqual(bypassContract(f.effect), {
    supported: true,
    input: 'path',
    type: 'PaintedPath',
    reason: 'Pass path through unchanged; keep all settings and connections.',
  });

  const session = new CommandSession(f.doc),
    layerId = f.layer.id,
    dispatch = (type, payload) =>
      session.dispatch({ ...session.state, type, payload: { layerId, ...payload } }),
    original = serialize(session.snapshot()),
    originalOutput = run(session.snapshot().layers[0], f.effect.id);
  assert.equal(deserialize(original).schemaVersion, 15);

  dispatch('set-operation', { nodeId: f.effect.id, operation: 'vertical' });
  const vertical = serialize(session.snapshot());
  assert.equal(
    session.snapshot().layers[0].nodes.find((item) => item.id === f.effect.id).operation,
    'vertical',
  );
  assert.notDeepEqual(
    run(session.snapshot().layers[0], f.effect.id).contours,
    originalOutput.contours,
  );
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), original);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), vertical);

  const bypassedLayer = session.snapshot().layers[0],
    beforeBypass = run(bypassedLayer, f.fill.id);
  setNodeBypassed(bypassedLayer, f.effect.id, true);
  assert.deepEqual(run(bypassedLayer, f.effect.id), beforeBypass);
});

test('Line Screen budget failure rolls back the upstream edit atomically', () => {
  const f = fixture({ pitch: 1 }),
    session = new CommandSession(f.doc),
    layerId = f.layer.id,
    stable = serialize(session.snapshot());
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId, nodeId: f.source.id, key: 'size', value: [65, 65] },
      }),
    /4096-cell limit/,
  );
  assert.equal(serialize(session.snapshot()), stable);
});
