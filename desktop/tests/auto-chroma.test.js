import test from 'node:test';
import assert from 'node:assert/strict';
import { proposeAutoChroma } from '../src/auto-chroma.js';

const roles = ['Primary', 'Secondary', 'Accent', 'Background', 'Text'];
const profiles = ['balanced', 'fidelity', 'vibrant', 'soft', 'uniform'];
const slot = (patch = {}) => ({ id: 'brand', L: 0.6, h: 35, role: 'Primary', ...patch });
const one = (patch = {}, settings = {}, coverage = null) =>
  proposeAutoChroma([slot(patch)], settings, coverage)[0];
const coverageOf = (id, percent) => ({
  slots: [{ id, percent }],
  other: { percent: 100 - percent },
});
const close = (actual, expected, tolerance = 1e-10) =>
  assert.ok(Math.abs(actual - expected) <= tolerance, `${actual} != ${expected}`);

function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

// An independent, forward transform from encoded sRGB bytes to OKLab. This
// checks output color numerics without copying the engine's inverse transform.
function hexToOklab(hex) {
  const [r, g, b] = hex.match(/[a-f\d]{2}/gi).map((byte) => {
    const encoded = parseInt(byte, 16) / 255;
    return encoded <= 0.04045 ? encoded / 12.92 : ((encoded + 0.055) / 1.055) ** 2.4;
  });
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return {
    L: 0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s,
    a: 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
    b: 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s,
  };
}

function inverse(matrix) {
  const rows = matrix.map((row, i) => [...row, ...[0, 1, 2].map((j) => Number(i === j))]);
  for (let column = 0; column < 3; column++) {
    const divisor = rows[column][column];
    rows[column] = rows[column].map((value) => value / divisor);
    for (let row = 0; row < 3; row++) {
      if (row === column) continue;
      const factor = rows[row][column];
      rows[row] = rows[row].map((value, j) => value - factor * rows[column][j]);
    }
  }
  return rows.map((row) => row.slice(3));
}
const inverseLab = inverse([
  [0.2104542553, 0.793617785, -0.0040720468],
  [1.9779984951, -2.428592205, 0.4505937099],
  [0.0259040371, 0.7827717662, -0.808675766],
]);
const inverseLms = inverse([
  [0.4122214708, 0.5363325363, 0.0514459929],
  [0.2119034982, 0.6806995451, 0.1073969566],
  [0.0883024619, 0.2817188376, 0.6299787005],
]);
const multiply = (matrix, vector) =>
  matrix.map((row) => row.reduce((sum, value, i) => sum + value * vector[i], 0));
function independentlyDerivedLinearRgb({ L, C, h }) {
  const lab = [L, C * Math.cos((h * Math.PI) / 180), C * Math.sin((h * Math.PI) / 180)];
  return multiply(
    inverseLms,
    multiply(inverseLab, lab).map((value) => value ** 3),
  );
}

test('Auto Chroma preserves stable IDs, order and arbitrary supported palette size', () => {
  for (const count of [0, 1, 2, 7, 256]) {
    const slots = Array.from({ length: count }, (_, i) =>
      slot({ id: `stable_${i}`, role: roles[i % roles.length], h: i * 17 }),
    );
    const output = proposeAutoChroma(slots);
    assert.equal(output.length, count);
    assert.deepEqual(
      output.map((row) => row.id),
      slots.map((row) => row.id),
    );
    assert.deepEqual(proposeAutoChroma([...slots].reverse()), [...output].reverse());
  }
});

test('Auto Chroma is deterministic, pure and returns independent proposal objects', () => {
  const slots = freeze([
    slot({ alpha: 0.25, h: -325 }),
    slot({ id: 'anchor', anchor: { hex: '#aBcD09', alpha: 0.42 } }),
  ]);
  const settings = freeze({ profile: 'soft', colorfulness: 0.8, spread: 0.5 });
  const coverage = freeze({ slots: [{ id: 'brand', percent: 30 }], other: { percent: 70 } });
  const before = structuredClone({ slots, settings, coverage });
  const first = proposeAutoChroma(slots, settings, coverage);
  assert.deepEqual(proposeAutoChroma(slots, settings, coverage), first);
  first[0].value.hex = '#000000';
  first[1].value.alpha = 0;
  assert.deepEqual({ slots, settings, coverage }, before);
  assert.notDeepEqual(proposeAutoChroma(slots, settings, coverage), first);
});

test('protected anchors preserve exact hex spelling and alpha over automatic/manual choices', () => {
  const anchor = { hex: '#aBcD09', alpha: 0.123456789 };
  for (const profile of profiles) {
    assert.deepEqual(
      one(
        { anchor, alpha: 0.9, overrideC: 0.45 },
        { profile, colorfulness: 0 },
        coverageOf('brand', 100),
      ),
      { id: 'brand', value: anchor, source: 'protected-anchor' },
    );
  }
});

