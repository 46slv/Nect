export const curveDeclaration = {
  typeId: 'Curve',
  revision: 1,
  implementationId: 'builtin.curve.v1',
  capabilities: [],
  resultType: 'Path',
  parameters: [{ id: 'closed', type: 'Boolean', default: true }],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'create',
    labelJa: '曲線',
    aliases: ['曲線', 'ベジェ'],
    description: '曲線 ベジェ',
    creationRoute: 'add',
  },
};
