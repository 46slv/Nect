export const closedOffsetDeclaration = {
  typeId: 'Closed Path Offset',
  revision: 1,
  implementationId: 'builtin.closed-path-offset.v1',
  resultType: 'Path',
  capabilities: [],
  defaultOperation: 'miter',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'amount', type: 'Scalar', default: 10 },
    { id: 'miterLimit', type: 'Scalar', default: 4 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  operations: [
    { id: 'miter', active: ['path', 'amount', 'miterLimit'] },
    { id: 'bevel', active: ['path', 'amount', 'miterLimit'] },
    { id: 'round', active: ['path', 'amount', 'miterLimit'] },
  ],
  catalog: {
    categoryId: 'path',
    labelJa: '閉じた輪郭',
    aliases: ['閉じた輪郭', 'オフセット'],
    description: '閉じた輪郭 オフセット',
    creationRoute: 'add',
  },
};
