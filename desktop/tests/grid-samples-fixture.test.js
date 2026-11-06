import test from 'node:test';
import assert from 'node:assert/strict';
import { gridSamplesFixtureV1 } from './fixtures/grid-samples-v1.js';

function descriptor() {
  return {
    namespaceId: 'fixture:一',
    frameToken: 'opaque-fixture-frame',
    origin: [10, 20],
    spacing: [2, 3],
    angle: { type: 'Angle', unit: 'degree', value: 0 },
    bounds: { minI: -1, maxI: 2, minJ: -2, maxJ: 1 },
  };
}

const coordinate = (sample) => [sample.attributes.gridI.value, sample.attributes.gridJ.value];

test('rectangular fixture uses half-open lattice bounds, typed indices and j-major order', () => {
  const input = descriptor(),
    before = structuredClone(input),
    result = gridSamplesFixtureV1(input);
  assert.deepEqual(result.samples.map(coordinate), [
    [-1, -2],
    [0, -2],
    [1, -2],
    [-1, -1],
    [0, -1],
    [1, -1],
    [-1, 0],
    [0, 0],
    [1, 0],
  ]);
  assert.deepEqual(result.samples[0].position, [8, 14]);
  assert.deepEqual(result.samples[0].attributes.gridI, {
    type: 'Integer',
    role: 'gridI',
    value: -1,
  });
  assert.deepEqual(result.samples[0].attributes.gridJ, {
    type: 'Integer',
    role: 'gridJ',
    value: -2,
  });
  assert.deepEqual(result.grid.basisX, [2, 0]);
  assert.ok(Object.is(result.grid.basisY[0], -0));
  assert.equal(result.grid.basisY[1], 3);
  assert.equal(result.grid.cellArea, 6);
  assert.equal(result.space, input.frameToken);
  assert.deepEqual(input, before);
});

test('expanding the window and changing a fixture mask preserve surviving identities and geometry', () => {
  const input = descriptor(),
    initial = gridSamplesFixtureV1(input),
    expanded = gridSamplesFixtureV1({
      ...input,
      bounds: { minI: -3, maxI: 2, minJ: -3, maxJ: 1 },
    });
  const oldIds = new Set(initial.samples.map((sample) => sample.id));
  assert.deepEqual(
    expanded.samples.filter((sample) => oldIds.has(sample.id)),
    initial.samples,
  );
  assert.notEqual(expanded.membershipRevision, initial.membershipRevision);
  assert.equal(expanded.geometryRevision, initial.geometryRevision);
  assert.equal(expanded.orderingRevision, initial.orderingRevision);
  assert.equal(expanded.attributeRevision, initial.attributeRevision);

  const kept = gridSamplesFixtureV1({
    ...input,
    included: [
      [1, 0],
      [-1, -2],
      [1, 0],
    ],
  });
  const reversed = gridSamplesFixtureV1({
    ...input,
    included: [
      [-1, -2],
      [1, 0],
    ],
  });
  assert.deepEqual(kept, reversed);
  assert.deepEqual(kept.samples, [initial.samples[0], initial.samples[8]]);
  assert.equal(kept.geometryRevision, initial.geometryRevision);
  assert.equal(kept.orderingRevision, initial.orderingRevision);
  assert.notEqual(kept.membershipRevision, initial.membershipRevision);
});

test('placement and opaque frame change geometry without changing lattice identities', () => {
  const input = descriptor(),
    initial = gridSamplesFixtureV1(input),
    ids = initial.samples.map((sample) => sample.id);
  for (const changed of [
    { ...input, origin: [100, -40] },
    { ...input, spacing: [4, 1] },
    { ...input, angle: { type: 'Angle', unit: 'degree', value: 90 } },
    { ...input, frameToken: 'another-opaque-frame' },
  ]) {
    const result = gridSamplesFixtureV1(changed);
    assert.deepEqual(
      result.samples.map((sample) => sample.id),
      ids,
    );
    assert.equal(result.membershipRevision, initial.membershipRevision);
    assert.equal(result.orderingRevision, initial.orderingRevision);
    assert.notEqual(result.geometryRevision, initial.geometryRevision);
  }
  const otherNamespace = gridSamplesFixtureV1({ ...input, namespaceId: 'other' });
  assert.notEqual(otherNamespace.samples[0].id, initial.samples[0].id);
});

