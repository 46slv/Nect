export const resamplePathDeclaration = {
  typeId: 'Resample Path',
  revision: 1,
  implementationId: 'builtin.resample-path.v1',
  capabilities: [],
  resultType: 'Path',
  defaultOperation: 'count',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'count', type: 'Scalar', default: 32 },
    { id: 'spacing', type: 'Scalar', default: 24 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  operations: [
    { id: 'count', active: ['path', 'count'] },
    { id: 'spacing', active: ['path', 'spacing'] },
  ],
  catalog: {
    categoryId: 'path',
    labelJa: '再サンプル',
    aliases: ['再サンプル'],
    description: '再サンプル',
    creationRoute: 'add',
  },
};
