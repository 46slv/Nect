export const linearGradientDeclaration = {
  typeId: 'Linear Gradient Fill',
  revision: 1,
  implementationId: 'builtin.linear-gradient-fill.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'PaintedPath', default: null },
    { id: 'start', type: 'Vector2', default: [200, 250] },
    { id: 'end', type: 'Vector2', default: [500, 250] },
    { id: 'startColor', type: 'Color', default: { hex: '#ff6b35', alpha: 1 } },
    { id: 'endColor', type: 'Color', default: { hex: '#6554c0', alpha: 1 } },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'paint',
    labelJa: '線形グラデーション',
    aliases: ['線形グラデーション', '塗り'],
    description: '線形グラデーション 塗り',
    creationRoute: 'add',
  },
};
