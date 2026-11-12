export const setPathStartDeclaration = {
  typeId: 'Set Path Start',
  revision: 1,
  implementationId: 'builtin.set-path-start.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'start', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '始点',
    aliases: ['始点'],
    description: '始点',
    creationRoute: 'add',
  },
};