test('manual C takes precedence, clips at the gamut boundary and reports clipping', () => {
  const manual = one({ overrideC: 0.05, alpha: 0.37 }, { colorfulness: 0 });
  assert.equal(manual.C, 0.05);
  assert.equal(manual.source, 'manual-chroma');
  assert.equal(manual.gamutLimited, false);
  assert.equal(manual.value.alpha, 0.37);
  const limited = one({ overrideC: 0.45 });
  assert.ok(limited.cmax > 0 && limited.cmax < 0.45);
  assert.equal(limited.C, limited.cmax);
  assert.equal(limited.gamutLimited, true);
  assert.equal(one({ overrideC: limited.cmax }).gamutLimited, false);
  assert.equal(one({ overrideC: 0 }).C, 0);
});

test('alpha is preserved exactly, including fully transparent and default opaque colors', () => {
  for (const alpha of [0, 0.00123456789, 0.5, 1]) assert.equal(one({ alpha }).value.alpha, alpha);
  assert.equal(one().value.alpha, 1);
});

test('coverage is a bounded soft chroma weight, not a quota or authored coverage rewrite', () => {
  const settings = { profile: 'uniform', spread: 1, colorfulness: 0.5 };
  const baseline = one({}, settings);
  const tiny = one({}, settings, coverageOf('brand', 0.00001));
  const large = one({}, settings, coverageOf('brand', 100));
  close(tiny.C / baseline.C, 1.34);
  assert.ok(large.C < baseline.C);
  close(one({ role: 'Text' }, settings, coverageOf('brand', 100)).C / baseline.C, 0.66 * 0.03);
  assert.equal(tiny.L, baseline.L);
  assert.equal(tiny.h, baseline.h);
  assert.equal(tiny.id, baseline.id);
  assert.equal('coverage' in tiny, false);
  assert.deepEqual(one({}, { ...settings, areaAware: false }, coverageOf('brand', 100)), baseline);
  assert.deepEqual(one({}, settings, coverageOf('brand', 0)), baseline);
  assert.deepEqual(one({}, settings, { slots: [], other: { percent: 0 } }), baseline);
  const neutralSpread = one({}, { ...settings, spread: 0 });
  assert.deepEqual(one({}, { ...settings, spread: 0 }, coverageOf('brand', 100)), neutralSpread);
});

