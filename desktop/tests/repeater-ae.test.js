import test from 'node:test';
import assert from 'node:assert/strict';
import {
  repeatGeometry,
  repeaterSettings,
  REPEATER_KEYS,
  assertRepeaterRoute,
} from '../src/repeater.js';
import {
  document,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
  renderElement,
  CommandSession,
} from '../src/core.js';

const options = (extra = {}) => ({
  count: 3,
  offset: 0,
  anchor: [0, 0],
  position: [10, 0],
  scale: [100, 100],
  rotation: 0,
  startOpacity: 100,
  endOpacity: 100,
  copiesAbove: true,
  prefix: 'r',
  ...extra,
});
const source = () => ({
  contours: [
    {
      id: 'c',
      closed: true,
      points: [{ id: 'p', position: [2, 3], handleA: [4, -1], handleB: [-2, 5] }],
    },
  ],
});
const near = (actual, expected) =>
  actual.forEach((v, i) => assert.ok(Math.abs(v - expected[i]) < 1e-8, `${actual} != ${expected}`));
function scene() {
  const doc = document();
  doc.schemaVersion = 37;
  const l = doc.layers[0],
    shape = node('Rectangle'),
    repeat = node('Geometry Repeater'),
    fill = node('Fill'),
    stroke = node('Stroke'),
    out = node('Output');
  shape.params.center.value = [40, 40];
  shape.params.size.value = [40, 40];
  repeat.operation = 'ae-2d';
  repeat.params.count.value = 3;
  repeat.params.position.value = [10, 0];
  l.nodes = [shape, repeat, fill, stroke, out];
  l.output = out.id;
  for (const [a, b] of [
    [shape, repeat],
    [repeat, fill],
    [fill, stroke],
    [stroke, out],
  ])
    connect(l, a.id, b.id, 'path');
  return { doc, l, shape, repeat, fill, stroke, out };
}
test('integer affine powers accumulate nonuniform scale, rotation and translation; handles omit translation', () => {
  const input = source(),
    before = structuredClone(input);
  const result = repeatGeometry(input, options({ scale: [200, 100], rotation: 90 }));
  near(result.contours[1].points[0].position, [7, 4]);
  near(result.contours[2].points[0].position, [6, 14]);
  near(result.contours[2].points[0].handleA, [-8, 2]);
  assert.deepEqual(input, before);
  const anchored = repeatGeometry(input, options({ anchor: [2, 3], rotation: 90 }));
  near(anchored.contours[1].points[0].position, [12, 3]);
});
test('negative offset is inverse, reflected and zero scale are bounded, fractional settings reject', () => {
  const r = repeatGeometry(source(), options({ offset: -1, scale: [-200, 100], rotation: 90 }));
  near(r.contours[1].points[0].position, [2, 3]);
  near(r.contours[2].points[0].position, [7, -4]);
  const collapse = repeatGeometry(source(), options({ scale: [0, 100] }));
  near(collapse.contours[1].points[0].position, [10, 3]);
  assert.throws(
    () => repeatGeometry(source(), options({ offset: -1, scale: [0, 100] })),
    /nonzero/,
  );
  for (const extra of [
    { count: 2.5 },
    { offset: -0.5 },
    { count: 257 },
    { offset: 257 },
    { scale: [1001, 100] },
    { startOpacity: -1 },
    { endOpacity: 101 },
    { rotation: Infinity },
  ])
    assert.throws(() => repeaterSettings((k) => options(extra)[k]), /Repeater/);
  assert.throws(
    () => repeatGeometry(source(), options({ count: 256, scale: [1000, 1000] })),
    /budget/,
  );
});
test('copy groups retain holes, IDs and opacity endpoints while reversing whole-copy order', () => {
  const input = source();
  input.contours.push({ ...structuredClone(input.contours[0]), id: 'hole' });
  const r = repeatGeometry(
    input,
    options({ startOpacity: 20, endOpacity: 80, copiesAbove: false }),
  );
  assert.deepEqual(
    r.contours.map((c) => c.repeaterOpacity),
    [0.8, 0.8, 0.5, 0.5, 0.2, 0.2],
  );
  assert.equal(r.contours[0].group, r.contours[1].group);
  assert.notEqual(r.contours[1].group, r.contours[2].group);
  assert.equal(new Set(r.contours.map((c) => c.points[0].id)).size, 6);
  assert.match(r.contours[0].id, /copy2/);
  assert.deepEqual(repeatGeometry(input, options({ count: 0 })), { contours: [] });
  assert.equal(
    repeatGeometry(input, options({ count: 1, startOpacity: 20, endOpacity: 80 })).contours[0]
      .repeaterOpacity,
    0.2,
  );
  assert.throws(() => repeatGeometry(r, options()), /Nested/);
  assert.throws(
    () =>
      assertRepeaterRoute(
        { kind: 'painted-group', contours: [], children: [r] },
        { type: 'Convert to Path' },
      ),
    /put geometry operations before/,
  );
  assert.throws(
    () =>
      repeatGeometry(
        { contours: Array.from({ length: 129 }, (_, i) => ({ ...input.contours[0], group: i })) },
        options({ count: 2 }),
      ),
    /budget/,
  );
});
test('shared Session edits are undoable, restore via JSON and refuse invalid edits atomically', () => {
  const { doc, l, repeat } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: repeat.id, ...payload },
    });
  send('set-parameter', { key: 'offset', value: -2 });
  const changed = serialize(session.snapshot());
  assert.deepEqual(
    evaluate(deserialize(changed).layers[0]).path,
    evaluate(session.snapshot().layers[0]).path,
  );
  send('undo');
  assert.equal(session.snapshot().layers[0].nodes[1].params.offset.value, 0);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
  for (const [key, value] of [
    ['count', 2.5],
    ['offset', 0.5],
    ['endOpacity', 101],
    ['scale', [0, 100]],
  ]) {
    assert.throws(() => send('set-parameter', { key, value }));
    assert.equal(serialize(session.snapshot()), changed);
  }
});
test('legacy modes preserve geometry and defaults on migration, new mode requires schema 37', () => {
  const { doc, l, repeat } = scene();
  repeat.operation = 'linear';
  doc.schemaVersion = 35;
  const before = evaluate(l).path.contours;
  for (const key of REPEATER_KEYS) delete repeat.params[key];
  const restored = deserialize(JSON.stringify(doc));
  assert.equal(restored.schemaVersion, 35);
  assert.deepEqual(evaluate(restored.layers[0]).path.contours, before);
  assert.equal(restored.layers[0].nodes[1].operation, 'linear');
  repeat.operation = 'ae-2d';
  assert.throws(() => deserialize(JSON.stringify(doc)), /schemaVersion 37/);
});

