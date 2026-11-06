// The v1 bridge is deliberately limited to the one-anchor contours emitted by
// legacy Luminance to Points. It is not a general Path-to-Sample conversion.
const ADAPTER = 'LegacyPointPathAdapterV1';
// An internal marker for the unchanged legacy document-unit coordinates. This
// does not define a general Field Space tag or a persisted project value.
const LEGACY_SPACE = 'legacy-document-units-v1';

function denseArray(value, length) {
  if (!Array.isArray(value) || Object.getPrototypeOf(value) !== Array.prototype) return false;
  if (length === undefined) length = value.length;
  if (value.length !== length || Reflect.ownKeys(value).length !== length + 1) return false;
  for (let index = 0; index < length; index++) {
    const descriptor = Object.getOwnPropertyDescriptor(value, String(index));
    if (!descriptor || !Object.hasOwn(descriptor, 'value')) return false;
  }
  return true;
}

function finiteVector(value) {
  return denseArray(value, 2) && Number.isFinite(value[0]) && Number.isFinite(value[1]);
}

function exactKeys(value, keys) {
  return (
    value &&
    typeof value === 'object' &&
    !Array.isArray(value) &&
    Object.getPrototypeOf(value) === Object.prototype &&
    Reflect.ownKeys(value).length === keys.length &&
    keys.every((key) => {
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      return descriptor && Object.hasOwn(descriptor, 'value');
    })
  );
}

function sampleId(contourId, pointId) {
  // Lengths make the mapping unambiguous even when an authored ID contains ':'.
  return `${ADAPTER}:${contourId.length}:${contourId}:${pointId.length}:${pointId}`;
}

function revisions(samples) {
  const ordered = [],
    sorted = [];
  for (let index = 0; index < samples.length; index++) {
    ordered.push(samples[index].id);
    sorted.push(samples[index]);
  }
  sorted.sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
  // Exact request-local descriptors avoid introducing a collision-prone hash or
  // claiming a persistent cache authority for this first legacy bridge. Pack
  // primitives directly: JSON.stringify would invoke inherited toJSON hooks.
  const pack = (value) => {
    const text = Object.is(value, -0) ? '-0' : String(value);
    return `${text.length}:${text}`;
  };
  return {
    membershipRevision: sorted.map((sample) => pack(sample.id)).join(''),
    geometryRevision: sorted
      .map((sample) => pack(sample.id) + pack(sample.position[0]) + pack(sample.position[1]))
      .join(''),
    orderingRevision: ordered.map(pack).join(''),
    attributeRevision: sorted
      .map(
        (sample) =>
          pack(sample.id) +
          pack(sample.scale[0]) +
          pack(sample.scale[1]) +
          pack(sample.rotation.value) +
          pack(sample.attributes.luminance.value) +
          pack(sample.attributes.darkness.value),
      )
      .join(''),
  };
}

function checkedLegacyContour(contour) {
  if (
    !exactKeys(contour, ['id', 'closed', 'points']) ||
    typeof contour.id !== 'string' ||
    !contour.id ||
    contour.closed !== false ||
    !denseArray(contour.points, 1)
  )
    throw Error(`${ADAPTER} requires disconnected one-anchor contours`);
  const point = contour.points[0];
  if (
    !exactKeys(point, [
      'id',
      'position',
      'handleA',
      'handleB',
      'instanceTransform',
      'luminance',
      'darkness',
    ]) ||
    typeof point.id !== 'string' ||
    !point.id ||
    !finiteVector(point.position) ||
    !finiteVector(point.handleA) ||
    !finiteVector(point.handleB) ||
    point.handleA[0] !== 0 ||
    point.handleA[1] !== 0 ||
    point.handleB[0] !== 0 ||
    point.handleB[1] !== 0 ||
    !exactKeys(point.instanceTransform, ['scale', 'rotation']) ||
    !Number.isFinite(point.instanceTransform.scale) ||
    !Number.isFinite(point.instanceTransform.rotation) ||
    !Number.isFinite(point.luminance) ||
    !Number.isFinite(point.darkness)
  )
    throw Error(`${ADAPTER} requires finite legacy point attributes and zero handles`);
  return point;
}

export function legacyPointPathToSamples(path) {
  if (!exactKeys(path, ['contours']) || !Array.isArray(path.contours))
    throw Error(`${ADAPTER} requires a legacy point Path`);
  if (path.contours.length > 256) throw Error(`${ADAPTER} exceeds the 256 point budget`);
  if (!denseArray(path.contours)) throw Error(`${ADAPTER} requires a legacy point Path`);
  const contourIds = new Set(),
    pointIds = new Set(),
    samples = [];
  for (let index = 0; index < path.contours.length; index++) {
    const contour = path.contours[index];
    const point = checkedLegacyContour(contour);
    if (contourIds.has(contour.id) || pointIds.has(point.id))
      throw Error(`${ADAPTER} requires unique stable IDs`);
    contourIds.add(contour.id);
    pointIds.add(point.id);
    samples.push({
      id: sampleId(contour.id, point.id),
      position: [point.position[0], point.position[1]],
      scale: [point.instanceTransform.scale, point.instanceTransform.scale],
      rotation: { type: 'Angle', unit: 'degree', value: point.instanceTransform.rotation },
      attributes: {
        luminance: { type: 'Scalar', role: 'luminance', value: point.luminance },
        darkness: { type: 'Scalar', role: 'darkness', value: point.darkness },
      },
      legacy: {
        contourId: contour.id,
        pointId: point.id,
        handleA: [point.handleA[0], point.handleA[1]],
        handleB: [point.handleB[0], point.handleB[1]],
      },
    });
  }
  return {
    kind: 'SampleSet',
    version: 1,
    namespaceId: ADAPTER,
    identityEpoch: 1,
    domain: 'Sample',
    space: LEGACY_SPACE,
    ...revisions(samples),
    samples,
  };
}

