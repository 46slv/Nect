import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  exportScene,
  CommandSession,
  serialize,
  deserialize,
} from '../src/core.js';
import { colorCoveragePasses as passes } from '../src/color-coverage-passes.js';
function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    stroke = node('Stroke'),
    output = layer.nodes[0];
  layer.nodes = [shape, fill, stroke, output];
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, stroke.id, 'path');
  connect(layer, stroke.id, output.id, 'path');
  return { doc, layer, scene: exportScene(doc) };
}
test('actual evaluated renderer emits fill then stroke without mutating artwork', () => {
  const { doc, layer, scene } = fixture(),
    before = JSON.stringify(scene);
  const result = passes(scene, doc, new Map([[layer.id, { fill: 'c1', stroke: 'c2' }]]));
  assert.deepEqual(
    result.map((p) => [p.paint, p.slotId]),
    [
      ['fill', 'c1'],
      ['stroke', 'c2'],
    ],
  );
  assert.match(result[0].svg, /<path/);
  assert.doesNotMatch(result[0].svg, / stroke=/);
  assert.match(result[1].svg, /fill="none"/);
  assert.match(result[1].svg, / stroke=/);
  assert.match(result[0].svg, /overflow="hidden"/);
  assert.equal(JSON.stringify(scene), before);
});
test('unbound paints are Other even when their display colors match', () => {
  const { doc, scene } = fixture();
  assert.deepEqual(
    passes(scene, doc).map((p) => p.slotId),
    [null, null],
  );
});
test('unsupported geometry rejects whole measurement rather than partial coverage', () => {
  const { doc, scene } = fixture();
  for (const patch of [
    { children: [] },
    { mask: {} },
    { kind: 'image' },
    { fill: { kind: 'angular-gradient' } },
    { filters: [] },
    { blendMode: 'multiply' },
    { contours: [{ group: 1 }] },
  ])
    assert.throws(
      () => passes([...scene, { ...scene[0], value: { ...scene[0].value, ...patch } }], doc),
      /plain solid or two-stop linear\/radial-gradient paths/,
    );
  assert.throws(() => passes(Array(129).fill(scene[0]), doc), /budget/);
  assert.throws(() => passes(scene, { width: NaN, height: 1 }), /bounds/);
});

test('scene order and paint alpha survive the production renderer bridge', () => {
  const a = fixture(),
    b = fixture();
  a.scene[0].value.fill.alpha = 0.25;
  a.scene[0].value.stroke.alpha = 0.5;
  const result = passes([...a.scene, ...b.scene], a.doc);
  assert.deepEqual(
    result.map((p) => p.layerId),
    [a.layer.id, a.layer.id, b.layer.id, b.layer.id],
  );
  assert.match(result[0].svg, /fill-opacity="0.25"/);
  assert.match(result[1].svg, /stroke-opacity="0.5"/);
  const reverse = passes([...b.scene, ...a.scene], a.doc);
  assert.equal(reverse[0].layerId, b.layer.id);
});

test('repeater appearance is never flattened into independent child alpha', () => {
  const { doc, scene } = fixture();
  for (const field of ['repeaterOpacity', 'repeaterFill', 'repeaterStroke']) {
    const value = structuredClone(scene[0].value);
    value.contours[0][field] = field === 'repeaterOpacity' ? 0.5 : { hex: '#ffffff', alpha: 1 };
    assert.throws(
      () => passes([{ ...scene[0], value }], doc),
      /plain solid or two-stop linear\/radial-gradient paths/,
    );
  }
});

