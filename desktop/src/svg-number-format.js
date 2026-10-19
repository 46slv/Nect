export const SVG_EXPORT_PRECISIONS = Object.freeze([2, 3, 4, 5, 6, 7, 8]);

export function validateSvgPrecision(precision) {
  if (precision !== undefined && !SVG_EXPORT_PRECISIONS.includes(precision))
    throw Error('SVG geometry precision must be an integer from 2 to 8');
  return precision;
}

export function formatSvgNumber(value, precision) {
  if (precision === undefined) return String(value);
  if (!Number.isFinite(value)) throw Error('SVG geometry values must be finite');
  const multiplier = 10 ** precision,
    scaled = value * multiplier,
    rounded = Number.isFinite(scaled) ? Math.round(scaled) / multiplier : value;
  return String(Object.is(rounded, -0) ? 0 : rounded);
}

export function formatSvgExtent(value, precision) {
  const formatted = formatSvgNumber(value, precision);
  if (precision !== undefined && value > 0 && Number(formatted) === 0)
    throw Error(
      `SVG precision ${precision} decimals collapses small geometry; choose a higher precision or Default`,
    );
  return formatted;
}
