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
