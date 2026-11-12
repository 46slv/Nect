export const wavePointsDeclaration = {
  typeId: 'Wave Points',
  revision: 1,
  implementationId: 'builtin.wave-points.v1',
  resultType: 'Path',
  capabilities: [],
  defaultOperation: 'vertical',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'amplitude', type: 'Scalar', default: 20 },
    { id: 'wavelength', type: 'Scalar', default: 150 },
    { id: 'phase', type: 'Scalar', default: 0 },
    { id: 'keepEndpoints', type: 'Boolean', default: false },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  operations: [
    { id: 'vertical', active: ['path', 'amplitude', 'wavelength', 'phase', 'keepEndpoints'] },
    { id: 'horizontal', active: ['path', 'amplitude', 'wavelength', 'phase', 'keepEndpoints'] },
  ],
  catalog: {
    categoryId: 'transform',
    labelJa: '波',
    aliases: ['波'],
    description: '波',
    creationRoute: 'add',
  },
};
