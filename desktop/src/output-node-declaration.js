export const outputDeclaration = {
  typeId: 'Output',
  revision: 1,
  implementationId: 'builtin.output.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  parameters: [{ id: 'path', type: 'PaintedPath', default: null }],
  outputs: [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }],
  catalog: {
    categoryId: 'combine',
    labelJa: 'Output',
    aliases: [],
    description: 'Output',
    creationRoute: 'add',
  },
};
