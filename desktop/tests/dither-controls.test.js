import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { generateLuminanceMarks } from '../src/luminance-marks.js';
import { ditherSettings } from '../src/dither-controls.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';
import {
  document,
  node,
  CommandSession,
  evaluate,
  exportSVG,
  exportScene,
  serialize,
  deserialize,
  pathArea,
  validate,
  remapFingerprintMatches,
} from '../src/core.js';

const types = ['Ordered Dither', 'Error Diffusion Dither'];
const modes = ['dither', 'error-diffusion'];
const cells = Array.from({ length: 64 }, (_, i) => ({
  i: i % 8,
  j: Math.floor(i / 8),
  x: (i % 8) * 16 + 8,
  y: Math.floor(i / 8) * 16 + 8,
  darkness: 0.5,
}));
function fixture(type) {
  const doc = document(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    effect = node(type),
    out = node('Output');
  shape.params.center.value = [32, 32];
  shape.params.size.value = [64, 64];
  fill.params.path.source = shape.id;
  fill.params.color.value = { hex: '#000000', alpha: 1 };
  effect.params.path.source = fill.id;
  effect.params.dotSize.value = 8;
  out.params.path.source = effect.id;
  layer.nodes = [shape, fill, effect, out];
  layer.output = out.id;
  return { doc, layer, shape, fill, effect };
}
const area = (path) => (path.contours.length ? pathArea(path) : 0);
const ids = (marks) => marks.map(({ i, j }) => [i, j]);

test('shape and fixed size preserve selected cells in both existing algorithms', () => {
  for (const mode of modes) {
    const square = generateLuminanceMarks({ cells, pitch: 16, mode, dotSize: 6 }).marks;
    const circle = generateLuminanceMarks({
      cells,
      pitch: 16,
      mode,
      dotSize: 10,
      shape: 'circle',
    }).marks;
    assert.deepEqual(ids(circle), ids(square));
    assert.equal(square[0].vertices[1][0] - square[0].vertices[0][0], 6);
    for (const mark of circle) {
      const cell = cells.find((cell) => cell.i === mark.i && cell.j === mark.j);
      for (const [x, y] of mark.vertices)
        assert.ok(Math.abs(Math.hypot(x - cell.x, y - cell.y) - 5) < 1e-10);
    }
    assert.deepEqual(
      generateLuminanceMarks({ cells, pitch: 16, mode }),
      generateLuminanceMarks({ cells, pitch: 16, mode, shape: 'square', dotSize: 16 }),
    );
  }
});

test('spacing changes density independently; overlap is unioned and clipped at the source', () => {
  for (const type of types) {
    const { layer, effect } = fixture(type);
    const run = () => evaluate(layer).run(effect.id);
    const sparse = run();
    assert.equal(sparse.contours.length, 16);
    assert.ok(Math.abs(area(sparse) - 16 * 64) < 0.001);
    effect.params.pitch.value = 32;
    assert.equal(run().contours.length, 4);
    assert.ok(Math.abs(area(run()) - 4 * 64) < 0.001);
    assert.equal(effect.params.dotSize.value, 8);
    effect.params.dotSize.value = 48;
    assert.ok(Math.abs(area(run()) - 4096) < 0.001);
    effect.operation = 'circle';
    effect.params.dotSize.value = 8;
    assert.ok(Math.abs(area(run()) - 4 * Math.PI * 16) < 3);
    assert.equal(effect.params.pitch.value, 32);
  }
});

test('all settings persist, undo individually, lock size, and share preview/export geometry', () => {
  for (const type of types) {
    const { doc, layer, effect } = fixture(type),
      session = new CommandSession(doc);
    const dispatch = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
    const run = () => evaluate(session.snapshot().layers[0]).run(effect.id);
    const initial = serialize(session.snapshot());
    dispatch('set-operation', { nodeId: effect.id, operation: 'circle' });
    const circle = serialize(session.snapshot());
    dispatch('set-parameter', { nodeId: effect.id, key: 'dotSize', value: 6 });
    const smaller = serialize(session.snapshot());
    dispatch('set-parameter', { nodeId: effect.id, key: 'pitch', value: 24 });
    const final = serialize(session.snapshot()),
      reopened = deserialize(final);
    assert.equal(reopened.schemaVersion, 36);
    assert.deepEqual(exportScene(reopened)[0].value, run());
    assert.equal(exportSVG(reopened), exportSVG(session.snapshot()));
    assert.equal(serialize(deserialize(final)), final);
    for (const previous of [smaller, circle, initial]) {
      dispatch('undo');
      assert.equal(serialize(session.snapshot()), previous);
    }
    for (let i = 0; i < 3; i++) dispatch('redo');
    assert.equal(serialize(session.snapshot()), final);
    dispatch('set-parameter-lock', { nodeId: effect.id, key: 'dotSize', locked: true });
    assert.throws(
      () => dispatch('set-parameter', { nodeId: effect.id, key: 'dotSize', value: 9 }),
      /locked/,
    );
  }
});

test('invalid direct, socket, expression, shape and geometry-budget edits reject atomically, even on empty paint', () => {
  for (const type of types) {
    const { doc, layer, effect, fill } = fixture(type);
    fill.params.color.value.alpha = 0;
    const driver = node('Value');
    driver.params.value.value = 2048;
    layer.nodes.push(driver);
    const session = new CommandSession(doc);
    const dispatch = (type, payload) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
    const before = serialize(session.snapshot());
    for (const value of [0, 0.5, 1025, Infinity, NaN])
      assert.throws(() => dispatch('set-parameter', { nodeId: effect.id, key: 'dotSize', value }));
    assert.throws(() => dispatch('set-operation', { nodeId: effect.id, operation: 'triangle' }));
    assert.throws(
      () => dispatch('connect', { nodeId: effect.id, key: 'dotSize', sourceId: driver.id }),
      /dot size/,
    );
    assert.throws(
      () =>
        dispatch('set-parameter-expression', {
          nodeId: effect.id,
          key: 'dotSize',
          expression: '2048',
        }),
      /dot size/,
    );
    assert.equal(serialize(session.snapshot()), before);
  }
  for (const mode of modes) {
    const full = Array.from({ length: 4096 }, (_, i) => ({
      i: i % 64,
      j: Math.floor(i / 64),
      x: i % 64,
      y: Math.floor(i / 64),
      darkness: 1,
    }));
    assert.throws(
      () => generateLuminanceMarks({ cells: full, pitch: 1, mode, shape: 'circle', dotSize: 1024 }),
      /100000/,
    );
  }
  assert.throws(() => ditherSettings('circle', null), /dot size/);
});

test('legacy files retain exact historical fingerprints and newer saved-edit identities; migrated pitch drivers stay valid; schema36 requires size', () => {
  // Both independently captured golden files remain unchanged. The compatibility
  // evaluator exposes the pre-expression canonical hash and privately retains the
  // exact empty-expression-field-era identity used by the dither-controls baseline.
  const baseline = JSON.parse(
      fs.readFileSync(
        new URL('./fixtures/dither-legacy-fingerprints.json', import.meta.url),
        'utf8',
      ),
    ),
    historical = JSON.parse(
      fs.readFileSync(
        new URL('./fixtures/luminance-tone-default-fingerprints.json', import.meta.url),
        'utf8',
      ),
    );
  assert.deepEqual(Object.keys(baseline), Object.keys(historical));
  for (const [name, expected] of Object.entries(baseline)) {
    const doc = deserialize(
        fs.readFileSync(new URL(`../examples/${name}.json`, import.meta.url), 'utf8'),
      ),
      before = serialize(doc),
      layers = doc.layers.filter((l) => l.nodes.some((n) => n.params.gridOrigin)),
      actual = layers.map((l) => evaluate(l, {}, doc.imageAssets).path);
    assert.deepEqual(
      actual.map((path) => path.fingerprint),
      historical[name],
      name,
    );
    assert.equal(actual.length, expected.length, name);
    for (let i = 0; i < actual.length; i++) {
      const path = actual[i],
        cached = evaluate(layers[i], {}, doc.imageAssets);
      assert.equal(cached.error, null, name);
      assert.ok(cached.stats.reused > 0, name);
      assert.deepEqual(cached.path, path, name);
      assert.equal(remapFingerprintMatches(path, expected[i]), true, name);
      assert.equal(remapFingerprintMatches(cached.path, expected[i]), true, name);
      assert.equal(remapFingerprintMatches(path, 'sha256:unrelated'), false, name);
      assert.equal(remapFingerprintMatches(structuredClone(path), expected[i]), false, name);
      const changed = structuredClone(layers[i]);
      changed.nodes.find((n) => n.params.gridOrigin).params.pitch.value += 1;
      const changedResult = evaluate(changed, {}, doc.imageAssets);
      assert.equal(changedResult.error, null, name);
      for (const retained of [historical[name][i], expected[i]])
        assert.equal(remapFingerprintMatches(changedResult.path, retained), false, name);
    }
    const reopened = deserialize(before),
      reopenedPaths = reopened.layers
        .filter((l) => l.nodes.some((n) => n.params.gridOrigin))
        .map((l) => evaluate(l, {}, reopened.imageAssets).path);
    assert.deepEqual(reopenedPaths, actual, name);
    reopenedPaths.forEach((path, i) =>
      assert.equal(remapFingerprintMatches(path, expected[i]), true, name),
    );
    assert.equal(serialize(reopened), before, name);
    assert.equal(serialize(doc), before, name);
  }
  for (const type of types) {
    const { doc, layer, effect } = fixture(type);
    doc.schemaVersion = 35;
    effect.params.pitch.value = 12;
    effect.params.pitch.expression = 'value * 2';
    doc.schemaVersion = 35;
    effect.operation = 'subtract';
    delete effect.params.dotSize;
    const migrated = deserialize(serialize(doc)),
      n = migrated.layers[0].nodes.find((n) => n.id === effect.id);
    assert.equal(n.operation, 'square');
    assert.deepEqual(n.params.dotSize, n.params.pitch);
    assert.ok(!evaluate(migrated.layers[0]).error);
    assert.notEqual(n.params.dotSize, n.params.pitch);
    delete n.params.dotSize;
    assert.throws(() => deserialize(serialize(migrated)));
  }
  const unrelated = document();
  assert.equal(validate(unrelated).schemaVersion, unrelated.schemaVersion);
});

test('source alpha is preserved and still composites over white before either algorithm', () => {
  for (const type of types)
    for (const shape of ['square', 'circle']) {
      const { doc, layer, effect, fill } = fixture(type);
      effect.operation = shape;
      const run = () => evaluate(layer).run(effect.id);
      const opaque = area(run());
      fill.params.color.value.alpha = 0.5;
      const before = serialize(doc),
        half = area(run());
      assert.ok(half > 0 && half < opaque);
      assert.equal(serialize(doc), before);
      fill.params.color.value.alpha = 0;
      assert.equal(area(run()), 0);
      assert.equal(fill.params.color.value.alpha, 0);
      effect.params.invert.value = true;
      assert.equal(area(run()), opaque);
    }
});

test('regular polygons and concave four-point stars rotate and retain size modulation beyond a cell', () => {
  const cell = { i: 0, j: 0, x: 8, y: 8, darkness: 1 };
  const generate = (shape, darkness = 1, rotation = 0) =>
    generateLuminanceMarks({
      cells: [{ ...cell, darkness }],
      pitch: 16,
      mode: 'halftone',
      shape,
      dotSize: 24,
      sides: 4,
      rotation,
      innerRatio: 0.18,
    }).marks[0].vertices;
  assert.equal(generate('polygon').length, 4);
  const star = generate('star');
  assert.equal(star.length, 8);
  for (let i = 0; i < star.length; i++)
    assert.ok(
      Math.abs(Math.hypot(star[i][0] - 8, star[i][1] - 8) - (i % 2 ? 12 * 0.18 : 12)) < 1e-9,
    );
  const small = generate('star', 0.25);
  assert.ok(Math.abs(Math.hypot(small[0][0] - 8, small[0][1] - 8) - 6) < 1e-9);
  const diamond = generate('square', 1, 45);
  assert.ok(
    diamond.some(([x]) => x > 16),
    'marks extend past cell edges',
  );
  const { doc, layer, effect } = fixture('Halftone');
  effect.operation = 'square';
  effect.params.dotRotation.value = 45;
  effect.params.dotSize.value = 20;
  const full = evaluate(layer).run(effect.id);
  // A square side between pitch/sqrt(2) and pitch*sqrt(2) joins neighbors
  // while leaving white diamond holes, as in the second reference.
  assert.ok(area(full) > 2048 && area(full) < 4096);
  assert.ok(full.contours.length > 1, 'connected black silhouette retains white holes');
  const session = new CommandSession(doc);
  const dispatch = (type, payload) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  dispatch('set-operation', { nodeId: effect.id, operation: 'star' });
  for (const [key, value] of [
    ['dotSides', 4],
    ['starInnerRatio', 0.18],
    ['dotRotation', 30],
  ])
    dispatch('set-parameter', { nodeId: effect.id, key, value });
  const saved = serialize(session.snapshot());
  assert.equal(exportSVG(deserialize(saved)), exportSVG(session.snapshot()));
  for (const [key, value] of [
    ['dotSides', 3.5],
    ['dotSides', 33],
    ['starInnerRatio', 0],
    ['dotRotation', 361],
  ])
    assert.throws(() => dispatch('set-parameter', { nodeId: effect.id, key, value }));
  assert.equal(serialize(session.snapshot()), saved);
});

test('PNG alpha still drives tone for every custom shape without changing the embedded image', () => {
  for (const type of [...types, 'Halftone'])
    for (const shape of ['square', 'circle', 'polygon', 'star']) {
      const { doc, layer, effect } = fixture(type),
        image = node('Image');
      image.asset = 'image-synthetic';
      image.params.position.value = [0, 0];
      image.params.size.value = [64, 64];
      effect.params.path.source = image.id;
      effect.operation = shape;
      layer.nodes.unshift(image);
      const run = () => evaluate(layer, {}, doc.imageAssets).run(effect.id);
      const measure = (alpha) => {
        doc.imageAssets = {
          'image-synthetic': {
            name: 'synthetic.png',
            mime: 'image/png',
            width: 1,
            height: 1,
            data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([0, 0, 0, alpha]))),
          },
        };
        const before = serialize(doc),
          result = area(run());
        assert.equal(serialize(doc), before);
        return result;
      };
      const opaque = measure(255),
        half = measure(128),
        transparent = measure(0);
      assert.ok(half > 0 && half < opaque);
      assert.equal(transparent, 0);
      effect.params.invert.value = true;
      assert.ok(Math.abs(area(run()) - opaque) < 0.001);
    }
});
