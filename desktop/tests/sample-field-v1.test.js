import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { evaluateSampleFieldV1 } from '../src/sample-field-v1.js';
import { legacyPointPathToSamples } from '../src/legacy-point-path-adapter.js';
import { gridSamplesFixtureV1 } from './fixtures/grid-samples-v1.js';

const constant = (valueType, role, unit, value) => ({
  kind: 'Constant',
  version: 1,
  valueType,
  role,
  unit,
  value,
});
const position = (frameToken, unit = 'fixture-unit') => ({
  kind: 'Position',
  version: 1,
  valueType: 'Vector2',
  role: 'position',
  unit,
  frameToken,
});
const grid = (bounds = { minI: -1, maxI: 2, minJ: -1, maxJ: 1 }) =>
  gridSamplesFixtureV1({
    namespaceId: 'fixture-grid',
    frameToken: 'fixture-frame',
    origin: [2, 3],
    spacing: [4, 5],
    angle: { type: 'Angle', unit: 'degree', value: 0 },
    bounds,
  });
const context = (set) => ({
  domain: 'Sample',
  count: set.samples.length,
  maxSamples: 4096,
  position: {
    frameToken: set.space,
    unit: 'fixture-unit',
    values: set.samples.map((sample) => sample.position),
  },
});

test('typed scale and selection Constants are Uniform without population-sized arrays', () => {
  const request = { domain: 'Sample', count: 1_000_000, maxSamples: 1_000_000 };
  for (const [field, expected] of [
    [constant('Scalar', 'scale', 'ratio', 0.25), 0.25],
    [constant('Vector2', 'scale', 'ratio', [2, -1]), [2, -1]],
    [constant('Boolean', 'selection', null, true), true],
  ]) {
    const result = evaluateSampleFieldV1(field, request);
    assert.deepEqual(result, {
      kind: 'Uniform',
      version: 1,
      valueType: field.valueType,
      role: field.role,
      unit: field.unit,
      count: request.count,
      value: expected,
    });
    if (Array.isArray(field.value)) {
      result.value[0] = 9;
      assert.deepEqual(field.value, [2, -1]);
    }
  }
});

test('empty populations retain typed Uniform/Varying behavior and still require Position capability', () => {
  const empty = { domain: 'Sample', count: 0, maxSamples: 0 };
  assert.deepEqual(evaluateSampleFieldV1(constant('Scalar', 'scale', 'ratio', 3), empty), {
    kind: 'Uniform',
    version: 1,
    valueType: 'Scalar',
    role: 'scale',
    unit: 'ratio',
    count: 0,
    value: 3,
  });
  assert.throws(() => evaluateSampleFieldV1(position('frame'), empty), /Position capability/);
  const present = {
    ...empty,
    position: { frameToken: 'frame', unit: 'fixture-unit', values: [] },
  };
  assert.throws(() => evaluateSampleFieldV1(position('different'), present), /frame/);
  assert.throws(() => evaluateSampleFieldV1(position('frame', 'other-unit'), present), /unit/);
  assert.deepEqual(evaluateSampleFieldV1(position('frame'), present), {
    kind: 'Varying',
    version: 1,
    valueType: 'Vector2',
    role: 'position',
    unit: 'fixture-unit',
    frameToken: 'frame',
    count: 0,
    values: [],
  });
});

test('Position follows the caller population order and copies its finite vectors', () => {
  const set = grid(),
    request = context(set),
    before = structuredClone(request),
    result = evaluateSampleFieldV1(position(set.space), request);
  assert.deepEqual(
    result.values,
    set.samples.map((sample) => sample.position),
  );
  assert.equal(result.count, set.samples.length);
  result.values[0][0] += 100;
  assert.deepEqual(request, before);
  assert.notStrictEqual(result.values[0], request.position.values[0]);
});

test('Position reevaluates membership and order even when grid geometry revision is unchanged', () => {
  const full = grid(),
    cropped = grid({ minI: 0, maxI: 2, minJ: -1, maxJ: 1 });
  assert.equal(cropped.geometryRevision, full.geometryRevision);
  const fullValues = evaluateSampleFieldV1(position(full.space), context(full));
  const croppedRequest = context(cropped);
  croppedRequest.position.values.reverse();
  const croppedValues = evaluateSampleFieldV1(position(cropped.space), croppedRequest);
  assert.equal(fullValues.count, 6);
  assert.equal(croppedValues.count, 4);
  assert.deepEqual(
    croppedValues.values,
    [...cropped.samples].reverse().map((sample) => sample.position),
  );
});

test('Position accepts a caller context derived from the legacy Path bridge', () => {
  const legacy = {
    contours: [
      {
        id: 'legacy:contour',
        closed: false,
        points: [
          {
            id: 'legacy:point',
            position: [-4, 7],
            handleA: [0, 0],
            handleB: [0, 0],
            instanceTransform: { scale: 2, rotation: 30 },
            luminance: 0.4,
            darkness: 0.6,
          },
        ],
      },
    ],
  };
  const set = legacyPointPathToSamples(legacy);
  const result = evaluateSampleFieldV1(position(set.space), context(set));
  assert.deepEqual(result.values, [[-4, 7]]);
});

