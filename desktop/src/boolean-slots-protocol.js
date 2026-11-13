import { joinSlotsProtocol } from './join-slots-protocol.js';

// Boolean shares the bounded slot shape, but its spare is authored only on use.
export const booleanSlotsProtocol = Object.freeze({
  staticOutputs: true,
  staticOutputType: 'Path',
  allShapeInputsActive: true,
  parameterKeys: (node) => node.booleanInputOrder ?? ['a', 'b'],
  validateDeclaration: (shape) =>
    !!shape &&
    Object.keys(shape).length === 2 &&
    shape.protocol === 'nect.boolean-slots' &&
    shape.revision === 1,
  snapshot: (node) =>
    joinSlotsProtocol.snapshot({
      id: node.id,
      // Read-only source navigation also accepts partial in-memory descriptors.
      params: Object.fromEntries(
        Object.entries(node.params).map(([id, input]) => [
          id,
          { type: input?.type, source: input?.source, locked: !!input?.locked },
        ]),
      ),
      joinInputOrder: node.booleanInputOrder ?? ['a', 'b'],
    }),
  resolve: joinSlotsProtocol.resolve,
  initialFields: () => ({}),
});
