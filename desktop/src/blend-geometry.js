import { subdividePathGeometry } from './subdivide-geometry.js';
// Geometry-only interpolation for one pair of compatible closed contours.
// Appearance and node integration are intentionally handled separately.
const vector = (v) => Array.isArray(v) && v.length === 2 && v.every(Number.isFinite);
const mix = (a, b, t) => {
  const result = a.map((x, i) => x + (b[i] - x) * t);
  if (!result.every(Number.isFinite)) throw Error('Blend exceeds finite coordinates');
  return result;
};

function validContour(c) {
  return (
    c?.closed === true &&
    Array.isArray(c.points) &&
    c.points.length >= 2 &&
    c.points.every((p) => vector(p.position) && vector(p.handleA) && vector(p.handleB))
  );
}

function reversedPoint(p) {
  return { ...p, handleA: p.handleB, handleB: p.handleA };
}

// Compare normalized positions so translation and scale do not dictate seam selection.
function normalized(points) {
  const cx = points.reduce((s, p) => s + p.position[0], 0) / points.length;
  const cy = points.reduce((s, p) => s + p.position[1], 0) / points.length;
  const size =
    Math.sqrt(
      points.reduce((s, p) => s + (p.position[0] - cx) ** 2 + (p.position[1] - cy) ** 2, 0),
    ) || 1;
  if (![cx, cy, size].every(Number.isFinite))
    throw Error('Blend alignment exceeds finite coordinates');
  const result = points.map((p) => [(p.position[0] - cx) / size, (p.position[1] - cy) / size]);
  if (!result.every(vector)) throw Error('Blend alignment exceeds finite coordinates');
  return result;
}

function align(a, b) {
  const an = normalized(a),
    bn = normalized(b),
    n = a.length;
  let best = Infinity,
    offset = 0,
    direction = 1;
  for (const dir of [1, -1])
    for (let start = 0; start < n; start++) {
      let score = 0;
      for (let i = 0; i < n; i++) {
        const q = bn[(start + dir * i + n) % n];
        score += (an[i][0] - q[0]) ** 2 + (an[i][1] - q[1]) ** 2;
      }
      if (score < best) {
        best = score;
        offset = start;
        direction = dir;
      }
    }
  return a.map((_, i) => {
    const p = b[(offset + direction * i + n) % n];
    return direction === -1 ? reversedPoint(p) : p;
  });
}

// Split each source edge exactly; unlike resampling, Bezier geometry is retained.
export function blendPointCount(aCount, bCount) {
  if (![aCount, bCount].every((n) => Number.isInteger(n) && n >= 2))
    throw Error('Blend requires valid contour point counts');
  let x = aCount,
    y = bCount;
  while (y) [x, y] = [y, x % y];
  const count = (aCount / x) * bCount;
  if (count > 512 || count / aCount > 32 || count / bCount > 32)
    throw Error('Blend contour subdivision exceeds geometry budget');
  return count;
}

export function copyClosedContourEndpoint(contour, id) {
  if (!validContour(contour)) throw Error('Blend requires valid closed contours');
  if (contour.points.length > 512) throw Error('Blend contour exceeds 512 points');
  return {
    id,
    closed: true,
    points: contour.points.map((p, i) => ({
      id: `${id}:p${i}`,
      position: [...p.position],
      handleA: [...p.handleA],
      handleB: [...p.handleB],
    })),
  };
}

export function prepareClosedContourBlend(a, b) {
  if (!validContour(a) || !validContour(b)) throw Error('Blend requires valid closed contours');
  for (const c of [a, b]) {
    const positions = c.points.map((p) => JSON.stringify(p.position));
    if (new Set(positions).size !== positions.length)
      throw Error('Blend cannot align repeated anchor positions');
  }
  if (a.points.length > 512 || b.points.length > 512)
    throw Error('Blend contour exceeds 512 points');
  const count = blendPointCount(a.points.length, b.points.length);
  const split = (contour, side) =>
    subdividePathGeometry(
      { contours: [contour] },
      { segments: count / contour.points.length, prefix: `blend:${side}` },
    ).contours[0];
  const first = a.points.length === count ? a : split(a, 'a');
  const second = b.points.length === count ? b : split(b, 'b');
  const aligned = align(first.points, second.points);

  return (t, id = 'blend') => {
    if (!Number.isFinite(t) || t < 0 || t > 1)
      throw Error('Blend interpolation must be between 0 and 1');
    if (t === 0) return copyClosedContourEndpoint(a, id);
    if (t === 1) return copyClosedContourEndpoint(b, id);
    return {
      id,
      closed: true,
      points: first.points.map((p, i) => ({
        id: `${id}:p${i}`,
        position: mix(p.position, aligned[i].position, t),
        handleA: mix(p.handleA, aligned[i].handleA, t),
        handleB: mix(p.handleB, aligned[i].handleB, t),
      })),
    };
  };
}

export function interpolateClosedContours(a, b, t, id = 'blend') {
  return prepareClosedContourBlend(a, b)(t, id);
}
