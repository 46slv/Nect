export const textNodeDeclaration = {
  typeId: 'Text',
  revision: 1,
  implementationId: 'builtin.text.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'position', type: 'Vector2', default: [200, 300] },
    { id: 'size', type: 'Scalar', default: 100 },
    { id: 'letterSpacing', type: 'Scalar', default: 0 },
    { id: 'lineSpacing', type: 'Scalar', default: 1.2 },
  ],
  outputs: [
    { id: 'default', label: 'Path', type: 'Path', legacyDefault: true },
    { id: 'position', label: 'Position', type: 'Vector2' },
  ],
  catalog: {
    categoryId: 'create',
    labelJa: '文字',
    aliases: ['文字', 'テキスト'],
    description: '文字 テキスト',
    creationRoute: 'add',
  },
};
