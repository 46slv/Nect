export const trimPathDeclaration = {
  typeId: 'Trim Path',
  revision: 1,
  implementationId: 'builtin.trim-path.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'start', type: 'Scalar', default: 0 },
    { id: 'end', type: 'Scalar', default: 1 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '切り取り',
    aliases: ['切り取り', 'トリム'],
    description: '切り取り トリム',
    creationRoute: 'add',
  },
};
