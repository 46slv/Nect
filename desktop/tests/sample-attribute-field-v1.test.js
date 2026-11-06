import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { evaluateSampleFieldV1 } from '../src/sample-field-v1.js';
import { legacyPointPathToSamples } from '../src/legacy-point-path-adapter.js';

const field = (name) => ({
  kind: 'Attribute',
  version: 1,
  valueType: 'Scalar',
  role: name,
  unit: null,
  attributeName: name,
});

function fixture() {
  const contours = [0.1, 0.8, 0.3].map((luminance, index) => ({
    id: `contour-${index}`,
    closed: false,
    points: [
      {
        id: `point-${index}`,
        position: [index * 10, index * -5],
        handleA: [0, 0],
        handleB: [0, 0],
        instanceTransform: { scale: 1, rotation: 0 },
        luminance,
        darkness: 1 - luminance,
      },
    ],
  }));
  return legacyPointPathToSamples({ contours });
}

function context(set, name) {
  return {
    domain: 'Sample',
    count: set.samples.length,
    maxSamples: 256,
    attribute: {
      name,
      domain: 'Sample',
      valueType: 'Scalar',
      role: name,
      unit: null,
      values: set.samples.map((sample) => sample.attributes[name].value),
    },
  };
}

test('explicit legacy luminance and darkness Attributes evaluate in caller order', () => {
  const set = fixture();
  for (const name of ['luminance', 'darkness']) {
    const request = context(set, name),
      before = structuredClone(request),
      result = evaluateSampleFieldV1(field(name), request);
    assert.deepEqual(result, {
      kind: 'Varying',
      version: 1,
      valueType: 'Scalar',
      role: name,
      unit: null,
      count: 3,
      values: set.samples.map((sample) => sample.attributes[name].value),
    });
    result.values[0] = 99;
    assert.deepEqual(request, before);
  }
});

test('reorder and crop preserve each surviving ID/value pairing without a stale cache', () => {
  const full = fixture(),
    reordered = { ...full, samples: [...full.samples].reverse() },
    cropped = { ...full, samples: [full.samples[2], full.samples[0]] };
  const values = (set) =>
    evaluateSampleFieldV1(field('luminance'), context(set, 'luminance')).values;
  const originalValues = values(full),
    mapping = new Map(full.samples.map((sample, index) => [sample.id, originalValues[index]]));
  for (const set of [reordered, cropped])
    assert.deepEqual(
      values(set),
      set.samples.map((sample) => mapping.get(sample.id)),
    );
});

test('empty population still needs a correctly typed Attribute capability', () => {
  const empty = { domain: 'Sample', count: 0, maxSamples: 0 };
  assert.throws(() => evaluateSampleFieldV1(field('luminance'), empty), /Attribute capability/);
  const request = {
    ...empty,
    attribute: {
      name: 'luminance',
      domain: 'Sample',
      valueType: 'Scalar',
      role: 'luminance',
      unit: null,
      values: [],
    },
  };
  assert.deepEqual(evaluateSampleFieldV1(field('luminance'), request).values, []);
  assert.throws(() => evaluateSampleFieldV1(field('darkness'), request), /Attribute.*mismatch/);
});

test('wrong domain, name, role, type, unit, length or value reject without mutation', () => {
  const request = context(fixture(), 'luminance');
  for (const attribute of [
    { ...request.attribute, name: 'darkness' },
    { ...request.attribute, domain: 'PathAnchor' },
    { ...request.attribute, valueType: 'Color' },
    { ...request.attribute, role: 'darkness' },
    { ...request.attribute, unit: 'ratio' },
    { ...request.attribute, values: [0.2] },
    { ...request.attribute, values: [0.1, Infinity, 0.3] },
  ]) {
    const bad = { ...request, attribute },
      before = structuredClone(bad);
    assert.throws(() => evaluateSampleFieldV1(field('luminance'), bad), /Sample Field v1/);
    assert.deepEqual(bad, before);
  }
  assert.throws(
    () => evaluateSampleFieldV1({ ...field('luminance'), attributeName: 'darkness' }, request),
    /unsupported/,
  );
  assert.throws(() => evaluateSampleFieldV1(field('sourceColor'), request), /unsupported/);
});

