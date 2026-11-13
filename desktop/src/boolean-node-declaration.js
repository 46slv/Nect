export const booleanNodeDeclaration = {
  typeId: 'Boolean',
  revision: 1,
  implementationId: 'builtin.boolean.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'a',
  preserveSupplementalInputs: true,
  shape: { protocol: 'nect.boolean-slots', revision: 1 },
  parameters: [
    { id: 'a', type: 'Path', default: null },
    { id: 'b', type: 'Path', default: null },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  operations: ['union', 'subtract', 'intersect', 'difference'].map((id) => ({
    id,
    active: ['a', 'b'],
  })),
  defaultOperation: 'subtract',
  catalog: {
    categoryId: 'combine',
    labelJa: '結合',
    aliases: ['結合', 'ブーリアン', 'シルエット'],
    description: '輪郭を演算して合成する',
    creationRoute: 'add',
  },
};
