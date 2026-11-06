// Data-only projection of the existing two-Scalar to Vector2 node.
export const combineVectorDeclaration = {
  typeId: 'Combine Vector',
  revision: 1,
  implementationId: 'builtin.combine-vector.v1',
  resultType: 'Vector2',
  capabilities: [],
  parameters: [
    { id: 'x', type: 'Scalar', default: 0 },
    { id: 'y', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', type: 'Vector2', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'value',
    labelJa: 'ベクトル合成',
    aliases: ['ベクトル合成'],
    description: 'ベクトル合成',
    creationRoute: 'add',
  },
};