test('schema-36 Dither survives Repeater migration, promotion, Undo and JSON reopen', () => {
  const { doc, l, repeat } = scene();
  doc.schemaVersion = 36;
  repeat.operation = 'linear';
  const dither = node('Ordered Dither');
  dither.operation = 'star';
  dither.params.dotSize.value = 19;
  l.nodes.push(dither);
  const ditherBefore = structuredClone(dither);
  const legacyGeometry = evaluate(l).path;
  for (const key of REPEATER_KEYS) delete repeat.params[key];
  const restored = deserialize(serialize(doc));
  assert.equal(restored.schemaVersion, 36, 'backfill does not promote a Dither document');
  assert.deepEqual(restored.layers[0].nodes.at(-1), ditherBefore);
  assert.deepEqual(evaluate(restored.layers[0]).path, legacyGeometry);
  const session = new CommandSession(restored);
  const before = serialize(session.snapshot());
  const send = (type, payload) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: repeat.id, ...payload },
    });
  send('set-operation', { operation: 'ae-2d' });
  const promoted = session.snapshot();
  assert.equal(promoted.schemaVersion, 37);
  assert.deepEqual(promoted.layers[0].nodes.at(-1), ditherBefore);
  const saved = serialize(promoted);
  assert.equal(serialize(deserialize(saved)), saved);
  const missing = structuredClone(promoted);
  delete missing.layers[0].nodes[1].params.offset;
  assert.throws(
    () => deserialize(serialize(missing)),
    /parameter|params/i,
    'schema37 requires the new authored controls',
  );
  const wrongVersion = structuredClone(promoted);
  wrongVersion.schemaVersion = 36;
  assert.throws(() => deserialize(serialize(wrongVersion)), /schemaVersion 37/);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), saved);
});
test('driven Copies/Offset enforce the same integer limits and parameter locks reject edits', () => {
  const { doc, l, repeat } = scene(),
    session = new CommandSession(doc);
  const send = (type, payload) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: repeat.id, ...payload },
    });
  const original = session.snapshot();
  assert.throws(
    () => send('set-parameter-expression', { key: 'offset', expression: '1 / 2' }),
    /integer/,
  );
  assert.deepEqual(session.snapshot(), original);
  send('set-parameter-lock', { key: 'position', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'position', value: [20, 0] }), /lock/i);
  assert.deepEqual(session.snapshot(), locked);
  const value = node('Value');
  value.params.value.value = 1.5;
  l.nodes.push(value);
  connect(l, value.id, repeat.id, 'count');
  assert.match(evaluate(l).error, /integer/);
});
test('renderer preserves per-copy opacity once for fill plus stroke and rejects lossy downstream geometry', () => {
  const { doc, l, repeat, fill, stroke, out } = scene();
  repeat.params.startOpacity.value = 20;
  repeat.params.endOpacity.value = 80;
  fill.params.color.value = { hex: '#ff0000', alpha: 0.5 };
  stroke.params.color.value = { hex: '#0000ff', alpha: 0.4 };
  const value = evaluate(l).path;
  const svg = exportSVG(doc);
  assert.match(svg, /<g opacity="0.2"><path/);
  assert.match(svg, /<g opacity="0.5"><path/);
  assert.match(svg, /<g opacity="0.8"><path/);
  assert.match(svg, /fill-opacity="0.5"/);
  assert.match(svg, /stroke-opacity="0.4"/);
  assert.ok(svg.includes(renderElement(value, l, doc.imageAssets, '', 'nect-gradient-0')));
  const transform = node('Transform');
  l.nodes.push(transform);
  connect(l, repeat.id, transform.id, 'path');
  connect(l, transform.id, fill.id, 'path');
  assert.deepEqual(
    evaluate(l).path.contours.map((c) => c.repeaterOpacity),
    [0.2, 0.5, 0.8],
  );
  const mirror = node('Mirror');
  l.nodes.push(mirror);
  connect(l, repeat.id, mirror.id, 'path');
  connect(l, mirror.id, fill.id, 'path');
  assert.match(evaluate(l).error, /put geometry operations before/);
  mirror.bypassed = true;
  assert.equal(evaluate(l).error, null);
  repeat.params.count.value = 0;
  assert.equal(evaluate(l).path.contours.length, 0);
  assert.doesNotThrow(() => exportSVG(doc));
});
