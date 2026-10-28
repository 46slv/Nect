import { measureColorCoverage } from './color-coverage.js';

// Bound temporary alpha storage across all passes, not just each canvas.
const MAX_SAMPLES = 4 * 1024 * 1024;
export function coverageSampleSize(width, height, passCount) {
  if (
    ![width, height].every((v) => Number.isFinite(v) && v > 0 && v <= 100000) ||
    !Number.isInteger(passCount) ||
    passCount < 0 ||
    passCount > 256
  )
    throw Error('Invalid coverage sampling bounds');
  const scale = Math.min(1, 256 / Math.max(width, height));
  const w = Math.max(1, Math.floor(width * scale));
  const h = Math.max(1, Math.floor(height * scale));
  if (w * h * passCount > MAX_SAMPLES) throw Error('Coverage sampling budget exceeded');
  return { width: w, height: h };
}

// rasterize must return normalized alpha, and owns decode/canvas cleanup.
// A stale or cancelled asynchronous sample never returns a usable result.
export async function sampleColorCoverage(
  slotIds,
  passes,
  bounds,
  rasterize,
  isCurrent = () => true,
) {
  if (
    !Array.isArray(slotIds) ||
    !Array.isArray(passes) ||
    typeof rasterize !== 'function' ||
    typeof isCurrent !== 'function'
  )
    throw Error('Invalid coverage sampling request');
  const size = coverageSampleSize(bounds.width, bounds.height, passes.length);
  const ids = [...slotIds];
  let svgBytes = 0;
  const inputs = Array.from(passes, (pass) => {
    if (!pass || typeof pass.svg !== 'string' || pass.svg.length > 4 * 1024 * 1024)
      throw Error('Invalid coverage paint pass');
    svgBytes += pass.svg.length;
    if (svgBytes > 16 * 1024 * 1024) throw Error('Coverage SVG budget exceeded');
    measureColorCoverage(ids, [{ slotId: pass.slotId, alpha: [] }], 0);
    return { slotId: pass.slotId, svg: pass.svg };
  });
  // Validate registry before starting an expensive asynchronous decode.
  measureColorCoverage(ids, [], 0);
  const guard = () => {
    if (!isCurrent()) throw Error('Coverage sample is stale');
  };
  guard();
  const layers = [];
  for (const pass of inputs) {
    guard();
    const alpha = await rasterize(pass.svg, { ...size });
    guard();
    measureColorCoverage(ids, [{ slotId: pass.slotId, alpha }], size.width * size.height);
    // Copy borrowed buffers so a later decoder call cannot rewrite prior masks.
    const copy =
      alpha instanceof Float32Array || alpha instanceof Float64Array || Array.isArray(alpha)
        ? Float32Array.from(alpha)
        : null;
    measureColorCoverage(ids, [{ slotId: pass.slotId, alpha: copy }], size.width * size.height);
    layers.push({ slotId: pass.slotId, alpha: copy });
  }
  guard();
  return {
    ...measureColorCoverage(ids, layers, size.width * size.height),
    sampleSize: size,
    approximate: true,
  };
}
