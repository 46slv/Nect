export const radialGradientDeclaration = {
  typeId: 'Radial Gradient Fill',
  revision: 1,
  implementationId: 'builtin.radial-gradient-fill.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'PaintedPath', default: null },
    { id: 'center', type: 'Vector2', default: [350, 250] },
    { id: 'radius', type: 'Scalar', default: 150 },
    { id: 'startColor', type: 'Color', default: { hex: '#fff3b0', alpha: 1 } },
    { id: 'endColor', type: 'Color', default: { hex: '#d94862', alpha: 1 } },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'paint',
    labelJa: '放射グラデーション',
    aliases: ['放射グラデーション', '塗り'],
    description: '放射グラデーション 塗り',
    creationRoute: 'add',
  },
};
