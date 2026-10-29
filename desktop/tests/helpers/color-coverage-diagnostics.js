// Failure receipts only, never production attribution or an acceptance oracle.
// Keep CI output bounded even when every sampled pixel disagrees.
export function summarizeCoverageRasters(full, components, width, height, probes = []) {
  const pixels = width * height;
  if (
    !Number.isInteger(width) ||
    !Number.isInteger(height) ||
    width < 1 ||
    height < 1 ||
    width > 256 ||
    height > 256 ||
    !Array.isArray(components) ||
    components.length !== 4 ||
    !Array.isArray(probes) ||
    probes.length > 16
  )
    throw Error('Invalid bounded coverage diagnostics');
  for (const mask of [full, ...components]) {
    if (!mask || mask.length !== pixels) throw Error('Mismatched diagnostic raster');
    for (const alpha of mask)
      if (!Number.isFinite(alpha) || alpha < 0 || alpha > 1)
        throw Error('Invalid diagnostic alpha');
  }
  const at = (pixel) => {
    const values = components.map((mask) => mask[pixel]);
    const sum = values.reduce((total, alpha) => total + alpha, 0);
    return {
      pixel,
      x: pixel % width,
      y: Math.floor(pixel / width),
      full: full[pixel],
      components: values,
      sum,
      difference: sum - full[pixel],
    };
  };
  let fullMass = 0,
    componentSumMass = 0,
    maxAbsoluteDifference = 0,
    beyondQuantizationPixels = 0,
    missingPositiveMassPixels = 0,
    surplusPixels = 0,
    deficitPixels = 0;
  const componentMasses = [0, 0, 0, 0],
    worstPixels = [];
  for (let pixel = 0; pixel < pixels; pixel++) {
    const sample = at(pixel),
      error = Math.abs(sample.difference);
    fullMass += sample.full;
    componentSumMass += sample.sum;
    sample.components.forEach((alpha, i) => {
      componentMasses[i] += alpha;
    });
    maxAbsoluteDifference = Math.max(maxAbsoluteDifference, error);
    if (error > 4 / 255 + 1e-7) beyondQuantizationPixels++;
    if (sample.full > 0 && sample.sum === 0) missingPositiveMassPixels++;
    if (sample.difference > 1e-7) surplusPixels++;
    if (sample.difference < -1e-7) deficitPixels++;
    if (error > 0 && (worstPixels.length < 16 || error > Math.abs(worstPixels.at(-1).difference))) {
      worstPixels.push(sample);
      worstPixels.sort(
        (a, b) => Math.abs(b.difference) - Math.abs(a.difference) || a.pixel - b.pixel,
      );
      if (worstPixels.length > 16) worstPixels.pop();
    }
  }
  const probePixels = probes.map(([x, y]) => {
    if (!Number.isInteger(x) || !Number.isInteger(y) || x < 0 || y < 0 || x >= width || y >= height)
      throw Error('Invalid diagnostic probe');
    return at(y * width + x);
  });
  return {
    width,
    height,
    fullMass,
    componentMasses,
    componentSumMass,
    maxAbsoluteDifference,
    beyondQuantizationPixels,
    missingPositiveMassPixels,
    surplusPixels,
    deficitPixels,
    worstPixels,
    probes: probePixels,
  };
}
