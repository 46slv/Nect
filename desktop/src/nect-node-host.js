// Trusted host adapters: declaration structure is separate from authored Document validation.
const scalar = Object.freeze({
  validateDefault: (value) => typeof value === 'number' && Number.isFinite(value),
  validateOutput: () => true,
});
export const nectNodeHost = Object.freeze({
  types: new Map([['Scalar', scalar]]),
  validateOutputs: (outputs) =>
    outputs.length === 1 &&
    outputs[0].legacyDefault === true &&
    typeof outputs[0].label === 'string',
});
export function nectOutputPort(output) {
  return output.legacyDefault ? undefined : output.id;
}