function lowerSample(sample) {
  return {
    id: sample.legacy.contourId,
    closed: false,
    points: [
      {
        id: sample.legacy.pointId,
        position: [sample.position[0], sample.position[1]],
        handleA: [sample.legacy.handleA[0], sample.legacy.handleA[1]],
        handleB: [sample.legacy.handleB[0], sample.legacy.handleB[1]],
        instanceTransform: { scale: sample.scale[0], rotation: sample.rotation.value },
        luminance: sample.attributes.luminance.value,
        darkness: sample.attributes.darkness.value,
      },
    ],
  };
}

// This synchronous path owns the just-created SampleSet; no caller can alter it
// between validation and lowering. Public SampleSet lowering still validates.
export function roundTripLegacyPointPathV1(path) {
  const set = legacyPointPathToSamples(path);
  return { contours: set.samples.map(lowerSample) };
}

export function samplesToLegacyPointPath(set) {
  if (
    !exactKeys(set, [
      'kind',
      'version',
      'namespaceId',
      'identityEpoch',
      'domain',
      'space',
      'membershipRevision',
      'geometryRevision',
      'orderingRevision',
      'attributeRevision',
      'samples',
    ]) ||
    set.kind !== 'SampleSet' ||
    set.version !== 1 ||
    set.namespaceId !== ADAPTER ||
    set.identityEpoch !== 1 ||
    set.domain !== 'Sample' ||
    set.space !== LEGACY_SPACE ||
    !Array.isArray(set.samples)
  )
    throw Error(`${ADAPTER} requires its v1 legacy-coordinate SampleSet`);
  if (set.samples.length > 256) throw Error(`${ADAPTER} exceeds the 256 point budget`);
  if (!denseArray(set.samples))
    throw Error(`${ADAPTER} requires its v1 legacy-coordinate SampleSet`);
  const contours = [],
    ids = new Set(),
    contourIds = new Set(),
    pointIds = new Set();
  for (let index = 0; index < set.samples.length; index++) {
    const sample = set.samples[index];
    if (
      !exactKeys(sample, ['id', 'position', 'scale', 'rotation', 'attributes', 'legacy']) ||
      !exactKeys(sample.legacy, ['contourId', 'pointId', 'handleA', 'handleB']) ||
      typeof sample.legacy.contourId !== 'string' ||
      !sample.legacy.contourId ||
      typeof sample.legacy.pointId !== 'string' ||
      !sample.legacy.pointId ||
      sample.id !== sampleId(sample.legacy.contourId, sample.legacy.pointId) ||
      ids.has(sample.id) ||
      contourIds.has(sample.legacy.contourId) ||
      pointIds.has(sample.legacy.pointId) ||
      !finiteVector(sample.position) ||
      !finiteVector(sample.scale) ||
      !Object.is(sample.scale[0], sample.scale[1]) ||
      !finiteVector(sample.legacy.handleA) ||
      !finiteVector(sample.legacy.handleB) ||
      sample.legacy.handleA[0] !== 0 ||
      sample.legacy.handleA[1] !== 0 ||
      sample.legacy.handleB[0] !== 0 ||
      sample.legacy.handleB[1] !== 0 ||
      !exactKeys(sample.rotation, ['type', 'unit', 'value']) ||
      sample.rotation.type !== 'Angle' ||
      sample.rotation.unit !== 'degree' ||
      !exactKeys(sample.attributes, ['luminance', 'darkness'])
    )
      throw Error(`${ADAPTER} cannot lower unsupported Sample data`);
    ids.add(sample.id);
    contourIds.add(sample.legacy.contourId);
    pointIds.add(sample.legacy.pointId);
    for (const role of ['luminance', 'darkness']) {
      const attribute = sample.attributes[role];
      if (
        !exactKeys(attribute, ['type', 'role', 'value']) ||
        attribute.type !== 'Scalar' ||
        attribute.role !== role
      )
        throw Error(`${ADAPTER} cannot lower unsupported Sample attributes`);
    }
    const contour = lowerSample(sample);
    checkedLegacyContour(contour);
    contours.push(contour);
  }
  const expected = revisions(set.samples);
  for (const key of Object.keys(expected))
    if (set[key] !== expected[key]) throw Error(`${ADAPTER} has stale SampleSet revisions`);
  return { contours };
}
