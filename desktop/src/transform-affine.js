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
