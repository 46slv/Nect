const MAX_CELLS = 4096;
const MAX_VERTICES = 100000;
const BAYER_BASE = [
  [0, 2],
  [3, 1],
];

function bayer8() {
  let matrix = BAYER_BASE.map((row) => row.slice());
  while (matrix.length < 8) {
    const size = matrix.length;
    const next = Array.from({ length: size * 2 }, () => Array(size * 2));
    for (let y = 0; y < size; y++) {
      for (let x = 0; x < size; x++) {
        const rank = matrix[y][x] * 4;
        next[y][x] = rank;
        next[y][x + size] = rank + 2;
        next[y + size][x] = rank + 3;
        next[y + size][x + size] = rank + 1;
      }
    }
    matrix = next;
  }
  return matrix;
}

const BAYER_8 = bayer8();
const positiveModulo = (value, divisor) => ((value % divisor) + divisor) % divisor;

function validateOptions(options) {
  if (!options || typeof options !== 'object' || Array.isArray(options))
    throw Error('Luminance mark options must be an object');
  const { cells, pitch, mode } = options;
  if (!Array.isArray(cells)) throw Error('Luminance mark cells must be an array');
  if (cells.length > MAX_CELLS)
    throw Error(`Luminance mark generation is limited to ${MAX_CELLS} cells`);
  if (!Number.isFinite(pitch) || pitch <= 0)
    throw Error('Luminance mark pitch must be a positive finite number');
  if (mode !== 'halftone' && mode !== 'dither')
    throw Error('Luminance mark mode must be halftone or dither');

  for (const cell of cells) {
    if (
      !cell ||
      typeof cell !== 'object' ||
      !Number.isInteger(cell.i) ||
      !Number.isInteger(cell.j) ||
      !Number.isFinite(cell.x) ||
      !Number.isFinite(cell.y) ||
      !Number.isFinite(cell.darkness) ||
      cell.darkness < 0 ||
      cell.darkness > 1
    )
      throw Error('Each luminance cell needs integer i/j, finite x/y, and darkness from 0 to 1');
    if (!Number.isFinite(cell.x - pitch / 2) || !Number.isFinite(cell.x + pitch / 2))
      throw Error('Luminance cell coordinates exceed the finite document range');
    if (!Number.isFinite(cell.y - pitch / 2) || !Number.isFinite(cell.y + pitch / 2))
      throw Error('Luminance cell coordinates exceed the finite document range');
  }
}

function segmentCount(pitch) {
  const maximumRadius = pitch / Math.SQRT2;
  for (let segments = 24; segments <= 256; segments += 8) {
    const sagitta = maximumRadius * (1 - Math.cos(Math.PI / segments));
    if (sagitta <= 0.1) return segments;
  }
  throw Error('Halftone pitch is too large to keep polygon sagitta at or below 0.1 document unit');
}

function clipAgainst(polygon, axis, bound, keepGreater) {
  if (polygon.length === 0) return polygon;
  const result = [];
  const coordinate = (point) => point[axis];
  const inside = (point) => (keepGreater ? coordinate(point) >= bound : coordinate(point) <= bound);
  let previous = polygon[polygon.length - 1];
  let previousInside = inside(previous);

  for (const current of polygon) {
    const currentInside = inside(current);
    if (currentInside !== previousInside) {
      const delta = coordinate(current) - coordinate(previous);
      const t = (bound - coordinate(previous)) / delta;
      result.push([
        axis === 0 ? bound : previous[0] + t * (current[0] - previous[0]),
        axis === 1 ? bound : previous[1] + t * (current[1] - previous[1]),
      ]);
    }
    if (currentInside) result.push(current);
    previous = current;
    previousInside = currentInside;
  }
  return result;
}

function clippedDisk(radius, segments) {
  let polygon = Array.from({ length: segments }, (_, index) => {
    const angle = Math.PI / 4 + (2 * Math.PI * index) / segments;
    return [radius * Math.cos(angle), radius * Math.sin(angle)];
  });
  polygon = clipAgainst(polygon, 0, -0.5, true);
  polygon = clipAgainst(polygon, 0, 0.5, false);
  polygon = clipAgainst(polygon, 1, -0.5, true);
  return clipAgainst(polygon, 1, 0.5, false);
}

function polygonArea(polygon) {
  let twiceArea = 0;
  for (let index = 0; index < polygon.length; index++) {
    const current = polygon[index];
    const next = polygon[(index + 1) % polygon.length];
    twiceArea += current[0] * next[1] - next[0] * current[1];
  }
  return Math.abs(twiceArea) / 2;
}

function calibratedDisk(darkness, segments) {
  const targetArea = darkness;
  let low = 0;
  let high = Math.SQRT1_2;
  for (let step = 0; step < 32; step++) {
    const middle = (low + high) / 2;
    if (polygonArea(clippedDisk(middle, segments)) < targetArea) low = middle;
    else high = middle;
  }
  return clippedDisk((low + high) / 2, segments);
}

function squareVertices(cell, pitch) {
  const half = pitch / 2;
  return [
    [cell.x - half, cell.y - half],
    [cell.x + half, cell.y - half],
    [cell.x + half, cell.y + half],
    [cell.x - half, cell.y + half],
  ];
}

function markable(cell, mode) {
  if (mode === 'halftone') return cell.darkness > 0;
  const rank = BAYER_8[positiveModulo(cell.j, 8)][positiveModulo(cell.i, 8)];
  return cell.darkness > (rank + 0.5) / 64;
}

function preflight(options) {
  validateOptions(options);
  const { cells, pitch, mode } = options;
  const segments = mode === 'halftone' ? segmentCount(pitch) : null;
  let maximumVertices = 0;
  for (const cell of cells) {
    if (!markable(cell, mode)) continue;
    maximumVertices += mode === 'dither' || cell.darkness === 1 ? 4 : segments + 4;
    if (maximumVertices > MAX_VERTICES)
      throw Error(`Luminance mark output is limited to ${MAX_VERTICES} generated vertices`);
  }
  return segments;
}

/**
 * Create pure-black geometry for sampled luminance cells. Cell x/y values are
 * document-space centers; output vertices are in document-space coordinates.
 * Halftone area calibration describes vector geometry, not printed appearance.
 */
export function generateLuminanceMarks(options) {
  const segments = preflight(options);
  const { cells, pitch, mode } = options;
  const marks = [];

  for (const cell of cells) {
    if (!markable(cell, mode)) continue;
    if (mode === 'dither' || cell.darkness === 1) {
      marks.push({ i: cell.i, j: cell.j, vertices: squareVertices(cell, pitch) });
      continue;
    }
    const normalized = calibratedDisk(cell.darkness, segments);
    const vertices = normalized.map(([x, y]) => [cell.x + x * pitch, cell.y + y * pitch]);
    marks.push({ i: cell.i, j: cell.j, vertices });
  }

  return { mode, pitch, marks };
}
