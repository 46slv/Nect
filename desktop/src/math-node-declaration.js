// Data only: this declaration is never serialized into an authored document.
export const mathDeclaration = {
  typeId: 'Math',
  revision: 1,
  implementationId: 'builtin.math.v1',
  resultType: 'Scalar',
  capabilities: [],
  defaultOperation: 'add',
  bypassInput: 'a',
  parameters: [
    { id: 'a', type: 'Scalar', default: 0 },
    { id: 'b', type: 'Scalar', default: 0 },
    { id: 'min', type: 'Scalar', default: 0 },
    { id: 'max', type: 'Scalar', default: 1 },
  ],
  outputs: [{ id: 'default', type: 'Scalar', label: '', legacyDefault: true }],
  operations: [
    { id: 'add', active: ['a', 'b'] },
    { id: 'subtract', active: ['a', 'b'] },
    { id: 'multiply', active: ['a', 'b'] },
    { id: 'divide', active: ['a', 'b'] },
    { id: 'min', active: ['a', 'b'] },
    { id: 'max', active: ['a', 'b'] },
    { id: 'abs', active: ['a'] },
    { id: 'clamp', active: ['a', 'min', 'max'] },
  ],
  catalog: {
    categoryId: 'value',
    labelJa: '計算',
    aliases: ['計算'],
    description: '計算',
    creationRoute: 'add',
  },
};
