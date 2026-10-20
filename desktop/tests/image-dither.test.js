import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  outputType,
  socketCompatibleFor,
  evaluate,
  serialize,
  deserialize,
  validate,
  exportSVG,
  CommandSession,
} from '../src/core.js';
import { imageDitherTypes, IMAGE_DITHER_SCHEMA_VERSION } from '../src/dither-controls.js';
import { encodeImage, decodeImage, MAX_IMAGE_BYTES } from '../src/images.js';
import { encodeRgbaPng, imageAssetFor, imageEffectCacheInfo } from '../src/image-keyer.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { renderImageDither, MAX_IMAGE_DITHER_EDGE_SAMPLES } from '../src/image-dither.js';
import { viewerMarkup } from '../src/node-viewers.js';
import { readFile } from 'node:fs/promises';

function asset(width, height, pixel = () => [0, 0, 0, 255]) {
  const rgba = new Uint8Array(width * height * 4);
  for (let y = 0; y < height; y++)
    for (let x = 0; x < width; x++) rgba.set(pixel(x, y), (y * width + x) * 4);
  return {
    name: 'synthetic.png',
    mime: 'image/png',
    width,
    height,
    data: encodeImage(encodeRgbaPng(width, height, rgba)),
  };
}
function fixture(type = imageDitherTypes[0], width = 8, height = 8, pixel) {
  const doc = document(),
    comp = doc.layers[0],
    image = node('Image'),
    effect = node(type),
    out = node('Image Output');
  doc.schemaVersion = IMAGE_DITHER_SCHEMA_VERSION;
  doc.width = width;
  doc.height = height;
  doc.imageAssets = { 'image-source': asset(width, height, pixel) };
  image.asset = 'image-source';
  image.params.position.value = [0, 0];
  image.params.size.value = [width, height];
  effect.params.pitch.value = 1;
  effect.params.dotSize.value = 1;
  comp.nodes = [image, effect, out];
  comp.output = out.id;
  connect(comp, image.id, effect.id, 'path');
  connect(comp, effect.id, out.id, 'path');
  return { doc, comp, image, effect, out };
}
function resolved(f) {
  const result = evaluate(f.comp, {}, f.doc.imageAssets, f.doc);
  assert.equal(result.error, null);
  const png = imageAssetFor(result.path, f.doc.imageAssets);
  return { value: result.path, png, rgba: copyImageRgba(png) };
}
const alpha = (rgba) => [...rgba].filter((_, index) => index % 4 === 3);

