import { validateTonePower, toneDarkness } from './luminance-tone.js';
import { validateLinearGradientFill } from './linear-gradient.js';
import { validateRadialGradientFill } from './radial-gradient.js';

const MAX_GRID_CELLS = 4096;
const COLOR_KEYS = ['hex', 'alpha'];

function hasExactKeys(value, keys) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !Array.isArray(value) &&
    Reflect.ownKeys(value).length === keys.length &&
    keys.every((key) => Object.hasOwn(value, key))
  );
}

function hasOnlyKeys(value, keys) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !Array.isArray(value) &&
    Reflect.ownKeys(value).every((key) => keys.includes(key))
  );
}

function copyColor(value) {
  if (!hasExactKeys(value, COLOR_KEYS))
    throw Error('Expected Color with hex #RRGGBB and alpha from 0 to 1');
  const { hex, alpha } = value;
  if (
    typeof hex !== 'string' ||
    hex.length !== 7 ||
    !/^#[0-9a-fA-F]{6}$/.test(hex) ||
    !Number.isFinite(alpha) ||
    alpha < 0 ||
    alpha > 1
  )
    throw Error('Expected Color with hex #RRGGBB and alpha from 0 to 1');
  const encoded = [
    Number.parseInt(hex.slice(1, 3), 16) / 255,
    Number.parseInt(hex.slice(3, 5), 16) / 255,
    Number.parseInt(hex.slice(5, 7), 16) / 255,
  ];
  return { encoded, alpha };
}

function prepareFill(fill) {
  if (fill !== null && typeof fill === 'object' && !Array.isArray(fill)) {
    if (fill.kind === 'angular-gradient')
      throw Error(
        'Angular gradient luminance sampling is not supported yet; use solid, linear or radial paint',
      );
    if (fill.kind === 'linear-gradient') {
      validateLinearGradientFill(fill);
      const { start, end, startColor, endColor } = fill,
        dx = end[0] - start[0],
        dy = end[1] - start[1],
        length = Math.hypot(dx, dy);
      return {
        type: 'linear',
        start,
        end,
        dx,
        dy,
        length,
        startColor: copyColor(startColor),
        endColor: copyColor(endColor),
      };
    }
    if (fill.kind === 'radial-gradient') {
      validateRadialGradientFill(fill);
      return {
        type: 'radial',
        center: fill.center,
        radius: fill.radius,
        startColor: copyColor(fill.startColor),
        endColor: copyColor(fill.endColor),
      };
    }
  }
  return { type: 'solid', color: copyColor(fill) };
}

function clampUnit(value) {
  if (value <= 0) return 0;
  if (value >= 1) return 1;
  if (!Number.isFinite(value)) throw Error('Could not compute a finite gradient sample');
  return value;
}

// Return the linear-gradient parameter without squaring large endpoint values.
function linearParameter(fill, x, y) {
  let rx = x - fill.start[0],
    ry = y - fill.start[1],
    scale,
    alreadyScaled = false;
  if (Number.isFinite(rx) && Number.isFinite(ry)) {
    scale = Math.max(Math.abs(rx), Math.abs(ry), fill.length);
  } else {
    // Finite inputs can still overflow when subtracting opposite, very large values.
    scale = Math.max(
      Math.abs(x),
      Math.abs(y),
      Math.abs(fill.start[0]),
      Math.abs(fill.start[1]),
      fill.length,
    );
    rx = x / scale - fill.start[0] / scale;
    ry = y / scale - fill.start[1] / scale;
    alreadyScaled = true;
  }
  const along =
    (alreadyScaled ? rx : rx / scale) * (fill.dx / fill.length) +
    (alreadyScaled ? ry : ry / scale) * (fill.dy / fill.length);
  if (along === 0) return 0;
  const ratio = scale / fill.length;
  if (Number.isFinite(ratio)) return along * ratio;
  const result = (along * scale) / fill.length;
  return Number.isNaN(result) ? Math.sign(along) * Infinity : result;
}

function encodedColorAt(fill, x, y) {
  if (fill.type === 'solid') return fill.color;
  let t;
  if (fill.type === 'linear') {
    t = clampUnit(linearParameter(fill, x, y));
  } else {
    const distance = Math.hypot(x - fill.center[0], y - fill.center[1]);
    t = clampUnit(distance / fill.radius);
  }
  // The existing SVG paint metadata declares color-interpolation="sRGB".
  // Keep the two stop RGB channels encoded and interpolate them directly; alpha
  // is independently interpolated between stop opacities.
  return {
    encoded: fill.startColor.encoded.map(
      (channel, index) => channel + (fill.endColor.encoded[index] - channel) * t,
    ),
    alpha: fill.startColor.alpha + (fill.endColor.alpha - fill.startColor.alpha) * t,
  };
}

// WCAG 2.1 relative luminance transfer function for sRGB:
// https://www.w3.org/TR/WCAG21/relative-luminance.html
function linearizeSrgb(channel) {
  return channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4;
}