test('profiles retain role differences, uniform non-text parity and true zero colorfulness', () => {
  for (const profile of profiles) {
    for (const role of roles) {
      const output = one({ role }, { profile, colorfulness: 0 });
      assert.equal(output.C, 0);
      assert.match(output.value.hex, /^#([a-f\d]{2})\1\1$/);
    }
  }
  const uniform = roles.slice(0, 4).map((role) => one({ role }, { profile: 'uniform' }).C);
  assert.ok(uniform.every((C) => C === uniform[0]));
  assert.ok(one({ role: 'Text' }, { profile: 'uniform' }).C < uniform[0]);
  assert.ok(
    one({ role: 'Accent' }, { profile: 'vibrant' }).C >
      one({ role: 'Accent' }, { profile: 'soft' }).C,
  );
  for (const role of roles) {
    const output = one({ role }, { colorfulness: 1, spread: 1 });
    assert.ok(output.C >= 0 && output.C <= output.cmax * 0.98 + 1e-12);
  }
});

test('hue is periodic and normalized, including permitted large/negative rotations', () => {
  const baseline = one({ h: 35 });
  for (const h of [-359965, -685, -325, 395, 755, 359675]) assert.deepEqual(one({ h }), baseline);
  assert.equal(one({ h: 360000 }).h, 0);
  assert.equal(one({ h: -360000 }).h, 0);
});

test('black and white endpoints have zero gamut capacity even with manual C', () => {
  for (const [L, hex] of [
    [0, '#000000'],
    [1, '#ffffff'],
  ]) {
    for (const h of [0, 120, 264, 359]) {
      const output = one({ L, h, overrideC: 0.45 });
      assert.equal(output.cmax, 0);
      assert.equal(output.C, 0);
      assert.equal(output.value.hex, hex);
      assert.equal(output.gamutLimited, true);
    }
  }
  assert.equal(one({ L: 0.5, overrideC: 0 }).value.hex, '#636363');
});

test('independent sRGB primary vectors survive manual OKLCH round trips', () => {
  for (const hex of ['#ff0000', '#00ff00', '#0000ff']) {
    const lab = hexToOklab(hex);
    const output = one({
      L: lab.L,
      h: (Math.atan2(lab.b, lab.a) * 180) / Math.PI,
      overrideC: Math.hypot(lab.a, lab.b),
    });
    assert.equal(
      output.value.hex,
      hex,
      `manual sRGB primary ${hex} must not be spuriously gamut clipped`,
    );
  }
});

test('blue cusp keeps the second valid interval but never proposes chroma inside its gamut gap', () => {
  const lab = hexToOklab('#0000ff');
  const blue = { L: lab.L, h: (Math.atan2(lab.b, lab.a) * 180) / Math.PI };
  const manual = one({ ...blue, overrideC: 0.28 });
  assert.ok(manual.cmax > 0.3132, 'the disconnected blue cusp must remain reachable');
  assert.ok(manual.C > 0.2655 && manual.C < 0.2657, 'clip downward to the first valid interval');
  assert.equal(manual.gamutLimited, true);
  const automatic = one(blue, { colorfulness: 0.9, spread: 0 });
  assert.ok(automatic.cmax * 0.9 > 0.28, 'unconstrained automatic C would land in the gap');
  close(automatic.C, manual.C, 1e-6);
});

test('encoded proposals independently reconstruct their requested OKLab coordinates', () => {
  for (const L of [0.1, 0.35, 0.6, 0.9]) {
    for (let h = 0; h < 360; h += 30) {
      const output = one({ L, h });
      const actual = hexToOklab(output.value.hex);
      close(actual.L, L, 0.004);
      close(actual.a, output.C * Math.cos((h * Math.PI) / 180), 0.004);
      close(actual.b, output.C * Math.sin((h * Math.PI) / 180), 0.004);
    }
  }
});

test('dense automatic/manual spectrum remains numerically in gamut before byte clipping', () => {
  let checked = 0;
  for (const L of [0.001, 0.01, 0.05, 0.1, 0.25, 0.45, 0.65, 0.85, 0.95, 0.99, 0.999]) {
    for (let h = 0; h < 360; h += 10) {
      for (const overrideC of [undefined, 0.05, 0.15, 0.3, 0.45]) {
        const proposal = one({ L, h, overrideC });
        const rgb = independentlyDerivedLinearRgb(proposal);
        // The inverse is derived numerically from forward matrices, whose
        // published rounded coefficients differ slightly from the engine's.
        assert.ok(
          rgb.every((channel) => channel >= -5e-7 && channel <= 1 + 5e-7),
          JSON.stringify({ proposal, rgb }),
        );
        assert.ok(proposal.C >= 0 && proposal.C <= proposal.cmax);
        checked++;
      }
    }
  }
  assert.equal(checked, 1980);
});

test('invalid, sparse, excessive and duplicate slots are rejected', () => {
  for (const slots of [
    null,
    {},
    'slots',
    [null],
    [undefined],
    new Array(1),
    [slot(), slot()],
    Array.from({ length: 257 }, (_, i) => slot({ id: `s${i}` })),
  ]) {
    assert.throws(() => proposeAutoChroma(slots));
  }
  for (const patch of [
    { id: '' },
    { id: 'bad id' },
    { id: 'x'.repeat(129) },
    { id: 1 },
    ...[undefined, null, NaN, Infinity, -Infinity, -0.1, 1.1, '0.5'].map((L) => ({ L })),
    ...[undefined, null, NaN, Infinity, -Infinity, 360001, -360001, '35'].map((h) => ({ h })),
    ...[undefined, null, '', 'primary', 'constructor', '__proto__'].map((role) => ({ role })),
    ...[null, NaN, Infinity, -0.1, 1.1, '0.5'].map((alpha) => ({ alpha })),
    ...[null, NaN, Infinity, -Infinity, -0.1, 0.451, '0.1'].map((overrideC) => ({ overrideC })),
    ...[
      null,
      {},
      { hex: '#abc', alpha: 1 },
      { hex: '#12345678', alpha: 1 },
      { hex: 'abcdef', alpha: 1 },
      { hex: '#123456' },
      { hex: '#123456', alpha: NaN },
      { hex: '#123456', alpha: 2 },
    ].map((anchor) => ({ anchor })),
  ])
    assert.throws(() => one(patch), JSON.stringify(patch));
});

test('invalid settings containers and values are rejected rather than silently defaulted', () => {
  for (const settings of [null, [], true, 1, '', 'balanced', () => {}])
    assert.throws(() => one({}, settings));
  for (const settings of [
    { unknownSetting: 1 },
    ...['unknown', null, 'constructor', '__proto__'].map((profile) => ({ profile })),
    ...[null, NaN, Infinity, -Infinity, -0.1, 1.1, '0.5'].flatMap((value) => [
      { colorfulness: value },
      { spread: value },
    ]),
    ...[null, 0, 1, 'true'].map((areaAware) => ({ areaAware })),
  ])
    assert.throws(() => one({}, settings));
});

test('coverage rejects unknown/duplicate/sparse rows, invalid values and non-total shares', () => {
  for (const coverage of [
    {},
    [],
    true,
    1,
    '',
    { slots: [], other: {} },
    { slots: new Array(1), other: { percent: 100 } },
    { slots: [{ id: 'missing', percent: 0 }], other: { percent: 100 } },
    {
      slots: [
        { id: 'brand', percent: 10 },
        { id: 'brand', percent: 10 },
      ],
      other: { percent: 80 },
    },
    {
      slots: Array.from({ length: 257 }, () => ({ id: 'brand', percent: 0 })),
      other: { percent: 100 },
    },
    { slots: [{ id: 'brand', percent: 10 }], other: { percent: 0 } },
    ...[undefined, null, NaN, Infinity, -Infinity, -1, 101, '100'].flatMap((percent) => [
      { slots: [{ id: 'brand', percent }], other: { percent: 0 } },
      { slots: [], other: { percent } },
    ]),
  ])
    assert.throws(() => one({}, {}, coverage));
});