test('both fixed Image types connect to Image Output and deliver actual deterministic pixels', () => {
  for (const type of imageDitherTypes) {
    const f = fixture(type, 8, 8, (x, y) => ((x + y) % 2 ? [255, 255, 255, 255] : [0, 0, 0, 255]));
    const before = serialize(f.doc),
      result = resolved(f);
    assert.equal(outputType(f.effect), 'Image');
    assert.equal(socketCompatibleFor(f.effect, f.out, 'path'), true);
    assert.equal(result.value.kind, 'image');
    assert.equal(result.value.data, undefined);
    assert.equal(result.value.asset, 'image-source');
    assert.equal(result.value.imageEffects.length, 1);
    assert.deepEqual(
      alpha(result.rgba),
      Array.from({ length: 64 }, (_, i) => (((i % 8) + Math.floor(i / 8)) % 2 ? 0 : 255)),
    );
    assert.ok([...result.rgba].every((v, i) => i % 4 === 3 || v === 0));
    assert.notEqual(result.png.data, f.doc.imageAssets['image-source'].data);
    assert.match(exportSVG(f.doc), /<image .*href="data:image\/png;base64,/);
    assert.match(viewerMarkup(result.value, f.doc.imageAssets, 0), /<image /);
    assert.equal(serialize(f.doc), before);
    assert.equal(
      resolved({ ...f, doc: deserialize(before), comp: deserialize(before).layers[0] }).png.data,
      result.png.data,
    );
  }
});

test('Image routing leaves both Shape output contracts and vector consumers intact', () => {
  for (const type of imageDitherTypes) {
    const f = fixture(type),
      shape = node(type.replace(/ Image$/, '')),
      vectorOut = node('Output');
    f.comp.nodes.push(shape, vectorOut);
    connect(f.comp, f.image.id, shape.id, 'path');
    assert.equal(outputType(shape), 'PaintedPath');
    assert.throws(() => connect(f.comp, shape.id, f.out.id, 'path'), /PaintedPath to Image/);
    assert.throws(() => connect(f.comp, f.effect.id, vectorOut.id, 'path'), /Image to PaintedPath/);
    assert.throws(() => connect(f.comp, shape.id, f.effect.id, 'path'), /PaintedPath to Image/);
    connect(f.comp, shape.id, vectorOut.id, 'path');
    assert.ok(evaluate(f.comp, {}, f.doc.imageAssets, f.doc).run(shape.id).contours);
  }
});

test('linear-light midgray retains the distinct Bayer and Floyd-Steinberg first scanline', () => {
  for (const [index, type] of imageDitherTypes.entries()) {
    const f = fixture(type, 8, 8, () => [188, 188, 188, 255]);
    assert.deepEqual(
      alpha(resolved(f).rgba).slice(0, 8),
      Array.from({ length: 8 }, (_, x) => (x % 2 === index ? 255 : 0)),
    );
  }
});

test('pixel dimensions are independent of placement and source alpha is sampling only', () => {
  for (const type of imageDitherTypes) {
    const f = fixture(type, 8, 8, () => [0, 0, 0, 128]);
    f.image.params.position.value = [20, 30];
    f.image.params.size.value = [80, 40];
    f.effect.params.pitch.value = 10;
    f.effect.params.dotSize.value = 10;
    const before = serialize(f.doc),
      result = resolved(f);
    assert.deepEqual([result.png.width, result.png.height], [8, 8]);
    assert.deepEqual(result.value.position, [20, 30]);
    assert.deepEqual(result.value.size, [80, 40]);
    assert.ok(alpha(result.rgba).includes(255));
    assert.ok(alpha(result.rgba).includes(0));
    assert.equal(serialize(f.doc), before);
    f.doc.imageAssets = { 'image-source': asset(8, 8, () => [55, 120, 200, 0]) };
    assert.ok(alpha(resolved(f).rgba).every((a) => a === 0));
    f.effect.params.invert.value = true;
    assert.ok(alpha(resolved(f).rgba).every((a) => a === 255));
  }
});

// Independent ray/winding oracle against the preserved Shape path, sampled at
// the documented 4x4 pixel positions. This also checks holes and overlap union.
function inside(path, x, y) {
  let winding = 0;
  for (const contour of path.contours) {
    const points = contour.points.map((point) => point.position);
    for (let i = 0; i < points.length; i++) {
      const a = points[i],
        b = points[(i + 1) % points.length];
      const cross = (b[0] - a[0]) * (y - a[1]) - (x - a[0]) * (b[1] - a[1]);
      if (a[1] <= y && b[1] > y && cross > 0) winding++;
      if (a[1] > y && b[1] <= y && cross < 0) winding--;
    }
  }
  return winding !== 0;
}
test('rotated overlapping squares, circles and stars match Shape support with transparent holes', () => {
  for (const type of imageDitherTypes)
    for (const shape of ['square', 'circle', 'polygon', 'star']) {
      const f = fixture(type, 32, 32),
        vector = node(type.replace(/ Image$/, ''));
      f.effect.operation = shape;
      f.effect.params.pitch.value = 8;
      f.effect.params.dotSize.value = 12;
      f.effect.params.dotSides.value = 4;
      f.effect.params.starInnerRatio.value = 0.18;
      f.effect.params.dotRotation.value = 31;
      f.effect.params.gridOrigin.value = [0.13, 0.29];
      vector.operation = shape;
      vector.params = structuredClone(f.effect.params);
      vector.params.path.type = 'PaintedPath';
      f.comp.nodes.push(vector);
      const path = evaluate(f.comp, {}, f.doc.imageAssets, f.doc).run(vector.id),
        actual = alpha(resolved(f).rgba);
      const expected = [];
      for (let y = 0; y < 32; y++)
        for (let x = 0; x < 32; x++) {
          let coverage = 0;
          for (let sy = 0; sy < 4; sy++)
            for (let sx = 0; sx < 4; sx++)
              if (inside(path, x + (sx + 0.5) / 4, y + (sy + 0.5) / 4)) coverage++;
          expected.push(Math.round((coverage * 255) / 16));
        }
      assert.deepEqual(actual, expected, `${type} ${shape}`);
    }
});

test('shared Image resolver supports Keyer chains and downstream Shape luminance consumers', () => {
  const f = fixture(),
    keyer = node('Keyer'),
    shape = node('Ordered Dither');
  keyer.params.keyColor.value = { hex: '#000000', alpha: 1 };
  keyer.params.tolerance.value = 0;
  f.comp.nodes.push(keyer, shape);
  connect(f.comp, f.effect.id, keyer.id, 'path');
  connect(f.comp, keyer.id, f.out.id, 'path');
  connect(f.comp, f.effect.id, shape.id, 'path');
  const result = resolved(f);
  assert.equal(result.value.imageEffects.length, 2);
  assert.ok(alpha(result.rgba).every((a) => a === 0));
  assert.ok(evaluate(f.comp, {}, f.doc.imageAssets, f.doc).run(shape.id).contours.length);
});

test('save/reopen, creation schema, settings, Bypass and Undo/Redo use common commands', () => {
  for (const type of imageDitherTypes) {
    const f = fixture(type),
      session = new CommandSession(f.doc);
    const send = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: f.comp.id, ...payload } });
    const before = serialize(session.snapshot());
    send('set-operation', { nodeId: f.effect.id, operation: 'star' });
    send('set-parameter', { nodeId: f.effect.id, key: 'dotSize', value: 2 });
    const saved = serialize(session.snapshot());
    send('undo');
    send('undo');
    assert.equal(serialize(session.snapshot()), before);
    send('redo');
    send('redo');
    assert.equal(serialize(session.snapshot()), saved);
    const reopened = deserialize(saved),
      after = resolved({ doc: reopened, comp: reopened.layers[0] });
    assert.equal(reopened.schemaVersion, 41);
    assert.equal(serialize(reopened), saved);
    assert.equal(
      after.png.data,
      resolved({ doc: session.snapshot(), comp: session.snapshot().layers[0] }).png.data,
    );
    send('set-node-bypassed', { nodeId: f.effect.id, bypassed: true });
    assert.equal(
      evaluate(session.snapshot().layers[0], {}, session.snapshot().imageAssets).path.imageEffects,
      undefined,
    );
    send('undo');
    assert.equal(serialize(session.snapshot()), saved);
    send('set-parameter-lock', { nodeId: f.effect.id, key: 'dotSize', locked: true });
    assert.throws(
      () => send('set-parameter', { nodeId: f.effect.id, key: 'dotSize', value: 5 }),
      /locked/,
    );
    f.doc.schemaVersion = 40;
    assert.throws(() => validate(f.doc), /schemaVersion 41/);
    const fresh = document(),
      s = new CommandSession(fresh);
    s.dispatch({
      ...s.state,
      type: 'create-node',
      payload: { layerId: fresh.layers[0].id, nodeType: type },
    });
    assert.equal(s.snapshot().schemaVersion, 41);
    s.dispatch({ ...s.state, type: 'undo' });
    assert.equal(s.snapshot().schemaVersion, fresh.schemaVersion);
  }
});

