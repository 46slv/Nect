// Preserve the existing Path input, tolerance and pass-through bypass contract.
export const simplifyPathDeclaration = {
  typeId: 'Simplify Path',
  revision: 1,
  implementationId: 'builtin.simplify-path.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'tolerance', type: 'Scalar', default: 1 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '簡略化',
    aliases: ['簡略化'],
    description: '簡略化',
    creationRoute: 'add',
  },
};
