export const smoothPathDeclaration = {
  typeId: 'Smooth Path',
  revision: 1,
  implementationId: 'builtin.smooth-path.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'factor', type: 'Scalar', default: 0.5 },
    { id: 'iterations', type: 'Scalar', default: 3 },
    { id: 'keepEndpoints', type: 'Boolean', default: true },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '滑らか',
    aliases: ['滑らか', 'スムーズ'],
    description: '滑らか スムーズ',
    creationRoute: 'add',
  },
};
