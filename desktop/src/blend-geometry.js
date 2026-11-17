// Geometry-only interpolation for one pair of compatible closed contours.
// Appearance and node integration are intentionally handled separately.
const vector = (v) => Array.isArray(v) && v.length === 2 && v.every(Number.isFinite);
const mix = (a, b, t) => a.map((x, i) => x + (b[i] - x) * t);

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
  return points.map((p) => [(p.position[0] - cx) / size, (p.position[1] - cy) / size]);
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

export function interpolateClosedContours(a, b, t, id = 'blend') {
  if (!validContour(a) || !validContour(b)) throw Error('Blend requires valid closed contours');
  for (const c of [a, b]) {
    const positions = c.points.map((p) => JSON.stringify(p.position));
    if (new Set(positions).size !== positions.length)
      throw Error('Blend cannot align repeated anchor positions');
  }
  if (a.points.length > 512 || b.points.length > 512)
    throw Error('Blend contour exceeds 512 points');
  if (a.points.length !== b.points.length) throw Error('Blend requires matching point counts');
  if (!Number.isFinite(t) || t < 0 || t > 1)
    throw Error('Blend interpolation must be between 0 and 1');
  const aligned = align(a.points, b.points);
  return {
    id,
    closed: true,
    points: a.points.map((p, i) => ({
      id: `${id}:p${i}`,
      position: mix(p.position, aligned[i].position, t),
      handleA: mix(p.handleA, aligned[i].handleA, t),
      handleB: mix(p.handleB, aligned[i].handleB, t),
    })),
  };
}
