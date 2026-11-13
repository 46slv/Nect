export const imageDeclaration = {
  typeId: 'Image',
  revision: 1,
  implementationId: 'builtin.image.v1',
  capabilities: [],
  resultType: 'Image',
  parameters: [
    { id: 'position', type: 'Vector2', default: [100, 100] },
    { id: 'size', type: 'Vector2', default: [320, 240] },
  ],
  outputs: [{ id: 'default', label: '', type: 'Image', legacyDefault: true }],
  catalog: {
    categoryId: 'image',
    labelJa: 'Image',
    aliases: [],
    description: 'Image',
    creationRoute: 'add',
  },
};
