export const imageOutputDeclaration = {
  typeId: 'Image Output',
  revision: 1,
  implementationId: 'builtin.image-output.v1',
  capabilities: [],
  resultType: 'Image',
  parameters: [{ id: 'path', type: 'Image', default: null }],
  outputs: [{ id: 'default', label: '', type: 'Image', legacyDefault: true }],
  catalog: {
    categoryId: 'image',
    labelJa: 'Image Output',
    aliases: [],
    description: 'Image Output',
    creationRoute: 'add',
  },
};
