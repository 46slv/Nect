// Preserve the existing authored Color source default and catalog metadata.
export const colorDeclaration = {
  typeId: 'Color',
  revision: 1,
  implementationId: 'builtin.read-value.v1',
  resultType: 'Color',
  capabilities: [],
  parameters: [{ id: 'value', type: 'Color', default: { hex: '#36434e', alpha: 1 } }],
  outputs: [{ id: 'default', type: 'Color', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'paint',
    labelJa: '色',
    aliases: ['色', 'カラー'],
    description: '色 カラー',
    creationRoute: 'add',
  },
};
