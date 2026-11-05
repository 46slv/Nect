// Data only; derived interface types are selected by operation.
export const vectorMathDeclaration = {
  typeId: 'Vector Math',
  revision: 1,
  implementationId: 'builtin.vector-math.v1',
  capabilities: [],
  defaultOperation: 'add',
  bypassInput: 'a',
  parameters: [
    { id: 'a', type: 'Vector2', default: [0, 0] },
    { id: 'b', type: 'Vector2', default: [0, 0] },
    { id: 'scale', type: 'Scalar', default: 1 },
  ],
  outputs: [{ id: 'default', label: '', legacyDefault: true, result: true }],
  operations: [
    { id: 'add', active: ['a', 'b'], resultType: 'Vector2' },
    { id: 'subtract', active: ['a', 'b'], resultType: 'Vector2' },
    { id: 'multiply', active: ['a', 'b'], resultType: 'Vector2' },
    { id: 'divide', active: ['a', 'b'], resultType: 'Vector2' },
    { id: 'scale', active: ['a', 'scale'], resultType: 'Vector2' },
    { id: 'normalize', active: ['a'], resultType: 'Vector2' },
    { id: 'length', active: ['a'], resultType: 'Scalar' },
    { id: 'distance', active: ['a', 'b'], resultType: 'Scalar' },
    { id: 'dot', active: ['a', 'b'], resultType: 'Scalar' },
  ],
  catalog: {
    categoryId: 'value',
    labelJa: 'ベクトル計算',
    aliases: ['ベクトル計算'],
    description: 'ベクトル計算',
    creationRoute: 'add',
  },
};
