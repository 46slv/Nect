import test from 'node:test';
import assert from 'node:assert/strict';
import {
  harmonyCells,
  wheelPoint,
  resolveHarmonyPalette,
  buildHarmonyWheel,
} from '../src/harmony-wheel.js';
import { proposeAutoChroma } from '../src/auto-chroma.js';

const settings = (patch = {}) => ({
  harmony: 'triadic',
  hueSteps: 12,
  hueRotation: 0,
  lightnessSteps: 4,
  lightnessRange: 0.8,
  ...patch,
});
const slot = (patch = {}) => ({
  id: 'brand',
  position: 0,
  ring: 1,
  role: 'Primary',
  ...patch,
});
const close = (actual, expected, tolerance = 1e-12) =>
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} != ${expected}`);
const normalize = (degrees) => ((degrees % 360) + 360) % 360;
function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

// Literal vectors intentionally do not derive expected cells using the source.
test('four accepted presets retain exact sector identities across compatible resolutions', () => {
  for (const [harmony, hueSteps, expected] of [
    ['triadic', 3, [0, 1, 2]],
    ['triadic', 6, [0, 2, 4]],
    ['triadic', 12, [0, 4, 8]],
    ['triadic', 72, [0, 24, 48]],
    ['complementary', 4, [0, 2, 2]],
    ['complementary', 12, [0, 6, 6]],
    ['complementary', 72, [0, 36, 36]],
    ['analogous', 3, [0, 1, 2]],
    ['analogous', 5, [0, 1, 4]],
    ['analogous', 12, [0, 1, 11]],
    ['analogous', 72, [0, 1, 71]],
    ['split', 6, [0, 2, 4]],
    ['split', 12, [0, 5, 7]],
    ['split', 72, [0, 35, 37]],
  ]) {
    assert.deepEqual(harmonyCells(settings({ harmony, hueSteps })), expected);
  }
});

test('impossible harmonies reject rather than rounding or silently changing the preset', () => {
  for (const [harmony, hueSteps] of [
    ['triadic', 4],
    ['triadic', 5],
    ['triadic', 71],
    ['complementary', 3],
    ['complementary', 5],
    ['complementary', 71],
    ['split', 3],
    ['split', 4],
    ['split', 5],
    ['split', 7],
    ['split', 71],
  ]) {
    const input = settings({ harmony, hueSteps });
    assert.throws(() => harmonyCells(input), `${harmony}/${hueSteps}`);
    assert.throws(() => wheelPoint(input, 0, 0));
    assert.throws(() => resolveHarmonyPalette(input, []));
    assert.throws(() => buildHarmonyWheel(input));
  }
});

test('markers sit at sector centers, not boundaries, on the requested ring center', () => {
  // Analogous n=4 selects top, right and left. Complementary supplies bottom.
  const examples = [
    ['analogous', 0, 0, 0, -0.125, -90],
    ['analogous', 1, 2, 0.625, 0, 0],
    ['analogous', 2, 3, -0.875, 0, 180],
    ['complementary', 1, 1, 0, 0.375, 90],
  ];
  for (const [harmony, position, ring, x, y, angle] of examples) {
    const point = wheelPoint(settings({ harmony, hueSteps: 4 }), position, ring);
    close(point.x, x);
    close(point.y, y);
    close(point.angle, angle);
    close(point.radius, Math.hypot(x, y));
    close(Math.hypot(point.x, point.y), point.radius);
  }
  const triad = settings({ hueSteps: 12 });
  close(wheelPoint(triad, 1, 0).angle, 30);
  close(wheelPoint(triad, 2, 0).angle, 150);
  assert.equal(wheelPoint(triad, 1, 0).cell, 4);
  assert.equal(wheelPoint(triad, 2, 0).cell, 8);
});

test('hue rotation is independent from marker geometry and wraps both signs', () => {
  const base = settings({ harmony: 'split', hueSteps: 12 });
  for (const rotation of [-360000, -725, -360, -15, 0, 15, 360, 725, 360000]) {
    for (let position = 0; position < 3; position++) {
      for (let ring = 0; ring < 4; ring++) {
        const unrotated = wheelPoint(base, position, ring);
        const rotated = wheelPoint({ ...base, hueRotation: rotation }, position, ring);
        for (const key of ['cell', 'L', 'x', 'y', 'angle', 'radius'])
          assert.equal(rotated[key], unrotated[key], `${key} must not rotate`);
        close(rotated.h, normalize(unrotated.h + rotation));
      }
    }
  }
});

test('lightness resampling uses bright-inner cell centers and preserves range endpoints', () => {
  for (const lightnessSteps of [1, 2, 3, 4, 12]) {
    for (const lightnessRange of [0, 0.25, 0.8, 1]) {
      const input = settings({ lightnessSteps, lightnessRange });
      const points = Array.from({ length: lightnessSteps }, (_, ring) =>
        wheelPoint(input, 0, ring),
      );
      close(points[0].L, 0.5 + (lightnessRange * (lightnessSteps - 1)) / (2 * lightnessSteps));
      close(points.at(-1).L, 1 - points[0].L);
      close(points[0].radius, 1 / (2 * lightnessSteps));
      close(points.at(-1).radius, 1 - 1 / (2 * lightnessSteps));
      for (let ring = 0; ring < lightnessSteps; ring++) {
        close(points[ring].radius, (ring + 0.5) / lightnessSteps);
        close(points[ring].L + points[lightnessSteps - 1 - ring].L, 1);
        assert.ok(points[ring].L > 0 && points[ring].L < 1);
        if (ring) close(points[ring - 1].L - points[ring].L, lightnessRange / lightnessSteps);
      }
    }
  }
});

test('resolution changes recalculate exact cells without rounding stored positions or rings', () => {
  for (const hueSteps of [6, 12, 24, 72]) {
    const input = settings({ hueSteps, lightnessSteps: 12 });
    for (const [position, expectedHue, expectedAngle] of [
      [0, 0, -90],
      [1, 120, 30],
      [2, 240, 150],
    ]) {
      const point = wheelPoint(input, position, 11);
      close(point.h, expectedHue);
      close(point.angle, expectedAngle);
      close(point.radius, 23 / 24);
    }
  }
  for (const position of [-1, 3, 0.5, NaN, Infinity, null, '1', undefined])
    assert.throws(() => wheelPoint(settings(), position, 0));
  for (const ring of [-1, 4, 0.5, NaN, Infinity, null, '1', undefined])
    assert.throws(() => wheelPoint(settings(), 0, ring));
});

// Direct AutoChroma comparisons assert adapter compatibility, while the literal
// geometry above prevents source helpers from serving as their own oracle.
test('palette resolution forwards role, coordinates, alpha and manual chroma by stable ID', () => {
  const input = settings({ hueRotation: 25 });
  const slots = [
    slot({ id: 'accent', position: 2, ring: 3, role: 'Accent', alpha: 0.2 }),
    slot({ id: 'background', position: 0, ring: 0, role: 'Background', alpha: 0 }),
    slot({ id: 'primary', position: 1, ring: 1, overrideC: 0.03 }),
    slot({ id: 'text', position: 1, ring: 1, role: 'Text' }),
  ];
  const options = { profile: 'vibrant', colorfulness: 0.65, spread: 0.7, areaAware: false };
  const expected = proposeAutoChroma(
    [
      { id: 'accent', L: 0.2, h: 265, role: 'Accent', alpha: 0.2 },
      { id: 'background', L: 0.8, h: 25, role: 'Background', alpha: 0 },
      { id: 'primary', L: 0.6, h: 145, role: 'Primary', overrideC: 0.03 },
      { id: 'text', L: 0.6, h: 145, role: 'Text' },
    ],
    options,
  );
  const resolved = resolveHarmonyPalette(input, slots, options);
  assert.equal(resolved.colors.length, 4);
  for (let i = 0; i < expected.length; i++) {
    assert.equal(resolved.colors[i].id, expected[i].id);
    assert.deepEqual(resolved.colors[i].value, expected[i].value);
    close(resolved.colors[i].C, expected[i].C);
    close(resolved.colors[i].L, expected[i].L);
    close(resolved.colors[i].h, expected[i].h);
  }
  assert.equal(resolved.markers.length, 4);
  assert.deepEqual(
    resolved.markers.map(({ id }) => id),
    slots.map(({ id }) => id),
  );
  assert.deepEqual(
    resolveHarmonyPalette(input, [...slots].reverse(), options).colors,
    [...resolved.colors].reverse(),
  );
});

test('complementary positions remain independent IDs even at coincident coordinates', () => {
  const slots = [slot({ id: 'a', position: 1 }), slot({ id: 'b', position: 2 })];
  const result = resolveHarmonyPalette(settings({ harmony: 'complementary' }), slots);
  assert.deepEqual(
    result.colors.map(({ id }) => id),
    ['a', 'b'],
  );
  assert.equal(result.markers.length, 2);
  assert.deepEqual(
    result.markers.map(({ id }) => id),
    ['a', 'b'],
  );
  for (const key of ['cell', 'L', 'h', 'x', 'y', 'angle', 'radius'])
    assert.equal(result.markers[0][key], result.markers[1][key]);
});

test('neutral slots stay achromatic and have no wheel marker; anchors preserve exact spelling and alpha', () => {
  const anchor = { hex: '#aBcD09', alpha: 0.123456789 };
  const slots = [
    slot({ id: 'locked', anchor, alpha: 0.8, overrideC: 0.45 }),
    { id: 'gray', kind: 'neutral', L: 0.5, role: 'Secondary', alpha: 0 },
    { id: 'black', kind: 'neutral', L: 0, role: 'Text' },
    { id: 'white', kind: 'neutral', L: 1, role: 'Background' },
    { id: 'neutral-anchor', kind: 'neutral', L: 0.4, role: 'Primary', anchor },
  ];
  const result = resolveHarmonyPalette(settings(), slots, { colorfulness: 1, spread: 1 });
  assert.deepEqual(
    result.colors.map(({ id }) => id),
    slots.map(({ id }) => id),
  );
  assert.deepEqual(
    result.markers.map(({ id }) => id),
    ['locked'],
  );
  assert.deepEqual(result.colors[0].value, anchor);
  assert.equal(result.colors[0].source, 'protected-anchor');
  assert.deepEqual(result.colors.at(-1).value, anchor);
  for (const [index, hex, alpha] of [
    [1, '#636363', 0],
    [2, '#000000', 1],
    [3, '#ffffff', 1],
  ]) {
    assert.equal(result.colors[index].C, 0);
    assert.deepEqual(result.colors[index].value, { hex, alpha });
  }
});

test('area weighting joins stable IDs and changes chroma without moving markers', () => {
  const input = settings({ lightnessRange: 0 });
  const slots = [slot({ id: 'large' }), slot({ id: 'small' }), slot({ id: 'zero' })];
  const options = { profile: 'uniform', colorfulness: 0.5, spread: 1 };
  const coverage = {
    slots: [
      { id: 'small', percent: 1 },
      { id: 'zero', percent: 0 },
      { id: 'large', percent: 99 },
    ],
    other: { percent: 0 },
  };
  const unweighted = resolveHarmonyPalette(input, slots, options);
  const weighted = resolveHarmonyPalette(input, slots, options, coverage);
  assert.deepEqual(weighted.markers, unweighted.markers);
  const expected = proposeAutoChroma(
    slots.map(({ id }) => ({ id, L: 0.5, h: 0, role: 'Primary' })),
    options,
    coverage,
  );
  assert.deepEqual(weighted.colors, expected);
  assert.ok(weighted.colors[0].C < unweighted.colors[0].C);
  assert.ok(weighted.colors[1].C > unweighted.colors[1].C);
  assert.equal(weighted.colors[2].C, unweighted.colors[2].C);
  assert.deepEqual(
    resolveHarmonyPalette(input, slots, { ...options, areaAware: false }, coverage).colors,
    unweighted.colors,
  );
});

test('palette inputs are pure, deterministic, deep-independent and support empty/maximum budgets', () => {
  const input = freeze(settings());
  const slots = freeze([
    slot({ alpha: 0.25 }),
    { id: 'anchor', kind: 'neutral', L: 0.5, role: 'Text', anchor: { hex: '#aBcD09', alpha: 0.5 } },
  ]);
  const options = freeze({ profile: 'soft', colorfulness: 0.8, spread: 0.5 });
  const coverage = freeze({ slots: [{ id: 'brand', percent: 30 }], other: { percent: 70 } });
  const before = structuredClone({ input, slots, options, coverage });
  const first = resolveHarmonyPalette(input, slots, options, coverage);
  assert.deepEqual(resolveHarmonyPalette(input, slots, options, coverage), first);
  first.colors[0].value.hex = '#000000';
  first.colors[1].value.alpha = 0;
  first.markers[0].x = 9;
  assert.deepEqual({ input, slots, options, coverage }, before);
  assert.notDeepEqual(resolveHarmonyPalette(input, slots, options, coverage), first);
  assert.deepEqual(resolveHarmonyPalette(input, []), { colors: [], markers: [] });
  const maximum = Array.from({ length: 256 }, (_, i) => slot({ id: `s_${i}`, position: i % 3 }));
  assert.equal(resolveHarmonyPalette(input, maximum).colors.length, 256);
});

test('palette rejects invalid, sparse, duplicate and excessive slots without mutating valid input', () => {
  for (const slots of [
    null,
    {},
    '',
    [null],
    [undefined],
    new Array(1),
    [slot(), slot()],
    Array.from({ length: 257 }, (_, i) => slot({ id: `s_${i}` })),
  ])
    assert.throws(() => resolveHarmonyPalette(settings(), slots));
  for (const patch of [
    ...['', 'bad id', 'x'.repeat(129), 1, null].map((id) => ({ id })),
    ...[-1, 3, 0.5, null, NaN, Infinity, '0', undefined].map((position) => ({ position })),
    ...[-1, 4, 0.5, null, NaN, Infinity, '0', undefined].map((ring) => ({ ring })),
    ...['primary', 'constructor', '__proto__', null, undefined].map((role) => ({ role })),
    ...[-0.1, 1.1, null, NaN, Infinity, '0.5'].map((alpha) => ({ alpha })),
    ...[-0.1, 0.451, null, NaN, Infinity, '0.1'].map((overrideC) => ({ overrideC })),
    ...[null, {}, { hex: '#abc', alpha: 1 }, { hex: '#123456', alpha: 2 }].map((anchor) => ({
      anchor,
    })),
    { kind: 'unexpected' },
  ])
    assert.throws(() => resolveHarmonyPalette(settings(), [slot(patch)]), JSON.stringify(patch));
  for (const L of [-0.1, 1.1, NaN, Infinity, null, '0.5', undefined])
    assert.throws(() =>
      resolveHarmonyPalette(settings(), [{ id: 'gray', kind: 'neutral', L, role: 'Text' }]),
    );
});

test('palette forwards AutoChroma settings and coverage validation instead of dropping bad values', () => {
  for (const options of [
    null,
    [],
    'balanced',
    { unknown: 1 },
    { profile: 'constructor' },
    { colorfulness: 2 },
    { spread: NaN },
    { areaAware: 1 },
  ])
    assert.throws(() => resolveHarmonyPalette(settings(), [slot()], options));
  for (const coverage of [
    {},
    [],
    { slots: new Array(1), other: { percent: 100 } },
    { slots: [{ id: 'missing', percent: 100 }], other: { percent: 0 } },
    {
      slots: [
        { id: 'brand', percent: 1 },
        { id: 'brand', percent: 99 },
      ],
      other: { percent: 0 },
    },
    { slots: [{ id: 'brand', percent: 10 }], other: { percent: 10 } },
  ])
    assert.throws(() => resolveHarmonyPalette(settings(), [slot()], {}, coverage));
});

test('wheel settings reject malformed containers, unknown keys and out-of-range values', () => {
  const invalid = [
    null,
    [],
    true,
    1,
    '',
    'triadic',
    () => {},
    settings({ extra: 1 }),
    ...['unknown', 'constructor', '__proto__', null, undefined].map((harmony) =>
      settings({ harmony }),
    ),
    ...[2, 73, 3.5, null, NaN, Infinity, '12', undefined].map((hueSteps) => settings({ hueSteps })),
    ...[0, 13, 1.5, null, NaN, Infinity, '4', undefined].map((lightnessSteps) =>
      settings({ lightnessSteps }),
    ),
    ...[-0.01, 1.01, null, NaN, Infinity, '0.8', undefined].map((lightnessRange) =>
      settings({ lightnessRange }),
    ),
    ...[-360001, 360001, null, NaN, Infinity, -Infinity, '0', undefined].map((hueRotation) =>
      settings({ hueRotation }),
    ),
  ];
  for (const input of invalid) {
    assert.throws(() => harmonyCells(input));
    assert.throws(() => wheelPoint(input, 0, 0));
    assert.throws(() => resolveHarmonyPalette(input, []));
    assert.throws(() => buildHarmonyWheel(input));
  }
});

test('full wheel tessellates every ring/sector exactly once around centered markers', () => {
  const input = settings({ harmony: 'analogous', hueSteps: 8, lightnessSteps: 3 });
  const cells = buildHarmonyWheel(input);
  assert.equal(cells.length, 24);
  assert.equal(new Set(cells.map(({ id }) => id)).size, 24);
  const byCoordinates = new Map(cells.map((cell) => [`${cell.cell}:${cell.ring}`, cell]));
  assert.equal(byCoordinates.size, 24);
  for (let ring = 0; ring < 3; ring++) {
    const band = cells.filter((cell) => cell.ring === ring);
    assert.deepEqual(
      band.map(({ cell }) => cell),
      [0, 1, 2, 3, 4, 5, 6, 7],
    );
    close(band[0].startAngle, -112.5);
    close(band.at(-1).endAngle, 247.5);
    for (let cell = 0; cell < 8; cell++) {
      const tile = byCoordinates.get(`${cell}:${ring}`);
      close(tile.angle, -90 + 45 * cell);
      close(tile.startAngle, -112.5 + 45 * cell);
      close(tile.endAngle, -67.5 + 45 * cell);
      close(tile.angle, (tile.startAngle + tile.endAngle) / 2);
      close(tile.innerRadius, ring / 3);
      close(tile.outerRadius, (ring + 1) / 3);
      close(tile.radius, (tile.innerRadius + tile.outerRadius) / 2);
      assert.ok(tile.radius > tile.innerRadius && tile.radius < tile.outerRadius);
      close(Math.hypot(tile.x, tile.y), tile.radius);
      close(tile.L, [23 / 30, 0.5, 7 / 30][ring]);
      close(tile.h, 45 * cell);
      assert.match(tile.hex, /^#[a-f\d]{6}$/);
      if (cell) close(band[cell - 1].endAngle, tile.startAngle);
      if (ring) close(byCoordinates.get(`${cell}:${ring - 1}`).outerRadius, tile.innerRadius);
    }
  }
  const sectors = [
    { cell: 0, ring: 0, x: 0, y: -1 / 6 },
    { cell: 2, ring: 1, x: 0.5, y: 0 },
    { cell: 4, ring: 2, x: 0, y: 5 / 6 },
    { cell: 6, ring: 1, x: -0.5, y: 0 },
  ];
  for (const { cell, ring, x, y } of sectors) {
    close(byCoordinates.get(`${cell}:${ring}`).x, x);
    close(byCoordinates.get(`${cell}:${ring}`).y, y);
  }
});

test('full wheel batches beyond authored-slot budget without losing cells or reusing IDs', () => {
  const input = settings({ hueSteps: 72, lightnessSteps: 12, hueRotation: -15 });
  const options = { profile: 'soft', colorfulness: 0.4, spread: 0.7 };
  const cells = buildHarmonyWheel(input, options);
  assert.equal(cells.length, 864);
  assert.equal(new Set(cells.map(({ id }) => id)).size, 864);
  for (const index of [0, 71, 72, 255, 256, 511, 512, 767, 768, 863]) {
    const tile = cells[index];
    const ring = Math.floor(index / 72),
      cell = index % 72;
    assert.equal(tile.cell, cell);
    assert.equal(tile.ring, ring);
    close(tile.L, 0.9 - (0.8 * (ring + 0.5)) / 12);
    close(tile.h, normalize(cell * 5 - 15));
    const expected = proposeAutoChroma(
      [{ id: tile.id, L: tile.L, h: tile.h, role: 'Primary' }],
      options,
    )[0];
    assert.equal(tile.hex, expected.value.hex, `cell ${index} must retain its own proposal`);
  }
});

test('full wheel rotation and profile changes preserve tessellation; visual colors never imply area estimates', () => {
  const input = settings({ harmony: 'analogous', hueSteps: 5, lightnessSteps: 2 });
  const baseline = buildHarmonyWheel(input);
  const rotated = buildHarmonyWheel({ ...input, hueRotation: 37 });
  const soft = buildHarmonyWheel(input, { profile: 'soft' });
  for (let index = 0; index < baseline.length; index++) {
    for (const key of [
      'id',
      'cell',
      'ring',
      'angle',
      'radius',
      'x',
      'y',
      'L',
      'startAngle',
      'endAngle',
      'innerRadius',
      'outerRadius',
    ]) {
      assert.equal(rotated[index][key], baseline[index][key]);
      assert.equal(soft[index][key], baseline[index][key]);
    }
    close(rotated[index].h, normalize(baseline[index].h + 37));
  }
  assert.notDeepEqual(
    rotated.map(({ hex }) => hex),
    baseline.map(({ hex }) => hex),
  );
  assert.notDeepEqual(
    soft.map(({ hex }) => hex),
    baseline.map(({ hex }) => hex),
  );
  assert.deepEqual(
    buildHarmonyWheel(input, { areaAware: true }),
    buildHarmonyWheel(input, { areaAware: false }),
  );
  const gray = buildHarmonyWheel(input, { colorfulness: 0 });
  for (const tile of gray) assert.match(tile.hex, /^#([a-f\d]{2})\1\1$/);
});

test('derived wheel results are pure, independent and still validate options with no implicit fallback', () => {
  const input = freeze(settings());
  const options = freeze({ profile: 'fidelity', colorfulness: 0.6 });
  const before = structuredClone({ input, options });
  const first = buildHarmonyWheel(input, options);
  assert.deepEqual(buildHarmonyWheel(input, options), first);
  first[0].hex = '#000000';
  first[0].x = 99;
  assert.deepEqual({ input, options }, before);
  assert.notDeepEqual(buildHarmonyWheel(input, options), first);
  for (const bad of [
    null,
    [],
    'soft',
    { profile: 'constructor' },
    { unknown: true },
    { colorfulness: -0.1 },
    { spread: Infinity },
    { areaAware: 1 },
  ])
    assert.throws(() => buildHarmonyWheel(input, bad));
});

test('slot schema rejects irrelevant coordinate fields and neutral chroma escape hatches', () => {
  for (const extra of [{ L: 0.4 }, { h: 22 }, { cell: 0 }, { unknown: true }])
    assert.throws(() => resolveHarmonyPalette(settings(), [slot(extra)]));
  const neutral = { id: 'gray', kind: 'neutral', L: 0.5, role: 'Text' };
  for (const extra of [{ position: 0 }, { ring: 0 }, { h: 22 }, { overrideC: 0.2 }])
    assert.throws(() => resolveHarmonyPalette(settings(), [{ ...neutral, ...extra }]));
  assert.deepEqual(
    resolveHarmonyPalette(settings(), [slot({ kind: 'chromatic' })]),
    resolveHarmonyPalette(settings(), [slot()]),
  );
});
