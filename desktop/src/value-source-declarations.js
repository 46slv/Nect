// Trusted authoring helper; each result is ordinary data captured by the manifest.
const declaration = (typeId, type, value, catalog) => ({
  typeId,
  revision: 1,
  implementationId: 'builtin.read-value.v1',
  resultType: type,
  capabilities: [],
  parameters: [{ id: 'value', type, default: value }],
  outputs: [{ id: 'default', type, label: '', legacyDefault: true }],
  catalog: { categoryId: 'value', ...catalog, creationRoute: 'add' },
});

export const valueDeclaration = declaration('Value', 'Scalar', 100, {
  labelJa: '数値',
  aliases: ['数値'],
  description: '数値',
});
export const vectorDeclaration = declaration('Vector', 'Vector2', [300, 250], {
  labelJa: 'ベクトル',
  aliases: ['ベクトル', '座標'],
  description: 'ベクトル 座標',
});
export const expressionDeclaration = declaration('Expression', 'Scalar', 100, {
  labelJa: '式',
  aliases: ['式'],
  description: '式',
});