test('source replacement, setting changes and cache eviction regenerate bytes without authored growth', () => {
  const f = fixture(),
    original = serialize(f.doc),
    initial = resolved(f).png.data;
  for (let i = 0; i < 80; i++) {
    f.effect.params.gridOrigin.value = [i / 10, 0];
    const { value, png } = resolved(f);
    assert.equal(value.data, undefined);
    assert.ok(JSON.stringify(value).length < 1000);
    assert.ok(decodeImage(png.data).length <= MAX_IMAGE_BYTES);
    const info = imageEffectCacheInfo();
    assert.equal(info.entries, 1);
    assert.equal(info.recipes, 1);
    assert.ok(info.base64Characters <= Math.ceil(MAX_IMAGE_BYTES / 3) * 4);
    assert.equal(Object.keys(f.doc.imageAssets).length, 1);
  }
  f.effect.params.gridOrigin.value = [0, 0];
  assert.equal(resolved(f).png.data, initial);
  assert.equal(serialize(f.doc), original);
  f.doc.imageAssets = { 'image-source': asset(8, 8, () => [255, 255, 255, 255]) };
  assert.notEqual(resolved(f).png.data, initial);
  assert.ok(alpha(resolved(f).rgba).every((a) => a === 0));
});

test('named presets keep Image source assets and schema 41 recipes editable with fresh node IDs', () => {
  const f = fixture(),
    session = new CommandSession(f.doc);
  const send = (type, payload) =>
    session.dispatch({ ...session.state, type, payload: { layerId: f.comp.id, ...payload } });
  const { presetId } = send('save-node-preset', {
    nodeIds: [f.image.id, f.effect.id],
    name: 'Image Dither',
  });
  const { nodeIds } = send('apply-node-preset', { presetId });
  assert.equal(nodeIds.length, 2);
  assert.ok(!nodeIds.includes(f.effect.id));
  const doc = deserialize(serialize(session.snapshot())),
    comp = doc.layers[0];
  const effect = comp.nodes.find((n) => nodeIds.includes(n.id) && n.type === f.effect.type);
  assert.equal(outputType(effect), 'Image');
  assert.equal(Object.keys(doc.imageAssets).length, 1);
  assert.equal(
    evaluate(comp, {}, doc.imageAssets, doc).run(effect.id).imageEffects[0].type,
    'dither',
  );
});

