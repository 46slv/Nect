export const taperDeclaration = {
  typeId: 'Taper',
  revision: 1,
  implementationId: 'builtin.taper.v1',
  resultType: 'Path',
  capabilities: [],
  defaultOperation: 'horizontal',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'amount', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  operations: [
    { id: 'horizontal', active: ['path', 'amount'] },
    { id: 'vertical', active: ['path', 'amount'] },
  ],
  catalog: {
    categoryId: 'transform',
    labelJa: '先細り',
    aliases: ['先細り'],
    description: '先細り',
    creationRoute: 'add',
  },
};
