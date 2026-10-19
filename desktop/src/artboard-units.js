export const ARTBOARD_UNITS = Object.freeze(['px', 'mm', 'cm']);

export const CSS_PIXELS_PER_INCH = 96;
export const MILLIMETRES_PER_INCH = 25.4;

const documentUnitsPerDisplayUnit = Object.freeze({
  px: 1,
  mm: CSS_PIXELS_PER_INCH / MILLIMETRES_PER_INCH,
  cm: CSS_PIXELS_PER_INCH / (MILLIMETRES_PER_INCH / 10),
});

function unitScale(unit) {
  if (!Object.hasOwn(documentUnitsPerDisplayUnit, unit))
    throw Error(`Unsupported artboard unit: ${unit}`);
  const scale = documentUnitsPerDisplayUnit[unit];
  return scale;
}

function finiteLength(value) {
  if (typeof value !== 'number' && typeof value !== 'string')
    throw Error('Enter a finite dimension');
  if (
    typeof value === 'string' &&
    !/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(value.trim())
  )
    throw Error('Enter a finite dimension');
  const number = typeof value === 'number' ? value : Number(value);
  if (!Number.isFinite(number)) throw Error('Enter a finite dimension');
  return number;
}

export function displayLengthToDocumentUnits(value, unit) {
  const documentLength = finiteLength(value) * unitScale(unit);
  if (!Number.isFinite(documentLength)) throw Error('Dimension is outside the supported range');
  return documentLength;
}

export function documentUnitsToDisplayLength(value, unit) {
  const documentLength = finiteLength(value);
  const displayLength = documentLength / unitScale(unit);
  if (!Number.isFinite(displayLength)) throw Error('Dimension is outside the supported range');
  return displayLength;
}

export function formatDocumentLength(value, unit) {
  const displayLength = documentUnitsToDisplayLength(value, unit);
  if (unit === 'px') return String(displayLength);
  return String(Number(displayLength.toPrecision(15)));
}

export function svgRootSizeAttributes(documentDimensions, unit = 'px') {
  if (!documentDimensions || typeof documentDimensions !== 'object' || Array.isArray(documentDimensions))
    throw Error('Invalid SVG dimensions');
  const widthPx = finiteLength(documentDimensions.width),
    heightPx = finiteLength(documentDimensions.height);
  unitScale(unit);
  if (widthPx <= 0 || heightPx <= 0) throw Error('SVG dimensions must be positive');
  const svgLength = (value) =>
    unit === 'px'
      ? formatDocumentLength(value, unit)
      : `${formatDocumentLength(value, unit)}${unit}`;
  return {
    width: svgLength(widthPx),
    height: svgLength(heightPx),
    viewBox: `0 0 ${formatDocumentLength(widthPx, 'px')} ${formatDocumentLength(heightPx, 'px')}`,
  };
}

export function svgRootAttributes(documentDimensions, unit = 'px') {
  const size = svgRootSizeAttributes(documentDimensions, unit);
  return `width="${size.width}" height="${size.height}" viewBox="${size.viewBox}"`;
}
