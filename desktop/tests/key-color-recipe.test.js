import test from 'node:test';
import assert from 'node:assert/strict';
import {
  KEY_COLOR_RECIPE_VERSION,
  KEY_COLOR_SOLVER_VERSION,
  createKeyColorRecipe,
  editKeyColorRecipe,
  restoreKeyColorRecipe,
  keyColorSlotToLinear,
} from '../src/key-color-recipe.js';
import { MAX_CHROMA, proposeAutoChroma } from '../src/auto-chroma.js';

const plan = (id, role, position, patch = {}) => ({
  id,
  role,
  position,
  lightnessScale: 0.7,
  lightnessOffset: 0.15,
  chromaScale: 0.8,
  chromaOffset: 0,
  ...patch,
});
const input = () => ({
  paletteId: 'palette_brand',
  keyColor: { id: 'brand', role: 'Identity', value: { hex: '#aBcD09', alpha: 0.123456789 } },
  preset: {
    id: 'explicit_test_plan',
    version: 1,
    harmony: 'triadic',
    hueSteps: 12,
    slots: [
      plan('surface', 'Base', 1),
      plan('detail', 'Accent', 2, { alpha: 0.27 }),
      plan('neutral', 'Neutral', 0, { chromaScale: 0 }),
    ],
  },
  slots: [
    { id: 'surface', role: 'Base', deltaL: 0, deltaRatioCmax: 0 },
    { id: 'detail', role: 'Accent', deltaL: 0.07, deltaRatioCmax: 0.1 },
    { id: 'neutral', role: 'Neutral', deltaL: 0, deltaRatioCmax: 0 },
  ],
});
const row = (state, id) => state.resolved.find((entry) => entry.id === id);
const slot = (state, id) => state.intent.slots.find((entry) => entry.id === id);
const edit = (state, change) =>
  editKeyColorRecipe(state, { expectedRevision: state.revision, ...change });
