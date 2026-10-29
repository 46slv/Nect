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
    passes.length > 256 ||
    typeof rasterize !== 'function' ||
    typeof isCurrent !== 'function'
  )
    throw Error('Invalid coverage sampling request');
  const ids = [...slotIds];
  let svgBytes = 0;
  const svgInput = (svg) => {
    if (typeof svg !== 'string' || svg.length > 4 * 1024 * 1024)
      throw Error('Invalid coverage paint pass');
    svgBytes += svg.length;
    if (svgBytes > 16 * 1024 * 1024) throw Error('Coverage SVG budget exceeded');
    return svg;
  };
  const inputs = Array.from(passes, (pass) => {
    if (!pass) throw Error('Invalid coverage paint pass');
    const svg = svgInput(pass.svg);
    let split;
    if (pass.split !== undefined) {
      const source = pass.split;
      if (!source || typeof source !== 'object' || Array.isArray(source))
        throw Error('Invalid two-stop coverage attribution');
      split = {
        startSlotId: source.startSlotId,
        endSlotId: source.endSlotId,
        startSvg: svgInput(source.startSvg),
      };
    }
    measureColorCoverage(
      ids,
      [
        {
          slotId: pass.slotId,
          alpha: [],
          ...(split ? { split: { ...split, startAlpha: [] } } : {}),
        },
      ],
      0,
    );
    return { slotId: pass.slotId, svg, ...(split ? { split } : {}) };
  });
  // Count the auxiliary stop masks in the same total storage/decode budget.
  const size = coverageSampleSize(
    bounds.width,
    bounds.height,
    inputs.reduce((count, pass) => count + (pass.split ? 2 : 1), 0),
  );
  // Validate registry before starting an expensive asynchronous decode.
  measureColorCoverage(ids, [], 0);
  const guard = () => {
    if (!isCurrent()) throw Error('Coverage sample is stale');
  };
  guard();
  const layers = [];
  const decode = async (svg) => {
    guard();
    const alpha = await rasterize(svg, { ...size });
    guard();
    measureColorCoverage(ids, [{ slotId: null, alpha }], size.width * size.height);
    // Copy borrowed buffers before another decoder call can rewrite them.
    return Float32Array.from(alpha);
  };
  for (const pass of inputs) {
    const alpha = await decode(pass.svg);
    let split;
    if (pass.split) {
      const startAlpha = await decode(pass.split.startSvg);
      // Independent SVG 8-bit alpha rounding can overshoot the full mask by
      // one quantum. Preserve the actual full paint, never invent extra mass.
      for (let pixel = 0; pixel < alpha.length; pixel++) {
        if (startAlpha[pixel] - alpha[pixel] > 2 / 255 + 1e-7)
          throw Error('Stop contribution exceeds paint alpha');
        startAlpha[pixel] = Math.min(startAlpha[pixel], alpha[pixel]);
      }
      split = {
        startSlotId: pass.split.startSlotId,
        endSlotId: pass.split.endSlotId,
        startAlpha,
      };
    }
    layers.push({ slotId: pass.slotId, alpha, ...(split ? { split } : {}) });
  }
  guard();
  return {
    ...measureColorCoverage(ids, layers, size.width * size.height),
    sampleSize: size,
    approximate: true,
  };
}
