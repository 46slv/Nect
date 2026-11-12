export const bendPointsDeclaration = {
  typeId: 'Bend Points',
  revision: 1,
  implementationId: 'builtin.bend-points.v1',
  resultType: 'Path',
  capabilities: [],
  defaultOperation: 'horizontal',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'angle', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  operations: [
    { id: 'horizontal', active: ['path', 'angle'] },
    { id: 'vertical', active: ['path', 'angle'] },
  ],
  catalog: {
    categoryId: 'transform',
    labelJa: '曲げる',
    aliases: ['曲げる'],
    description: '曲げる',
    creationRoute: 'add',
  },
};
