// Trusted developer-only fixture, never imported by the production manifest.
export const fixtureDeclaration = {
  typeId: 'Dev Affine Fixture',
  revision: 1,
  implementationId: 'fixture.affine.v1',
  capabilities: [],
  resultType: 'Scalar',
  parameters: [
    { id: 'x', type: 'Scalar', default: 2 },
    { id: 'scale', type: 'Scalar', default: 3 },
    { id: 'offset', type: 'Scalar', default: 4 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Scalar', legacyDefault: true }],
  catalog: {
    categoryId: 'value',
    labelJa: '',
    aliases: [],
    description: 'developer fixture',
    creationRoute: 'add',
  },
};