const close = (actual, expected, tolerance = 1e-10) =>
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} != ${expected}`);
function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

test('C1 keeps the exact input literal and alpha, not a newly generated tone', () => {
  const source = input(),
    state = createKeyColorRecipe(source);
  assert.equal(state.version, KEY_COLOR_RECIPE_VERSION);
  assert.equal(state.solverVersion, KEY_COLOR_SOLVER_VERSION);
  assert.deepEqual(row(state, 'brand').value, source.keyColor.value);
  assert.deepEqual(state.intent.keyColor.value, source.keyColor.value);
  assert.equal(state.intent.keyColor.role, 'Identity'); // Identity is not forced to be Base.
  assert.equal(Object.hasOwn(state.intent.keyColor, 'deltaL'), false);
  assert.equal(row(state, 'surface').value.alpha, 1);
  assert.equal(row(state, 'detail').value.alpha, 0.27);
  assert.deepEqual(state.diagnostics, ['coverage-unavailable-unweighted']);
});

test('creation and explicit recomputation are deterministic and detached', () => {
  const source = freeze(input()),
    before = structuredClone(source);
  const a = createKeyColorRecipe(source),
    b = createKeyColorRecipe(source);
  assert.deepEqual(a, b);
  assert.deepEqual(source, before);
  a.intent.slots[0].deltaL = 99;
  a.resolved[0].value.hex = '#000000';
  assert.deepEqual(createKeyColorRecipe(source), b);
  const committed = freeze(b),
    next = edit(committed, { type: 'recompute' });
  assert.equal(next.revision, committed.revision + 1);
  assert.deepEqual(next.resolved, committed.resolved);
  assert.deepEqual(next.intent, committed.intent);
});

test('the four existing harmony relationships derive hue relative to C1', () => {
  const offsets = {
    triadic: [120, 240],
    complementary: [180, 180],
    analogous: [30, 330],
    split: [150, 210],
  };
  for (const [harmony, expected] of Object.entries(offsets)) {
    const source = input();
    source.preset.harmony = harmony;
    const state = createKeyColorRecipe(source),
      h = row(state, 'brand').h;
    for (const [index, id] of ['surface', 'detail'].entries())
      close((row(state, id).h - h + 360) % 360, expected[index]);
  }
});

test('a key-color edit recomputes every active derived slot and reapplies all deltas', () => {
  const state = freeze(createKeyColorRecipe(input()));
  const next = edit(state, { type: 'key-color', value: { hex: '#4477EE', alpha: 0.91 } });
  assert.deepEqual(row(next, 'brand').value, { hex: '#4477EE', alpha: 0.91 });
  assert.deepEqual(next.intent.slots, state.intent.slots);
  for (const id of ['surface', 'detail', 'neutral']) {
    assert.notEqual(row(next, id).h, row(state, id).h);
    assert.notEqual(row(next, id).baseL, row(state, id).baseL);
    close(row(next, id).L, row(next, id).baseL + slot(next, id).deltaL);
  }
  assert.notEqual(row(next, 'surface').baseRatioCmax, row(state, 'surface').baseRatioCmax);
  assert.equal(row(next, 'surface').value.alpha, 1);
  assert.equal(row(next, 'detail').value.alpha, 0.27);
});

test('editing a derived slot only changes that slot; hue and all other colors stay exact', () => {
  const state = freeze(createKeyColorRecipe(input())),
    before = structuredClone(state);
  const next = edit(state, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaL: -0.1,
    deltaRatioCmax: 0.16,
  });
  assert.notDeepEqual(row(next, 'surface').value, row(state, 'surface').value);
  assert.equal(row(next, 'surface').h, row(state, 'surface').h);
  for (const id of ['brand', 'detail', 'neutral']) {
    assert.deepEqual(row(next, id), row(state, id));
    if (id !== 'brand') assert.deepEqual(slot(next, id), slot(state, id));
  }
  assert.deepEqual(state, before);
  next.resolved[1].value.alpha = 0;
  assert.deepEqual(state, before);
});

test('additive chroma deltas recover color from a zero-chroma base', () => {
  const state = createKeyColorRecipe(input());
  assert.equal(row(state, 'neutral').C, 0);
  assert.equal(row(state, 'neutral').baseRatioCmax, 0);
  const next = edit(state, { type: 'slot-delta', slotId: 'neutral', deltaRatioCmax: 0.6 });
  assert.ok(row(next, 'neutral').C > 0);
  close(row(next, 'neutral').C, row(next, 'neutral').cmax * 0.6);
  assert.equal(slot(next, 'neutral').deltaRatioCmax, 0.6);
});

test('effective L and ratio clamp, but arbitrarily large finite intent deltas survive', () => {
  const state = createKeyColorRecipe(input());
  for (const sign of [-1, 1]) {
    const next = edit(state, {
      type: 'slot-delta',
      slotId: 'surface',
      deltaL: sign * 5,
      deltaRatioCmax: sign * 8,
    });
    assert.equal(slot(next, 'surface').deltaL, sign * 5);
    assert.equal(slot(next, 'surface').deltaRatioCmax, sign * 8);
    const sample = row(next, 'surface');
    assert.equal(sample.L, sign < 0 ? 0 : 1);
    assert.equal(sample.effectiveRatioCmax, sign < 0 ? 0 : 1);
    assert.deepEqual(sample.diagnostics, ['lightness-clamped', 'ratio-clamped', 'zero-cmax']);
    assert.equal(sample.C, 0);
    const moved = edit(next, { type: 'key-color', value: { hex: '#114488', alpha: 0.4 } });
    assert.deepEqual(moved.intent.slots, next.intent.slots);
    assert.deepEqual(restoreKeyColorRecipe(JSON.parse(JSON.stringify(next))), next);
  }
});

test('reducing a formerly clamped delta recovers chroma without losing the previous request', () => {
  const state = createKeyColorRecipe(input());
  const clipped = edit(state, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaL: 3,
    deltaRatioCmax: 4,
  });
  const recovered = edit(clipped, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaL: 0,
    deltaRatioCmax: 0.2,
  });
  assert.ok(row(recovered, 'surface').C > 0);
  assert.ok(row(recovered, 'surface').L < 1);
  assert.deepEqual(row(recovered, 'surface').diagnostics, []);
  assert.equal(slot(clipped, 'surface').deltaRatioCmax, 4);
});

test('black, white and gray C1 have exact neutral chroma and a stable persisted hue policy', () => {
  for (const hex of ['#000000', '#FFFFFF', '#808080']) {
    const source = input();
    source.keyColor = { ...source.keyColor, value: { hex, alpha: 0.25 }, hueHint: 41 };
    const state = createKeyColorRecipe(source),
      key = row(state, 'brand');
    assert.equal(key.h, 41);
    assert.equal(key.C, 0);
    assert.equal(key.effectiveRatioCmax, 0);
    assert.deepEqual(key.value, source.keyColor.value);
    assert.ok(key.diagnostics.includes('achromatic-hue-hint'));
    if (hex !== '#808080') {
      assert.equal(key.L, hex === '#000000' ? 0 : 1);
      assert.equal(key.cmax, 0);
      assert.ok(key.diagnostics.includes('zero-cmax'));
    }
    assert.deepEqual(restoreKeyColorRecipe(JSON.parse(JSON.stringify(state))), state);
  }
  const neutral = input();
  neutral.keyColor.value.hex = '#808080';
  assert.equal(row(createKeyColorRecipe(neutral), 'brand').h, 0);
  const colored = createKeyColorRecipe(input());
  const gray = edit(colored, { type: 'key-color', value: { hex: '#777777', alpha: 1 } });
  assert.equal(row(gray, 'brand').h, row(colored, 'brand').h);
  const black = edit(gray, { type: 'key-color', value: { hex: '#000000', alpha: 0 } });
  assert.equal(row(black, 'brand').h, row(colored, 'brand').h);
});

test('gray input does not prevent an explicit positive derived chroma delta', () => {
  const source = input();
  source.keyColor.value.hex = '#808080';
  const state = createKeyColorRecipe(source);
  assert.equal(row(state, 'surface').C, 0);
  const next = edit(state, { type: 'slot-delta', slotId: 'surface', deltaRatioCmax: 0.4 });
  assert.ok(row(next, 'surface').C > 0);
  assert.equal(row(next, 'brand').value.hex, '#808080');
});

test('preset switches use stable IDs and preserve unused referenced slots, roles and deltas', () => {
  const state = createKeyColorRecipe(input());
  const preset = {
    ...state.intent.preset,
    version: 2,
    harmony: 'complementary',
    slots: [state.intent.preset.slots[1]],
  };
  const next = edit(state, { type: 'preset', preset });
  assert.deepEqual(next.intent.slots, state.intent.slots);
  assert.deepEqual(
    next.resolved.map((entry) => entry.id),
    state.resolved.map((entry) => entry.id),
  );
  for (const id of ['surface', 'neutral']) {
    assert.equal(row(next, id).active, false);
    assert.deepEqual({ ...row(next, id), active: true }, row(state, id));
    assert.deepEqual(keyColorSlotToLinear(next, id).value, keyColorSlotToLinear(state, id).value);
  }
  assert.equal(row(next, 'detail').role, 'Accent');
  assert.notEqual(row(next, 'detail').h, row(state, 'detail').h);
  const keyChanged = edit(next, { type: 'key-color', value: { hex: '#7755AA', alpha: 0.4 } });
  assert.deepEqual(row(keyChanged, 'surface'), row(next, 'surface'));
  const restored = edit(keyChanged, { type: 'preset', preset: state.intent.preset });
  assert.equal(row(restored, 'surface').active, true);
  assert.deepEqual(restored.intent.slots, state.intent.slots);
});

test('reordering explicit mappings never retargets stable slots', () => {
  const source = input(),
    state = createKeyColorRecipe(source);
  source.preset.slots.reverse();
  source.slots.reverse();
  const reordered = createKeyColorRecipe(source);
  for (const id of ['brand', 'surface', 'detail', 'neutral'])
    assert.deepEqual(row(reordered, id), row(state, id));
});

test('invalid and stale edits cannot partially commit intent, colors or revision', () => {
  const state = freeze(createKeyColorRecipe(input())),
    before = structuredClone(state);
  const invalid = [
    { type: 'slot-delta', slotId: 'surface', deltaL: NaN },
    { type: 'slot-delta', slotId: 'surface', deltaRatioCmax: Infinity },
    { type: 'slot-delta', slotId: 'brand', deltaL: 0.2 },
    { type: 'slot-delta', slotId: 'missing', deltaL: 0.2 },
    { type: 'slot-delta', slotId: 'surface', h: 25 },
    { type: 'slot-delta', slotId: 'surface' },
    { type: 'key-color', value: { hex: '#12345', alpha: 1 } },
    { type: 'key-color', value: { hex: '#123456', alpha: -1 } },
    { type: 'key-color', value: { hex: '#123456', alpha: 1 }, deltaL: 1 },
    { type: 'recompute', expectedRevision: -1 },
    { type: 'unknown' },
  ];
  for (const change of invalid) {
    assert.throws(() => edit(state, change));
    assert.deepEqual(state, before);
  }
  const overflow = structuredClone(state.intent.preset);
  overflow.slots[0].lightnessOffset = Number.MAX_VALUE;
  const large = edit(state, { type: 'slot-delta', slotId: 'surface', deltaL: Number.MAX_VALUE });
  assert.throws(() => edit(large, { type: 'preset', preset: overflow }), /Nonfinite/);
  assert.deepEqual(state, before);
});

test('preset role retargeting, dangling IDs and invalid harmony reject atomically', () => {
  const state = freeze(createKeyColorRecipe(input())),
    before = structuredClone(state);
  for (const change of [
    (p) => (p.slots[0].role = 'Accent'),
    (p) => (p.slots[0].id = 'missing'),
    (p) => (p.slots[0].id = 'brand'),
    (p) => p.slots.push(p.slots[0]),
    (p) => (p.slots[0].position = 3),
    (p) => (p.slots[0].alpha = 1.1),
    (p) => (p.hueSteps = 11),
  ]) {
    const preset = structuredClone(state.intent.preset);
    change(preset);
    assert.throws(() => edit(state, { type: 'preset', preset }));
    assert.deepEqual(state, before);
  }
});

test('save/reload restores last committed floats and literals without regeneration', () => {
  let state = createKeyColorRecipe(input());
  state = edit(state, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaL: 0.13,
    deltaRatioCmax: 0.17,
  });
  const saved = JSON.parse(JSON.stringify(state)),
    restored = restoreKeyColorRecipe(saved);
  assert.deepEqual(restored, state);
  assert.notStrictEqual(restored.intent, saved.intent);
  assert.notStrictEqual(restored.resolved, saved.resolved);
  // The restore path validates the committed snapshot, not current solver output.
  // A snapshot can retain output calculated in a different evaluation context.
  saved.resolved[1].value.hex = '#123456';
  saved.resolved[1].linear = [0.01, 0.02, 0.03, saved.resolved[1].value.alpha];
  assert.deepEqual(restoreKeyColorRecipe(saved).resolved, saved.resolved);
  assert.equal(restoreKeyColorRecipe(saved).revision, saved.revision);
});

test('linear adapter depends on palette/revision/stable ID/semantics, not rounded HEX', () => {
  const state = createKeyColorRecipe(input()),
    linear = keyColorSlotToLinear(state, 'surface');
  assert.deepEqual(linear, {
    paletteId: state.paletteId,
    paletteRevision: state.revision,
    slotId: 'surface',
    colorSemanticsVersion: state.colorSemanticsVersion,
    type: 'ColorF32V1',
    space: 'linear-srgb',
    alphaMode: 'straight',
    value: row(state, 'surface').linear.map(Math.fround),
  });
  const decode = (v) => (v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4);
  const quantized = [1, 3, 5].map((i) =>
    Math.fround(decode(parseInt(row(state, 'surface').value.hex.slice(i, i + 2), 16) / 255)),
  );
  assert.notDeepEqual(linear.value.slice(0, 3), quantized);
  assert.equal(keyColorSlotToLinear(state, 'detail').value[3], Math.fround(0.27));
  assert.equal(linear.alphaMode, 'straight');
  const next = edit(state, { type: 'slot-delta', slotId: 'detail', deltaL: -0.04 });
  assert.equal(keyColorSlotToLinear(next, 'surface').paletteRevision, state.revision + 1);
  assert.deepEqual(keyColorSlotToLinear(next, 'surface').value, linear.value);
  assert.throws(() => keyColorSlotToLinear(state, 'unknown'));
});

test('resolved chroma reuses the existing gamut engine without changing hue', () => {
  for (const hex of ['#ff0000', '#00ff00', '#0000ff', '#ffff00', '#00ffff', '#ff00ff']) {
    for (const harmony of ['triadic', 'complementary', 'analogous', 'split']) {
      const source = input();
      source.keyColor.value.hex = hex;
      source.preset.harmony = harmony;
      const state = createKeyColorRecipe(source);
      for (const sample of state.resolved.slice(1)) {
        const existing = proposeAutoChroma(
          [
            {
              id: sample.id,
              role: 'Primary',
              L: sample.L,
              h: sample.h,
              overrideC: sample.cmax * sample.effectiveRatioCmax,
              alpha: sample.value.alpha,
            },
          ],
          { areaAware: false },
        )[0];
        assert.deepEqual(sample.value, existing.value);
        assert.equal(sample.C, existing.C);
        assert.ok(sample.linear.every((n) => n >= 0 && n <= 1));
        assert.ok(sample.C <= sample.cmax + 1e-9);
      }
    }
  }
});

test('legacy inputs are rejected without conversion, lock removal or mutation', () => {
  const legacy = freeze({
    version: 2,
    lockedSlotIds: ['old'],
    palette: [{ id: 'old', kind: 'neutral', L: 0.7, overrideC: 0.2 }],
    wheel: { harmony: 'triadic' },
  });
  const before = structuredClone(legacy);
  assert.throws(() => createKeyColorRecipe(legacy));
  assert.throws(() => restoreKeyColorRecipe(legacy));
  assert.deepEqual(legacy, before);
});

test('closed prototype validation rejects offsets on C1, bad snapshots and unmapped initial slots', () => {
  const source = input();
  source.keyColor.deltaL = 0;
  assert.throws(() => createKeyColorRecipe(source));
  const missing = input();
  missing.preset.slots.pop();
  assert.throws(() => createKeyColorRecipe(missing), /explicit preset mapping/);
  const state = createKeyColorRecipe(input());
  for (const mutate of [
    (s) => (s.version = 99),
    (s) => s.resolved.pop(),
    (s) => (s.resolved[1].linear[0] = NaN),
    (s) => (s.resolved[1].role = 'wrong'),
    (s) => (s.resolved[0].value.alpha = 0),
    (s) => (s.intent.slots[0].id = 'brand'),
    (s) => (s.revision = Number.MAX_SAFE_INTEGER + 1),
  ]) {
    const saved = structuredClone(state);
    mutate(saved);
    assert.throws(() => restoreKeyColorRecipe(saved));
  }
});

test('explicit transparent preset alpha and disabled area reflection remain exact', () => {
  const source = input();
  source.keyColor.value.alpha = 0;
  source.preset.slots[0].alpha = 0;
  source.settings = { areaAware: false };
  const state = createKeyColorRecipe(source);
  assert.equal(row(state, 'brand').value.alpha, 0);
  assert.equal(row(state, 'surface').value.alpha, 0);
  assert.equal(row(state, 'detail').value.alpha, 0.27);
  assert.deepEqual(state.diagnostics, []);
  const next = edit(state, { type: 'slot-delta', slotId: 'surface', deltaL: 0.2 });
  assert.equal(keyColorSlotToLinear(next, 'surface').value[3], 0);
});

test('disconnected blue gamut gap reduces resolved C and diagnoses without losing the ratio delta', () => {
  const source = input();
  source.keyColor.value.hex = '#0000ff';
  source.preset.slots[0] = plan('surface', 'Base', 0, {
    lightnessScale: 1,
    lightnessOffset: 0,
    chromaScale: 0,
    chromaOffset: 0,
  });
  const state = createKeyColorRecipe(source);
  const next = edit(state, { type: 'slot-delta', slotId: 'surface', deltaRatioCmax: 0.9 });
  const sample = row(next, 'surface');
  assert.equal(MAX_CHROMA, 0.45); // Existing gamut profile is not changed.
  close(sample.cmax, 0.3132144087901359);
  close(sample.C, 0.26559335414277563);
  assert.ok(sample.cmax * sample.rTarget > 0.26559335414277563);
  assert.ok(sample.cmax * sample.rTarget < 0.31321265627693484);
  assert.equal(sample.rTarget, 0.9);
  close(sample.rRealized, 0.8479602045406922);
  assert.equal(sample.effectiveRatioCmax, sample.rTarget); // Deprecated target alias.
  assert.deepEqual(row(next, 'brand').value, source.keyColor.value);
  close(sample.h, row(next, 'brand').h);
  assert.deepEqual(sample.diagnostics, ['gamut-limited']);
  assert.equal(slot(next, 'surface').deltaRatioCmax, 0.9);
  assert.equal(sample.effectiveRatioCmax, 0.9);
  // A request in the second island must remain reachable, not be mistaken for
  // the first interval's maximum by a global monotonic binary search.
  const islandC = (0.31321265627693484 + 0.31321440879013596) / 2;
  const island = edit(next, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaRatioCmax: islandC / sample.cmax,
  });
  close(row(island, 'surface').C, islandC);
  close(row(island, 'surface').rRealized, row(island, 'surface').rTarget);
  assert.deepEqual(row(island, 'surface').diagnostics, []);
  assert.deepEqual(row(island, 'brand'), row(next, 'brand'));
});

test('slot budget, sparse containers and explicit malformed optional values reject', () => {
  const source = input();
  source.slots = Array.from({ length: 255 }, (_, i) => ({
    id: `slot_${i}`,
    role: 'Base',
    deltaL: 0,
    deltaRatioCmax: 0,
  }));
  source.preset.slots = source.slots.map((slot) => plan(slot.id, slot.role, 1));
  const state = createKeyColorRecipe(source);
  assert.equal(state.resolved.length, 256);
  assert.equal(restoreKeyColorRecipe(state).resolved.length, 256);
  source.slots.push({ id: 'excess', role: 'Base', deltaL: 0, deltaRatioCmax: 0 });
  source.preset.slots.push(plan('excess', 'Base', 1));
  assert.throws(() => createKeyColorRecipe(source));
  for (const mutate of [
    (s) => (s.slots = new Array(1)),
    (s) => (s.preset.slots = new Array(1)),
    (s) => (s.keyColor.hueHint = null),
    (s) => (s.settings = null),
    (s) => (s.slots[1].id = s.slots[0].id),
  ]) {
    const invalid = input();
    mutate(invalid);
    assert.throws(() => createKeyColorRecipe(invalid));
  }
});

test('restore rejects a C1 literal/linear mismatch before Field use or any edit', () => {
  const source = input();
  source.keyColor.value = { hex: '#0000ff', alpha: 0.4 };
  const committed = createKeyColorRecipe(source),
    corrupted = structuredClone(committed);
  row(corrupted, 'brand').linear = [1, 0, 0, 0.4];
  const before = structuredClone(corrupted);
  assert.throws(() => restoreKeyColorRecipe(corrupted), /decoded linear channels/);
  assert.throws(() => keyColorSlotToLinear(corrupted, 'brand'), /decoded linear channels/);
  assert.throws(
    () => edit(corrupted, { type: 'slot-delta', slotId: 'surface', deltaL: 0.1 }),
    /decoded linear channels/,
  );
  assert.deepEqual(corrupted, before);
  const next = edit(committed, { type: 'slot-delta', slotId: 'surface', deltaL: 0.1 });
  assert.deepEqual(row(next, 'brand'), row(committed, 'brand'));
});

test('L=.25/h264.1 gap exposes target versus realized ratio and preserves the second island', () => {
  const source = input();
  source.keyColor.value.hex = '#808080';
  source.keyColor.hueHint = 264.1; // Existing explicit hint, only a coordinate fixture.
  source.preset.slots[0] = plan('surface', 'Base', 0, {
    lightnessScale: 0,
    lightnessOffset: 0.25,
    chromaScale: 0,
    chromaOffset: 0,
  });
  source.slots[0].deltaRatioCmax = 0.9;
  const state = createKeyColorRecipe(source),
    sample = row(state, 'surface');
  close(sample.L, 0.25);
  close(sample.h, 264.1);
  close(sample.cmax, 0.1731505535276372);
  close(sample.C, 0.14900193677697282);
  close(sample.rTarget * sample.cmax, 0.1558354981748735);
  assert.ok(sample.rTarget * sample.cmax > 0.14900193677697282);
  assert.ok(sample.rTarget * sample.cmax < 0.1709049361551201);
  assert.equal(sample.rTarget, 0.9);
  close(sample.rRealized, 0.8605339904569815);
  assert.equal(sample.effectiveRatioCmax, sample.rTarget);
  assert.deepEqual(sample.diagnostics, ['gamut-limited']);
  assert.equal(slot(state, 'surface').deltaRatioCmax, 0.9);
  assert.deepEqual(row(state, 'brand').value, source.keyColor.value);
  assert.deepEqual(restoreKeyColorRecipe(JSON.parse(JSON.stringify(state))), state);
  assert.ok(keyColorSlotToLinear(state, 'surface').value.every((v) => v >= 0 && v <= 1));
  const islandC = (0.1709049361551201 + 0.1731505535276372) / 2;
  const island = edit(state, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaRatioCmax: islandC / sample.cmax,
  });
  close(row(island, 'surface').C, islandC);
  close(row(island, 'surface').rRealized, row(island, 'surface').rTarget);
  assert.deepEqual(row(island, 'surface').diagnostics, []);
});

test('raw ratio intent, clamped target, realized ratio and deprecated alias are distinct', () => {
  const state = createKeyColorRecipe(input());
  for (const sample of state.resolved) {
    assert.equal(sample.effectiveRatioCmax, sample.rTarget);
    assert.equal(sample.rRealized, sample.cmax === 0 ? 0 : sample.C / sample.cmax);
  }
  for (const deltaRatioCmax of [-8, 8]) {
    const next = edit(state, { type: 'slot-delta', slotId: 'surface', deltaRatioCmax });
    const sample = row(next, 'surface'),
      rawRatio = sample.baseRatioCmax + deltaRatioCmax;
    assert.equal(slot(next, 'surface').deltaRatioCmax, deltaRatioCmax);
    assert.notEqual(rawRatio, sample.rTarget);
    assert.equal(sample.rTarget, deltaRatioCmax < 0 ? 0 : 1);
    close(sample.rRealized, sample.rTarget);
    assert.equal(sample.effectiveRatioCmax, sample.rTarget);
    assert.ok(sample.diagnostics.includes('ratio-clamped'));
  }
});

test('Cmax zero reports realized zero without destroying a positive target or C1 input', () => {
  const source = input();
  source.keyColor.value = { hex: '#000000', alpha: 0.4 };
  const state = createKeyColorRecipe(source);
  const next = edit(state, {
    type: 'slot-delta',
    slotId: 'surface',
    deltaL: 4,
    deltaRatioCmax: 0.9,
  });
  for (const id of ['brand', 'surface']) {
    const sample = row(next, id);
    assert.equal(sample.cmax, 0);
    assert.equal(sample.C, 0);
    assert.equal(sample.rRealized, 0);
    assert.ok(sample.diagnostics.includes('zero-cmax'));
  }
  assert.equal(row(next, 'surface').rTarget, 0.9);
  assert.equal(row(next, 'surface').effectiveRatioCmax, 0.9);
  assert.equal(row(next, 'brand').rTarget, 0);
  assert.deepEqual(row(next, 'brand').value, source.keyColor.value);
  assert.deepEqual(restoreKeyColorRecipe(JSON.parse(JSON.stringify(next))), next);
  const invalid = structuredClone(next);
  row(invalid, 'surface').diagnostics = [];
  assert.throws(() => restoreKeyColorRecipe(invalid), /ratio metadata/);
});

// Actual prototype-v1 receipt captured from source 9f15aa233dae5cbf1098ef54030f23ab8e280a1d.
const savedV1 = {
  version: 1,
  solverVersion: 'experimental-affine-v1',
  colorSemanticsVersion: 'linear-srgb-straight-v1',
  paletteId: 'saved_v1_blue',
  revision: 0,
  intent: {
    keyColor: {
      id: 'key',
      role: 'Identity',
      value: {
        hex: '#0000ff',
        alpha: 0.4,
      },
      hueHint: 264.052020638055,
    },
    preset: {
      id: 'v1_gap',
      version: 1,
      harmony: 'triadic',
      hueSteps: 12,
      slots: [
        {
          id: 'gap',
          role: 'Accent',
          position: 0,
          lightnessScale: 1,
          lightnessOffset: 0,
          chromaScale: 0,
          chromaOffset: 0,
        },
      ],
    },
    slots: [
      {
        id: 'gap',
        role: 'Accent',
        deltaL: 0,
        deltaRatioCmax: 0.9,
      },
    ],
    settings: {
      areaAware: false,
    },
  },
  resolved: [
    {
      id: 'key',
      role: 'Identity',
      active: true,
      value: {
        hex: '#0000ff',
        alpha: 0.4,
      },
      linear: [0, 0, 1, 0.4],
      L: 0.4520137183853429,
      C: 0.31321437166460114,
      h: 264.052020638055,
      cmax: 0.3132144087901359,
      effectiveRatioCmax: 0.9999998814692629,
      diagnostics: [],
    },
    {
      id: 'gap',
      role: 'Accent',
      active: true,
      value: {
        hex: '#0031e5',
        alpha: 1,
      },
      linear: [0, 0.030263386730899122, 0.7859763109604404, 1],
      L: 0.4520137183853429,
      C: 0.26559335414277563,
      h: 264.05202063805496,
      cmax: 0.3132144087901359,
      baseL: 0.4520137183853429,
      baseRatioCmax: 0,
      effectiveRatioCmax: 0.9,
      diagnostics: ['gamut-limited'],
    },
  ],
  diagnostics: [],
};

test('prototype v1 snapshots upgrade ratio metadata only, without solving or changing revision', () => {
  const saved = freeze(structuredClone(savedV1)),
    restored = restoreKeyColorRecipe(saved);
  assert.equal(saved.version, 1);
  assert.equal(restored.version, 2);
  assert.equal(restored.revision, saved.revision);
  assert.deepEqual(restored.intent, saved.intent);
  const stripNewMetadata = (sample) => {
    const { rTarget, rRealized, ...old } = sample;
    return old;
  };
  assert.deepEqual(restored.resolved.map(stripNewMetadata), saved.resolved);
  assert.deepEqual(
    { ...restored, version: 1, resolved: restored.resolved.map(stripNewMetadata) },
    saved,
  );
  assert.equal(row(restored, 'gap').rTarget, 0.9);
  close(row(restored, 'gap').rRealized, 0.8479602045406922);
  assert.deepEqual(restoreKeyColorRecipe(JSON.parse(JSON.stringify(restored))), restored);
  const adapter = keyColorSlotToLinear(saved, 'gap');
  assert.deepEqual(adapter.value, row(saved, 'gap').linear.map(Math.fround));
  const next = edit(saved, { type: 'slot-delta', slotId: 'gap', deltaRatioCmax: 0.8 });
  assert.equal(next.version, 2);
  assert.equal(next.revision, saved.revision + 1);
  assert.deepEqual(row(next, 'key').value, row(saved, 'key').value);
  assert.deepEqual(row(next, 'key').linear, row(saved, 'key').linear);
});

test('v2 restore rejects inconsistent target/realized/alias metadata without committing', () => {
  const state = freeze(restoreKeyColorRecipe(savedV1)),
    before = structuredClone(state);
  for (const change of [
    (s) => (row(s, 'gap').rTarget = 0.8),
    (s) => (row(s, 'gap').rRealized = 0.9),
    (s) => (row(s, 'gap').effectiveRatioCmax = row(s, 'gap').rRealized),
    (s) => (row(s, 'gap').baseRatioCmax = 0.2),
    (s) => (row(s, 'gap').rRealized = NaN),
    (s) => delete row(s, 'gap').rTarget,
    (s) => delete row(s, 'gap').rRealized,
    (s) => (row(s, 'key').rRealized = 0),
    (s) => (s.version = 1), // v1 has its own closed shape, not mixed v2 fields.
    (s) => (s.version = 3),
  ]) {
    const invalid = structuredClone(state);
    change(invalid);
    const invalidBefore = structuredClone(invalid);
    assert.throws(() => restoreKeyColorRecipe(invalid));
    assert.throws(() => edit(invalid, { type: 'recompute' }));
    assert.deepEqual(invalid, invalidBefore);
    assert.deepEqual(state, before);
  }
});