test('degree angle rotates the basis and the origin is p(0,0)', () => {
  const input = descriptor();
  input.bounds = { minI: 0, maxI: 2, minJ: 0, maxJ: 2 };
  input.angle.value = 90;
  const result = gridSamplesFixtureV1(input);
  assert.deepEqual(result.samples[0].position, input.origin);
  assert.ok(Math.abs(result.grid.basisX[0]) < 1e-12);
  assert.ok(Math.abs(result.grid.basisX[1] - 2) < 1e-12);
  assert.ok(Math.abs(result.grid.basisY[0] + 3) < 1e-12);
  assert.ok(Math.abs(result.grid.basisY[1]) < 1e-12);
  assert.ok(Math.abs(result.grid.cellArea - 6) < 1e-12);
  assert.ok(Math.abs(result.samples[3].position[0] - 7) < 1e-12);
  assert.ok(Math.abs(result.samples[3].position[1] - 22) < 1e-12);
});

test('fixture descriptor JSON readback is deterministic and empty windows are valid', () => {
  const input = descriptor();
  assert.deepEqual(
    gridSamplesFixtureV1(JSON.parse(JSON.stringify(input))),
    gridSamplesFixtureV1(input),
  );
  const empty = gridSamplesFixtureV1({
    ...input,
    bounds: { minI: 0, maxI: 0, minJ: -Number.MAX_SAFE_INTEGER, maxJ: Number.MAX_SAFE_INTEGER },
  });
  assert.deepEqual(empty.samples, []);
  assert.equal(empty.membershipRevision, '');
  assert.equal(empty.geometryRevision, gridSamplesFixtureV1(input).geometryRevision);
  assert.equal(empty.orderingRevision, gridSamplesFixtureV1(input).orderingRevision);
  assert.deepEqual(gridSamplesFixtureV1({ ...input, included: [] }).samples, []);
});

test('signed zero and the largest half-open safe-integer endpoint keep stable IDs', () => {
  const input = descriptor();
  input.origin = [-0, -0];
  input.angle.value = -0;
  assert.deepEqual(
    gridSamplesFixtureV1(JSON.parse(JSON.stringify(input))),
    gridSamplesFixtureV1(input),
  );
  const zero = gridSamplesFixtureV1({
    ...input,
    bounds: { minI: -0, maxI: 1, minJ: -0, maxJ: 1 },
  });
  const positive = gridSamplesFixtureV1({
    ...input,
    bounds: { minI: 0, maxI: 1, minJ: 0, maxJ: 1 },
  });
  assert.equal(zero.samples[0].id, positive.samples[0].id);
  assert.ok(Object.is(zero.samples[0].attributes.gridI.value, 0));
  assert.ok(Object.is(zero.samples[0].attributes.gridJ.value, 0));
  assert.equal(zero.membershipRevision, positive.membershipRevision);
  const upper = gridSamplesFixtureV1({
    ...input,
    bounds: {
      minI: Number.MAX_SAFE_INTEGER - 1,
      maxI: Number.MAX_SAFE_INTEGER,
      minJ: 0,
      maxJ: 1,
    },
  });
  assert.equal(upper.samples.length, 1);
  assert.equal(upper.samples[0].attributes.gridI.value, Number.MAX_SAFE_INTEGER - 1);
});

