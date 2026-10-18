// Bounded 2D Repeater. Integer powers deliberately avoid ambiguous fractional
// affine transforms (especially reflections and singular scales).
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
  if (!Number.isInteger(count) || count < 0 || count > 256)
    throw Error(
      '2D Repeater Copies must be an integer from 0 to 256; fractional copies are unsupported',
    );
  if (!Number.isInteger(offset) || Math.abs(offset) > 256)
    throw Error(
      '2D Repeater Offset must be an integer from -256 to 256; fractional offsets are unsupported',
    );
  if (
    ![anchor, position, scale].every(
      (v) => Array.isArray(v) && v.length === 2 && v.every(Number.isFinite),
    ) ||
    !Number.isFinite(rotation)
  )
    throw Error('2D Repeater requires finite Anchor, Position, Scale and Rotation');
  if (scale.some((v) => Math.abs(v) > 1000))
    throw Error('2D Repeater Scale must be from -1000 to 1000 percent');
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
  const { count, offset, startOpacity, endOpacity, copiesAbove } = settings;
  const groups = new Map();
  for (const c of source.contours)
    if (!groups.has(c.group ?? '')) groups.set(c.group ?? '', groups.size);
  if (
    source.contours.length * count > 10000 ||
    source.contours.reduce((sum, c) => sum + c.points.length, 0) * count > 100000 ||
    groups.size * count > 256
  )
    throw Error(
      '2D Repeater exceeds geometry budget (256 groups / 10000 contours / 100000 points)',
    );
  if (!count) return { contours: [] };
  const step = repeaterStep(settings);
  let matrix = identity();
  if (offset) {
    const direction = offset < 0 ? inverse(step) : step;
    for (let i = 0; i < Math.abs(offset); i++) matrix = multiply(matrix, direction);
  }
  const copies = [];
  for (let copy = 0; copy < count; copy++) {
    const opacity =
      (count === 1
        ? startOpacity
        : startOpacity + ((endOpacity - startOpacity) * copy) / (count - 1)) / 100;
    const transform = (v, handle = false) =>
      checked([
        matrix[0] * v[0] + matrix[2] * v[1] + (handle ? 0 : matrix[4]),
        matrix[1] * v[0] + matrix[3] * v[1] + (handle ? 0 : matrix[5]),
      ]);
    copies.push(
      source.contours.map((c, ci) => ({
        id: `${options.prefix}:copy${copy}:c${ci}`,
        group: `${options.prefix}:copy${copy}:g${groups.get(c.group ?? '')}`,
        repeaterOpacity: opacity,
        closed: c.closed,
        points: c.points.map((pt, pi) => ({
          id: `${options.prefix}:copy${copy}:c${ci}:p${pi}`,
          position: transform(pt.position),
          handleA: transform(pt.handleA, true),
          handleB: transform(pt.handleB, true),
        })),
      })),
    );
    if (copy + 1 < count) matrix = multiply(matrix, step);
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
      ? '<small class="repeater-limits">2D only. Integer Copies / Offset. After this node use Transform, paint and Output only. Full limits in Help.</small>'
      : '';
  return (
    summary +
    '<p class="font-note">Geometry before paint. AE-style 2D: Copies includes the original (0–256); Offset shifts the transform index (−256–256). Both require integers; fractional copies/offsets are unsupported. Anchor is the pivot, Position is the per-step displacement, Scale is %, Rotation is degrees. Transforms accumulate; negative Offset uses the inverse (zero Scale cannot invert). Negative Scale mirrors; zero Scale collapses. Start/End Opacity is % per independent contour group (one copy uses Start). Copies Above puts later copies in front; off puts them behind. After 2D use Transform / Fill / Stroke / Output only; no nested 2D, paint duplication, 3D or timeline. Gradients stay in artboard space. Not complete AE compatibility. Legacy Linear / Radial retain their original behavior.</p>'
  );
}
