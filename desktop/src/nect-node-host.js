import { joinSlotsProtocol } from './join-slots-protocol.js';
import { isVector2 } from './node-value-validation.js';

// Trusted host adapters: declaration structure is separate from authored Document validation.
const scalar = Object.freeze({
  validateDefault: (value) => typeof value === 'number' && Number.isFinite(value),
  validateOutput: () => true,
});
export const nectNodeHost = Object.freeze({
  shapeProtocols: new Map([['nect.join-slots', joinSlotsProtocol]]),
  types: new Map([
    ['Scalar', scalar],
    [
      'Boolean',
      Object.freeze({
        validateDefault: (value) => typeof value === 'boolean',
        validateOutput: () => true,
      }),
    ],
    [
      'PaintedPath',
      Object.freeze({ validateDefault: (value) => value === null, validateOutput: () => true }),
    ],
    [
      'Path',
      Object.freeze({ validateDefault: (value) => value === null, validateOutput: () => true }),
    ],
    ['Vector2', Object.freeze({ validateDefault: isVector2, validateOutput: () => true })],
  ]),
  validateOutputs: (outputs) =>
    outputs.every(
      (output) =>
        typeof output.label === 'string' &&
        (output.legacyDefault === undefined || output.legacyDefault === true),
    ) &&
    (outputs.some((output) => output.legacyDefault === true)
      ? outputs.length === 1
      : outputs.every((output) => output.legacyDefault === undefined)),
});
export function nectOutputPort(output) {
  return output.legacyDefault ? undefined : output.id;
}
