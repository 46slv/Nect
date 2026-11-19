import { toPaper } from './paper-runtime.js';

const vector = (value) =>
  Array.isArray(value) && value.length === 2 && value.every(Number.isFinite);
function sampleSpine(spine, a, b, sampleCount, orient, label = 'Spine Steps') {
  const contour = spine?.contours?.[0];
  if (
    !spine ||
    spine.kind ||
    spine.mask ||
    spine.contours?.length !== 1 ||
    contour?.closed !== false ||
    contour.group !== undefined ||
    !Array.isArray(contour.points) ||
    contour.points.length < 2 ||
    contour.points.length > 512 ||
    contour.points.some((p) => !['position', 'handleA', 'handleB'].every((key) => vector(p[key])))
  )
    throw Error(`${label} requires one open finite path with 2 to 512 anchors`);
  const items = [];
  try {
    const item = toPaper(spine);
    items.push(item);
    const pa = toPaper({ contours: [a] });
    items.push(pa);
    const pb = toPaper({ contours: [b] });
    items.push(pb);
    const path = item.children[0],
      length = path.length;
    const ca = [pa.bounds.center.x, pa.bounds.center.y],
      cb = [pb.bounds.center.x, pb.bounds.center.y];
    if (!Number.isFinite(length) || length <= 0 || !vector(ca) || !vector(cb))
      throw Error(`${label} requires positive finite length and source bounds`);
    const count = sampleCount(length);
    return Array.from({ length: count }, (_, i) => {
      const t = i / (count - 1),
        sample =
          i === 0
            ? contour.points[0].position
            : i === count - 1
              ? contour.points.at(-1).position
              : (() => {
                  const p = path.getPointAt(length * t);
                  return p ? [p.x, p.y] : null;
                })();
      if (!vector(sample)) throw Error(`${label} cannot sample a finite position`);
      const offset = sample.map((value, j) => value - ((1 - t) * ca[j] + t * cb[j]));
      if (!vector(offset)) throw Error(`${label} exceeds finite coordinates`);
      if (!orient) return offset;
      const location = path.getLocationAt(length * t);
      const tangent = sampleTangent(path, location);
      return {
        position: sample,
        pivot: ca.map((value, j) => (1 - t) * value + t * cb[j]),
        tangent,
      };
    });
  } finally {
    for (const item of items) item.remove();
  }
}

export function translateBlendContour(contour, offset, label = 'Spine Steps') {
  const points = contour.points.map((point) => {
    const position = point.position.map((value, j) => value + offset[j]);
    if (!vector(position)) throw Error(`${label} exceeds finite coordinates`);
    return { ...point, position };
  });
  return { ...contour, points };
}

export function blendSpineOffsets(spine, a, b, count) {
  return sampleSpine(spine, a, b, () => count, false);
}
export function blendSpineOrientations(spine, a, b, count) {
  return sampleSpine(spine, a, b, () => count, true);
}

export function blendSpineDistanceOffsets(spine, a, b, distance, checkBudget) {
  if (!Number.isFinite(distance) || distance <= 0)
    throw Error('Spine Distance must be positive and finite');
  return sampleSpine(
    spine,
    a,
    b,
    (length) => {
      const intervals = Math.max(1, Math.ceil(length / distance));
      if (intervals > 255) throw Error('Spine Distance exceeds 255 intervals');
      checkBudget(intervals - 1);
      return intervals + 1;
    },
    false,
    'Spine Distance',
  );
}

function unitTangent(value) {
  const tangent = value && [value.x, value.y];
  const length = tangent && Math.hypot(...tangent);
  if (!vector(tangent) || !Number.isFinite(length) || length === 0)
    throw Error('Spine tangent Steps requires a finite nonzero tangent at every sample');
  return tangent.map((value) => value / length);
}
function sampleTangent(path, location) {
  if (!location || !Number.isFinite(location.time))
    throw Error('Spine tangent Steps cannot locate a tangent sample');
  const curves = path.curves,
    index = location.curve.index,
    time = location.time;
  const tangent = unitTangent(location.curve.getTangentAtTime(time));
  // At an exact Paper curve boundary both one-sided directions must agree.
  // Never probe offset +/- epsilon: that can silently smooth a real corner.
  const neighbor =
    time === 0 && index > 0
      ? curves[index - 1].getTangentAtTime(1)
      : time === 1 && index < curves.length - 1
        ? curves[index + 1].getTangentAtTime(0)
        : null;
  if (neighbor) {
    const other = unitTangent(neighbor);
    if (tangent.some((value, j) => Math.abs(value - other[j]) > 1e-10))
      throw Error('Spine tangent Steps cannot orient a sample at a discontinuous join');
  } else if ((time === 0 && index > 0) || (time === 1 && index < curves.length - 1)) {
    throw Error('Spine tangent Steps requires a tangent on both sides of a sampled join');
  }
  return tangent;
}

export function orientBlendContour(contour, placement) {
  const [c, s] = placement.tangent;
  const rotate = ([x, y]) => [c * x - s * y, s * x + c * y];
  const points = contour.points.map((point) => {
    const relative = point.position.map((value, j) => value - placement.pivot[j]);
    const position = rotate(relative).map((value, j) => value + placement.position[j]);
    const handleA = rotate(point.handleA),
      handleB = rotate(point.handleB);
    if (![position, handleA, handleB].every(vector))
      throw Error('Spine tangent Steps exceeds finite coordinates');
    return { ...point, position, handleA, handleB };
  });
  return { ...contour, points };
}
