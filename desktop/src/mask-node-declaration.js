export const maskDeclaration = {
  typeId: 'Mask',
  revision: 1,
  implementationId: 'builtin.mask.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'PaintedPath', default: null },
    { id: 'mask', type: 'Path', default: null },
    { id: 'invert', type: 'Boolean', default: false },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'image',
    labelJa: 'マスク',
    aliases: ['Mask', '切り抜き', 'クリップ'],
    description: '画像・Compositionを閉じたパスで切り抜く。',
    creationRoute: 'add',
  },
};
