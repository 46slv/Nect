import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { CommandSession, deserialize, evaluate, exportSVG, serialize } from '../src/core.js';
import {
  legacyPointPathToSamples,
  samplesToLegacyPointPath,
} from '../src/legacy-point-path-adapter.js';
import { legacyLuminanceFixture } from './fixtures/legacy-luminance-v1.js';

const oracle = JSON.parse(
  readFileSync(new URL('./fixtures/legacy-luminance-v1.json', import.meta.url)),
);
const legacyPath = () => ({ contours: structuredClone(oracle.pointPath.contours) });

test('legacy Luminance graph retains its fixed Path, materialization, SVG and saved bytes', () => {
  const { doc, layer, points, instances } = legacyLuminanceFixture();
  const evaluated = evaluate(layer);
  assert.deepEqual(evaluated.run(points.id), oracle.pointPath);
  assert.equal(evaluated.run(instances.id).fingerprint, oracle.instanceFingerprint);
  assert.equal(exportSVG(doc), oracle.svg);
  assert.equal(serialize(doc), oracle.saved);

  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  send('set-parameter', { nodeId: points.id, key: 'maxScale', value: 2 });
  const changed = serialize(session.snapshot());
  assert.notEqual(exportSVG(session.snapshot()), oracle.svg);
  assert.equal(exportSVG(deserialize(changed)), exportSVG(session.snapshot()));
  send('undo');
  assert.equal(serialize(session.snapshot()), oracle.saved);
  assert.equal(exportSVG(session.snapshot()), oracle.svg);
  send('redo');
  assert.equal(serialize(session.snapshot()), changed);
  assert.equal(session.snapshot().schemaVersion, oracle.schemaVersion);
});

test('v1 samples preserve identity, order, coordinates, degree angles and typed scalars', () => {
  const path = legacyPath();
  path.contours[0].points[0].instanceTransform.rotation = 22.5;
  const before = structuredClone(path);
  const samples = legacyPointPathToSamples(path);
  const first = samples.samples[0];
  assert.equal(samples.domain, 'Sample');
  assert.equal(samples.space, 'legacy-document-units-v1');
  assert.deepEqual(first.position, path.contours[0].points[0].position);
  assert.deepEqual(first.scale, [0.75, 0.75]);
  assert.deepEqual(first.rotation, { type: 'Angle', unit: 'degree', value: 22.5 });
  assert.deepEqual(first.attributes.luminance, { type: 'Scalar', role: 'luminance', value: 0.5 });
  assert.deepEqual(first.attributes.darkness, { type: 'Scalar', role: 'darkness', value: 0.5 });
  assert.deepEqual(samplesToLegacyPointPath(JSON.parse(JSON.stringify(samples))), path);
  first.position[0] += 4;
  assert.deepEqual(path, before, 'sample edits do not mutate the input Path');
  assert.throws(() => samplesToLegacyPointPath(samples), /stale SampleSet revisions/);
});

test('sample identity and separate revision descriptors reflect the actual change', () => {
  const path = legacyPath();
  const original = legacyPointPathToSamples(path);
  const reversed = legacyPointPathToSamples({ contours: [...path.contours].reverse() });
  assert.deepEqual(
    original.samples.map((sample) => sample.id),
    reversed.samples.map((sample) => sample.id).reverse(),
  );
  assert.equal(reversed.membershipRevision, original.membershipRevision);
  assert.equal(reversed.geometryRevision, original.geometryRevision);
  assert.equal(reversed.attributeRevision, original.attributeRevision);
  assert.notEqual(reversed.orderingRevision, original.orderingRevision);

  const moved = structuredClone(path);
  moved.contours[0].points[0].position[0] += 1;
  const movedSamples = legacyPointPathToSamples(moved);
  assert.equal(movedSamples.membershipRevision, original.membershipRevision);
  assert.equal(movedSamples.orderingRevision, original.orderingRevision);
  assert.equal(movedSamples.attributeRevision, original.attributeRevision);
  assert.notEqual(movedSamples.geometryRevision, original.geometryRevision);

  const changed = structuredClone(path);
  changed.contours[0].points[0].instanceTransform.scale = 0;
  const changedSamples = legacyPointPathToSamples(changed);
  assert.deepEqual(changedSamples.samples[0].scale, [0, 0]);
  assert.equal(changedSamples.geometryRevision, original.geometryRevision);
  assert.notEqual(changedSamples.attributeRevision, original.attributeRevision);
  assert.deepEqual(samplesToLegacyPointPath(changedSamples), changed);
});

