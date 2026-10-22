import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  connect,
  deserialize,
  serialize,
  exportSVG,
  evaluate,
  node,
  socketCompatibleFor,
  validate,
} from '../src/core.js';
import { maskAppearance } from '../src/appearance-mask.js';
import { imageAssetFor } from '../src/image-keyer.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { imageAlphaFixture } from './fixtures/mask-image-alpha.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

function session(options = {}) {
  const f = imageAlphaFixture(options),
    s = new CommandSession(f.doc),
    send = (type, payload = {}, envelope = s.state) =>
      s.dispatch({
        ...envelope,
        type,
        payload: { layerId: f.layer.id, nodeId: f.mask.id, ...payload },
      });
  return { ...f, s, send };
}
const checkpoint = (h) => ({
  json: serialize(h.s.snapshot()),
  state: h.s.state,
  past: h.s.historyView.past.length,
  future: h.s.historyView.future.length,
});

test('Image-typed alpha compatibility is local to Mask material; Path and other socket contracts remain', () => {
  const f = imageAlphaFixture({ connectMask: false });
  assert.equal(f.mask.params.mask.type, 'Path');
  for (const source of [f.material, f.alphaKeyer]) {
    assert.equal(socketCompatibleFor(source, f.mask, 'mask'), true);
    assert.equal(socketCompatibleFor(source, f.mask, 'mask', 'x'), false);
    assert.equal(socketCompatibleFor(source, node('Transform'), 'path'), false);
  }
  assert.equal(socketCompatibleFor(f.shape, f.mask, 'mask'), true);
  assert.equal(socketCompatibleFor(f.mask, f.mask, 'mask'), false);
  assert.equal(socketCompatibleFor(node('Value'), f.mask, 'mask'), false);
});

test('connecting Image/Keyer authors schema51 in one Undo; repeat is neutral and Redo/JSON/project preserve IDs/assets', () => {
  for (const keyed of [false, true]) {
    const h = session({ connectMask: false }),
      before = checkpoint(h),
      sourceId = keyed ? h.alphaKeyer.id : h.material.id;
    h.send('connect', { sourceId, key: 'mask' });
    const after = h.s.snapshot(),
      state = checkpoint(h);
    assert.equal(after.schemaVersion, 51);
    assert.equal(after.layers[0].nodes.find((n) => n.id === h.mask.id).params.mask.type, 'Path');
    assert.equal(state.past, before.past + 1);
    assert.deepEqual(after.imageAssets, h.doc.imageAssets);
    assert.deepEqual(deserialize(serialize(after)), after);
    const p = migrateLegacyDocumentToCompositionProject(after);
    assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(p), deserialize), p);
    assert.equal(exportSVG(deserialize(serialize(after))), exportSVG(after));
    h.send('connect', { sourceId, key: 'mask' });
    assert.deepEqual(checkpoint(h), state);
    h.send('undo');
    assert.equal(serialize(h.s.snapshot()), before.json);
    h.send('redo');
    assert.equal(serialize(h.s.snapshot()), state.json);
  }
});

test('existing Keyer decoder supplies processed alpha, and source/material remain immutable', () => {
  const f = imageAlphaFixture({ keyed: true }),
    before = serialize(f.doc),
    r = evaluate(f.layer, {}, f.doc.imageAssets, f.doc);
  assert.equal(r.error, null);
  assert.deepEqual(r.path.mask.shape, r.run(f.alphaKeyer.id));
  assert.deepEqual(r.path.children[0], r.run(f.keyer.id));
  const pixels = copyImageRgba(imageAssetFor(r.path.mask.shape, f.doc.imageAssets));
  assert.equal(pixels[3], 0);
  assert.equal(pixels[2 * 4 + 3], 0);
  assert.equal(pixels[4 * 4 + 3], 128);
  assert.equal(pixels[6 * 4 + 3], 255);
  assert.equal(serialize(f.doc), before);
});

