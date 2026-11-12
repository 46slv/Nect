export const fillDeclaration = {
  typeId: 'Fill',
  revision: 1,
  implementationId: 'builtin.fill.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'PaintedPath', default: null },
    { id: 'color', type: 'Color', default: { hex: '#36434e', alpha: 1 } },
  ],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'paint',
    labelJa: '塗り',
    aliases: ['塗り'],
    description: '塗り',
    creationRoute: 'add',
  },
};