test('invalid descriptors, unsafe ranges and unrepresentable geometry reject', () => {
  const input = descriptor();
  for (const changed of [
    { ...input, frameToken: '' },
    { ...input, origin: [Infinity, 0] },
    { ...input, spacing: [0, 2] },
    { ...input, spacing: [Number.MIN_VALUE, Number.MIN_VALUE] },
    { ...input, angle: { type: 'Angle', unit: 'radian', value: 0 } },
    { ...input, angle: { type: 'Angle', unit: 'degree', value: Infinity } },
    { ...input, bounds: { minI: 2, maxI: 1, minJ: 0, maxJ: 1 } },
    { ...input, bounds: { minI: 0, maxI: 1.5, minJ: 0, maxJ: 1 } },
    { ...input, bounds: { minI: 0, maxI: Number.MAX_SAFE_INTEGER + 1, minJ: 0, maxJ: 1 } },
    { ...input, bounds: { minI: 0, maxI: 65, minJ: 0, maxJ: 64 } },
    {
      ...input,
      spacing: [1e308, 1e-308],
      bounds: { minI: 2, maxI: 3, minJ: 0, maxJ: 1 },
    },
    { ...input, transform: [1, 0, 0, 1, 0, 0] },
  ]) {
    const before = structuredClone(changed);
    assert.throws(() => gridSamplesFixtureV1(changed), /Grid fixture/);
    assert.deepEqual(changed, before);
  }
});

test('candidate budget is preflighted and unsupported accessors do not execute', () => {
  const input = descriptor();
  const full = gridSamplesFixtureV1({
    ...input,
    bounds: { minI: 0, maxI: 64, minJ: 0, maxJ: 64 },
  });
  assert.equal(full.samples.length, 4096);
  let reads = 0;
  Object.defineProperty(input.origin, '0', {
    configurable: true,
    get() {
      reads++;
      return 10;
    },
  });
  assert.throws(() => gridSamplesFixtureV1(input), /Grid fixture/);
  assert.equal(reads, 0);
});

test('bounds and spacing use one captured data snapshot even for changing Proxies', () => {
  const input = descriptor();
  let boundReads = 0,
    spacingReads = 0;
  input.bounds = new Proxy(
    { minI: 0, maxI: 1, minJ: 0, maxJ: 1 },
    {
      get(target, key, receiver) {
        if (key === 'maxI') return ++boundReads <= 3 ? 1 : 4101;
        return Reflect.get(target, key, receiver);
      },
    },
  );
  input.spacing = new Proxy([2, 3], {
    get(target, key, receiver) {
      if (key === '0') return ++spacingReads <= 3 ? 2 : 200;
      return Reflect.get(target, key, receiver);
    },
  });
  const result = gridSamplesFixtureV1(input);
  assert.equal(result.samples.length, 1);
  assert.deepEqual(result.grid.spacing, [2, 3]);
  assert.deepEqual(result.grid.basisX, [2, 0]);
  assert.equal(boundReads, 0);
  assert.equal(spacingReads, 0);
});

test('nested fixture descriptors are captured without property gets or repeated bound reads', () => {
  const input = descriptor(),
    reads = {};
  const noGet = (value) =>
    new Proxy(value, {
      get() {
        throw Error('unexpected property get');
      },
    });
  input.origin = noGet(input.origin);
  input.spacing = noGet(input.spacing);
  input.angle = noGet(input.angle);
  input.included = noGet([noGet([0, 0])]);
  input.bounds = new Proxy(input.bounds, {
    get() {
      throw Error('unexpected bound get');
    },
    getOwnPropertyDescriptor(target, key) {
      reads[key] = (reads[key] || 0) + 1;
      return Reflect.getOwnPropertyDescriptor(target, key);
    },
  });
  const result = gridSamplesFixtureV1(noGet(input));
  assert.equal(result.samples.length, 1);
  assert.deepEqual(result.samples[0].position, [10, 20]);
  assert.deepEqual(reads, { minI: 1, maxI: 1, minJ: 1, maxJ: 1 });
});

test('captured descriptors do not invoke inherited prototype getters or setters', () => {
  const previous = Object.getOwnPropertyDescriptor(Object.prototype, 'bounds');
  let calls = 0;
  try {
    Object.defineProperty(Object.prototype, 'bounds', {
      configurable: true,
      get() {
        calls++;
        return { minI: 0, maxI: 4101, minJ: 0, maxJ: 1 };
      },
      set() {
        calls++;
      },
    });
    assert.equal(gridSamplesFixtureV1(descriptor()).samples.length, 9);
    assert.equal(calls, 0);
  } finally {
    if (previous) Object.defineProperty(Object.prototype, 'bounds', previous);
    else delete Object.prototype.bounds;
  }
});
