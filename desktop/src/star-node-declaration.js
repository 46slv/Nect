export const starDeclaration = {
  typeId: 'Star',
  revision: 1,
  implementationId: 'builtin.star.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'center', type: 'Vector2', default: [350, 250] },
    { id: 'points', type: 'Scalar', default: 5 },
    { id: 'outerRadius', type: 'Scalar', default: 120 },
    { id: 'innerRadius', type: 'Scalar', default: 60 },
    { id: 'rotation', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '星',
    aliases: ['星'],
    description: '星',
    creationRoute: 'add',
  },
};
