import { curvePointReference } from './curve-point-reference.js';
import { colorExpression, usesColorOperations } from './color-operations.js';

const nodeIdPattern = /^[a-zA-Z0-9][a-zA-Z0-9:_-]{0,180}$/;

export function expression(source, resolveReference, referencesOnly = false, baseValue) {
  if (source.length > 4096) throw Error('Expression exceeds 4096 characters');
  const tokens =
    source.match(
      /"[a-zA-Z0-9:_.-]+"|(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?|[a-zA-Z]+|[()+\-*/%,]/g,
    ) || [];
  if (tokens.join('') !== source.replace(/\s/g, '')) throw Error('Unsupported expression syntax');
  let i = 0;
  const funcs = {
    sin: Math.sin,
    cos: Math.cos,
    tan: Math.tan,
    abs: Math.abs,
    sqrt: Math.sqrt,
    min: Math.min,
    max: Math.max,
    round: Math.round,
  };
  function atom() {
    let t = tokens[i++];
    if (t === '-') return -atom();
    if (t === '+') return atom();
    if (t === '(') {
      let v = parse(0);
      if (tokens[i++] !== ')') throw Error('Missing closing parenthesis');
      return v;
    }
    if (t === 'pi') return Math.PI;
    if (t === 'e') return Math.E;
    if (t === 'value') {
      if (referencesOnly) return 0;
      if (!Number.isFinite(baseValue)) throw Error('Expected finite Scalar');
      return baseValue;
    }
    if (t === 'ref') {
      if (tokens[i++] !== '(') throw Error('Reference requires parentheses');
      const readString = () => {
        const token = tokens[i++];
        if (!token || !/^"[a-zA-Z0-9:_.-]+"$/.test(token))
          throw Error('Reference requires quoted IDs and parameter keys');
        return token.slice(1, -1);
      };
      let layerId, compositionId, qualifier, componentComposition;
      if (['composition', 'colorComposition', 'componentComposition'].includes(tokens[i])) {
        if (tokens[i] === 'colorComposition') qualifier = 'colorComposition';
        if (tokens[i] === 'componentComposition') componentComposition = true;
        i++;
        if (tokens[i++] !== ',') throw Error('Invalid composition reference');
        compositionId = readString();
        if (!nodeIdPattern.test(compositionId) || tokens[i++] !== ',')
          throw Error('Invalid composition reference');
        layerId = readString();
        if (!nodeIdPattern.test(layerId) || tokens[i++] !== ',')
          throw Error('Invalid composition reference');
      } else if (tokens[i] === 'artwork' || tokens[i] === 'colorArtwork') {
        if (tokens[i] === 'colorArtwork') qualifier = 'colorArtwork';
        i++;
        if (tokens[i++] !== ',') throw Error('Invalid artwork reference');
        layerId = readString();
        if (!nodeIdPattern.test(layerId) || tokens[i++] !== ',')
          throw Error('Invalid artwork reference');
      }
      const nodeId = readString();
      if (!nodeIdPattern.test(nodeId) || tokens[i++] !== ',') throw Error('Invalid reference');
      const key = readString();
      let component;
      if (tokens[i] === ',') {
        i++;
        component = readString();
      }
      if (qualifier && component !== undefined)
        throw Error('Qualified Color references cannot select a component');
      if (compositionId && !componentComposition && component !== undefined)
        throw Error('Composition references require a Scalar property');
      if (componentComposition && component === undefined)
        throw Error('Component Composition references require a component');
      if (component !== undefined && !['x', 'y', 'length'].includes(component))
        throw Error('Reference component must be x, y or length');
      if (component === 'length' && !curvePointReference(key, component))
        throw Object.assign(Error('Reference length requires a Curve handle key'), {
          code: 'EXPRESSION_INVALID_LENGTH_REFERENCE',
        });
      if (tokens[i++] !== ')') throw Error('Invalid reference arguments');
      if (!resolveReference) throw Error('Reference requires a composition');
      return resolveReference({
        ...(compositionId ? { compositionId } : {}),
        ...(layerId ? { layerId } : {}),
        ...(qualifier ? { qualifier } : {}),
        ...(componentComposition ? { componentComposition } : {}),
        nodeId,
        key,
        component,
      });
    }
    if (Object.hasOwn(funcs, t)) {
      if (tokens[i++] !== '(') throw Error('Function requires parentheses');
      let args = [parse(0)];
      while (tokens[i] === ',') {
        i++;
        args.push(parse(0));
      }
      if (tokens[i++] !== ')') throw Error('Missing closing parenthesis');
      return funcs[t](...args);
    }
    if (t && /^\d|^\./.test(t)) return Number(t);
    throw Error('Expected a number');
  }
  function parse(min) {
    let v = atom();
    while (i < tokens.length) {
      let op = tokens[i],
        prec = { '+': 1, '-': 1, '*': 2, '/': 2, '%': 2 }[op];
      if (!prec || prec < min) break;
      i++;
      let r = parse(prec + 1);
      v = op === '+' ? v + r : op === '-' ? v - r : op === '*' ? v * r : op === '/' ? v / r : v % r;
    }
    return v;
  }
  let result = parse(0);
  if (i !== tokens.length || (!referencesOnly && !Number.isFinite(result)))
    throw Error('Expression must produce a finite number');
  return result;
}

// The project wrapper is a pure codec, so it validates the new reserved syntax
// without importing the full rendering/Session module. Old unrelated text is
// never sent through this parser by the wrapper.
export function qualifiedReferencesForCodec(q, { allowComponentComposition = true } = {}) {
  if (!['Scalar', 'Vector2', 'Color'].includes(q.type))
    throw Error('Artwork reference requires a numeric expression parameter');
  const refs = [];
  const record = (ref, type) => {
    // Older wrappers parsed expressions containing their own qualifier. Keep
    // that parser's rejection of this newer reserved word in mixed expressions.
    if (!allowComponentComposition && ref.componentComposition)
      throw Error('Component Composition references require schemaVersion 65');
    refs.push({ ...ref, type });
  };
  const scalar = (source) => {
    if (q.type === 'Color' && /\bvalue\b/.test(source.replace(/"[^"]*"/g, '')))
      throw Error('Color value cannot be used as a Scalar');
    const before = refs.length;
    expression(
      source,
      (ref) => {
        if (ref.qualifier) throw Error('Qualified Color references require a Color expression');
        record(ref, 'Scalar');
        return 0;
      },
      true,
    );
    return refs.length === before ? expression(source) : 0.5;
  };
  if (q.type === 'Color') {
    const dummy = { hex: '#000000', alpha: 1 };
    const reference = (source) =>
      expression(
        source,
        (ref) => {
          if (ref.layerId && !ref.qualifier)
            throw Error('Qualified numeric references require a numeric parameter');
          record(ref, 'Color');
          return dummy;
        },
        true,
      );
    if (usesColorOperations(q.expression))
      colorExpression(q.expression, dummy, { reference, scalar });
    else {
      if (!/^ref\s*\([^()]*\)$/.test(q.expression.trim()))
        throw Error('Color expression requires a single Color reference');
      reference(q.expression);
    }
  } else {
    const parts = q.expression.split(';');
    if (parts.length !== (q.type === 'Vector2' ? 2 : 1)) throw Error('Invalid numeric expression');
    for (const part of parts)
      expression(
        part,
        (ref) => {
          if (ref.qualifier) throw Error('Qualified Color references require a Color expression');
          record(ref, 'Scalar');
          return 0;
        },
        true,
      );
  }
  return refs;
}
