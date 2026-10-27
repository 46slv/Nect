import { formatSvgNumber, formatSvgExtent } from './svg-number-format.js';

export const ANGULAR_KEYS = [
  'center',
  'angle',
  'clockwise',
  'startColor',
  'color1',
  'position1',
  'color2',
  'position2',
  'endColor',
];
const STEPS = 720;
const exact = (v, keys) =>
  v &&
  typeof v === 'object' &&
  !Array.isArray(v) &&
  Reflect.ownKeys(v).length === keys.length &&
  keys.every((k) => Object.hasOwn(v, k));
function color(v) {
  if (
    !exact(v, ['hex', 'alpha']) ||
    typeof v.hex !== 'string' ||
    v.hex.length !== 7 ||
    !/^#[0-9a-fA-F]{6}$/.test(v.hex) ||
    !Number.isFinite(v.alpha) ||
    v.alpha < 0 ||
    v.alpha > 1
  )
    throw Error('Angular gradient requires #RRGGBB colors and alpha 0–1');
  return { hex: v.hex, alpha: v.alpha };
}
export function createAngularGradientFill(o) {
  if (!exact(o, ANGULAR_KEYS)) throw Error('Invalid angular gradient inputs');
  if (
    !Array.isArray(o.center) ||
    o.center.length !== 2 ||
    !Object.hasOwn(o.center, 0) ||
    !Object.hasOwn(o.center, 1) ||
    o.center.some((v) => !Number.isFinite(v) || Math.abs(v) > 100000)
  )
    throw Error('Angular gradient center must be within ±100000 px');
  if (!Number.isFinite(o.angle) || Math.abs(o.angle) > 360 || typeof o.clockwise !== 'boolean')
    throw Error('Angular gradient needs angle within ±360° and a Boolean direction');
  if (
    !Number.isFinite(o.position1) ||
    !Number.isFinite(o.position2) ||
    !(0 < o.position1 && o.position1 < o.position2 && o.position2 < 1)
  )
    throw Error('Angular gradient stop positions must satisfy 0 < position1 < position2 < 1');
  return {
    kind: 'angular-gradient',
    center: [...o.center],
    angle: o.angle,
    clockwise: o.clockwise,
    startColor: color(o.startColor),
    color1: color(o.color1),
    position1: o.position1,
    color2: color(o.color2),
    position2: o.position2,
    endColor: color(o.endColor),
  };
}
export function validateAngularGradientFill(fill) {
  if (!exact(fill, ['kind', ...ANGULAR_KEYS]) || fill.kind !== 'angular-gradient')
    throw Error('Invalid angular gradient fill');
  return createAngularGradientFill(Object.fromEntries(ANGULAR_KEYS.map((k) => [k, fill[k]])));
}
function interpolate(a, b, t) {
  const alpha = a.alpha * (1 - t) + b.alpha * t;
  const channels = [1, 3, 5].map((i) =>
    alpha === 0
      ? 0
      : Math.round(
          (parseInt(a.hex.slice(i, i + 2), 16) * a.alpha * (1 - t) +
            parseInt(b.hex.slice(i, i + 2), 16) * b.alpha * t) /
            alpha,
        ),
  );
  return {
    hex:
      '#' +
      channels.map((v) => Math.max(0, Math.min(255, v)).toString(16).padStart(2, '0')).join(''),
    alpha,
  };
}
export function sampleAngularGradient(fill, position) {
  const f = validateAngularGradientFill(fill);
  if (!Number.isFinite(position)) throw Error('Invalid angular gradient sample');
  const t = Math.max(0, Math.min(1, position)),
    stops = [
      [0, f.startColor],
      [f.position1, f.color1],
      [f.position2, f.color2],
      [1, f.endColor],
    ];
  for (let i = 1; i < stops.length; i++)
    if (t <= stops[i][0])
      return interpolate(
        stops[i - 1][1],
        stops[i][1],
        (t - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0]),
      );
  return { ...f.endColor };
}
export function renderAngularGradientFill(fill, id, path, precision) {
  const f = validateAngularGradientFill(fill);
  if (typeof id !== 'string' || id.length > 128 || !/^[A-Za-z_][A-Za-z0-9_.-]*$/.test(id))
    throw Error('Invalid angular gradient ID');
  if (!path || !Array.isArray(path.contours) || path.contours.length > 10000)
    throw Error('Invalid angular gradient geometry');
  let minX = Infinity,
    minY = Infinity,
    maxX = -Infinity,
    maxY = -Infinity,
    count = 0;
  for (const contour of path.contours)
    for (const p of contour.points) {
      if (++count > 100000) throw Error('Angular gradient exceeds geometry budget');
      for (const h of [[0, 0], p.handleA, p.handleB]) {
        const x = p.position[0] + h[0],
          y = p.position[1] + h[1];
        if (!Number.isFinite(x) || !Number.isFinite(y) || Math.abs(x) > 1e9 || Math.abs(y) > 1e9)
          throw Error('Angular gradient exceeds coordinate bounds');
        minX = Math.min(minX, x);
        minY = Math.min(minY, y);
        maxX = Math.max(maxX, x);
        maxY = Math.max(maxY, y);
      }
    }
  if (!count) {
    minX = f.center[0];
    minY = f.center[1];
    maxX = minX + 1;
    maxY = minY + 1;
  }
  const width = Math.max(1, maxX - minX),
    height = Math.max(1, maxY - minY),
    [cx, cy] = f.center;
  const radius =
    Math.max(
      1,
      ...[
        [minX, minY],
        [maxX, minY],
        [minX, maxY],
        [maxX, maxY],
      ].map(([x, y]) => Math.hypot(x - cx, y - cy)),
    ) * 1.0001;
  const fmt = (v) => formatSvgNumber(v, precision),
    direction = f.clockwise ? 1 : -1,
    start = (f.angle * Math.PI) / 180;
  const triangles = [];
  for (let i = 0; i < STEPS; i++) {
    const a = start + (direction * i * 2 * Math.PI) / STEPS,
      b = start + (direction * (i + 1) * 2 * Math.PI) / STEPS;
    const c = sampleAngularGradient(f, (i + 0.5) / STEPS);
    triangles.push(
      `<path d="M${fmt(cx)} ${fmt(cy)}L${fmt(cx + radius * Math.cos(a))} ${fmt(cy + radius * Math.sin(a))}L${fmt(cx + radius * Math.cos(b))} ${fmt(cy + radius * Math.sin(b))}Z" fill="${c.hex}" fill-opacity="${fmt(c.alpha)}"/>`,
    );
  }
  return {
    definition: `<pattern id="${id}" patternUnits="userSpaceOnUse" x="${fmt(minX)}" y="${fmt(minY)}" width="${formatSvgExtent(width, precision)}" height="${formatSvgExtent(height, precision)}" viewBox="${fmt(minX)} ${fmt(minY)} ${formatSvgExtent(width, precision)} ${formatSvgExtent(height, precision)}" preserveAspectRatio="none"><g shape-rendering="crispEdges">${triangles.join('')}</g></pattern>`,
    paint: `url(#${id})`,
  };
}
