export const autoHandlesDeclaration = {
  typeId: 'Auto Handles',
  revision: 1,
  implementationId: 'builtin.auto-handles.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'strength', type: 'Scalar', default: 1 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '自動ハンドル',
    aliases: ['自動ハンドル'],
    description: '自動ハンドル',
    creationRoute: 'add',
  },
};
