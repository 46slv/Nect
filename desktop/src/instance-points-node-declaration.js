export const instancePointsDeclaration = {
  typeId: 'Instance on Points',
  revision: 1,
  implementationId: 'builtin.instance-points.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'points', type: 'Path', default: null },
    { id: 'alignTangent', type: 'Boolean', default: false },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'repeat',
    labelJa: '点に配置',
    aliases: ['点に配置'],
    description: '点に配置',
    creationRoute: 'add',
  },
};
