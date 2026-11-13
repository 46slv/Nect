export const transformNodeDeclaration = {
  typeId: 'Transform',
  revision: 1,
  implementationId: 'builtin.transform.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'path',
  shape: { protocol: 'nect.transform-mode', revision: 1 },
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'anchor', type: 'Vector2', default: [0, 0] },
    { id: 'offset', type: 'Vector2', default: [0, 0] },
    { id: 'scale', type: 'Scalar', default: 1 },
    { id: 'rotation', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'transform',
    labelJa: '移動',
    aliases: ['移動', '回転', '拡大縮小'],
    description: '形状の移動・回転・拡大縮小',
    creationRoute: 'add',
  },
};
