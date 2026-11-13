const dotParameters = [
  { id: 'path', type: 'PaintedPath', default: null },
  { id: 'pitch', type: 'Scalar', default: 16 },
  { id: 'dotSize', type: 'Scalar', default: 16 },
  { id: 'dotSides', type: 'Scalar', default: 4 },
  { id: 'dotRotation', type: 'Scalar', default: 0 },
  { id: 'starInnerRatio', type: 'Scalar', default: 0.25 },
  { id: 'invert', type: 'Boolean', default: false },
  { id: 'gridOrigin', type: 'Vector2', default: [0, 0] },
  { id: 'tonePower', type: 'Scalar', default: 1 },
];
const lineParameters = dotParameters.filter(
  ({ id }) => !['dotSize', 'dotSides', 'dotRotation', 'starInnerRatio'].includes(id),
);
const outputs = [{ id: 'default', label: '', type: 'PaintedPath', legacyDefault: true }];
const declaration = (
  typeId,
  implementationId,
  parameters,
  operations,
  defaultOperation,
  catalog,
) => ({
  typeId,
  revision: 1,
  implementationId,
  capabilities: [],
  resultType: 'PaintedPath',
  bypassInput: 'path',
  preserveSupplementalInputs: true,
  parameters,
  outputs,
  operations: operations.map((id) => ({ id, active: parameters.map((parameter) => parameter.id) })),
  defaultOperation,
  catalog: { categoryId: 'image', ...catalog, creationRoute: 'add' },
});

export const halftoneDeclaration = declaration(
  'Halftone',
  'builtin.halftone.v1',
  dotParameters,
  ['subtract', 'square', 'circle', 'polygon', 'star'],
  'subtract',
  { labelJa: '網点', aliases: ['網点', 'ハーフトーン'], description: '網点 ハーフトーン' },
);
export const orderedDitherDeclaration = declaration(
  'Ordered Dither',
  'builtin.ordered-dither.v1',
  dotParameters,
  ['square', 'circle', 'polygon', 'star'],
  'square',
  { labelJa: '規則ディザ', aliases: ['規則ディザ'], description: '規則ディザ' },
);
export const errorDiffusionDeclaration = declaration(
  'Error Diffusion Dither',
  'builtin.error-diffusion.v1',
  dotParameters,
  ['square', 'circle', 'polygon', 'star'],
  'square',
  { labelJa: '誤差拡散ディザ', aliases: ['誤差拡散ディザ'], description: '誤差拡散ディザ' },
);
export const lineScreenDeclaration = declaration(
  'Line Screen',
  'builtin.line-screen.v1',
  lineParameters,
  ['horizontal', 'vertical'],
  'horizontal',
  { labelJa: '線スクリーン', aliases: ['線スクリーン'], description: '線スクリーン' },
);
