// Closed built-in metadata; slots remain Session/codec-owned.
export const joinGeometryDeclaration = {
  typeId: 'Join Geometry',
  factoryName: 'Join Paths',
  revision: 1,
  implementationId: 'builtin.join-geometry.v1',
  capabilities: [],
  resultType: 'Path',
  bypassInput: 'a',
  shape: { protocol: 'nect.join-slots', revision: 1 },
  parameters: [
    { id: 'a', type: 'Path', default: null },
    { id: 'b', type: 'Path', default: null },
  ],
  outputs: [{ id: 'default', label: '', type: 'Path', legacyDefault: true }],
  catalog: {
    categoryId: 'combine',
    labelJa: '結合',
    aliases: ['結合', '独立パス'],
    description: '独立したパスを束ねる / independent path groups',
    creationRoute: 'add',
  },
};
