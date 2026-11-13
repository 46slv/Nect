export const keyerDeclaration = {
  typeId: 'Keyer',
  revision: 1,
  implementationId: 'builtin.keyer.v1',
  capabilities: [],
  resultType: 'Image',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Image', default: null },
    { id: 'keyColor', type: 'Color', default: { hex: '#00ff00', alpha: 1 } },
    { id: 'tolerance', type: 'Scalar', default: 0.1 },
    { id: 'softness', type: 'Scalar', default: 0.05 },
  ],
  outputs: [{ id: 'default', label: '', type: 'Image', legacyDefault: true }],
  catalog: {
    categoryId: 'image',
    labelJa: 'カラーキー',
    aliases: ['Keyer', 'Chroma key', '透過', 'クロマキー'],
    description: '指定色を透明にする。範囲と境界の柔らかさを調整。',
    creationRoute: 'add',
  },
};