test('request budget and data snapshot protect Attribute evaluation', () => {
  let enumerations = 0;
  const tooMany = new Proxy(new Array(5), {
    ownKeys(target) {
      enumerations++;
      return Reflect.ownKeys(target);
    },
  });
  const request = {
    domain: 'Sample',
    count: 5,
    maxSamples: 4,
    attribute: {
      name: 'luminance',
      domain: 'Sample',
      valueType: 'Scalar',
      role: 'luminance',
      unit: null,
      values: tooMany,
    },
  };
  assert.throws(() => evaluateSampleFieldV1(field('luminance'), request), /budget/);
  assert.equal(enumerations, 0);
  const noGet = (value) =>
    new Proxy(value, {
      get() {
        throw Error('unexpected get');
      },
    });
  const valid = noGet({
    domain: 'Sample',
    count: 2,
    maxSamples: 2,
    attribute: noGet({
      name: 'luminance',
      domain: 'Sample',
      valueType: 'Scalar',
      role: 'luminance',
      unit: null,
      values: noGet([0.25, 0.75]),
    }),
  });
  assert.deepEqual(evaluateSampleFieldV1(noGet(field('luminance')), valid).values, [0.25, 0.75]);
});

test('finite legacy values are copied without clamping or signed-zero loss', () => {
  const request = context(fixture(), 'luminance');
  request.attribute.values = [-0, 1.1, -0.1];
  const values = evaluateSampleFieldV1(field('luminance'), request).values;
  assert.ok(Object.is(values[0], -0));
  assert.deepEqual(values.slice(1), [1.1, -0.1]);
});

test('missing Attribute data cannot be supplied by inherited getters', () => {
  const saved = new Map(
    ['attributeName', 'attribute'].map((key) => [
      key,
      Object.getOwnPropertyDescriptor(Object.prototype, key),
    ]),
  );
  let calls = 0;
  try {
    Object.defineProperty(Object.prototype, 'attributeName', {
      configurable: true,
      get() {
        calls++;
        return 'luminance';
      },
    });
    Object.defineProperty(Object.prototype, 'attribute', {
      configurable: true,
      get() {
        calls++;
        return {
          name: 'luminance',
          domain: 'Sample',
          valueType: 'Scalar',
          role: 'luminance',
          unit: null,
          values: [],
        };
      },
    });
    const absentName = field('luminance');
    delete absentName.attributeName;
    const empty = { domain: 'Sample', count: 0, maxSamples: 0 };
    assert.throws(() => evaluateSampleFieldV1(absentName, empty), /unsupported/);
    assert.throws(() => evaluateSampleFieldV1(field('luminance'), empty), /Attribute capability/);
    assert.equal(calls, 0);
  } finally {
    for (const [key, previous] of saved) {
      if (previous) Object.defineProperty(Object.prototype, key, previous);
      else delete Object.prototype[key];
    }
  }
});

test('Attribute snapshots preserve values and reject NaN with inherited numeric accessors', () => {
  const program = `
    import { evaluateSampleFieldV1 as evaluate } from ${JSON.stringify(new URL('../src/sample-field-v1.js', import.meta.url).href)};
    const saved = Object.getOwnPropertyDescriptor(Object.prototype, '0');
    const field = { kind: 'Attribute', version: 1, valueType: 'Scalar', role: 'luminance', unit: null, attributeName: 'luminance' };
    const attribute = { name: 'luminance', domain: 'Sample', valueType: 'Scalar', role: 'luminance', unit: null, values: [NaN] };
    const request = { domain: 'Sample', count: 1, maxSamples: 1, attribute };
    const good = { ...request, attribute: { ...attribute, values: [-0] } };
    let calls = 0, rejected = false, result;
    try {
      Object.defineProperty(Object.prototype, '0', {
        configurable: true,
        get() { calls++; return 0.5; },
        set() { calls++; },
      });
      try { evaluate(field, request); } catch { rejected = true; }
      result = evaluate(field, good);
    } finally {
      if (saved) Object.defineProperty(Object.prototype, '0', saved);
      else delete Object.prototype[0];
    }
    if (!rejected || calls !== 0 || !Object.hasOwn(result.values, 0) || !Object.is(result.values[0], -0))
      throw Error('numeric prototype accessor changed an Attribute value');
  `;
  execFileSync(process.execPath, ['--input-type=module', '--eval', program], { timeout: 10_000 });
});
