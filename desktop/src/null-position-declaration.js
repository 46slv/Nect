// Data-only projection of the existing non-rendering position controller.
export const nullPositionDeclaration = {
  typeId: 'Null',
  revision: 1,
  implementationId: 'builtin.null-position.v1',
  resultType: 'Vector2',
  capabilities: [],
  parameters: [{ id: 'position', type: 'Vector2', default: [300, 250] }],
  outputs: [{ id: 'default', type: 'Vector2', label: 'Position', legacyDefault: true }],
  catalog: {
    categoryId: 'value',
    labelJa: '基準点',
    aliases: ['基準点'],
    description: '基準点',
    creationRoute: 'add',
  },
};
