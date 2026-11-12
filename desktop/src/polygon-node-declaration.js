export const polygonDeclaration = {
  typeId: 'Polygon',
  revision: 1,
  implementationId: 'builtin.polygon.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'center', type: 'Vector2', default: [350, 250] },
    { id: 'sides', type: 'Scalar', default: 6 },
    { id: 'radius', type: 'Scalar', default: 120 },
    { id: 'rotation', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '多角形',
    aliases: ['多角形'],
    description: '多角形',
    creationRoute: 'add',
  },
};
