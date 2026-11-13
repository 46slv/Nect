export const pathRemapDeclaration = {
  typeId: 'Path Remap',
  revision: 1,
  implementationId: 'builtin.path-remap.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'offset', type: 'Vector2', default: [0, 0] },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'transform',
    labelJa: 'パス再配置',
    aliases: ['パス再配置'],
    description: 'パス再配置',
    creationRoute: 'add',
  },
};
