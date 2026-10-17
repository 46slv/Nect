// Artistic darkness response, after linear-light alpha/luminance and invert.
// This is not a change to the source color space or its rendering.
export function validateTonePower(value = 1) {
  if (!Number.isFinite(value) || value < 0.1 || value > 10)
    throw Error('Luminance tone power must be finite and from 0.1 to 10');
  return value;
}

export function toneDarkness(luminance, invert, power) {
  const darkness = invert ? luminance : 1 - luminance;
  // Preserve the exact pre-tone numerical result at the migrated default.
  return power === 1 ? darkness : Math.pow(darkness, power);
}
