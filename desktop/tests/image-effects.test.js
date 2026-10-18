import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  layer,
  connect,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  validate,
  CommandSession,
} from '../src/core.js';
import { keyRgba, keyerSettings, encodeRgbaPng, imageAssetFor } from '../src/image-keyer.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { encodeImage, pngSize } from '../src/images.js';
import { groupGeometry } from '../src/composition-reference.js';
import { viewerMarkup } from '../src/node-viewers.js';
import { maskAppearance } from '../src/appearance-mask.js';

export function imageFixture() {
  const doc = document(),
    comp = doc.layers[0],
    image = node('Image'),
    keyer = node('Keyer'),
    mask = node('Mask'),
    shape = node('Ellipse'),
    out = node('Image Output');
  const rgba = new Uint8Array([0, 255, 0, 255, 255, 0, 0, 128, 255, 0, 0, 255, 255, 0, 0, 0]);
  doc.width = 80;
  doc.height = 80;
  doc.schemaVersion = 35;
  doc.imageAssets = {
    'image-pixels': {
      name: 'synthetic.png',
      mime: 'image/png',
      width: 2,
      height: 2,
      data: encodeImage(encodeRgbaPng(2, 2, rgba)),
    },
  };
  image.asset = 'image-pixels';
  image.params.position.value = [0, 0];
  image.params.size.value = [80, 80];
  shape.params.center.value = [40, 40];
  shape.params.size.value = [40, 40];
  comp.nodes = [image, keyer, shape, mask, out];
  comp.output = out.id;
  connect(comp, image.id, keyer.id, 'path');
  connect(comp, keyer.id, out.id, 'path');
  connect(comp, keyer.id, mask.id, 'path');
  connect(comp, shape.id, mask.id, 'mask');
  return { doc, comp, image, keyer, mask, shape, out, rgba };
}