test('common SVG takes only SourceAlpha; Feather occurs once and Invert complements after blur', () => {
  for (const feather of [0, 4])
    for (const invert of [false, true]) {
      const f = imageAlphaFixture({ feather, invert }),
        svg = exportSVG(f.doc);
      assert.match(svg, /-alpha" filterUnits="userSpaceOnUse"/);
      assert.equal((svg.match(/<feGaussianBlur/g) || []).length, feather ? 1 : 0);
      assert.equal((svg.match(/<feFuncA/g) || []).length, invert ? 1 : 0);
      assert.match(
        svg,
        /<feComposite in="white" in2="(?:SourceAlpha|soft|coverage)" operator="in"/,
      );
      assert.doesNotMatch(svg, /feColorMatrix|feBlend/);
    }
});

test('Image position/size and later appearance transform share document coordinates once', () => {
  for (const postTransform of [undefined, 'scale', 'rotate']) {
    const f = imageAlphaFixture({ postTransform });
    f.material.params.position.value = [10, 20];
    f.material.params.size.value = [40, 30];
    const r = evaluate(f.layer, {}, f.doc.imageAssets, f.doc),
      masked = postTransform ? r.path.children[0] : r.path;
    assert.equal(r.error, null);
    assert.deepEqual(masked.mask.shape.position, [10, 20]);
    assert.deepEqual(masked.mask.shape.size, [40, 30]);
    assert.deepEqual(masked.matrix, [1, 0, 0, 1, 0, 0]);
    assert.match(exportSVG(f.doc), /x="10" y="20" width="40" height="30"/);
  }
});

test('type/lock/stale/invalid geometry/cycles reject atomically without history loss', () => {
  for (const payload of [
    { sourceId: 'missing', key: 'mask' },
    { sourceId: 'example-mask', key: 'mask' },
    { sourceId: 'alpha-material', sourcePort: 'x', key: 'mask' },
  ]) {
    const h = session({ connectMask: false }),
      before = checkpoint(h);
    assert.throws(() => h.send('connect', payload));
    assert.deepEqual(checkpoint(h), before);
  }
  for (const lock of ['parameter', 'composition']) {
    const f = imageAlphaFixture({ connectMask: false });
    if (lock === 'parameter') f.mask.params.mask.locked = true;
    else f.layer.locked = true;
    const s = new CommandSession(f.doc),
      before = serialize(s.snapshot()),
      state = s.state;
    assert.throws(() =>
      s.dispatch({
        ...state,
        type: 'connect',
        payload: { layerId: f.layer.id, nodeId: f.mask.id, key: 'mask', sourceId: f.material.id },
      }),
    );
    assert.equal(serialize(s.snapshot()), before);
    assert.deepEqual(s.state, state);
  }
  const h = session(),
    old = h.s.state;
  h.send('set-parameter', { key: 'invert', value: true });
  const before = checkpoint(h);
  assert.throws(() => h.send('connect', { key: 'mask', sourceId: h.alphaKeyer.id }, old));
  assert.deepEqual(checkpoint(h), before);
  for (const value of [
    [0, 80],
    [-1, 80],
    [Infinity, 80],
  ]) {
    assert.throws(() => h.send('set-parameter', { nodeId: h.material.id, key: 'size', value }));
    assert.deepEqual(checkpoint(h), before);
  }
  const extra = h.send('create-node', { nodeType: 'Keyer' }).nodeId;
  h.send('connect', { nodeId: extra, key: 'path', sourceId: h.alphaKeyer.id });
  const cycleBefore = checkpoint(h);
  assert.throws(
    () => h.send('connect', { nodeId: h.alphaKeyer.id, key: 'path', sourceId: extra }),
    /cycle/i,
  );
  assert.deepEqual(checkpoint(h), cycleBefore);
});

test('image mask filter region budget applies at sigma0 and invalid material geometry is explicit', () => {
  const h = session(),
    before = checkpoint(h);
  assert.throws(
    () => h.send('set-parameter', { nodeId: h.image.id, key: 'size', value: [10000, 10000] }),
    /region exceeds/,
  );
  assert.deepEqual(checkpoint(h), before);
  const r = evaluate(h.layer, {}, h.doc.imageAssets, h.doc);
  for (const bad of [
    { ...r.path.mask.shape, size: [0, 1] },
    { ...r.path.mask.shape, position: [NaN, 0] },
    { ...r.path.mask.shape, asset: '' },
  ])
    assert.throws(
      () =>
        maskAppearance(r.path.children[0], bad, {
          invert: false,
          prefix: 'bad',
          bounds: [
            [0, 40, 80],
            [0, 40, 80],
          ],
        }),
      /Invalid Image/,
    );
});

test('unknown/missing assets and old-version alpha connections fail safely; incomplete Mask stays an explicit error', () => {
  const f = imageAlphaFixture();
  f.doc.schemaVersion = 50;
  assert.throws(() => validate(f.doc), /schemaVersion 51/);
  f.doc.schemaVersion = 51;
  delete f.doc.imageAssets[f.material.asset];
  assert.throws(() => validate(f.doc), /asset/i);
  const g = imageAlphaFixture();
  delete g.mask.params.mask.source;
  const r = evaluate(g.layer, {}, g.doc.imageAssets, g.doc);
  assert.match(r.error, /Connect appearance/);
  assert.throws(() => exportSVG(g.doc), /Connect appearance/);
});
