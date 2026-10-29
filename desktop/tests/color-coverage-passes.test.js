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
  validate,
} from '../src/core.js';
import { colorCoveragePasses as passes } from '../src/color-coverage-passes.js';
import { sampleColorCoverage } from '../src/color-coverage-sampling.js';
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
    { fill: { kind: 'unsupported-gradient' } },
    { filters: [] },
    { blendMode: 'multiply' },
    { contours: [{ group: 1 }] },
  ])
    assert.throws(
      () => passes([...scene, { ...scene[0], value: { ...scene[0].value, ...patch } }], doc),
      /plain solid, two-stop linear\/radial-gradient or four-stop angular-gradient paths/,
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
      /plain solid, two-stop linear\/radial-gradient or four-stop angular-gradient paths/,
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
  assert.equal(fill.slotId, null);
  assert.equal(fill.split, undefined);
  fill = passes(
    scene,
    doc,
    new Map([[scene[0].layer.id, { fill: { startColor: 'first', endColor: 'first' } }]]),
  )[0];
  assert.equal(fill.slotId, 'first');
  assert.equal(fill.split, undefined);
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
    { fill: { kind: 'unsupported-gradient' } },
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

const angularKeys = ['startColor', 'color1', 'color2', 'endColor'];
function angularFixture() {
  const { doc, layer, gradient } = gradientFixture('Angular Gradient Fill');
  doc.colorSlots = angularKeys.map((id, index) => ({
    id,
    name: id,
    value: { hex: '#123456', alpha: index / 3 },
  }));
  angularKeys.forEach((key) => {
    gradient.params[key].colorSlotId = key;
  });
  return { doc, layer, gradient };
}

test('angular four-stop provenance uses evaluated identities and production wedges without mutation', () => {
  const { doc, gradient } = angularFixture();
  gradient.params.center.value = [-20, 40];
  gradient.params.angle.value = -173;
  gradient.params.clockwise.value = false;
  gradient.params.position1.value = 0.12;
  gradient.params.position2.value = 0.84;
  const scene = exportScene(doc),
    before = JSON.stringify(scene),
    saved = serialize(doc);
  assert.deepEqual(
    scene[0].paintProvenance.fill,
    Object.fromEntries(angularKeys.map((key) => [key, key])),
  );
  const [fill, stroke] = passes(scene, doc, new Map([[scene[0].layer.id, { fill: 'wrong' }]]));
  assert.equal(fill.split, undefined);
  assert.equal(fill.slotId, null);
  assert.equal(stroke.paint, 'stroke');
  assert.deepEqual(
    fill.contributions.map((stop) => stop.slotId),
    angularKeys,
  );
  for (const mask of fill.contributions) {
    const fullPaths = [...fill.svg.matchAll(/<path d="([^"]+)"/g)].map((m) => m[1]);
    const maskPaths = [...mask.svg.matchAll(/<path d="([^"]+)"/g)].map((m) => m[1]);
    assert.equal(fullPaths.length, 720);
    assert.deepEqual(maskPaths, fullPaths);
    assert.match(mask.svg, /shape-rendering="crispEdges"/);
    assert.equal(mask.channel, 'premultiplied-red');
    assert.deepEqual(
      [...mask.svg.matchAll(/fill-opacity="([^"]+)"/g)].map((m) => m[1]),
      [...fill.svg.matchAll(/fill-opacity="([^"]+)"/g)].map((m) => m[1]),
      'Every original wedge alpha must survive; changing it changes overlap occlusion',
    );
    assert.equal(mask.svg.match(/<pattern[^>]*>/)[0], fill.svg.match(/<pattern[^>]*>/)[0]);
  }
  assert.equal(JSON.stringify(scene), before);
  assert.equal(serialize(doc), saved);
  assert.deepEqual(
    Object.keys(scene[0].value.fill).sort(),
    [
      'kind',
      'center',
      'angle',
      'clockwise',
      'startColor',
      'color1',
      'position1',
      'color2',
      'position2',
      'endColor',
    ].sort(),
  );
});

test('angular source chains, equal HEX unbound stops, same IDs and replacement remain explicit', () => {
  const { doc, layer, gradient } = angularFixture();
  const color = node('Color');
  color.params.value.colorSlotId = 'color1';
  layer.nodes.push(color);
  delete gradient.params.startColor.colorSlotId;
  connect(layer, color.id, gradient.id, 'startColor');
  delete gradient.params.color2.colorSlotId;
  gradient.params.color2.value = { hex: '#123456', alpha: 2 / 3 };
  gradient.params.endColor.colorSlotId = 'color1';
  const ids = () => passes(exportScene(doc), doc)[0].contributions.map((stop) => stop.slotId);
  assert.deepEqual(ids(), ['color1', 'color1', null, 'color1']);
  const fill = layer.nodes.find((n) => n.type === 'Fill');
  connect(layer, gradient.id, fill.id, 'path');
  connect(layer, fill.id, layer.nodes.find((n) => n.type === 'Output').id, 'path');
  assert.equal(passes(exportScene(doc), doc)[0].contributions, undefined);
  fill.bypassed = true;
  assert.deepEqual(ids(), ['color1', 'color1', null, 'color1']);
  const scene = exportScene(doc);
  delete scene[0].paintProvenance;
  assert.equal(passes(scene, doc)[0].slotId, null);
  assert.equal(passes(scene, doc)[0].contributions, undefined);
});

test('angular identities survive atomic history and native save/reopen', () => {
  const { doc, layer, gradient } = angularFixture();
  const session = new CommandSession(doc);
  const dispatch = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const read = () => exportScene(session.snapshot())[0].paintProvenance.fill;
  dispatch('bind-color-slot', {
    layerId: layer.id,
    nodeId: gradient.id,
    key: 'color2',
    slotId: 'color1',
  });
  assert.equal(read().color2, 'color1');
  dispatch('undo');
  assert.equal(read().color2, 'color2');
  dispatch('redo');
  const saved = serialize(session.snapshot()),
    reopened = deserialize(saved);
  assert.deepEqual(exportScene(reopened)[0].paintProvenance.fill, read());
  passes(exportScene(reopened), reopened);
  assert.equal(serialize(reopened), saved);
});

test('angular exact input bounds and unsupported descriptors reject whole scenes', () => {
  const { doc } = angularFixture();
  const scene = exportScene(doc),
    original = JSON.stringify(scene);
  for (const patch of [
    { center: [100001, 0] },
    { center: [0] },
    { center: [Infinity, 0] },
    { angle: 361 },
    { clockwise: 1 },
    { position1: 0 },
    { position2: 1 },
    { position1: 0.9, position2: 0.1 },
    { stops: [] },
    { matrix: [] },
    { color1: { hex: '#123456', alpha: -1 } },
    { color2: { hex: '#123456', alpha: 2 } },
  ]) {
    const invalid = structuredClone(scene[0]);
    Object.assign(invalid.value.fill, patch);
    assert.throws(() => passes([...scene, invalid], doc));
  }
  for (const identity of ['first', [], { color1: 7 }, { color2: 'invalid id' }]) {
    const invalid = structuredClone(scene[0]);
    invalid.paintProvenance.fill = identity;
    assert.throws(() => passes([...scene, invalid], doc), /provenance/);
  }
  assert.equal(JSON.stringify(scene), original);
});

test('four angular contribution SVGs count toward generation byte budgets before returning passes', () => {
  const { doc } = angularFixture();
  const scene = exportScene(doc);
  for (const nameLength of [4 * 1024 * 1024, Math.ceil(3.5 * 1024 * 1024)]) {
    const invalid = structuredClone(scene[0]);
    invalid.layer.name = 'x'.repeat(nameLength);
    assert.throws(() => passes([invalid], doc), /SVG budget/);
  }
});

test('four identical explicit angular IDs collapse exactly after validation, never by equal HEX', () => {
  const { doc } = angularFixture();
  for (const id of ['startColor', null]) {
    const scene = exportScene(doc);
    scene[0].paintProvenance.fill = Object.fromEntries(angularKeys.map((key) => [key, id]));
    const [fill, stroke] = passes(scene, doc);
    assert.equal(fill.slotId, id);
    assert.equal(fill.contributions, undefined);
    assert.equal(fill.split, undefined);
    assert.equal(stroke.paint, 'stroke');
    assert.equal((fill.svg.match(/<path d=/g) ?? []).length, 720);
    // Optimization never bypasses paint or provenance validation.
    const invalid = structuredClone(scene);
    invalid[0].value.fill.position1 = 0;
    assert.throws(() => passes(invalid, doc));
    scene[0].paintProvenance.fill.endColor = 'invalid id';
    assert.throws(() => passes(scene, doc), /provenance/);
  }
  for (const ids of [
    ['startColor', null, null, null],
    [null, null, null, 'endColor'],
    ['startColor', 'color1', 'color2', 'endColor'],
  ]) {
    const scene = exportScene(doc);
    scene[0].paintProvenance.fill = Object.fromEntries(angularKeys.map((key, i) => [key, ids[i]]));
    const fill = passes(scene, doc)[0];
    assert.equal(fill.slotId, null);
    assert.deepEqual(
      fill.contributions.map((stop) => stop.slotId),
      ids,
    );
  }
});

test('collapsed same-ID angular paint uses one decode and one backdrop occlusion before stroke', async () => {
  const { doc } = angularFixture();
  const scene = exportScene(doc);
  scene[0].paintProvenance.fill = Object.fromEntries(angularKeys.map((key) => [key, 'startColor']));
  const input = passes(scene, doc);
  let calls = 0;
  const result = await sampleColorCoverage(
    ['startColor'],
    input,
    { width: 1, height: 1 },
    (svg) => {
      calls++;
      return [svg === input[0].svg ? 0.5 : 0.25];
    },
  );
  assert.equal(calls, 2, 'one angular full paint and one solid stroke; no auxiliary masks');
  assert.equal(result.slots[0].contribution, 0.375);
  assert.equal(result.other.contribution, 0.25);
  assert.equal(result.paintedPixelEquivalent, 0.625);
});

test('production basis colors preserve stop contribution through overlapping wedge source-over', () => {
  for (const alphas of [
    [1, 1, 1, 1],
    [0, 0.25, 0.75, 1],
  ]) {
    const { doc, gradient } = angularFixture();
    doc.colorSlots.forEach((slot, i) => {
      slot.value.alpha = alphas[i];
    });
    gradient.params.position1.value = 0.17;
    gradient.params.position2.value = 0.73;
    const scene = exportScene(doc),
      fill = passes(scene, doc)[0];
    const wedges = (svg) =>
      [...svg.matchAll(/<path d="[^"]+" fill="(#[0-9a-f]+)" fill-opacity="([^"]+)"\/>/g)].map(
        (m) => ({ red: parseInt(m[1].slice(1, 3), 16) / 255, alpha: Number(m[2]) }),
      );
    const full = wedges(fill.svg),
      basis = fill.contributions.map((stop) => wedges(stop.svg));
    assert.equal(full.length, 720);
    for (const indices of [
      [20, 21],
      [360, 361],
      [0, 719],
    ]) {
      const actual = [0, 0, 0, 0],
        expected = [0, 0, 0, 0];
      let fullAlpha = 0;
      for (const index of indices) {
        const t = (index + 0.5) / 720,
          positions = [0, 0.17, 0.73, 1];
        const right = positions.findIndex((position) => position >= t),
          left = right - 1;
        const fraction = (t - positions[left]) / (positions[right] - positions[left]);
        const masses = [0, 0, 0, 0];
        masses[left] = alphas[left] * (1 - fraction);
        masses[right] = alphas[right] * fraction;
        const alpha = masses.reduce((sum, mass) => sum + mass, 0);
        assert.ok(Math.abs(full[index].alpha - alpha) < 1e-12);
        for (let stop = 0; stop < 4; stop++) {
          assert.equal(basis[stop][index].alpha, full[index].alpha);
          actual[stop] = actual[stop] * (1 - alpha) + basis[stop][index].red * alpha;
          expected[stop] = expected[stop] * (1 - alpha) + masses[stop];
        }
        fullAlpha = fullAlpha * (1 - alpha) + alpha;
      }
      actual.forEach((mass, i) =>
        assert.ok(
          Math.abs(mass - expected[i]) <= 1 / 255,
          'independent premultiplied overlapping-wedge oracle',
        ),
      );
      assert.ok(Math.abs(actual.reduce((sum, mass) => sum + mass, 0) - fullAlpha) <= 2 / 255);
      if (alphas[0] === 0) assert.equal(actual[0], 0);
    }
  }
});

for (const type of ['Linear Gradient Fill', 'Radial Gradient Fill']) {
  test(`${type}: validated identical IDs and Other collapse without changing production paint`, async () => {
    const { doc, gradient } = gradientFixture(type);
    const distinct = passes(exportScene(doc), doc)[0];
    assert.deepEqual([distinct.split.startSlotId, distinct.split.endSlotId], ['first', 'second']);
    for (const id of ['first', null]) {
      const scene = exportScene(doc);
      scene[0].paintProvenance.fill = { startColor: id, endColor: id };
      const before = JSON.stringify(scene);
      const input = passes(scene, doc);
      assert.equal(input[0].slotId, id);
      assert.equal(input[0].split, undefined);
      assert.equal(input[0].contributions, undefined);
      assert.equal(input[0].svg, distinct.svg, 'original alpha and product rendering stay exact');
      assert.equal(JSON.stringify(scene), before);
      let calls = 0;
      const result = await sampleColorCoverage(
        ['first', 'second'],
        input,
        { width: 1, height: 1 },
        (svg) => {
          calls++;
          return [svg === input[0].svg ? 0.5 : 0.25];
        },
      );
      assert.equal(calls, 2, 'one full gradient and one stroke, no redundant weight raster');
      assert.equal(result.slots[0].contribution, id === null ? 0 : 0.375);
      assert.equal(result.other.contribution, id === null ? 0.625 : 0.25);
      assert.equal(result.paintedPixelEquivalent, 0.625);
      assert.equal(result.approximate, true);
      for (const key of ['startColor', 'endColor']) {
        const invalid = structuredClone(scene);
        invalid[0].paintProvenance.fill[key] = 'invalid id';
        assert.throws(() => passes(invalid, doc), /provenance/);
      }
      const invalid = structuredClone(scene);
      if (type === 'Linear Gradient Fill') invalid[0].value.fill.end = invalid[0].value.fill.start;
      else invalid[0].value.fill.radius = 0;
      assert.throws(() => passes(invalid, doc));
    }
    // Authored dangling references still reject before a validated scene exists.
    gradient.params.startColor.colorSlotId = 'missing';
    gradient.params.endColor.colorSlotId = 'missing';
    assert.throws(() => validate(doc), /slot|binding/i);
  });

  test(`${type}: identical-ID collapse removes redundant SVG and sampling budget charges`, async () => {
    const { doc } = gradientFixture(type);
    const scene = exportScene(doc);
    scene[0].value.stroke = undefined;
    // Each full SVG is below 4 MiB, but five full + split SVGs exceed 16 MiB.
    scene[0].layer.name = 'x'.repeat(1800000);
    assert.throws(() => passes(Array(5).fill(scene[0]), doc), /SVG budget/);
    scene[0].paintProvenance.fill = { startColor: 'first', endColor: 'first' };
    const input = passes(Array(5).fill(scene[0]), doc);
    assert.equal(input.length, 5);
    assert.ok(input.every((paint) => !paint.split));
    // 33 full masks fit 4 Mi samples; 66 full + weight masks would not.
    scene[0].layer.name = 'small';
    const many = passes(Array(33).fill(scene[0]), doc);
    let calls = 0;
    await sampleColorCoverage(
      ['first', 'second'],
      many,
      { width: 256, height: 256 },
      (_svg, size) => {
        calls++;
        return new Float32Array(size.width * size.height);
      },
    );
    assert.equal(calls, 33);
    scene[0].paintProvenance.fill.endColor = 'second';
    calls = 0;
    await assert.rejects(
      sampleColorCoverage(
        ['first', 'second'],
        passes(Array(33).fill(scene[0]), doc),
        { width: 256, height: 256 },
        () => {
          calls++;
        },
      ),
      /sampling budget/,
    );
    assert.equal(
      calls,
      0,
      'distinct IDs still require masks and preflight rejects before decoding',
    );
  });
}
