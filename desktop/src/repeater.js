// Bounded 2D Repeater. Integer powers retain the original affine contract;
// fractional Offset uses an explicit bounded partial-step policy below.
export const REPEATER_SCHEMA_VERSION = 37;
export const REPEATER_KEYS = [
  'offset',
  'anchor',
  'position',
  'scale',
  'rotation',
  'startOpacity',
  'endOpacity',
  'copiesAbove',
];

export function repeaterSettings(read) {
  const settings = Object.fromEntries(['count', ...REPEATER_KEYS].map((key) => [key, read(key)]));
  const {
    count,
    offset,
    anchor,
    position,
    scale,
    rotation,
    startOpacity,
    endOpacity,
    copiesAbove,
  } = settings;
  if (!Number.isFinite(count) || count < 0 || count > 256)
    throw Error('2D Repeater Copies must be a finite number from 0 to 256');
  if (!Number.isFinite(offset) || Math.abs(offset) > 256)
    throw Error('2D Repeater Offset must be a finite number from -256 to 256');
  if (
    ![anchor, position, scale].every(
      (v) => Array.isArray(v) && v.length === 2 && v.every(Number.isFinite),
    ) ||
    !Number.isFinite(rotation)
  )
    throw Error('2D Repeater requires finite Anchor, Position, Scale and Rotation');
  if (scale.some((v) => Math.abs(v) > 1000))
    throw Error('2D Repeater Scale must be from -1000 to 1000 percent');
  if (!Number.isInteger(offset) && scale.some((v) => v < 0))
    throw Error(
      '2D Repeater fractional Offset requires nonnegative Scale; use an integer Offset with negative Scale',
    );
  if (offset < 0 && scale.some((v) => v === 0))
    throw Error('2D Repeater negative Offset requires nonzero Scale on both axes');
  if (![startOpacity, endOpacity].every((v) => Number.isFinite(v) && v >= 0 && v <= 100))
    throw Error('2D Repeater Start/End Opacity must be from 0 to 100 percent');
  if (typeof copiesAbove !== 'boolean') throw Error('2D Repeater Copies Above must be Boolean');
  return settings;
}

const identity = () => [1, 0, 0, 1, 0, 0];
function checked(values) {
  if (!values.every(Number.isFinite) || values.some((v) => Math.abs(v) > 1e12))
    throw Error('2D Repeater exceeds finite transform/coordinate budget (1e12)');
  return values;
}
function multiply(a, b) {
  return checked([
    a[0] * b[0] + a[2] * b[1],
    a[1] * b[0] + a[3] * b[1],
    a[0] * b[2] + a[2] * b[3],
    a[1] * b[2] + a[3] * b[3],
    a[0] * b[4] + a[2] * b[5] + a[4],
    a[1] * b[4] + a[3] * b[5] + a[5],
  ]);
}
function inverse(m) {
  const determinant = m[0] * m[3] - m[1] * m[2];
  if (!Number.isFinite(determinant) || determinant === 0)
    throw Error('2D Repeater negative Offset requires invertible Scale');
  return checked([
    m[3] / determinant,
    -m[1] / determinant,
    -m[2] / determinant,
    m[0] / determinant,
    (m[2] * m[5] - m[3] * m[4]) / determinant,
    (m[1] * m[4] - m[0] * m[5]) / determinant,
  ]);
}
function integerPower(step, exponent) {
  const direction = exponent < 0 ? inverse(step) : step;
  let matrix = identity();
  for (let i = 0; i < Math.abs(exponent); i++) matrix = multiply(matrix, direction);
  return matrix;
}
function fractionalStep(settings, fraction) {
  // This is a bounded Nect interpolation policy, not an Adobe-compatible
  // fractional affine power. Position and rotation interpolate linearly;
  // positive scale interpolates geometrically. Negative scale has no single
  // real fractional power and is rejected by repeaterSettings.
  const scale = settings.scale.map((value) => {
    if (value === 0) return 0;
    return 100 * Math.pow(value / 100, fraction);
  });
  return repeaterStep({
    ...settings,
    position: settings.position.map((value) => value * fraction),
    scale,
    rotation: settings.rotation * fraction,
  });
}
function offsetMatrix(settings, step) {
  const whole = settings.offset < 0 ? Math.ceil(settings.offset) : Math.floor(settings.offset);
  const fraction = Math.abs(settings.offset - whole);
  let matrix = integerPower(step, whole);
  if (fraction > 0) {
    const partial = fractionalStep(settings, fraction);
    matrix = multiply(matrix, settings.offset < 0 ? inverse(partial) : partial);
  }
  return matrix;
}
export function repeaterStep({ anchor, position, scale, rotation }) {
  const radians = ((rotation % 360) * Math.PI) / 180;
  const c = Math.cos(radians),
    s = Math.sin(radians);
  const a = (c * scale[0]) / 100,
    b = (s * scale[0]) / 100;
  const d = (c * scale[1]) / 100,
    e = (-s * scale[1]) / 100;
  return checked([
    a,
    b,
    e,
    d,
    anchor[0] + position[0] - a * anchor[0] - e * anchor[1],
    anchor[1] + position[1] - b * anchor[0] - d * anchor[1],
  ]);
}