test('authored slots reach render passes without caller identity guesses', () => {
  const { doc, layer } = fixture();
  doc.schemaVersion = 54;
  doc.colorSlots = ['c1', 'c2'].map((id) => ({
    id,
    name: id,
    value: { hex: '#123456', alpha: 0.5 },
  }));
  layer.nodes.find((n) => n.type === 'Fill').params.color.colorSlotId = 'c1';
  layer.nodes.find((n) => n.type === 'Stroke').params.color.colorSlotId = 'c2';
  const scene = exportScene(doc);
  assert.deepEqual(scene[0].paintProvenance, { fill: 'c1', stroke: 'c2' });
  assert.deepEqual(
    passes(scene, doc).map((p) => p.slotId),
    ['c1', 'c2'],
  );
  assert.deepEqual(Object.keys(scene[0].value.fill).sort(), ['alpha', 'hex']);
  assert.equal(Object.hasOwn(scene[0].value, 'paintProvenance'), false);
});

test('Color-node source chains preserve exact identity and literal replacement clears it', () => {
  const { doc, layer } = fixture();
  doc.schemaVersion = 54;
  doc.colorSlots = [{ id: 'brand', name: 'Brand', value: { hex: '#123456', alpha: 1 } }];
  const color = node('Color');
  color.params.value.colorSlotId = 'brand';
  layer.nodes.push(color);
  const fill = layer.nodes.find((n) => n.type === 'Fill'),
    output = layer.nodes.find((n) => n.type === 'Output');
  connect(layer, color.id, fill.id, 'color');
  assert.equal(passes(exportScene(doc), doc)[0].slotId, 'brand');
  const literal = node('Fill');
  literal.params.color.value = { hex: '#123456', alpha: 1 };
  layer.nodes.push(literal);
  connect(layer, output.params.path.source, literal.id, 'path');
  connect(layer, literal.id, output.id, 'path');
  assert.equal(passes(exportScene(doc), doc)[0].slotId, null);
});

function gradientFixture(type = 'Linear Gradient Fill') {
  const { doc, layer } = fixture();
  const gradient = node(type);
  const shape = layer.nodes.find((n) => n.type === 'Rectangle');
  const stroke = layer.nodes.find((n) => n.type === 'Stroke');
  layer.nodes.push(gradient);
  connect(layer, shape.id, gradient.id, 'path');
  connect(layer, gradient.id, stroke.id, 'path');
  doc.schemaVersion = 54;
  doc.colorSlots = ['first', 'second'].map((id) => ({
    id,
    name: id,
    value: { hex: '#123456', alpha: 0.5 },
  }));
  gradient.params.startColor.colorSlotId = 'first';
  gradient.params.endColor.colorSlotId = 'second';
  return { doc, layer, gradient };
}

test('two-stop linear gradients preserve stable stop IDs, production alpha and one paint order', () => {
  const { doc, gradient } = gradientFixture();
  gradient.params.end.value = [125, 80];
  const scene = exportScene(doc),
    before = JSON.stringify(scene);
  const [fill, stroke] = passes(scene, doc);
  assert.deepEqual(scene[0].paintProvenance.fill, { startColor: 'first', endColor: 'second' });
  assert.equal(fill.paint, 'fill');
  assert.equal(fill.slotId, null);
  assert.deepEqual([fill.split.startSlotId, fill.split.endSlotId], ['first', 'second']);
  assert.equal(stroke.paint, 'stroke');
  assert.match(fill.svg, /stop-opacity="0.5"/);
  assert.doesNotMatch(fill.svg, /<mask/);
  assert.match(fill.split.startSvg, /<mask id="coverage-start-mask"/);
  assert.match(fill.split.startSvg, /mask="url\(#coverage-start-mask\)"/);
  assert.match(fill.split.startSvg, /color-interpolation="sRGB"/);
  assert.equal(JSON.stringify(scene), before);
  assert.equal(Object.hasOwn(scene[0].value.fill, 'startSlotId'), false);
  // No external alias map can replace evaluator-owned stop identities.
  assert.equal(
    passes(scene, doc, new Map([[scene[0].layer.id, { fill: 'wrong' }]]))[0].split.startSlotId,
    'first',
  );
  fill.split.startSlotId = 'changed';
  scene[0].paintProvenance.fill.startColor = 'changed';
  assert.equal(exportScene(doc)[0].paintProvenance.fill.startColor, 'first');
});

