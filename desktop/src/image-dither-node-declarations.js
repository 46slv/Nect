import {
  orderedDitherDeclaration,
  errorDiffusionDeclaration,
} from './luminance-marks-node-declarations.js';

const declaration = (typeId, implementationId, shape, catalog) => ({
  typeId,
  revision: 1,
  implementationId,
  capabilities: [],
  resultType: 'Image',
  bypassInput: 'path',
  preserveSupplementalInputs: true,
  parameters: shape.parameters.map((parameter) =>
    parameter.id === 'path' ? { ...parameter, type: 'Image' } : { ...parameter },
  ),
  outputs: [{ id: 'default', label: '', type: 'Image', legacyDefault: true }],
  operations: shape.operations.map(({ id, active }) => ({ id, active: [...active] })),
  defaultOperation: shape.defaultOperation,
  catalog: { categoryId: 'image', ...catalog, creationRoute: 'add' },
});

export const orderedImageDitherDeclaration = declaration(
  'Ordered Dither Image',
  'builtin.ordered-dither-image.v1',
  orderedDitherDeclaration,
  {
    labelJa: '規則ディザ Image',
    aliases: ['規則ディザ', '画像', 'ラスター'],
    description: '規則ディザ Image',
  },
);
export const errorDiffusionImageDitherDeclaration = declaration(
  'Error Diffusion Dither Image',
  'builtin.error-diffusion-image.v1',
  errorDiffusionDeclaration,
  {
    labelJa: '誤差拡散ディザ Image',
    aliases: ['誤差拡散ディザ', '画像', 'ラスター'],
    description: '誤差拡散ディザ Image',
  },
);
