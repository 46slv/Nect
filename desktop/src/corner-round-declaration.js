export const cornerRoundDeclaration = {
  typeId: 'Corner Round',
  revision: 1,
  implementationId: 'builtin.corner-round.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'radius', type: 'Scalar', default: 12 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '角丸',
    aliases: ['角丸'],
    description: '角丸',
    creationRoute: 'add',
  },
};
