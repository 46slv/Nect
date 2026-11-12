export const twistPointsDeclaration = {
  typeId: 'Twist Points',
  revision: 1,
  implementationId: 'builtin.twist-points.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'angle', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'transform',
    labelJa: 'ねじる',
    aliases: ['ねじる'],
    description: 'ねじる',
    creationRoute: 'add',
  },
};
