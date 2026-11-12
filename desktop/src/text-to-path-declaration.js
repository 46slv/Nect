export const textToPathDeclaration = {
  typeId: 'Text to Path',
  revision: 1,
  implementationId: 'builtin.text-to-path.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  parameters: [{ id: 'path', type: 'Path', default: null }],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '文字をパス',
    aliases: ['文字をパス', 'アウトライン'],
    description: '文字をパス アウトライン',
    creationRoute: 'add',
  },
};
