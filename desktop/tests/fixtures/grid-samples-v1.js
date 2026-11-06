// A Phase 1 identity fixture, not a public Grid producer or project codec.
// The bounds and optional mask below are lattice coordinates, not world bounds
// or geometric clipping. The caller's frame token is opaque and untransformed.
const MAX_CANDIDATES = 4096;
const MAX_TOKEN_LENGTH = 256;

function plainData(value, keys) {
  return (
    value !== null &&
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

function plainArray(value, length) {
  if (!Array.isArray(value) || Object.getPrototypeOf(value) !== Array.prototype) return false;
  if (length === undefined) length = value.length;
  if (value.length !== length || Reflect.ownKeys(value).length !== length + 1) return false;
  for (let index = 0; index < length; index++) {
    const descriptor = Object.getOwnPropertyDescriptor(value, String(index));
    if (!descriptor || !Object.hasOwn(descriptor, 'value')) return false;
  }
  return true;
}

function vector(value) {
  return plainArray(value, 2) && Number.isFinite(value[0]) && Number.isFinite(value[1]);
}

function token(value) {
  return typeof value === 'string' && value.length > 0 && value.length <= MAX_TOKEN_LENGTH;
}

function pack(value) {
  const text = Object.is(value, -0) ? '-0' : String(value);
  return `${text.length}:${text}`;
}

function indexKey(i, j) {
  return `${i === 0 ? 0 : i},${j === 0 ? 0 : j}`;
}

function sampleId(namespaceId, i, j) {
  return `GridFixture:Rectangular:1:${pack(namespaceId)}:${indexKey(i, j)}`;
}

function position(origin, basisX, basisY, i, j) {
  return [origin[0] + i * basisX[0] + j * basisY[0], origin[1] + i * basisX[1] + j * basisY[1]];
}

/** Internal rectangular-v1 fixture; no project or public producer contract. */
export function gridSamplesFixtureV1(input) {
  const keys = ['namespaceId', 'frameToken', 'origin', 'spacing', 'angle', 'bounds'];
  if (input && Object.hasOwn(input, 'included')) keys.push('included');
  if (!plainData(input, keys)) throw Error('Grid fixture requires plain descriptor data');
  const { namespaceId, frameToken, origin, spacing, angle, bounds } = input;
  if (!token(namespaceId) || !token(frameToken)) throw Error('Grid fixture needs bounded tokens');
  if (!vector(origin) || !vector(spacing) || spacing[0] <= 0 || spacing[1] <= 0)
    throw Error('Grid fixture needs finite origin and positive finite spacing');
  if (
    !plainData(angle, ['type', 'unit', 'value']) ||
    angle.type !== 'Angle' ||
    angle.unit !== 'degree' ||
    !Number.isFinite(angle.value)
  )
    throw Error('Grid fixture needs a finite degree Angle');
  if (
    !plainData(bounds, ['minI', 'maxI', 'minJ', 'maxJ']) ||
    !['minI', 'maxI', 'minJ', 'maxJ'].every((key) => Number.isSafeInteger(bounds[key]))
  )
    throw Error('Grid fixture needs safe integer lattice bounds');

  if (bounds.minI > bounds.maxI || bounds.minJ > bounds.maxJ)
    throw Error('Grid fixture lattice bounds must not be reversed');
  const columns = bounds.maxI - bounds.minI,
    rows = bounds.maxJ - bounds.minJ,
    empty = columns === 0 || rows === 0;
  if (
    !empty &&
    (!Number.isSafeInteger(columns) ||
      !Number.isSafeInteger(rows) ||
      columns > MAX_CANDIDATES ||
      rows > Math.floor(MAX_CANDIDATES / columns))
  )
    throw Error(`Grid fixture exceeds the ${MAX_CANDIDATES} candidate budget`);

  let included = null;
  if (Object.hasOwn(input, 'included')) {
    if (!Array.isArray(input.included) || input.included.length > MAX_CANDIDATES)
      throw Error('Grid fixture mask exceeds its candidate budget');
    if (!plainArray(input.included)) throw Error('Grid fixture needs a plain lattice mask');
    included = new Set();
    for (let index = 0; index < input.included.length; index++) {
      const item = input.included[index];
      if (!plainArray(item, 2) || !Number.isSafeInteger(item[0]) || !Number.isSafeInteger(item[1]))
        throw Error('Grid fixture mask needs safe integer coordinates');
      included.add(indexKey(item[0], item[1]));
    }
  }

  // JSON has one zero representation. Normalize it in this fixture descriptor
  // so a descriptor round-trip does not create a spurious geometry revision.
  const gridOrigin = [origin[0] === 0 ? 0 : origin[0], origin[1] === 0 ? 0 : origin[1]],
    degrees = angle.value === 0 ? 0 : angle.value,
    radians = ((degrees % 360) * Math.PI) / 180,
    cosine = Math.cos(radians),
    sine = Math.sin(radians),
    basisX = [cosine * spacing[0], sine * spacing[0]],
    basisY = [-sine * spacing[1], cosine * spacing[1]],
    cellArea = Math.abs(basisX[0] * basisY[1] - basisX[1] * basisY[0]);
  if (!vector(basisX) || !vector(basisY) || !Number.isFinite(cellArea) || cellArea === 0)
    throw Error('Grid fixture basis or area is not representable');

  // Reject non-finite coordinates before creating any Sample. The explicit
  // finite candidate cap bounds this pass even for an empty fixture mask.
  if (!empty)
    for (let j = bounds.minJ; j < bounds.maxJ; j++)
      for (let i = bounds.minI; i < bounds.maxI; i++) {
        const x = gridOrigin[0] + i * basisX[0] + j * basisY[0],
          y = gridOrigin[1] + i * basisX[1] + j * basisY[1];
        if (!Number.isFinite(x) || !Number.isFinite(y))
          throw Error('Grid fixture coordinates must remain finite');
      }

  const samples = [],
    memberIds = [];
  if (!empty)
    for (let j = bounds.minJ; j < bounds.maxJ; j++)
      for (let i = bounds.minI; i < bounds.maxI; i++) {
        if (included && !included.has(indexKey(i, j))) continue;
        const gridI = i === 0 ? 0 : i,
          gridJ = j === 0 ? 0 : j,
          id = sampleId(namespaceId, gridI, gridJ);
        memberIds.push(pack(id));
        samples.push({
          id,
          position: position(gridOrigin, basisX, basisY, i, j),
          attributes: {
            gridI: { type: 'Integer', role: 'gridI', value: gridI },
            gridJ: { type: 'Integer', role: 'gridJ', value: gridJ },
          },
        });
      }

  return {
    kind: 'SampleSet',
    version: 1,
    namespaceId,
    identityEpoch: 'Rectangular:1',
    domain: 'Sample',
    space: frameToken,
    membershipRevision: memberIds.join(''),
    geometryRevision: [
      'GridFixture:Rectangular:1',
      frameToken,
      gridOrigin[0],
      gridOrigin[1],
      spacing[0],
      spacing[1],
      degrees,
    ]
      .map(pack)
      .join(''),
    orderingRevision: 'GridFixture:Rectangular:1:j-major:i-minor',
    attributeRevision: 'GridFixture:Rectangular:1:grid-indices',
    grid: {
      lattice: 'Rectangular',
      version: 1,
      origin: gridOrigin,
      spacing: [spacing[0], spacing[1]],
      angle: { type: 'Angle', unit: 'degree', value: degrees },
      basisX,
      basisY,
      cellArea,
    },
    samples,
  };
}
