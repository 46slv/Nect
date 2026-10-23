import { sampleImageLuminanceGrid } from './image-luminance-field.js';
import { generateLuminanceMarks } from './luminance-marks.js';
import { ditherSettings } from './dither-controls.js';
import { validateTonePower } from './luminance-tone.js';

export const MAX_IMAGE_DITHER_PIXELS = 4_000_000;
export const MAX_IMAGE_DITHER_EDGE_SAMPLES = 8_000_000;
export const MAX_ACTIVE_IMAGE_DITHERS = 8;
const COVERAGE_SAMPLES = 4;
const recipeKeys = [
  'type',
  'mode',
  'position',
  'size',
  'pitch',
  'origin',
  'invert',
  'tonePower',
  'shape',
  'dotSize',
  'sides',
  'rotation',
  'innerRatio',
];

/** A small runtime recipe; PNG/pixel data never enters authored nodes or history. */
export function imageDitherSettings(recipe) {
  if (
    !recipe ||
    recipe.type !== 'dither' ||
    Object.keys(recipe).some((key) => !recipeKeys.includes(key)) ||
    !['dither', 'error-diffusion'].includes(recipe.mode) ||
    ![recipe.position, recipe.size].every(
      (point) => Array.isArray(point) && point.length === 2 && point.every(Number.isFinite),
    ) ||
    recipe.size.some((value) => value <= 0 || value > 100000) ||
    !recipe.origin ||
    Object.keys(recipe.origin).length !== 2 ||
    !['x', 'y'].every(
      (axis) => Number.isFinite(recipe.origin[axis]) && Math.abs(recipe.origin[axis]) <= 100000,
    ) ||
    !Number.isFinite(recipe.pitch) ||
    recipe.pitch < 1 ||
    recipe.pitch > 1024 ||
    typeof recipe.invert !== 'boolean' ||
    recipe.shape === 'legacy'
  )
    throw Error('Invalid Image Dither recipe');
  return {
    type: 'dither',
    mode: recipe.mode,
    position: [...recipe.position],
    size: [...recipe.size],
    pitch: recipe.pitch,
    origin: { ...recipe.origin },
    invert: recipe.invert,
    tonePower: validateTonePower(recipe.tonePower),
    ...ditherSettings(
      recipe.shape,
      recipe.dotSize,
      recipe.sides,
      recipe.rotation,
      recipe.innerRatio,
    ),
  };
}

function markEdges(marks, width, height, position, size) {
  const edges = [];
  let work = 0;
  for (const { vertices } of marks) {
    const points = vertices.map(([x, y]) => [
      ((x - position[0]) / size[0]) * width,
      ((y - position[1]) / size[1]) * height,
    ]);
    if (points.some((point) => !point.every(Number.isFinite)))
      throw Error('Image Dither coordinates exceed the finite raster range');
    for (let i = 0; i < points.length; i++) {
      const [x, y] = points[i],
        [nextX, nextY] = points[(i + 1) % points.length];
      if (y === nextY) continue;
      const start = Math.max(0, Math.ceil(Math.min(y, nextY) * COVERAGE_SAMPLES - 0.5)),
        end = Math.min(
          height * COVERAGE_SAMPLES,
          Math.ceil(Math.max(y, nextY) * COVERAGE_SAMPLES - 0.5),
        );
      if (start >= end) continue;
      work += end - start;
      if (work > MAX_IMAGE_DITHER_EDGE_SAMPLES)
        throw Error(
          'Image Dither raster work exceeds 8000000 edge samples; reduce dot size or detail',
        );
      const slope = (nextX - x) / (nextY - y);
      if (!Number.isFinite(slope)) throw Error('Image Dither edge exceeds the finite raster range');
      edges.push({ start, end, x, y, slope, winding: nextY > y ? 1 : -1 });
    }
  }
  return edges.sort((a, b) => a.start - b.start);
}

function coverSpan(coverage, left, right, width) {
  const first = Math.max(0, Math.ceil(left * COVERAGE_SAMPLES - 0.5)),
    end = Math.min(width * COVERAGE_SAMPLES, Math.ceil(right * COVERAGE_SAMPLES - 0.5));
  if (first >= end) return;
  const from = Math.floor(first / COVERAGE_SAMPLES),
    to = Math.floor((end - 1) / COVERAGE_SAMPLES);
  if (from === to) coverage[from] += end - first;
  else {
    coverage[from] += COVERAGE_SAMPLES - (first % COVERAGE_SAMPLES);
    for (let x = from + 1; x < to; x++) coverage[x] += COVERAGE_SAMPLES;
    coverage[to] += ((end - 1) % COVERAGE_SAMPLES) + 1;
  }
}

/** Only generated black Dither polygons, unioned and clipped to Image support. */
export function renderImageDither(asset, recipe) {
  const settings = imageDitherSettings(recipe),
    { width, height } = asset;
  if (
    !Number.isInteger(width) ||
    !Number.isInteger(height) ||
    width < 1 ||
    height < 1 ||
    width * height > MAX_IMAGE_DITHER_PIXELS
  )
    throw Error('Image Dither is limited to 4 megapixels');
  const field = sampleImageLuminanceGrid(asset, settings.position, settings.size, {
    pitch: settings.pitch,
    origin: settings.origin,
    invert: settings.invert,
    tonePower: settings.tonePower,
  });
  const { marks } = generateLuminanceMarks({ ...settings, cells: field.cells });
  // Work admission precedes the output allocation. One coverage row avoids a
  // supersampled RGBA surface; all marks share one NonZero black silhouette.
  const edges = markEdges(marks, width, height, settings.position, settings.size),
    rgba = new Uint8Array(width * height * 4),
    coverage = new Uint8Array(width);
  let cursor = 0,
    active = [];
  for (let y = 0; y < height; y++) {
    coverage.fill(0);
    for (let sy = 0; sy < COVERAGE_SAMPLES; sy++) {
      const row = y * COVERAGE_SAMPLES + sy,
        sampleY = (row + 0.5) / COVERAGE_SAMPLES;
      active = active.filter((edge) => edge.end > row);
      while (cursor < edges.length && edges[cursor].start <= row) active.push(edges[cursor++]);
      const crossings = active
        .map((edge) => {
          const x = edge.x + (sampleY - edge.y) * edge.slope;
          if (!Number.isFinite(x))
            throw Error('Image Dither crossing exceeds the finite raster range');
          return { x: Math.max(0, Math.min(width, x)), winding: edge.winding };
        })
        .sort((a, b) => a.x - b.x);
      let winding = 0,
        left = 0;
      for (const crossing of crossings) {
        if (winding === 0) left = crossing.x;
        winding += crossing.winding;
        if (winding === 0) coverSpan(coverage, left, crossing.x, width);
      }
    }
    for (let x = 0; x < width; x++)
      rgba[(y * width + x) * 4 + 3] = Math.round((coverage[x] * 255) / COVERAGE_SAMPLES ** 2);
  }
  return rgba;
}
