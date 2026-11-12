export const circleDeclaration = {
  typeId: 'Circle',
  revision: 1,
  implementationId: 'builtin.circle.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'center', type: 'Vector2', default: [350, 250] },
    { id: 'radius', type: 'Scalar', default: 120 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '円',
    aliases: ['円'],
    description: '円',
    creationRoute: 'add',
  },
};
