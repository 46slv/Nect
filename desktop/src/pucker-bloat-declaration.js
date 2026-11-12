// Keep the Path pass-through and the authored manual center even when it is inactive.
export const puckerBloatDeclaration = {
  typeId: 'Pucker & Bloat',
  revision: 1,
  implementationId: 'builtin.pucker-bloat.v1',
  resultType: 'Path',
  capabilities: [],
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'amount', type: 'Scalar', default: 0 },
    { id: 'autoCenter', type: 'Boolean', default: true },
    { id: 'center', type: 'Vector2', default: [0, 0] },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'transform',
    labelJa: '収縮',
    aliases: ['収縮', '膨張'],
    description: '収縮 膨張',
    creationRoute: 'add',
  },
};