test('editable example reopens both representations and regenerates Image data without asset growth', async () => {
  const text = await readFile(
      new URL('../examples/image-dither-output.json', import.meta.url),
      'utf8',
    ),
    doc = deserialize(text);
  const before = serialize(doc);
  assert.equal(doc.layers.length, 4);
  assert.equal(Object.keys(doc.imageAssets).length, 1);
  assert.deepEqual(
    doc.layers.map((comp) => evaluate(comp, {}, doc.imageAssets, doc).path.kind || 'shape'),
    ['image', 'shape', 'image', 'shape'],
  );
  assert.match(exportSVG(doc), /<image /);
  assert.match(exportSVG(doc), /<path /);
  assert.equal(serialize(doc), before);
});

test('cell/vertex limits and upstream edits reject atomically, including history/revision', () => {
  const f = fixture(imageDitherTypes[0], 64, 64),
    s = new CommandSession(f.doc);
  const send = (type, payload) =>
    s.dispatch({ ...s.state, type, payload: { layerId: f.comp.id, ...payload } });
  const before = serialize(s.snapshot()),
    state = { ...s.state };
  assert.throws(
    () => send('set-parameter', { nodeId: f.image.id, key: 'size', value: [65, 65] }),
    /4096/,
  );
  assert.equal(serialize(s.snapshot()), before);
  assert.deepEqual(s.state, state);
  send('set-operation', { nodeId: f.effect.id, operation: 'circle' });
  const circle = serialize(s.snapshot()),
    circleState = { ...s.state };
  assert.throws(
    () => send('set-parameter', { nodeId: f.effect.id, key: 'dotSize', value: 1024 }),
    /100000/,
  );
  assert.equal(serialize(s.snapshot()), circle);
  assert.deepEqual(s.state, circleState);
});

test('raster work, pixel dimensions, active-node and mixed-recipe limits are finite', () => {
  assert.equal(MAX_IMAGE_DITHER_EDGE_SAMPLES, 8_000_000);
  const f = fixture(),
    recipe = resolved(f).value.imageEffects[0];
  assert.throws(
    () =>
      renderImageDither(
        { ...f.doc.imageAssets['image-source'], width: 2001, height: 2000 },
        recipe,
      ),
    /4 megapixels/,
  );
  const large = asset(2000, 2000);
  assert.throws(
    () => renderImageDither(large, { ...recipe, size: [64, 64], pitch: 1, dotSize: 1024 }),
    /8000000 edge samples/,
  );
  const big = fixture();
  big.doc.imageAssets = { 'image-source': large };
  big.image.params.size.value = [64, 64];
  const session = new CommandSession(big.doc),
    before = serialize(session.snapshot()),
    state = { ...session.state };
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-parameter',
        payload: {
          layerId: big.comp.id,
          nodeId: big.effect.id,
          key: 'dotSize',
          value: 1024,
        },
      }),
    /8000000 edge samples/,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
  for (let i = 0; i < 8; i++) f.comp.nodes.push(node(imageDitherTypes[0]));
  assert.throws(() => validate(f.doc), /8 active Image/);
  assert.throws(
    () =>
      imageAssetFor(
        { ...resolved(f).value, imageEffects: Array(9).fill(recipe) },
        f.doc.imageAssets,
      ),
    /8 effects/,
  );
});
