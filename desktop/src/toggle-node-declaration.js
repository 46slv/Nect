// Data-only projection of the existing Boolean value source.
export const toggleDeclaration = {
  typeId: 'Toggle',
  revision: 1,
  implementationId: 'builtin.read-value.v1',
  resultType: 'Boolean',
  capabilities: [],
  parameters: [{ id: 'value', type: 'Boolean', default: true }],
  outputs: [{ id: 'default', type: 'Boolean', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'value',
    labelJa: '切り替え',
    aliases: ['切り替え', '真偽'],
    description: '切り替え 真偽',
    creationRoute: 'add',
  },
};