for (const type of ['Linear Gradient Fill', 'Radial Gradient Fill']) {
  test(`${type}: source chains, unbound equal-HEX stops, replacement and bypass do not retarget IDs`, () => {
    const { doc, layer, gradient } = gradientFixture(type);
    const color = node('Color');
    color.params.value.colorSlotId = 'first';
    layer.nodes.push(color);
    delete gradient.params.startColor.colorSlotId;
    connect(layer, color.id, gradient.id, 'startColor');
    delete gradient.params.endColor.colorSlotId;
    gradient.params.endColor.value = { hex: '#123456', alpha: 0.5 };
    assert.deepEqual(exportScene(doc)[0].paintProvenance.fill, {
      startColor: 'first',
      endColor: null,
    });
    const fill = layer.nodes.find((n) => n.type === 'Fill');
    connect(layer, gradient.id, fill.id, 'path');
    connect(layer, fill.id, layer.nodes.find((n) => n.type === 'Output').id, 'path');
    assert.equal(passes(exportScene(doc), doc)[0].split, undefined);
    assert.equal(passes(exportScene(doc), doc)[0].slotId, null);
    fill.bypassed = true;
    assert.equal(passes(exportScene(doc), doc)[0].split.startSlotId, 'first');
  });

  test(`${type}: stop identity survives atomic binding history and native save/reopen without authored mutation`, () => {
    const { doc, layer, gradient } = gradientFixture(type);
    const session = new CommandSession(doc);
    const read = () => exportScene(session.snapshot())[0].paintProvenance.fill;
    const dispatch = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
    dispatch('bind-color-slot', {
      layerId: layer.id,
      nodeId: gradient.id,
      key: 'endColor',
      slotId: 'first',
    });
    assert.deepEqual(read(), { startColor: 'first', endColor: 'first' });
    dispatch('undo');
    assert.deepEqual(read(), { startColor: 'first', endColor: 'second' });
    dispatch('redo');
    const saved = serialize(session.snapshot());
    const reopened = deserialize(saved);
    assert.deepEqual(exportScene(reopened)[0].paintProvenance.fill, read());
    passes(exportScene(reopened), reopened);
    assert.equal(serialize(reopened), saved);
  });
}

test('malformed gradients, stop provenance and unsupported appearances reject the entire scene', () => {
  const { doc } = gradientFixture();
  const scene = exportScene(doc);
  for (const identity of ['first', [], { startColor: 'invalid id' }, { endColor: 7 }]) {
    const modified = structuredClone(scene);
    modified[0].paintProvenance.fill = identity;
    assert.throws(() => passes(modified, doc), /provenance/);
  }
  const modified = structuredClone(scene);
  modified[0].value.fill.end = modified[0].value.fill.start;
  assert.throws(() => passes(modified, doc), /nonzero distance/);
  for (const key of ['mask', 'filter', 'opacity', 'matrix']) {
    const value = { ...scene[0].value, [key]: {} };
    assert.throws(() => passes([...scene, { ...scene[0], value }], doc), /requires plain/);
  }
});

