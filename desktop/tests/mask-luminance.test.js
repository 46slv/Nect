import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  evaluate,
  validate,
  node,
  parameter,
  connect,
} from '../src/core.js';
import { maskAppearance } from '../src/appearance-mask.js';
import {
  luminanceFixture,
  expectedLuminance,
  equalLuminanceRGB,
} from './fixtures/mask-luminance.js';
import { featherFixture } from './fixtures/mask-feather.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
function session(f = luminanceFixture()) {
  const s = new CommandSession(f.doc),
    send = (type, payload = {}, envelope = s.state) =>
      s.dispatch({
        ...envelope,
        type,
        payload: { layerId: f.layer.id, nodeId: f.mask.id, ...payload },
      });
  return { ...f, s, send };
}
const state = (h) => ({
  json: serialize(h.s.snapshot()),
  command: h.s.state,
  past: h.s.historyView.past.length,
  future: h.s.historyView.future.length,
});
test('legacy Alpha documents and their SVG remain byte-exact; implicit Alpha reads/edits are neutral', async () => {
  for (const prefix of ['01-image-alpha', '02-keyer-feather']) {
    const json = await fs.readFile(
        new URL('../examples/image-alpha-mask/' + prefix + '.json', import.meta.url),
        'utf8',
      ),
      svg = await fs.readFile(
        new URL('../examples/image-alpha-mask/' + prefix + '.svg', import.meta.url),
        'utf8',
      ),
      doc = deserialize(json),
      n = doc.layers[0].nodes.find((n) => n.type === 'Mask');
    assert.equal(parameter(n, 'luminance').value, false);
    assert.equal(serialize(doc), json);
    assert.equal(exportSVG(doc), svg);
    assert.equal(Object.hasOwn(n.params, 'luminance'), false);
  }
  const h = session(),
    before = state(h);
  h.send('set-parameter', { key: 'luminance', value: false });
  assert.deepEqual(state(h), before);
});
test('one Luminance mode edit authors schema52 with one Undo, retained IDs/assets and exact project reload', () => {
  const h = session(),
    before = state(h),
    svg = exportSVG(h.s.snapshot());
  h.send('set-parameter', { key: 'luminance', value: true });
  const after = h.s.snapshot(),
    saved = state(h);
  assert.equal(after.schemaVersion, 52);
  assert.equal(saved.past, before.past + 1);
  assert.deepEqual(after.imageAssets, h.doc.imageAssets);
  assert.deepEqual(
    after.layers[0].nodes.map((n) => n.id),
    h.layer.nodes.map((n) => n.id),
  );
  assert.deepEqual(deserialize(serialize(after)), after);
  const p = migrateLegacyDocumentToCompositionProject(after);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(p), deserialize), p);
  assert.match(exportSVG(after), /feColorMatrix/);
  h.send('set-parameter', { key: 'luminance', value: true });
  assert.deepEqual(state(h), saved);
  h.send('undo');
  assert.equal(serialize(h.s.snapshot()), before.json);
  const undone = state(h);
  h.send('set-parameter', { key: 'luminance', value: false });
  assert.deepEqual(state(h), undone);
  h.send('redo');
  assert.equal(serialize(h.s.snapshot()), saved.json);
  h.send('set-parameter', { key: 'luminance', value: false });
  assert.equal(exportSVG(h.s.snapshot()), svg);
});
test('conversion is sRGB weighted straight RGB times original alpha, before exactly one blur then Invert', () => {
  assert.deepEqual(expectedLuminance, [0, 255, 128, 54, 182, 18, 128, 64, 0]);
  for (const feather of [0, 4])
    for (const invert of [false, true]) {
      const f = luminanceFixture({ luminance: true, feather, invert }),
        r = evaluate(f.layer, {}, f.doc.imageAssets, f.doc),
        svg = exportSVG(f.doc);
      assert.equal(r.error, null);
      assert.equal(r.path.mask.luminance, true);
      assert.deepEqual(r.path.children[0], r.run(f.keyer.id));
      assert.match(svg, /color-interpolation-filters="sRGB"/);
      assert.match(svg, /0.2126 0.7152 0.0722 0 0/);
      assert.match(
        svg,
        /feComposite in="luminance" in2="SourceAlpha" operator="in" result="material-alpha"/,
      );
      assert.equal((svg.match(/<feGaussianBlur/g) || []).length, feather ? 1 : 0);
      assert.equal((svg.match(/<feFuncA/g) || []).length, invert ? 1 : 0);
      if (feather)
        assert.ok(
          svg.indexOf('result="material-alpha"') <
            svg.indexOf('<feGaussianBlur in="material-alpha"'),
        );
      if (invert)
        assert.match(
          svg,
          new RegExp('feComponentTransfer in="' + (feather ? 'soft' : 'material-alpha') + '"'),
        );
    }
});
test('Path coverage ignores the Image-only mode, preserving old geometry/paint and SVG', () => {
  const f = featherFixture(4),
    h = session(f),
    before = exportSVG(f.doc);
  h.send('set-parameter', { key: 'luminance', value: true });
  const after = h.s.snapshot(),
    r = evaluate(after.layers[0], {}, after.imageAssets, after);
  assert.equal(after.schemaVersion, 52);
  assert.equal(r.path.mask.luminance, undefined);
  assert.equal(exportSVG(after), before);
});
test('invalid values and stale commands reject atomically before materializing the virtual parameter', () => {
  for (const value of [null, 0, 1, 'alpha', 'luminance', NaN, {}, []]) {
    const h = session(),
      before = state(h);
    assert.throws(() => h.send('set-parameter', { key: 'luminance', value }));
    assert.deepEqual(state(h), before);
  }
  const h = session(),
    old = h.s.state;
  h.send('set-parameter', { key: 'luminance', value: true });
  const before = state(h);
  assert.throws(() => h.send('set-parameter', { key: 'luminance', value: false }, old));
  assert.deepEqual(state(h), before);
  const f = luminanceFixture({ luminance: true });
  f.doc.schemaVersion = 51;
  assert.throws(() => validate(f.doc), /schemaVersion 52/);
  f.doc.schemaVersion = 52;
  f.mask.params.luminance.value = 'luminance';
  assert.throws(() => validate(f.doc), /Boolean/);
});
test('existing Boolean source/lock/cycle guards also protect Image mode, with no partial type connection', () => {
  const h = session(),
    before = state(h),
    scalar = node('Value');
  h.layer.nodes.push(scalar);
  assert.throws(() => connect(h.layer, scalar.id, h.mask.id, 'luminance'));
  assert.equal(Object.hasOwn(h.mask.params, 'luminance'), false);
  assert.deepEqual(state(h), before);
  const a = h.send('create-node', { nodeType: 'Toggle' }).nodeId,
    b = h.send('create-node', { nodeType: 'Toggle' }).nodeId;
  h.send('set-parameter', { nodeId: a, key: 'value', value: true });
  h.send('connect', { sourceId: a, key: 'luminance' });
  let d = h.s.snapshot();
  assert.equal(d.schemaVersion, 52);
  assert.equal(evaluate(d.layers[0], {}, d.imageAssets, d).path.mask.luminance, true);
  h.send('connect', { nodeId: b, key: 'value', sourceId: a });
  const cycle = state(h);
  assert.throws(() => h.send('connect', { nodeId: a, key: 'value', sourceId: b }), /cycle/i);
  assert.deepEqual(state(h), cycle);
  for (const lock of ['parameter', 'composition']) {
    const f = luminanceFixture();
    if (lock === 'parameter')
      ((f.mask.params.luminance = { ...parameter(f.mask, 'luminance'), locked: true }),
        (f.doc.schemaVersion = 52));
    else f.layer.locked = true;
    const x = session(f),
      saved = state(x);
    assert.throws(() => x.send('set-parameter', { key: 'luminance', value: true }));
    assert.deepEqual(state(x), saved);
  }
});
test('material coordinates and later appearance Transform preserve luminance metadata once', () => {
  const f = luminanceFixture({ luminance: true, postTransform: 'scale' });
  f.material.params.position.value = [10, 20];
  f.material.params.size.value = [90, 40];
  const r = evaluate(f.layer, {}, f.doc.imageAssets, f.doc);
  assert.equal(r.error, null);
  assert.equal(r.path.children[0].mask.luminance, true);
  assert.deepEqual(r.path.children[0].mask.shape.position, [10, 20]);
  assert.deepEqual(r.path.matrix, [2, 0, 0, 1, 10, 10]);
  assert.equal((exportSVG(f.doc).match(/<feColorMatrix/g) || []).length, 1);
});
test('unconnected masks remain explicit errors; invalid mode/assets/region never partially commit', () => {
  const h = session(luminanceFixture({ luminance: true })),
    before = state(h);
  assert.throws(
    () => h.send('set-parameter', { nodeId: h.image.id, key: 'size', value: [10000, 10000] }),
    /region exceeds/,
  );
  assert.deepEqual(state(h), before);
  const f = luminanceFixture({ luminance: true });
  delete f.mask.params.mask.source;
  assert.match(evaluate(f.layer, {}, f.doc.imageAssets, f.doc).error, /Connect appearance/);
  assert.throws(() => exportSVG(f.doc), /Connect appearance/);
  const g = luminanceFixture({ luminance: true });
  delete g.doc.imageAssets[g.material.asset];
  assert.throws(() => validate(g.doc), /asset/i);
  const q = luminanceFixture(),
    r = evaluate(q.layer, {}, q.doc.imageAssets, q.doc);
  assert.throws(
    () =>
      maskAppearance(r.path.children[0], r.path.mask.shape, {
        invert: false,
        luminance: 'bad',
        prefix: 'bad',
        bounds: [
          [0, 90, 180],
          [0, 40, 80],
        ],
      }),
    /Boolean/,
  );
});

test('different unpremultiplied RGB have exactly the same declared sRGB luminance', () => {
  assert.notDeepEqual(equalLuminanceRGB[0], equalLuminanceRGB[1]);
  for (const [r, g, b] of equalLuminanceRGB) assert.equal(2126 * r + 7152 * g + 722 * b, 1280000);
});
