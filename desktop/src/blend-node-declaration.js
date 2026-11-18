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
    { id: 'distance', type: 'Scalar', default: 20 },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  operations: [
    { id: 'steps', active: ['a', 'b', 'steps'] },
    { id: 'distance', active: ['a', 'b', 'distance'] },
  ],
  defaultOperation: 'steps',
  operationAliases: { union: 'steps', subtract: 'steps', intersect: 'steps', difference: 'steps' },
  catalog: {
    categoryId: 'combine',
    labelJa: 'ブレンド',
    aliases: ['Blend', '形状補間'],
    description: '二つの図形の中間形状を作成',
    creationRoute: 'add',
  },
};
