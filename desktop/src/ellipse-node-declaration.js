export const ellipseDeclaration = {
  typeId: 'Ellipse',
  revision: 1,
  implementationId: 'builtin.ellipse.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'center', type: 'Vector2', default: [350, 250] },
    { id: 'size', type: 'Vector2', default: [240, 160] },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '楕円',
    aliases: ['楕円'],
    description: '楕円',
    creationRoute: 'add',
  },
};
