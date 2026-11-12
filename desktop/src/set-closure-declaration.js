export const setClosureDeclaration = {
  typeId: 'Set Closure',
  revision: 1,
  implementationId: 'builtin.set-closure.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'closed', type: 'Boolean', default: true },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '開閉',
    aliases: ['開閉', '閉じる'],
    description: '開閉 閉じる',
    creationRoute: 'add',
  },
};
