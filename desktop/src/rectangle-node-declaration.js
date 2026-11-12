export const rectangleDeclaration = {
  typeId: 'Rectangle',
  revision: 1,
  implementationId: 'builtin.rectangle.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [
    { id: 'center', type: 'Vector2', default: [460, 250] },
    { id: 'size', type: 'Vector2', default: [180, 220] },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '四角形',
    aliases: ['四角形', '長方形'],
    description: '四角形 長方形',
    creationRoute: 'add',
  },
};
