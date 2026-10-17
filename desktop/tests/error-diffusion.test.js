import test from 'node:test';
import assert from 'node:assert/strict';
import { generateLuminanceMarks } from '../src/luminance-marks.js';
import {
  CommandSession,
  document,
  node,
  connect,
  deserialize,
  serialize,
  evaluate,
  validate,
} from '../src/core.js';
const cell = (i, j, darkness) => ({ i, j, x: i * 8 + 4, y: j * 8 + 4, darkness });
const run = (cells) => generateLuminanceMarks({ cells, pitch: 8, mode: 'error-diffusion' }).marks;
const ids = (cells) => run(cells).map(({ i, j }) => [i, j]);

test('Floyd–Steinberg uses each forward weight and signed residual with no edge renormalization', () => {
  for (const [i, j, weight] of [
    [1, 0, 7],
    [-1, 1, 3],
    [0, 1, 5],
    [1, 1, 1],
  ]) {
    const threshold = 0.5 - (0.4 * weight) / 16;
    assert.deepEqual(ids([cell(0, 0, 0.4), cell(i, j, threshold + 0.001)]), [[i, j]]);
    assert.deepEqual(ids([cell(0, 0, 0.4), cell(i, j, threshold - 0.001)]), []);
    assert.deepEqual(ids([cell(0, 0, 0.6), cell(i, j, 0.5 + (0.4 * weight) / 16 - 0.001)]), [
      [0, 0],
    ]);
  }
  assert.deepEqual(ids([cell(0, 0, 0.49), cell(2, 0, 0.4)]), [], 'no jump across missing cells');
  assert.deepEqual(ids([cell(0, 0, 0.49), cell(0, 2, 0.4)]), [], 'no carry across missing rows');
});

test('fixed raster order, tie, negative indices, immutable cells and square geometry', () => {
  const cells = Array.from({ length: 16 }, (_, k) => cell((k % 4) - 2, Math.floor(k / 4) - 2, 0.5));
  const before = structuredClone(cells),
    expected = [
      [-2, -2],
      [0, -2],
      [-1, -1],
      [1, -1],
      [-2, 0],
      [0, 0],
      [-1, 1],
      [1, 1],
    ];
  assert.deepEqual(ids(cells), expected);
  assert.deepEqual(ids(cells.slice().reverse()), expected);
  assert.deepEqual(cells, before);
  assert.deepEqual(run([cell(0, 0, 0.5)])[0].vertices, [
    [0, 0],
    [8, 0],
    [8, 8],
    [0, 8],
  ]);
  assert.deepEqual(ids([cell(0, 0, 0)]), []);
  assert.deepEqual(ids([cell(0, 0, 1)]), [[0, 0]]);
  assert.deepEqual(run([]), []);
});

test('diffusion input guards preserve finite bounds and unchanged cell cap', () => {
  assert.throws(() => run([cell(0, 0, 0.4), cell(0, 0, 0.5)]), /unique/);
  assert.throws(() => run([cell(Number.MAX_SAFE_INTEGER + 1, 0, 0.4)]), /safe integers/);
  assert.throws(() => run([cell(0, 0, NaN)]), /darkness/);
  assert.throws(() => run(Array.from({ length: 4097 }, (_, i) => cell(i, 0, 0))), /4096/);
  assert.equal(run(Array.from({ length: 4096 }, (_, i) => cell(i, 0, 1))).length, 4096);
});

test('new type alone promotes schema 19 and keeps exact cold reopen output', () => {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Linear Gradient Fill'),
    effect = node('Error Diffusion Dither'),
    out = node('Output');
  shape.params.center.value = [32, 16];
  shape.params.size.value = [64, 32];
  fill.params.start.value = [0, 0];
  fill.params.end.value = [64, 0];
  layer.nodes = [shape, fill, effect, out];
  layer.output = out.id;
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, effect.id, 'path');
  connect(layer, effect.id, out.id, 'path');
  doc.schemaVersion = 18;
  const before = evaluate(layer).path,
    restored = deserialize(serialize(doc));
  assert.equal(restored.schemaVersion, 19);
  assert.deepEqual(evaluate(restored.layers[0]).path, before);
  assert.equal(serialize(deserialize(serialize(restored))), serialize(restored));
  const old = document();
  old.schemaVersion = 18;
  assert.equal(validate(old).schemaVersion, 18);
  restored.layers[0].nodes.find((n) => n.id === effect.id).params.pitch.value = 0;
  assert.throws(() => validate(restored), /pitch/i);
});

test('error diffusion budget failures reject the entire source transaction', () => {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    effect = node('Error Diffusion Dither'),
    out = node('Output');
  shape.params.center.value = [8, 8];
  shape.params.size.value = [16, 16];
  layer.nodes = [shape, fill, effect, out];
  layer.output = out.id;
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, effect.id, 'path');
  connect(layer, effect.id, out.id, 'path');
  const session = new CommandSession(doc),
    before = serialize(session.snapshot());
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: { layerId: layer.id, nodeId: shape.id, key: 'size', value: [2048, 2048] },
      }),
    /4096/,
  );
  assert.equal(serialize(session.snapshot()), before);
});
