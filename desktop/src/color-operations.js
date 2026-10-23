// Version 1: linear-light sRGB, premultiplied alpha; public boundary is hex + alpha.
export const COLOR_OPERATIONS_SCHEMA_VERSION = 43;
export const COLOR_OPERATIONS_VERSION = 1;
const working = Symbol('linear Color');
const decode = (v) => (v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4);
const encode = (v) => (v <= 0.0031308 ? 12.92 * v : 1.055 * v ** (1 / 2.4) - 0.055);
function linearColor(value) {
  if (value?.[working]) return value;
  if (
    !value ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Object.keys(value).length !== 2 ||
    typeof value.hex !== 'string' ||
    !/^#[0-9a-fA-F]{6}$/.test(value.hex) ||
    !Number.isFinite(value.alpha) ||
    value.alpha < 0 ||
    value.alpha > 1
  )
    throw Error('Expected Color with hex #RRGGBB and alpha from 0 to 1');
  return {
    [working]: true,
    rgb: [1, 3, 5].map((i) => decode(parseInt(value.hex.slice(i, i + 2), 16) / 255)),
    alpha: value.alpha,
  };
}
function unitScalar(value, name) {
  if (!Number.isFinite(value) || value < 0 || value > 1)
    throw Error(`${name} requires a finite Scalar from 0 to 1`);
  return value;
}
// The sole operation implementation; callers can compose working Colors without quantization.
export function colorOperation(name, args) {
  if (name === 'alpha' && args.length === 2) {
    const color = linearColor(args[0]),
      alpha = unitScalar(args[1], 'alpha');
    return { [working]: true, rgb: [...color.rgb], alpha };
  }
  if (name === 'mix' && args.length === 3) {
    const a = linearColor(args[0]),
      b = linearColor(args[1]),
      t = unitScalar(args[2], 'mix');
    const alpha = a.alpha * (1 - t) + b.alpha * t;
    return {
      [working]: true,
      alpha,
      rgb:
        alpha > 0
          ? a.rgb.map((v, i) => (v * a.alpha * (1 - t) + b.rgb[i] * b.alpha * t) / alpha)
          : [0, 0, 0],
    };
  }
  throw Error('Expected mix(Color, Color, Scalar) or alpha(Color, Scalar)');
}
export function publicColor(value) {
  const color = linearColor(value);
  const hex =
    '#' +
    color.rgb
      .map((v) =>
        Math.round(Math.max(0, Math.min(1, encode(v))) * 255)
          .toString(16)
          .padStart(2, '0'),
      )
      .join('');
  return { hex, alpha: color.alpha };
}
export function usesColorOperations(source) {
  return /#|\b(?:mix|alpha)\s*\(/.test(source.replace(/"[^"]*"/g, ''));
}
// Numeric/reference syntax is delegated to the existing bounded Scalar parser by the caller.
export function colorExpression(source, baseValue, { reference, scalar }) {
  if (source.length > 4096) throw Error('Expression exceeds 4096 characters');
  let i = 0;
  const space = () => {
    while (/\s/.test(source[i] || '') && i < source.length) i++;
  };
  function expect(token) {
    space();
    if (source[i++] !== token) throw Error(`Color expression expected ${token}`);
  }
  function argumentText() {
    space();
    const start = i;
    let depth = 0,
      quoted = false;
    while (i < source.length) {
      const char = source[i];
      if (char === '"') quoted = !quoted;
      if (!quoted) {
        if (char === '(') depth++;
        if ((char === ')' || char === ',') && depth === 0) break;
        if (char === ')') depth--;
      }
      i++;
    }
    return source.slice(start, i);
  }
  function color() {
    space();
    if (source[i] === '(') {
      i++;
      const result = color();
      expect(')');
      return result;
    }
    const literal = source.slice(i).match(/^#[0-9a-fA-F]{6}(?![\da-zA-Z])/);
    if (literal) {
      i += literal[0].length;
      return linearColor({ hex: literal[0], alpha: 1 });
    }
    const name = source.slice(i).match(/^[a-zA-Z]+/)?.[0];
    if (!name) throw Error('Expected Color value, ref, #RRGGBB, mix or alpha');
    const start = i;
    i += name.length;
    if (name === 'value') return linearColor(baseValue);
    if (name === 'ref') {
      expect('(');
      argumentText();
      while (source[i] === ',') {
        i++;
        argumentText();
      }
      expect(')');
      return linearColor(reference(source.slice(start, i)));
    }
    if (name === 'mix' || name === 'alpha') {
      expect('(');
      const args = [color()];
      expect(',');
      if (name === 'mix') {
        args.push(color());
        expect(',');
      }
      args.push(scalar(argumentText()));
      expect(')');
      return colorOperation(name, args);
    }
    throw Error('Expected Color value, ref, #RRGGBB, mix or alpha');
  }
  const result = color();
  space();
  if (i !== source.length) throw Error('Unsupported Color expression syntax');
  return publicColor(result);
}