test('radial coverage uses production concentric-circle mask and preserves scene and explicit IDs', () => {
  const { doc, gradient } = gradientFixture('Radial Gradient Fill');
  gradient.params.center.value = [-30, 40];
  gradient.params.radius.value = 12.5;
  const scene = exportScene(doc),
    before = JSON.stringify(scene);
  const [fill, stroke] = passes(scene, doc);
  assert.deepEqual(scene[0].paintProvenance.fill, { startColor: 'first', endColor: 'second' });
  assert.deepEqual([fill.split.startSlotId, fill.split.endSlotId], ['first', 'second']);
  assert.equal(fill.slotId, null);
  assert.equal(stroke.paint, 'stroke');
  assert.match(fill.svg, /<radialGradient[^>]+cx="-30" cy="40" r="12.5" fx="-30" fy="40" fr="0"/);
  assert.match(fill.svg, /spreadMethod="pad" color-interpolation="sRGB"/);
  assert.match(fill.svg, /stop-opacity="0.5"/);
  assert.doesNotMatch(fill.svg, /<mask/);
  assert.match(
    fill.split.startSvg,
    /<radialGradient id="coverage-start-weight"[^>]+cx="-30" cy="40" r="12.5"/,
  );
  assert.match(fill.split.startSvg, /offset="0" stop-color="#ffffff" stop-opacity="1"/);
  assert.match(fill.split.startSvg, /offset="1" stop-color="#ffffff" stop-opacity="0"/);
  assert.match(fill.split.startSvg, /mask="url\(#coverage-start-mask\)"/);
  assert.equal(JSON.stringify(scene), before);
  assert.equal(Object.hasOwn(scene[0].value.fill, 'startSlotId'), false);
  assert.equal(
    passes(scene, doc, new Map([[scene[0].layer.id, { fill: 'wrong' }]]))[0].split.startSlotId,
    'first',
  );
  fill.split.startSlotId = 'changed';
  scene[0].paintProvenance.fill.startColor = 'changed';
  assert.equal(exportScene(doc)[0].paintProvenance.fill.startColor, 'first');
});

test('radial stop provenance is explicit, optional and never inferred from equal HEX', () => {
  const { doc } = gradientFixture('Radial Gradient Fill');
  const scene = exportScene(doc);
  delete scene[0].paintProvenance;
  let fill = passes(scene, doc)[0];
  assert.deepEqual([fill.split.startSlotId, fill.split.endSlotId], [null, null]);
  fill = passes(
    scene,
    doc,
    new Map([[scene[0].layer.id, { fill: { startColor: 'first', endColor: 'first' } }]]),
  )[0];
  assert.deepEqual([fill.split.startSlotId, fill.split.endSlotId], ['first', 'first']);
  fill = passes(scene, doc, new Map([[scene[0].layer.id, { fill: { endColor: 'second' } }]]))[0];
  assert.deepEqual([fill.split.startSlotId, fill.split.endSlotId], [null, 'second']);
});

test('radial attribution rejects malformed or expanded descriptors and unsupported scenes atomically', () => {
  const { doc } = gradientFixture('Radial Gradient Fill');
  const scene = exportScene(doc);
  const original = JSON.stringify(scene);
  for (const patch of [
    { radius: 0 },
    { radius: 0.00001 },
    { radius: 100001 },
    { radius: NaN },
    { center: [Infinity, 0] },
    { center: [0] },
    { center: [100001, 0] },
    { focal: [0, 0] },
    { fx: 1 },
    { fr: 1 },
    { matrix: [2, 0, 0, 1, 0, 0] },
    { stops: [] },
    { spreadMethod: 'repeat' },
    { startColor: { hex: '#123456', alpha: 2 } },
  ]) {
    const invalid = structuredClone(scene[0]);
    Object.assign(invalid.value.fill, patch);
    assert.throws(() => passes([...scene, invalid], doc));
  }
  for (const identity of ['first', [], { startColor: 'invalid id' }, { endColor: 7 }]) {
    const invalid = structuredClone(scene[0]);
    invalid.paintProvenance.fill = identity;
    assert.throws(() => passes([...scene, invalid], doc), /provenance/);
  }
  for (const patch of [
    { fill: { kind: 'angular-gradient' } },
    { stroke: { kind: 'radial-gradient' } },
    { mask: {} },
    { filter: {} },
    { opacity: 0.5 },
    { matrix: [] },
    { children: [] },
    { contours: [{ group: 1 }] },
    { contours: [{ repeaterOpacity: 0.5 }] },
  ]) {
    const invalid = { ...scene[0], value: { ...scene[0].value, ...patch } };
    assert.throws(() => passes([...scene, invalid], doc), /requires plain/);
  }
  assert.equal(JSON.stringify(scene), original);
});