test('Keyer multiplies existing alpha, retains straight partial RGB and sanitizes zero-alpha RGB', () => {
  const source = new Uint8Array([0, 0, 0, 128, 51, 51, 51, 128, 255, 0, 0, 0, 255, 255, 255, 255]),
    before = source.slice();
  const result = keyRgba(source, keyerSettings({ hex: '#000000', alpha: 0.2 }, 0.1, 0.2));
  assert.deepEqual([...result], [0, 0, 0, 0, 51, 51, 51, 64, 0, 0, 0, 0, 255, 255, 255, 255]);
  assert.deepEqual(source, before);
  assert.deepEqual(
    [
      ...keyRgba(
        new Uint8Array([0, 255, 0, 128, 1, 255, 0, 128]),
        keyerSettings({ hex: '#00ff00' }, 0, 0),
      ),
    ],
    [0, 0, 0, 0, 1, 255, 0, 128],
  );
});
test('streaming canonical PNG encoder roundtrips exact RGBA and validates CRC, dimensions and limits', () => {
  const f = imageFixture(),
    asset = f.doc.imageAssets['image-pixels'];
  assert.deepEqual(pngSize(encodeRgbaPng(2, 2, f.rgba)), { width: 2, height: 2 });
  assert.deepEqual(copyImageRgba(asset), f.rgba);
  assert.throws(() => encodeRgbaPng(2001, 2000, new Uint8Array()), /4 megapixels/);
  assert.throws(
    () => keyRgba(new Uint8Array(3), keyerSettings({ hex: '#000000' }, 0, 0)),
    /4 megapixels/,
  );
  assert.throws(() => keyerSettings({ hex: '#bad' }, 0, 0), /RGB key/);
  assert.throws(() => keyerSettings({ hex: '#000000' }, -1, 0), /range/);
  const bytes = encodeRgbaPng(2, 2, f.rgba);
  bytes[45] ^= 1;
  assert.throws(() => copyImageRgba({ ...asset, data: encodeImage(bytes) }), /checksum|CRC/i);
});
test('Keyer recipes share preview/export/luminance assets, invalidate replacements and preserve authored data', () => {
  const f = imageFixture(),
    before = serialize(f.doc),
    r = evaluate(f.comp, {}, f.doc.imageAssets, f.doc);
  assert.equal(r.error, null);
  assert.deepEqual(
    [...copyImageRgba(imageAssetFor(r.path, f.doc.imageAssets))],
    [0, 0, 0, 0, 255, 0, 0, 128, 255, 0, 0, 255, 0, 0, 0, 0],
  );
  assert.equal(r.path.asset, 'image-pixels');
  assert.equal(r.path.imageEffects.length, 1);
  assert.equal(r.path.data, undefined);
  const svg = exportSVG(f.doc);
  assert.equal(exportSVG(deserialize(before)), svg);
  assert.equal(serialize(f.doc), before);
  const luma = node('Halftone');
  f.comp.nodes.push(luma);
  connect(f.comp, f.keyer.id, luma.id, 'path');
  assert.ok(evaluate(f.comp, {}, f.doc.imageAssets, f.doc).run(luma.id).contours);
  f.doc.imageAssets = {
    'image-pixels': {
      ...f.doc.imageAssets['image-pixels'],
      data: encodeImage(encodeRgbaPng(2, 2, new Uint8Array(16).fill(255))),
    },
  };
  assert.notEqual(exportSVG(f.doc), svg);
  f.keyer.bypassed = true;
  const bypass = evaluate(f.comp, {}, f.doc.imageAssets, f.doc).path;
  assert.equal(bypass.imageEffects, undefined);
});
test('schema promotion, Bypass, parameter edits and output conversion are atomic common commands', () => {
  const f = imageFixture();
  f.doc.schemaVersion = 34;
  f.comp.nodes = f.comp.nodes.filter((n) => !['Keyer', 'Mask'].includes(n.type));
  connect(f.comp, f.image.id, f.out.id, 'path');
  const s = new CommandSession(f.doc),
    send = (type, payload = {}) =>
      s.dispatch({ ...s.state, type, payload: { layerId: f.comp.id, ...payload } }),
    before = serialize(s.snapshot());
  const k = send('create-node', { nodeType: 'Keyer' }).nodeId;
  assert.equal(s.snapshot().schemaVersion, 35);
  send('undo');
  assert.equal(serialize(s.snapshot()), before);
  send('redo');
  send('connect', { sourceId: f.image.id, nodeId: k, key: 'path' });
  send('connect', { sourceId: k, nodeId: f.out.id, key: 'path' });
  const saved = serialize(s.snapshot());
  send('set-parameter', { nodeId: k, key: 'tolerance', value: 0.4 });
  send('undo');
  assert.equal(serialize(s.snapshot()), saved);
  assert.throws(() => send('set-parameter', { nodeId: k, key: 'softness', value: 2 }), /range/);
  assert.equal(serialize(s.snapshot()), saved);
  send('set-node-bypassed', { nodeId: k, bypassed: true });
  assert.equal(
    evaluate(s.snapshot().layers[0], {}, s.snapshot().imageAssets, s.snapshot()).path.imageEffects,
    undefined,
  );
  send('undo');
  const m = send('create-node', { nodeType: 'Mask' }).nodeId;
  send('connect', { sourceId: k, nodeId: m, key: 'path' });
  send('connect', { sourceId: f.shape.id, nodeId: m, key: 'mask' });
  const originalOutput = serialize(s.snapshot());
  send('preview-node', { nodeId: m });
  const upgraded = s.snapshot();
  assert.equal(upgraded.layers[0].output, f.out.id);
  assert.equal(upgraded.layers[0].nodes.find((n) => n.id === f.out.id).type, 'Output');
  assert.match(exportSVG(upgraded), /<mask /);
  send('undo');
  assert.equal(serialize(s.snapshot()), originalOutput);
  send('redo');
  assert.equal(exportSVG(s.snapshot()), exportSVG(deserialize(serialize(upgraded))));
  send('set-parameter-lock', { nodeId: f.out.id, key: 'path', locked: true });
  const locked = serialize(s.snapshot());
  assert.throws(() => send('preview-node', { nodeId: m }), /locked/);
  assert.equal(serialize(s.snapshot()), locked);
});
test('Mask preserves image/Composition alpha and nested transforms, rejects lossy geometry conversion/open masks', () => {
  const f = imageFixture();
  f.out.type = 'Output';
  f.out.params.path.type = 'PaintedPath';
  connect(f.comp, f.mask.id, f.out.id, 'path');
  const r = evaluate(f.comp, {}, f.doc.imageAssets, f.doc);
  assert.equal(r.error, null);
  assert.equal(r.path.children[0].kind, 'image');
  assert.throws(() => groupGeometry(r.path), /Masked appearance/);
  assert.match(exportSVG(f.doc), /maskUnits="userSpaceOnUse"/);
  f.mask.params.invert.value = true;
  assert.match(exportSVG(f.doc), /<rect[^>]+fill="white"\/><path[^>]+fill="black"/);
  const second = layer('Masked composition'),
    ref = node('Composition Reference'),
    tr = node('Transform'),
    out = node('Output');
  ref.compositionReference = { compositionId: f.comp.id };
  second.nodes = [ref, tr, out];
  second.output = out.id;
  f.doc.layers.push(second);
  f.comp.visible = false;
  connect(second, ref.id, tr.id, 'path');
  tr.params.offset.value = [10, 20];
  connect(second, tr.id, out.id, 'path');
  const nested = evaluate(second, {}, f.doc.imageAssets, f.doc);
  assert.equal(nested.error, null);
  assert.match(exportSVG(f.doc), /<mask /);
  assert.throws(() => groupGeometry(nested.path), /Masked appearance/);
  f.mask.bypassed = true;
  assert.doesNotMatch(exportSVG(f.doc), /<mask /);
  f.mask.bypassed = false;
  const curve = node('Curve');
  curve.params.closed.value = false;
  f.comp.nodes.push(curve);
  connect(f.comp, curve.id, f.mask.id, 'mask');
  assert.match(evaluate(f.comp, {}, f.doc.imageAssets, f.doc).error, /closed/);
});
test('new nodes reject older schemas, bounded key chains and invalid fallback ranges', () => {
  const f = imageFixture();
  f.doc.schemaVersion = 34;
  assert.throws(() => validate(f.doc), /schemaVersion 35/);
  f.doc.schemaVersion = 35;
  f.keyer.params.softness.value = -0.1;
  assert.throws(() => validate(f.doc), /range/);
  f.keyer.params.softness.value = 0.05;
  for (let i = 0; i < 8; i++) f.comp.nodes.push(node('Keyer'));
  assert.throws(() => validate(f.doc), /8 active/);
  assert.throws(
    () =>
      imageAssetFor(
        {
          asset: 'image-pixels',
          imageEffects: Array.from({ length: 9 }, () => keyerSettings({ hex: '#000000' }, 0, 0)),
        },
        f.doc.imageAssets,
      ),
    /8 Keyers/,
  );
});
test('Mask after Composition keeps independent paint, stroke bounds, blockers and editable shape placement', () => {
  const f = imageFixture();
  const rectangle = node('Rectangle'),
    stroke = node('Stroke'),
    sourceOut = node('Output');
  rectangle.params.center.value = [40, 40];
  rectangle.params.size.value = [60, 60];
  stroke.params.width.value = 20;
  f.comp.nodes = [rectangle, stroke, sourceOut];
  f.comp.output = sourceOut.id;
  f.comp.visible = false;
  connect(f.comp, rectangle.id, stroke.id, 'path');
  connect(f.comp, stroke.id, sourceOut.id, 'path');
  const dest = layer('Masked reference'),
    ref = node('Composition Reference'),
    out = node('Output');
  ref.compositionReference = { compositionId: f.comp.id };
  dest.nodes = [ref, f.mask, f.shape, out];
  dest.output = out.id;
  f.doc.layers.push(dest);
  connect(dest, ref.id, f.mask.id, 'path');
  connect(dest, f.shape.id, f.mask.id, 'mask');
  connect(dest, f.mask.id, out.id, 'path');
  let r = evaluate(dest, {}, f.doc.imageAssets, f.doc);
  assert.equal(r.error, null);
  assert.equal(r.path.children[0].children[0].stroke.width, 20);
  assert.ok(
    r.path.mask.region[0] < 0,
    'inverse region must include visible stroke outside geometric bounds',
  );
  const before = exportSVG(f.doc);
  f.shape.params.center.value = [55, 30];
  f.shape.params.size.value = [20, 40];
  assert.notEqual(exportSVG(f.doc), before);
  assert.equal(rectangle.params.center.value[0], 40);
  const blocked = maskAppearance(
    { ...r.path, exportBlockers: ['Source suspended'] },
    { contours: [], exportBlockers: ['Mask suspended'] },
    {
      invert: false,
      prefix: 'blocked',
      bounds: [
        [0, 40, 80],
        [0, 40, 80],
      ],
    },
  );
  assert.deepEqual(blocked.exportBlockers, ['Source suspended', 'Mask suspended']);
});
test('alpha viewer is scoped sRGB inspection and never changes authored export', () => {
  const f = imageFixture(),
    before = serialize(f.doc),
    svg = exportSVG(f.doc),
    value = evaluate(f.comp, {}, f.doc.imageAssets, f.doc).path;
  const alpha = viewerMarkup(value, f.doc.imageAssets, 1, true);
  assert.match(alpha, /color-interpolation-filters="sRGB"/);
  assert.match(alpha, /nect-viewer-1-alpha/);
  assert.doesNotMatch(viewerMarkup(value, f.doc.imageAssets, 0, false), /feColorMatrix/);
  assert.equal(serialize(f.doc), before);
  assert.equal(exportSVG(f.doc), svg);
});
