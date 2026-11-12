export const vectorTransformDeclaration = {
  typeId: 'Vector Transform',
  revision: 1,
  implementationId: 'builtin.vector-transform.v1',
  capabilities: [],
  resultType: 'Vector2',
  bypassInput: 'vector',
  parameters: [
    { id: 'vector', type: 'Vector2', default: [0, 0] },
    { id: 'anchor', type: 'Vector2', default: [0, 0] },
    { id: 'offset', type: 'Vector2', default: [0, 0] },
    { id: 'scale', type: 'Scalar', default: 1 },
    { id: 'rotation', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Vector2', legacyDefault: true }],
  catalog: {
    categoryId: 'value',
    labelJa: 'ベクトル変換',
    aliases: ['ベクトル変換'],
    description: '数値ベクトルの移動・回転・拡大縮小',
    creationRoute: 'add',
  },
};
