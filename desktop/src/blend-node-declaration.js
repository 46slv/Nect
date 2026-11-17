export const blendNodeDeclaration = {
  typeId: 'Blend',
  revision: 1,
  implementationId: 'builtin.blend.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'a',
  parameters: [
    { id: 'a', type: 'PaintedPath', default: null },
    { id: 'b', type: 'PaintedPath', default: null },
    { id: 'steps', type: 'Scalar', default: 5 },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'combine',
    labelJa: 'ブレンド',
    aliases: ['Blend', '形状補間'],
    description: '二つの図形の中間形状を作成',
    creationRoute: 'add',
  },
};
