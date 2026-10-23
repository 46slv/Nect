import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { sampleLuminanceGrid } from '../src/luminance-field.js';
import { validateTonePower } from '../src/luminance-tone.js';
import {
  document,
  node,
  evaluate,
  serialize,
  deserialize,
  CommandSession,
  exportSVG,
} from '../src/core.js';
const types = [
  'Halftone',
  'Ordered Dither',
  'Line Screen',
  'Error Diffusion Dither',
  'Luminance Instances',
];
const bounds = { minX: 0, minY: 0, maxX: 64, maxY: 32 };
function fixture(type) {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    effect = node(type),
    motif = node('Rectangle'),
    output = node('Output');
  shape.params.center.value = [32, 16];
  shape.params.size.value = [64, 32];
  motif.params.size.value = [10, 4];
  fill.params.path.source = shape.id;
  fill.params.color.value = { hex: '#000000', alpha: 0.5 };
  effect.params.path.source = fill.id;
  effect.params.pitch.value = 8;
  if (type === 'Luminance Instances') effect.params.motif.source = motif.id;
  output.params.path.source = effect.id;
  layer.nodes = [shape, fill, motif, effect, output];
  layer.output = output.id;
  return { doc, layer, effect };
}
test('power defaults to exact identity, acts after invert/alpha, preserves endpoints and validates bounds', () => {
  for (const invert of [false, true])
    for (const alpha of [0, 0.25, 0.5, 1]) {
      const fill = { hex: '#000000', alpha };
      const initial = sampleLuminanceGrid(fill, bounds, { invert });
      assert.deepEqual(initial, sampleLuminanceGrid(fill, bounds, { invert, tonePower: 1 }));
      for (const power of [0.1, 0.5, 2, 10]) {
        const transformed = sampleLuminanceGrid(fill, bounds, { invert, tonePower: power });
        transformed.cells.forEach((cell, i) => {
          assert.equal(cell.luminance, initial.cells[i].luminance);
          assert.equal(cell.darkness, Math.pow(initial.cells[i].darkness, power));
        });
      }
    }
  for (const value of [0, -1, 0.09, 10.01, NaN, Infinity, -Infinity, null, '2', true]) {
    assert.throws(() => validateTonePower(value), /tone power/);
    assert.throws(
      () => sampleLuminanceGrid({ hex: '#000000', alpha: 1 }, bounds, { tonePower: value }),
      /tone power/,
    );
  }
});
test('all five controls serialize, change output, undo/redo, lock and reject invalid direct/driven edits atomically', () => {
  for (const type of types) {
    const { doc, layer, effect } = fixture(type),
      driver = node('Value');
    driver.params.value.value = 11;
    layer.nodes.push(driver);
    const session = new CommandSession(doc),
      dispatch = (type, payload = {}) =>
        session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
    const run = () => evaluate(session.snapshot().layers[0]).run(effect.id),
      before = run(),
      stable = serialize(session.snapshot());
    for (const value of [0, 10.01, NaN, Infinity]) {
      assert.throws(() =>
        dispatch('set-parameter', { nodeId: effect.id, key: 'tonePower', value }),
      );
      assert.equal(serialize(session.snapshot()), stable);
    }
    assert.throws(
      () => dispatch('connect', { nodeId: effect.id, key: 'tonePower', sourceId: driver.id }),
      /tone power/,
    );
    assert.equal(serialize(session.snapshot()), stable);
    assert.throws(
      () =>
        dispatch('set-parameter-expression', {
          nodeId: effect.id,
          key: 'tonePower',
          expression: '11',
        }),
      /tone power/,
    );
    assert.equal(serialize(session.snapshot()), stable);
    dispatch('set-parameter', { nodeId: effect.id, key: 'tonePower', value: 2 });
    assert.doesNotThrow(() => exportSVG(session.snapshot()));
    const after = serialize(session.snapshot());
    assert.notDeepEqual(run().contours, before.contours, type);
    assert.notEqual(run().fingerprint, before.fingerprint);
    assert.deepEqual(evaluate(deserialize(after).layers[0]).run(effect.id), run());
    dispatch('undo');
    assert.deepEqual(run(), before);
    dispatch('redo');
    assert.equal(serialize(session.snapshot()), after);
    dispatch('set-parameter-lock', { nodeId: effect.id, key: 'tonePower', locked: true });
    const locked = serialize(session.snapshot());
    assert.throws(
      () => dispatch('set-parameter', { nodeId: effect.id, key: 'tonePower', value: 0.5 }),
      /locked/,
    );
    assert.equal(serialize(session.snapshot()), locked);
  }
});
test('pre-21 backfill is node-order independent and current schema rejects a missing control', () => {
  for (const reverse of [false, true]) {
    const { doc, layer } = fixture('Luminance Instances');
    layer.nodes.push(...types.slice(0, 4).map((type) => node(type)));
    doc.schemaVersion = 14;
    for (const n of layer.nodes)
      if (n.params.tonePower) {
        delete n.params.tonePower;
        if (n.type !== 'Luminance Instances') delete n.params.gridOrigin;
      }
    if (reverse) layer.nodes.reverse();
    const loaded = deserialize(serialize(doc));
    assert.equal(loaded.schemaVersion, 36);
    for (const n of loaded.layers[0].nodes.filter((n) => types.includes(n.type))) {
      assert.equal(n.params.tonePower.value, 1);
      assert.deepEqual(n.params.gridOrigin.value, [0, 0]);
    }
    delete loaded.layers[0].nodes.find((n) => types.includes(n.type)).params.tonePower;
    assert.throws(() => deserialize(serialize(loaded)));
  }
});
test('existing editable playgrounds retain pre-tone geometry and fingerprints', () => {
  const baseline = JSON.parse(
    fs.readFileSync(
      new URL('./fixtures/luminance-tone-default-fingerprints.json', import.meta.url),
      'utf8',
    ),
  );
  for (const [name, expected] of Object.entries(baseline)) {
    const doc = deserialize(
      fs.readFileSync(new URL(`../examples/${name}.json`, import.meta.url), 'utf8'),
    );
    const actual = doc.layers
      .filter((l) => l.nodes.some((n) => n.params.gridOrigin))
      .map((l) => evaluate(l, {}, doc.imageAssets).path);
    assert.deepEqual(
      actual.map((path) => path.fingerprint),
      expected,
    );
  }
});
