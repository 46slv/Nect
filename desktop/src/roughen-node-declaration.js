export const roughenDeclaration = {
  typeId: 'Roughen',
  revision: 1,
  implementationId: 'builtin.roughen.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'amount', type: 'Scalar', default: 3 },
    { id: 'seed', type: 'Scalar', default: 1 },
    { id: 'keepEndpoints', type: 'Boolean', default: true },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'transform',
    labelJa: 'ラフ',
    aliases: ['ラフ', 'ランダム'],
    description: 'ラフ ランダム',
    creationRoute: 'add',
  },
};
