const position = [{ id: 'position', type: 'Vector2', default: [0, 0] }];
const outputs = [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }];

export const svgReferenceDeclaration = {
  typeId: 'SVG',
  revision: 1,
  implementationId: 'builtin.svg-reference.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  parameters: position,
  outputs,
  catalog: {
    categoryId: 'combine',
    labelJa: 'SVG',
    aliases: [],
    description: 'SVG',
    creationRoute: 'add',
  },
};

export const compositionReferenceDeclaration = {
  typeId: 'Composition Reference',
  revision: 1,
  implementationId: 'builtin.composition-reference.v1',
  capabilities: [],
  resultType: 'PaintedPath',
  parameters: position,
  outputs,
  catalog: {
    categoryId: 'combine',
    labelJa: 'Composition参照',
    aliases: ['コンポジション参照', 'コンポ', 'composition output', 'parts'],
    description: '別Compositionの出力を配置',
    creationRoute: 'add',
  },
};
