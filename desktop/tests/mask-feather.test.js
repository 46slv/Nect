import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {
  CommandSession,
  serialize,
  deserialize,
  exportSVG,
  evaluate,
  node,
  parameter,
  connect,
  validate,
  layer as newLayer,
} from '../src/core.js';
import { maskAppearance, MAX_MASK_FEATHER } from '../src/appearance-mask.js';
import { featherFixture } from './fixtures/mask-feather.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

function session(f = featherFixture()) {
  const s = new CommandSession(f.doc),
    send = (type, payload = {}, envelope = s.state) =>
      s.dispatch({
        ...envelope,
        type,
        payload: { layerId: f.layer.id, nodeId: f.mask.id, ...payload },
      }),
    set = (value, envelope) => send('set-parameter', { key: 'feather', value }, envelope);
  return { ...f, s, send, set };
}
test('PR70 old documents remain byte-identical JSON and SVG without materialized default/filter', async () => {
  for (const prefix of ['01-pick-background', '02-keyed-mask']) {
    const json = await fs.readFile(
        new URL('../examples/keyer-mask/' + prefix + '.json', import.meta.url),
        'utf8',
      ),
      svg = await fs.readFile(
        new URL('../examples/keyer-mask/' + prefix + '.svg', import.meta.url),
        'utf8',
      ),
      doc = deserialize(json);
    assert.equal(serialize(doc), json);
    assert.equal(exportSVG(doc), svg);
    assert.equal(
      Object.hasOwn(doc.layers[0].nodes.find((n) => n.type === 'Mask').params, 'feather'),
      false,
    );
    assert.doesNotMatch(svg, /<filter/);
    assert.equal(
      parameter(
        doc.layers[0].nodes.find((n) => n.type === 'Mask'),
        'feather',
      ).value,
      0,
    );
    assert.equal(serialize(doc), json);
  }
});
test('first implicit zero is document/history/Redo neutral, positive authorship is one Undo and schema50', () => {
  const h = session(),
    before = h.s.snapshot(),
    svg = exportSVG(before),
    state = h.s.state;
  h.set(0);
  assert.deepEqual(h.s.snapshot(), before);
  assert.deepEqual(h.s.state, state);
  assert.equal(h.s.historyView.past.length, 0);
  h.set(4);
  const after = h.s.snapshot();
  assert.equal(after.schemaVersion, 50);
  assert.equal(after.layers[0].nodes.find((n) => n.id === h.mask.id).params.feather.value, 4);
  assert.equal(h.s.historyView.past.length, 1);
  assert.match(exportSVG(after), /feGaussianBlur in="SourceAlpha" stdDeviation="4"/);
  assert.match(exportSVG(after), /filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse"/);
  assert.deepEqual(deserialize(serialize(after)), after);
  const project = migrateLegacyDocumentToCompositionProject(after),
    reopened = deserializeCompositionProject(serializeCompositionProject(project), deserialize);
  assert.deepEqual(reopened, project);
  assert.equal(reopened.schemaVersion, 50);
  assert.equal(exportSVG(deserialize(serialize(after))), exportSVG(after));
  h.send('undo');
  assert.deepEqual(h.s.snapshot(), before);
  assert.equal(exportSVG(h.s.snapshot()), svg);
  const redo = h.s.historyView.future;
  h.set(0);
  assert.deepEqual(h.s.historyView.future, redo);
  assert.deepEqual(h.s.snapshot(), before);
  h.send('redo');
  assert.deepEqual(h.s.snapshot(), after);
  const same = h.s.state;
  h.set(4);
  assert.deepEqual(h.s.state, same);
  h.set(0);
  assert.equal(exportSVG(h.s.snapshot()), svg);
  assert.doesNotMatch(exportSVG(h.s.snapshot()), /<filter/);
});
test('invalid values, stale envelopes and resolved expressions/inputs reject without partial history', () => {
  for (const value of [-1, NaN, Infinity, -Infinity, MAX_MASK_FEATHER + 0.1, 1e100, '4', null]) {
    const h = session(),
      before = serialize(h.s.snapshot()),
      state = h.s.state;
    assert.throws(() => h.set(value));
    assert.equal(serialize(h.s.snapshot()), before);
    assert.deepEqual(h.s.state, state);
  }
  const h = session(),
    old = h.s.state;
  h.set(4);
  const before = serialize(h.s.snapshot()),
    state = h.s.state;
  assert.throws(() => h.set(3, old));
  assert.equal(serialize(h.s.snapshot()), before);
  assert.deepEqual(h.s.state, state);
  assert.throws(() => h.send('set-parameter-expression', { key: 'feather', expression: '-1' }));
  assert.equal(serialize(h.s.snapshot()), before);
  const id = h.send('create-node', { nodeType: 'Math' }).nodeId;
  h.send('set-parameter', { nodeId: id, key: 'a', value: -1 });
  const saved = serialize(h.s.snapshot());
  assert.throws(() => h.send('connect', { sourceId: id, key: 'feather' }));
  assert.equal(serialize(h.s.snapshot()), saved);
});
test('finite max works on bounded input; huge filter bounds reject atomically and zero stays compatible', () => {
  const h = session();
  h.set(MAX_MASK_FEATHER);
  assert.match(exportSVG(h.s.snapshot()), /stdDeviation="256"/);
  const f = featherFixture();
  f.image.params.size.value = [10000, 10000];
  const large = session(f),
    before = serialize(large.s.snapshot());
  assert.doesNotMatch(exportSVG(large.s.snapshot()), /<filter/);
  assert.throws(() => large.set(1), /region exceeds/);
  assert.equal(serialize(large.s.snapshot()), before);
  assert.equal(large.s.historyView.past.length, 0);
});
test('source opacity/paint, region padding and inversion preserve the same blurred alpha contract', () => {
  const f = featherFixture(4),
    result = evaluate(f.layer, {}, f.doc.imageAssets, f.doc),
    mask = result.path.mask;
  assert.equal(result.error, null);
  assert.deepEqual(mask.region, [-18, -18, 116, 116]);
  assert.equal(mask.feather, 4);
  assert.deepEqual(result.path.children[0], result.run(f.keyer.id));
  assert.doesNotMatch(exportSVG(f.doc), /feComponentTransfer/);
  f.mask.params.invert.value = true;
  const inverted = exportSVG(f.doc);
  assert.match(inverted, /<feGaussianBlur in="SourceAlpha" stdDeviation="4"/);
  assert.match(inverted, /<feFuncA type="linear" slope="-1" intercept="1"/);
  assert.match(inverted, /<feFlood flood-color="white"/);
});
test('before-Mask shape transforms leave sigma isotropic; after-Mask transforms wrap mask and appearance once', () => {
  const f = featherFixture(4, { shapeTransform: true }),
    r = evaluate(f.layer, {}, f.doc.imageAssets, f.doc);
  assert.deepEqual(r.path.mask.shape.contours, r.run('feather-shape-transform').contours);
  assert.equal(r.path.mask.feather, 4);
  assert.deepEqual(r.path.matrix, [1, 0, 0, 1, 0, 0]);
  for (const postTransform of ['scale', 'rotate']) {
    const g = featherFixture(4, { postTransform }),
      out = evaluate(g.layer, {}, g.doc.imageAssets, g.doc).path;
    assert.equal(out.children[0].mask.feather, 4);
    assert.deepEqual(out.children[0].matrix, [1, 0, 0, 1, 0, 0]);
    if (postTransform === 'scale') assert.deepEqual(out.matrix, [2, 0, 0, 1, 10, 10]);
    assert.match(exportSVG(g.doc), /stdDeviation="4"/);
  }
});
test('Composition paint/stroke and blocked export metadata survive feather bounds', () => {
  const f = featherFixture(),
    source = newLayer('Source'),
    rect = node('Rectangle'),
    stroke = node('Stroke'),
    out = node('Output'),
    ref = node('Composition Reference');
  rect.params.center.value = [40, 40];
  rect.params.size.value = [40, 40];
  stroke.params.width.value = 12;
  stroke.params.color.value = { hex: '#336699', alpha: 0.5 };
  source.nodes = [rect, stroke, out];
  source.output = out.id;
  source.visible = false;
  connect(source, rect.id, stroke.id, 'path');
  connect(source, stroke.id, out.id, 'path');
  ref.compositionReference = { compositionId: source.id };
  f.layer.nodes = [ref, f.shape, f.mask, f.out];
  f.doc.layers.unshift(source);
  connect(f.layer, ref.id, f.mask.id, 'path');
  f.doc.schemaVersion = 50;
  f.mask.params.feather = { ...parameter(f.mask, 'feather'), value: 4 };
  const r = evaluate(f.layer, {}, f.doc.imageAssets, f.doc);
  assert.equal(r.error, null);
  assert.ok(r.path.mask.region[0] < 0);
  assert.equal(r.path.children[0].children[0].stroke.width, 12);
  assert.equal(r.path.children[0].children[0].stroke.alpha, 0.5);
  const blocked = maskAppearance(
    { ...r.path, exportBlockers: ['held source'] },
    { ...r.path.mask.shape, exportBlockers: ['held shape'] },
    {
      invert: false,
      prefix: 'check',
      bounds: [
        [0, 40, 80],
        [0, 40, 80],
      ],
      feather: 4,
    },
  );
  assert.deepEqual(blocked.exportBlockers, ['held source', 'held shape']);
});
test('virtual parameter is not retained by failed direct connection or lock/stale rejection', () => {
  const f = featherFixture(),
    before = serialize(f.doc);
  assert.throws(() => connect(f.layer, f.image.id, f.mask.id, 'feather'), /Cannot connect/);
  assert.equal(serialize(f.doc), before);
  const h = session();
  h.send('set-parameter-lock', { key: 'feather', locked: true });
  const locked = serialize(h.s.snapshot());
  assert.throws(() => h.set(2), /locked/);
  assert.equal(serialize(h.s.snapshot()), locked);
  const old = featherFixture();
  old.layer.locked = true;
  const k = session(old);
  assert.throws(() => k.set(0), /locked/);
  assert.equal(k.s.historyView.past.length, 0);
  const invalid = featherFixture(4);
  invalid.doc.schemaVersion = 49;
  assert.throws(() => validate(invalid.doc), /schemaVersion 50/);
});
test('SVG geometry precision does not quantize a positive sigma to zero', () => {
  const f = featherFixture(0.00025);
  assert.match(exportSVG(f.doc, {}, { precision: 2 }), /stdDeviation="0.00025"/);
});
