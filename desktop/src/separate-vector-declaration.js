// Data-only projection of the existing Separate Vector node.
export const separateVectorDeclaration = {
  typeId: 'Separate Vector',
  revision: 1,
  implementationId: 'builtin.separate-vector.v1',
  resultType: 'Vector2',
  capabilities: [],
  parameters: [{ id: 'vector', type: 'Vector2', default: [0, 0] }],
  outputs: [
    { id: 'x', label: 'X', type: 'Scalar' },
    { id: 'y', label: 'Y', type: 'Scalar' },
  ],
  catalog: {
    categoryId: 'value',
    labelJa: 'ベクトル分離',
    aliases: ['ベクトル分離'],
    description: 'ベクトル分離',
    creationRoute: 'add',
  },
};
