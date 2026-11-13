export const strokeDeclaration = {
  typeId: 'Stroke',
  revision: 1,
  implementationId: 'builtin.stroke.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'PaintedPath', default: null },
    { id: 'color', type: 'Color', default: { hex: '#36434e', alpha: 1 } },
    { id: 'width', type: 'Scalar', default: 2 },
    { id: 'miterLimit', type: 'Scalar', default: 4 },
    { id: 'dashOffset', type: 'Scalar', default: 0 },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'paint',
    labelJa: '線',
    aliases: ['線', 'ストローク'],
    description: '線 ストローク',
    creationRoute: 'add',
  },
};