test('v1 IDs are unambiguous and finite legacy values round-trip exactly', () => {
  const path = legacyPath();
  path.contours = path.contours.slice(0, 2);
  path.contours[0].id = 'a:b';
  path.contours[0].points[0].id = '点:c';
  path.contours[0].points[0].handleA[0] = -0;
  path.contours[0].points[0].instanceTransform.scale = -0;
  path.contours[0].points[0].instanceTransform.rotation = -360;
  path.contours[1].id = 'a';
  path.contours[1].points[0].id = 'b:点:c';
  const samples = legacyPointPathToSamples(path);
  assert.notEqual(samples.samples[0].id, samples.samples[1].id);
  assert.ok(Object.is(samples.samples[0].scale[0], -0));
  assert.deepEqual(samplesToLegacyPointPath(samples), path);
});

test('unsupported Sample and Path data are rejected without loss or mutation', () => {
  const original = legacyPointPathToSamples(legacyPath());
  const cases = [
    (set) => (set.samples[0].scale[1] = 2),
    (set) => (set.samples[0].rotation.unit = 'radian'),
    (set) => (set.samples[0].attributes.color = { type: 'Color', value: '#112233' }),
    (set) => (set.samples[0].id = 'wrong'),
    (set) => (set.samples[1] = structuredClone(set.samples[0])),
    (set) => (set.space = 'Local'),
    (set) => (set.samples.extra = true),
  ];
  for (const change of cases) {
    const set = structuredClone(original);
    change(set);
    const before = structuredClone(set);
    assert.throws(() => samplesToLegacyPointPath(set), /LegacyPointPathAdapterV1/);
    assert.deepEqual(set, before);
  }
  for (const change of [
    (path) => (path.contours[0].closed = true),
    (path) => (path.contours[0].points[0].handleA[0] = 1),
    (path) => (path.contours[0].points.extra = true),
    (path) => (path.contours[0].points[0].extra = 'preserve me'),
  ]) {
    const path = legacyPath();
    change(path);
    const before = structuredClone(path);
    assert.throws(() => legacyPointPathToSamples(path), /LegacyPointPathAdapterV1/);
    assert.deepEqual(path, before);
  }
});

test('unsupported array accessors are rejected before reading the value', () => {
  const path = legacyPath();
  let reads = 0;
  Object.defineProperty(path.contours[0].points, '0', {
    configurable: true,
    get() {
      reads++;
      return null;
    },
  });
  assert.throws(() => legacyPointPathToSamples(path), /LegacyPointPathAdapterV1/);
  assert.equal(reads, 0);
});

test('v1 descriptor construction does not invoke inherited toJSON hooks', () => {
  const path = legacyPath();
  const previous = Object.getOwnPropertyDescriptor(Object.prototype, 'toJSON');
  let calls = 0;
  Object.defineProperty(Object.prototype, 'toJSON', {
    configurable: true,
    value() {
      calls++;
      return null;
    },
  });
  try {
    const samples = legacyPointPathToSamples(path);
    assert.deepEqual(samplesToLegacyPointPath(samples), path);
    assert.equal(calls, 0);
  } finally {
    if (previous) Object.defineProperty(Object.prototype, 'toJSON', previous);
    else delete Object.prototype.toJSON;
  }
});