export function repeatGeometry(source, options) {
  const settings = repeaterSettings((key) => options[key]);
  if (!source?.contours) throw Error('Connect a Path input');
  if (source.contours.some((c) => c.repeaterOpacity !== undefined))
    throw Error('Nested 2D Repeaters are unsupported; put legacy repeaters before the 2D Repeater');
  const { count, startOpacity, endOpacity, copiesAbove } = settings;
  const fullCopies = Math.floor(count);
  const partialCopy = count - fullCopies;
  const copyCount = Math.ceil(count);
  const groups = new Map();
  for (const c of source.contours)
    if (!groups.has(c.group ?? '')) groups.set(c.group ?? '', groups.size);
  if (
    source.contours.length * copyCount > 10000 ||
    source.contours.reduce((sum, c) => sum + c.points.length, 0) * copyCount > 100000 ||
    groups.size * copyCount > 256
  )
    throw Error(
      '2D Repeater exceeds geometry budget (256 groups / 10000 contours / 100000 points)',
    );
  if (!copyCount) return { contours: [] };
  const step = repeaterStep(settings);
  let matrix = offsetMatrix(settings, step);
  const copies = [];
  for (let copy = 0; copy < copyCount; copy++) {
    const opacity =
      (copyCount === 1
        ? startOpacity
        : startOpacity + ((endOpacity - startOpacity) * copy) / (copyCount - 1)) / 100;
    const tailAlpha = partialCopy > 0 && copy === fullCopies ? partialCopy : 1;
    const transform = (v, handle = false) =>
      checked([
        matrix[0] * v[0] + matrix[2] * v[1] + (handle ? 0 : matrix[4]),
        matrix[1] * v[0] + matrix[3] * v[1] + (handle ? 0 : matrix[5]),
      ]);
    copies.push(
      source.contours.map((c, ci) => ({
        id: `${options.prefix}:copy${copy}:c${ci}`,
        group: `${options.prefix}:copy${copy}:g${groups.get(c.group ?? '')}`,
        repeaterOpacity: opacity * tailAlpha,
        closed: c.closed,
        points: c.points.map((pt, pi) => ({
          id: `${options.prefix}:copy${copy}:c${ci}:p${pi}`,
          position: transform(pt.position),
          handleA: transform(pt.handleA, true),
          handleB: transform(pt.handleB, true),
        })),
      })),
    );
    if (copy + 1 < copyCount) matrix = multiply(matrix, step);
  }
  return { contours: (copiesAbove ? copies : copies.reverse()).flat() };
}

export function assertRepeaterRoute(value, node) {
  const hasRepeater = (item) =>
    item?.contours?.some((c) => c.repeaterOpacity !== undefined) ||
    item?.children?.some(hasRepeater);
  if (!hasRepeater(value) || node.bypassed) return;
  if (
    ![
      'Fill',
      'Stroke',
      'Linear Gradient Fill',
      'Radial Gradient Fill',
      'Transform',
      'Output',
    ].includes(node.type)
  )
    throw Error(
      '2D Repeater output supports Transform, Fill, Stroke and Output only; put geometry operations before it',
    );
}

export function repeaterControls(node) {
  if (node.type !== 'Geometry Repeater') return '';
  const summary =
    node.operation === 'ae-2d'
      ? '<small class="repeater-limits">2D only. Fractional Copies / Offset use bounded partial-step rules. After this node use Transform, paint and Output only. Full limits in Help.</small>'
      : '';
  return (
    summary +
    '<p class="font-note">Geometry before paint. AE-style 2D: Copies includes the original (0 to 256); Offset shifts the transform index (-256 to 256). Fractional Copies materialize ceil(Copies) copies and multiply the final copy alpha by the fractional remainder. Fractional Offset uses integer powers plus one partial step: Position and Rotation are linear, positive Scale is geometric; negative Offset uses the inverse partial step. Negative Scale mirrors for integer transform indices, but fractional Offset with negative Scale is rejected because its fractional power is ambiguous; zero Scale collapses for nonnegative Offset and cannot invert. Anchor is the pivot, Scale is %, and Rotation is degrees. Transforms accumulate. Start/End Opacity is % per independent contour group (one rendered copy uses Start). Copies Above puts later copies in front; off puts them behind. After 2D use Transform / Fill / Stroke / Output only; no nested 2D, paint duplication, 3D or timeline. Gradients stay in artboard space. Adobe does not define these fractional edge cases; this is bounded Nect behavior, not complete AE compatibility. Legacy Linear / Radial retain their original behavior.</p>'
  );
}