test('unsupported types, roles, missing capabilities and invalid values reject', () => {
  const set = grid(),
    request = context(set);
  for (const field of [
    constant('Color', 'fillColor', 'srgb', '#112233'),
    constant('Scalar', 'scale', 'ratio', Infinity),
    constant('Scalar', 'opacity', 'ratio', 1),
    constant('Boolean', 'selection', null, 1),
    constant('Vector2', 'scale', 'ratio', [1, NaN]),
    constant('Vector2', 'scale', 'ratio', [1, 2, 3]),
    { ...position(set.space), role: 'scale' },
    { ...position(set.space), version: 2 },
    { ...position(set.space), frameToken: '' },
    { ...position(set.space), extra: true },
  ])
    assert.throws(() => evaluateSampleFieldV1(field, request), /Sample Field v1/);
  for (const bad of [
    { ...request, domain: 'PathAnchor' },
    { ...request, count: -1 },
    { ...request, maxSamples: -1 },
    { ...request, count: request.maxSamples + 1 },
    { ...request, position: { ...request.position, frameToken: 'other' } },
    { ...request, position: { ...request.position, unit: 'other' } },
    { ...request, position: { ...request.position, values: [] } },
    {
      ...request,
      position: {
        ...request.position,
        values: [[Infinity, 0], ...request.position.values.slice(1)],
      },
    },
  ]) {
    const before = structuredClone(bad);
    assert.throws(() => evaluateSampleFieldV1(position(set.space), bad), /Sample Field v1/);
    assert.deepEqual(bad, before);
  }
});

test('request cap is checked before examining an oversized Position population', () => {
  let enumerations = 0;
  const values = new Proxy(new Array(10), {
    ownKeys(target) {
      enumerations++;
      return Reflect.ownKeys(target);
    },
  });
  const request = {
    domain: 'Sample',
    count: 10,
    maxSamples: 9,
    position: { frameToken: 'frame', unit: 'fixture-unit', values },
  };
  assert.throws(() => evaluateSampleFieldV1(position('frame'), request), /budget/);
  assert.equal(enumerations, 0);
});

test('data descriptors are captured once without invoking nested Proxy property gets', () => {
  const reads = {};
  const noGet = (value) =>
    new Proxy(value, {
      get() {
        throw Error('unexpected property get');
      },
    });
  const request = new Proxy(
    {
      domain: 'Sample',
      count: 1,
      maxSamples: 1,
      position: noGet({
        frameToken: 'frame',
        unit: 'fixture-unit',
        values: noGet([noGet([4, 5])]),
      }),
    },
    {
      get() {
        throw Error('unexpected context get');
      },
      getOwnPropertyDescriptor(target, key) {
        reads[key] = (reads[key] || 0) + 1;
        return Reflect.getOwnPropertyDescriptor(target, key);
      },
    },
  );
  assert.deepEqual(evaluateSampleFieldV1(noGet(position('frame')), request).values, [[4, 5]]);
  assert.deepEqual(reads, { domain: 1, count: 1, maxSamples: 1, position: 1 });
});

test('missing capabilities never fall back to inherited getters or setters', () => {
  const saved = new Map(
    ['frameToken', 'position', 'count'].map((key) => [
      key,
      Object.getOwnPropertyDescriptor(Object.prototype, key),
    ]),
  );
  let calls = 0;
  const fieldWithoutFrame = {
    kind: 'Position',
    version: 1,
    valueType: 'Vector2',
    role: 'position',
    unit: 'fixture-unit',
  };
  const empty = { domain: 'Sample', count: 0, maxSamples: 0 };
  try {
    Object.defineProperty(Object.prototype, 'frameToken', {
      configurable: true,
      get() {
        calls++;
        return 'frame';
      },
    });
    Object.defineProperty(Object.prototype, 'position', {
      configurable: true,
      get() {
        calls++;
        return { frameToken: 'frame', unit: 'fixture-unit', values: [] };
      },
    });
    Object.defineProperty(Object.prototype, 'count', {
      configurable: true,
      set() {
        calls++;
      },
    });
    assert.throws(() => evaluateSampleFieldV1(fieldWithoutFrame, empty), /Sample Field v1/);
    assert.throws(() => evaluateSampleFieldV1(position('frame'), empty), /Position capability/);
    assert.equal(evaluateSampleFieldV1(constant('Scalar', 'scale', 'ratio', 1), empty).count, 0);
    assert.equal(calls, 0);
  } finally {
    for (const [key, previous] of saved) {
      if (previous) Object.defineProperty(Object.prototype, key, previous);
      else delete Object.prototype[key];
    }
  }
});

test('array snapshots and Position results ignore inherited numeric accessors', () => {
  // Isolate the temporary prototype modification from the test runner itself.
  const program = `
    import { evaluateSampleFieldV1 as evaluate } from ${JSON.stringify(new URL('../src/sample-field-v1.js', import.meta.url).href)};
    const saved = Object.getOwnPropertyDescriptor(Object.prototype, '0');
    const invalid = { kind: 'Constant', version: 1, valueType: 'Vector2', role: 'scale', unit: 'ratio', value: [NaN, 2] };
    const position = { kind: 'Position', version: 1, valueType: 'Vector2', role: 'position', unit: 'px', frameToken: 'frame' };
    const request = { domain: 'Sample', count: 1, maxSamples: 1, position: { frameToken: 'frame', unit: 'px', values: [[4, 5]] } };
    let calls = 0, rejected = false, result;
    try {
      Object.defineProperty(Object.prototype, '0', {
        configurable: true,
        get() { calls++; return 4; },
        set() { calls++; },
      });
      try { evaluate(invalid, request); } catch { rejected = true; }
      result = evaluate(position, request);
    } finally {
      if (saved) Object.defineProperty(Object.prototype, '0', saved);
      else delete Object.prototype[0];
    }
    if (!rejected || calls !== 0 || !Object.hasOwn(result.values, 0) ||
        !Object.hasOwn(result.values[0], 0) || result.values[0][0] !== 4 || result.values[0][1] !== 5)
      throw Error('numeric prototype accessor changed the captured value or output');
  `;
  execFileSync(process.execPath, ['--input-type=module', '--eval', program], { timeout: 10_000 });
});
