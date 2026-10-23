export function formatParameterResult(value) {
  const rounded = Number(value.toFixed(3));
  // Keep compact ordinary readouts without flattening a tiny nonzero result.
  return String(rounded === 0 && value !== 0 ? Number(value.toPrecision(3)) : rounded);
}
