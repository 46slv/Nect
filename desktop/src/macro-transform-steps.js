import { isVector2 } from './node-value-validation.js';
import {
  resolveMacroInstance,
  snapshotMacroDefinition,
  snapshotMacroInstance,
} from './macro-package.js';
import { transformValues } from './transform-affine.js';

// Prepare the two authored Transform matrices without editing a Project or
// retaining a cache. The caller must validate its own definition pin and apply
// each step to its actual connected Path inside an atomic Session candidate.
export function macroTransformSteps(definitionValue, instanceValue) {
  const definition = snapshotMacroDefinition(definitionValue),
    instance = snapshotMacroInstance(instanceValue, definition),
    nodes = new Map(definition.nodes.map((item) => [item.id, structuredClone(item.params)]));
  for (const item of resolveMacroInstance(definition, instance))
    nodes.get(item.nodeId)[item.key] = item.value;
  return [definition.input.nodeId, definition.output.nodeId].map((id) => {
    const params = nodes.get(id),
      state = transformValues({ params }, (key) => params[key]);
    return { id, matrix: [...state.matrix] };
  });
}

// Check the actual intermediate Path before each existing Transform operation.
// A finite matrix alone cannot guarantee finite transformed point positions or
// handle endpoints. This is read-only and does not replace output evaluation.
export function assertMacroPathStep(path, matrix) {
  if (
    !path ||
    !Array.isArray(path.contours) ||
    !Array.isArray(matrix) ||
    matrix.length !== 6 ||
    ![0, 1, 2, 3, 4, 5].every(
      (index) => Object.hasOwn(matrix, index) && Number.isFinite(matrix[index]),
    )
  )
    throw Error('Macro requires a finite Path and Transform');
  const [a, b, c, d, tx, ty] = matrix;
  for (const contour of path.contours) {
    if (!contour || !Array.isArray(contour.points)) throw Error('Invalid Macro Path');
    for (const point of contour.points) {
      if (!point || !['position', 'handleA', 'handleB'].every((key) => isVector2(point[key])))
        throw Error('Macro Path requires finite coordinates');
      for (const handle of [point.handleA, point.handleB])
        if (!point.position.every((value, axis) => Number.isFinite(value + handle[axis])))
          throw Error('Macro Path exceeds finite coordinates');
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
}
