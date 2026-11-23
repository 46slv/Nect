export const lineDeclaration = {
  typeId: 'Line',
  revision: 1,
  implementationId: 'builtin.line.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'position', type: 'Vector2', default: [250, 250] },
    { id: 'length', type: 'Scalar', default: 200 },
    { id: 'useWidth', type: 'Boolean', default: false },
    { id: 'width', type: 'Scalar', default: 10 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '直線',
    aliases: ['直線', '線分', 'line segment'],
    description: '直線',
    creationRoute: 'add',
  },
};
