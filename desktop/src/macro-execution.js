import { blankDocument, connect, evaluate, node, parameters, validate } from './core.js';
import { isVector2 } from './node-value-validation.js';
import { transformValues } from './transform-affine.js';
import {
  resolveMacroInstance,
  snapshotMacroDefinition,
  snapshotMacroInstance,
} from './macro-package.js';

// A detached, literal Curve is the only source supported by this prototype.
// It is not a Project edit or a substitute for validating a real Session candidate.
function copyData(value, active = new Set()) {
  if (value === null || ['string', 'boolean', 'undefined'].includes(typeof value)) return value;
  if (typeof value === 'number' && Number.isFinite(value)) return value;
  if (typeof value !== 'object' || active.has(value)) throw Error('Curve must be plain data');
  const array = Array.isArray(value),
    prototype = Object.getPrototypeOf(value);
  if (array ? prototype !== Array.prototype : prototype !== Object.prototype && prototype !== null)
    throw Error('Curve must be plain data');
  const descriptors = Object.getOwnPropertyDescriptors(value),
    keys = Reflect.ownKeys(descriptors);
  if (keys.some((key) => typeof key !== 'string')) throw Error('Curve must be plain data');
  active.add(value);
  const result = array ? [] : {};
  if (array) {
    for (let index = 0; index < value.length; index++) {
      const item = descriptors[String(index)];
      if (!item || !item.enumerable || !Object.hasOwn(item, 'value'))
        throw Error('Curve must be plain data');
      result.push(copyData(item.value, active));
    }
    if (
      keys.some(
        (key) =>
          key !== 'length' && (!/^(0|[1-9][0-9]*)$/.test(key) || Number(key) >= value.length),
      )
    )
      throw Error('Curve must be plain data');
  } else {
    for (const key of keys) {
      const item = descriptors[key];
      if (!item.enumerable || !Object.hasOwn(item, 'value'))
        throw Error('Curve must be plain data');
      Object.defineProperty(result, key, {
        value: copyData(item.value, active),
        enumerable: true,
        writable: true,
        configurable: true,
      });
    }
  }
  active.delete(value);
  return result;
}

function literalCurve(value) {
  const source = copyData(value);
  if (
    source?.type !== 'Curve' ||
    source.bypassed ||
    source.reference != null ||
    source.compositionReference != null ||
    source.pathAttachment != null ||
    source.asset != null ||
    source.rawPath !== '' ||
    !Array.isArray(source.edits) ||
    source.edits.length ||
    !source.params ||
    Object.keys(source.params).length !== 1 ||
    !Object.hasOwn(source.params, 'closed')
  )
    throw Error('Macro prototype requires a literal Curve');
  if (
    parameters(source).some(
      ([, input]) =>
        !input ||
        input.expression !== '' ||
        input.source !== undefined ||
        input.sourcePort !== undefined,
    )
  )
    throw Error('Macro prototype cannot use driven Curve inputs');
  return source;
}

function assertPath(path, source = null) {
  if (!path || !Array.isArray(path.contours)) throw Error('Macro requires a Path');
  if (source && path.contours.length !== source.contours.length)
    throw Error('Macro changed Path identities');
  for (const [index, contour] of path.contours.entries()) {
    if (!contour || !Array.isArray(contour.points)) throw Error('Invalid Macro Path');
    const original = source?.contours[index];
    if (
      original &&
      (contour.id !== original.id ||
        contour.closed !== original.closed ||
        contour.points.length !== original.points.length)
    )
      throw Error('Macro changed Path identities');
    for (const [pointIndex, point] of contour.points.entries()) {
      if (
        !point ||
        !['position', 'handleA', 'handleB'].every((key) => isVector2(point[key])) ||
        (original && point.id !== original.points[pointIndex].id)
      )
        throw Error('Macro Path exceeds finite coordinates or changed identity');
      for (const handle of [point.handleA, point.handleB])
        if (!point.position.every((value, axis) => Number.isFinite(value + handle[axis])))
          throw Error('Macro Path exceeds finite coordinates');
    }
  }
}

function assertFiniteTransform(path, transform) {
  const [a, b, c, d, tx, ty] = transformValues(
    transform,
    (key) => transform.params[key].value,
  ).matrix;
  for (const contour of path.contours)
    for (const point of contour.points) {
      const map = ([x, y], translate) => [
        a * x + c * y + (translate ? tx : 0),
        b * x + d * y + (translate ? ty : 0),
      ];
      const position = map(point.position, true),
        handles = [map(point.handleA, false), map(point.handleB, false)];
      if (
        !position.every(Number.isFinite) ||
        handles.some(
          (handle) =>
            !handle.every(Number.isFinite) ||
            !position.every((value, axis) => Number.isFinite(value + handle[axis])),
        )
      )
        throw Error('Macro Path exceeds finite coordinates');
    }
}

// The result is a detached probe document, never an authored Project candidate.
// A future Session owner must recheck target/definition identity and evaluate
// the actual connected candidate immediately before its atomic commit.
export function prepareCurveMacroCandidate(sourceValue, definitionValue, instanceValue) {
  const source = literalCurve(sourceValue),
    definition = snapshotMacroDefinition(definitionValue),
    instance = snapshotMacroInstance(instanceValue, definition),
    resolved = resolveMacroInstance(definition, instance),
    document = blankDocument(),
    layer = document.layers[0],
    nodes = new Map();
  layer.nodes.push(source);
  for (const item of definition.nodes) {
    const transform = node('Transform');
    for (const [key, value] of Object.entries(item.params))
      transform.params[key].value = structuredClone(value);
    nodes.set(item.id, transform);
    layer.nodes.push(transform);
  }
  for (const item of resolved) nodes.get(item.nodeId).params[item.key].value = item.value;
  const first = nodes.get(definition.input.nodeId),
    second = nodes.get(definition.output.nodeId);
  connect(layer, source.id, first.id, 'path');
  connect(layer, first.id, second.id, 'path');
  validate(document);
  const evaluation = evaluate(layer, {}, {}, document, null, true),
    original = evaluation.run(source.id);
  assertPath(original);
  assertFiniteTransform(original, first);
  const intermediate = evaluation.run(first.id);
  assertPath(intermediate, original);
  assertFiniteTransform(intermediate, second);
  const output = evaluation.run(second.id);
  assertPath(output, original);
  return {
    document,
    definition,
    instance,
    sourceNodeId: source.id,
    firstNodeId: first.id,
    outputNodeId: second.id,
    path: output,
  };
}
