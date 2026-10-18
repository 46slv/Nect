import { instancePointsGeometry } from './instance-points-geometry.js';
import { LUMINANCE_MASK_LIMITS } from './luminance-mask.js';

export function luminanceInstanceSettings(read) {
  const result = Object.fromEntries(
    ['minScale', 'maxScale', 'minRotation', 'maxRotation'].map((key) => [key, read(key)]),
  );
  for (const key of ['minScale', 'maxScale'])
    if (!Number.isFinite(result[key]) || result[key] < 0 || result[key] > 100)
      throw Error('Luminance Instances scale endpoints must be from 0 to 100');
  for (const key of ['minRotation', 'maxRotation'])
    if (!Number.isFinite(result[key]) || Math.abs(result[key]) > 360)
      throw Error('Luminance Instances angle endpoints must be within ±360 degrees');
  return result;
}

// Nonzero winding, including boundaries, on the shared flattened mask.
export function selectLuminanceCenters(cells, mask, image) {
  if (!Array.isArray(cells) || cells.length > 4096)
    throw Error('Luminance Instances exceeds sampled-cell budget');
  const vertices = mask?.polygons.reduce((sum, polygon) => sum + polygon.length, 0) || 0;
  if (vertices * cells.length > LUMINANCE_MASK_LIMITS.pairWork)
    throw Error('Luminance Instances exceeds mask work budget');
  return cells.filter(({ x, y }) => {
    if (image)
      return (
        x >= image.position[0] &&
        x <= image.position[0] + image.size[0] &&
        y >= image.position[1] &&
        y <= image.position[1] + image.size[1]
      );
    if (!mask) return false;
    let winding = 0;
    for (const polygon of mask.polygons)
      for (let i = 0; i < polygon.length; i++) {
        const [ax, ay] = polygon[i],
          [bx, by] = polygon[(i + 1) % polygon.length];
        const cross = (bx - ax) * (y - ay) - (x - ax) * (by - ay);
        if (
          cross === 0 &&
          x >= Math.min(ax, bx) &&
          x <= Math.max(ax, bx) &&
          y >= Math.min(ay, by) &&
          y <= Math.max(ay, by)
        )
          return true;
        if (ay <= y && by > y && cross > 0) winding++;
        if (ay > y && by <= y && cross < 0) winding--;
      }
    return winding !== 0;
  });
}

export function luminanceInstancesGeometry(motif, cells, options) {
  const settings = luminanceInstanceSettings((key) => options[key]);
  if (!Array.isArray(cells) || cells.length > 4096)
    throw Error('Luminance Instances exceeds sampled-cell budget');
  const active = [];
  for (const cell of cells) {
    if (
      !cell ||
      !Number.isFinite(cell.x) ||
      !Number.isFinite(cell.y) ||
      !Number.isFinite(cell.darkness) ||
      cell.darkness < 0 ||
      cell.darkness > 1
    )
      throw Error('Luminance Instances needs finite sampled cells');
    const scale = settings.minScale + (settings.maxScale - settings.minScale) * cell.darkness;
    const rotation =
      settings.minRotation + (settings.maxRotation - settings.minRotation) * cell.darkness;
    if (scale !== 0) active.push({ cell, scale, rotation });
  }
  const targets = {
    contours: [
      {
        id: 'grid',
        closed: false,
        points: active.map(({ cell }, i) => ({
          id: `grid:${i}`,
          position: [cell.x, cell.y],
          handleA: [0, 0],
          handleB: [0, 0],
        })),
      },
    ],
  };
  const result = instancePointsGeometry(motif, targets, {
    ...options,
    transforms: active.map(({ scale, rotation }) => ({ scale, rotation })),
  });
  delete result.fill;
  delete result.stroke;
  return result;
}
