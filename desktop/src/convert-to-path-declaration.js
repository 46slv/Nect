export const convertToPathDeclaration = {
  typeId: 'Convert to Path',
  revision: 1,
  implementationId: 'builtin.convert-to-path.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [{ id: 'path', type: 'Path', default: null }],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: 'パス変換',
    aliases: ['パス変換'],
    description: 'パス変換',
    creationRoute: 'add',
  },
};