function luminanceAtPreparedFill(fill, x, y) {
  const color = encodedColorAt(fill, x, y),
    alpha = color.alpha;
  // Chosen composite model: decode each encoded sRGB channel first, then
  // source-over composite each linear-light channel against opaque white.
  // This models the requested white paper/canvas backdrop for transparent paint.
  const [r, g, b] = color.encoded.map((channel) => {
    const linear = linearizeSrgb(channel);
    return alpha * linear + (1 - alpha);
  });
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

function copyPoint(value, name) {
  if (!hasExactKeys(value, ['x', 'y']) || !Number.isFinite(value.x) || !Number.isFinite(value.y))
    throw Error(`Expected finite ${name} point with x and y`);
  return { x: value.x, y: value.y };
}

function copyBounds(value) {
  if (!hasExactKeys(value, ['minX', 'minY', 'maxX', 'maxY']))
    throw Error('Expected rectangular bounds with minX, minY, maxX and maxY');
  const { minX, minY, maxX, maxY } = value;
  if (
    !Number.isFinite(minX) ||
    !Number.isFinite(minY) ||
    !Number.isFinite(maxX) ||
    !Number.isFinite(maxY) ||
    maxX < minX ||
    maxY < minY
  )
    throw Error('Expected finite non-inverted rectangular bounds');
  return { minX, minY, maxX, maxY };
}

function gridIndexRange(min, max, origin, pitch) {
  const from = (min - origin) / pitch,
    to = (max - origin) / pitch;
  if (!Number.isFinite(from) || !Number.isFinite(to))
    throw Error('Grid bounds produce indices outside the supported numeric range');
  const firstValue = Math.floor(from),
    lastValue = Math.ceil(to) - 1,
    first = firstValue === 0 ? 0 : firstValue,
    last = lastValue === 0 ? 0 : lastValue;
  if (first > last) return { first: 0, last: -1, count: 0 };
  if (!Number.isSafeInteger(first) || !Number.isSafeInteger(last))
    throw Error('Grid bounds produce indices outside the supported numeric range');
  const count = last - first + 1;
  if (!Number.isSafeInteger(count)) throw Error('Grid bounds produce too many cells');
  return { first, last, count };
}

/**
 * Sample a solid Color, linear gradient, or radial gradient's relative
 * luminance at a world-space point. Transparent paint is composited against
 * white after sRGB decoding, in linear-light RGB.
 */
export function sampleLuminance(fill, x, y) {
  if (!Number.isFinite(x) || !Number.isFinite(y))
    throw Error('Luminance sample coordinates must be finite numbers');
  return luminanceAtPreparedFill(prepareFill(fill), x, y);
}

/**
 * Sample paint at the centers of world-aligned cells with positive-area
 * intersection with a half-open rectangle. Sample centers may lie outside
 * that rectangle; geometry containment is left to a separate mask/clipping
 * step. Grid indices can be negative. The full cell count is checked before
 * any sample objects are allocated.
 */
export function sampleLuminanceGrid(fill, bounds, options = {}) {
  if (!hasOnlyKeys(options, ['pitch', 'origin', 'invert', 'tonePower']))
    throw Error('Grid options may contain only pitch, origin, invert and tonePower');
  const { minX, minY, maxX, maxY } = copyBounds(bounds),
    pitch = options.pitch === undefined ? 16 : options.pitch,
    origin = copyPoint(
      options.origin === undefined ? { x: 0, y: 0 } : options.origin,
      'grid origin',
    ),
    invert = options.invert === undefined ? false : options.invert,
    tonePower = validateTonePower(options.tonePower);
  if (!Number.isFinite(pitch) || pitch < 1 || pitch > 1024)
    throw Error('Grid pitch must be finite and from 1 to 1024');
  if (typeof invert !== 'boolean') throw Error('Grid invert option must be boolean');

  let columns = { first: 0, last: -1, count: 0 },
    rows = { first: 0, last: -1, count: 0 };
  if (minX < maxX && minY < maxY) {
    columns = gridIndexRange(minX, maxX, origin.x, pitch);
    rows = gridIndexRange(minY, maxY, origin.y, pitch);
  }
  if (
    columns.count > MAX_GRID_CELLS ||
    rows.count > MAX_GRID_CELLS ||
    columns.count * rows.count > MAX_GRID_CELLS
  )
    throw Error(`Luminance grid exceeds the ${MAX_GRID_CELLS}-cell limit`);

  const preparedFill = prepareFill(fill),
    cells = [];
  for (let j = rows.first; j <= rows.last; j++) {
    const y = origin.y + (j + 0.5) * pitch;
    for (let i = columns.first; i <= columns.last; i++) {
      const x = origin.x + (i + 0.5) * pitch,
        luminance = luminanceAtPreparedFill(preparedFill, x, y),
        darkness = toneDarkness(luminance, invert, tonePower);
      cells.push({ i, j, x, y, luminance, darkness });
    }
  }
  return { pitch, origin, bounds: { minX, minY, maxX, maxY }, cells };
}
