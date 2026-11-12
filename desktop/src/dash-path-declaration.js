export const dashPathDeclaration = {
  typeId: 'Dash Path',
  revision: 1,
  implementationId: 'builtin.dash-path.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'dashLength', type: 'Scalar', default: 12 },
    { id: 'gapLength', type: 'Scalar', default: 6 },
    { id: 'offset', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '破線',
    aliases: ['破線'],
    description: '破線',
    creationRoute: 'add',
  },
};
