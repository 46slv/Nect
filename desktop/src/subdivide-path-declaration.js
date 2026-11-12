export const subdividePathDeclaration = {
  typeId: 'Subdivide Path',
  revision: 1,
  implementationId: 'builtin.subdivide-path.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'segments', type: 'Scalar', default: 2 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '分割',
    aliases: ['分割'],
    description: '分割',
    creationRoute: 'add',
  },
};
