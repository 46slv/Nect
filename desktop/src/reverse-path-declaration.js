// Closed built-in declaration; Path-null default is authored fallback, not geometry.
export const reversePathDeclaration = {
  typeId: 'Reverse Path',
  revision: 1,
  implementationId: 'builtin.reverse-path.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [{ id: 'path', type: 'Path', default: null }],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '反転',
    aliases: ['反転', '方向'],
    description: '反転 方向',
    creationRoute: 'add',
  },
};
