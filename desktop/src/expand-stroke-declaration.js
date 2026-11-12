// PaintedPath-to-Path conversion has no same-type identity bypass.
export const expandStrokeDeclaration = {
  typeId: 'Expand Stroke',
  revision: 1,
  implementationId: 'builtin.expand-stroke.v1',
  resultType: 'Path',
  capabilities: [],
  parameters: [
    { id: 'path', type: 'PaintedPath', default: null },
    { id: 'startWidthMultiplier', type: 'Scalar', default: 1 },
    { id: 'endWidthMultiplier', type: 'Scalar', default: 1 },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  catalog: {
    categoryId: 'path',
    labelJa: '線をパス',
    aliases: ['線をパス', 'アウトライン'],
    description: '線をパス アウトライン',
    creationRoute: 'add',
  },
};
