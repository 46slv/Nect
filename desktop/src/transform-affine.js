import {
  characterTransform,
  multiplyMatrices,
  transformCharacterPoint,
} from './character-geometry.js';

const p = (type, value) => ({ type, value, expression: '', locked: false });
export const transformExtensions = {
  position: p('Vector2', [0, 0]),
  uniformScale: p('Boolean', true),
  scaleWidth: p('Scalar', 1),
  skew: p('Scalar', 0),
  skewAxis: p('Scalar', 0),
};
export const isTransform = (n) => ['Transform', 'Vector Transform'].includes(n?.type);
export function transformParameter(n, key) {
  if (!isTransform(n) || !Object.hasOwn(transformExtensions, key)) return undefined;
  if (n.params[key]) return n.params[key];
  const fallback =
    key === 'position' ? n.params.anchor : key === 'scaleWidth' ? n.params.scale : null;
  // A virtual default follows the evaluated legacy value, not its authoring
  // connection, expression or lock. The new control owns its own descriptor.
  return {
    ...structuredClone(transformExtensions[key]),
    value: structuredClone(fallback?.value ?? transformExtensions[key].value),
  };
}
export function transformValues(n, read) {
  const anchor = n.params.anchor ? read('anchor') : [0, 0];
  const scale = read('scale'),
    offset = read('offset'),
    rotation = read('rotation');
  const position = n.params.position ? read('position') : anchor;
  const uniformScale = n.params.uniformScale ? read('uniformScale') : true;
  const scaleWidth = uniformScale || !n.params.scaleWidth ? scale : read('scaleWidth');
  const skew = n.params.skew ? read('skew') : 0,
    skewAxis = n.params.skewAxis ? read('skewAxis') : 0;
  if (
    ![...anchor, ...position, ...offset, scale, scaleWidth, skew, skewAxis, rotation].every(
      Number.isFinite,
    ) ||
    typeof uniformScale !== 'boolean'
  )
    throw Error('Transform requires finite coordinates');
  if (Math.abs(skew) >= 89.9) throw Error('Skew must be between -89.9 and 89.9 degrees');
  const pivot = position.map((v, i) => v + offset[i]);
  let matrix;
  if (skew === 0 && scaleWidth === scale) {
    matrix = characterTransform(
      anchor,
      pivot.map((v, i) => v - anchor[i]),
      scale,
      rotation,
    );
  } else {
    const turn = (a) => characterTransform([0, 0], [0, 0], 1, a);
    const shear = [1, 0, -Math.tan((skew * Math.PI) / 180), 1, 0, 0];
    const linear = multiplyMatrices(
      turn(rotation),
      multiplyMatrices(
        turn(skewAxis),
        multiplyMatrices(shear, multiplyMatrices(turn(-skewAxis), [scaleWidth, 0, 0, scale, 0, 0])),
      ),
    );
    matrix = [
      ...linear.slice(0, 4),
      pivot[0] - linear[0] * anchor[0] - linear[2] * anchor[1],
      pivot[1] - linear[1] * anchor[0] - linear[3] * anchor[1],
    ];
  }
  if (!matrix.every(Number.isFinite)) throw Error('Transform exceeds finite coordinates');
  return {
    anchor,
    position,
    offset,
    scale,
    scaleWidth,
    uniformScale,
    rotation,
    skew,
    skewAxis,
    pivot,
    matrix,
  };
}
export const transformVector = (state, vector) => transformCharacterPoint(state.matrix, vector);

// Rebase the pivot without changing the affine translation. Position is absolute;
// retained Offset is deliberately never rewritten (it may have its own driver).
export function repositionTransformAnchor(state, anchor) {
  if (!Array.isArray(anchor) || anchor.length !== 2 || !anchor.every(Number.isFinite))
    throw Error('Anchor requires finite XY coordinates');
  const delta = anchor.map((v, i) => v - state.anchor[i]);
  const [a, b, c, d] = state.matrix;
  const position = [
    state.position[0] + a * delta[0] + c * delta[1],
    state.position[1] + b * delta[0] + d * delta[1],
  ];
  if (!position.every(Number.isFinite))
    throw Error('Anchor compensation exceeds finite coordinates');
  return { anchor: [...anchor], position };
}
export function anchorFromOutputDelta(state, delta) {
  const [a, b, c, d] = state.matrix,
    determinant = a * d - b * c;
  const norm = Math.max(Math.abs(a), Math.abs(b), Math.abs(c), Math.abs(d));
  if (!Number.isFinite(determinant) || !norm || Math.abs(determinant) <= 1e-12 * norm * norm)
    throw Error('Collapsed Transform: use an anchor preset or restore scale before dragging');
  if (!Array.isArray(delta) || delta.length !== 2 || !delta.every(Number.isFinite))
    throw Error('Anchor drag requires finite XY coordinates');
  if (Math.hypot(...delta) < 1e-12) return [...state.anchor];
  return [
    state.anchor[0] + (d * delta[0] - c * delta[1]) / determinant,
    state.anchor[1] + (a * delta[1] - b * delta[0]) / determinant,
  ];
}
