export const mirrorDeclaration = {
  typeId: 'Mirror',
  revision: 1,
  implementationId: 'builtin.mirror.v1',
  resultType: 'Path',
  capabilities: [],
  defaultOperation: 'left-right',
  bypassInput: 'path',
  parameters: [
    { id: 'path', type: 'Path', default: null },
    { id: 'center', type: 'Vector2', default: [480, 270] },
    { id: 'angle', type: 'Scalar', default: 0 },
    { id: 'keepOriginal', type: 'Boolean', default: false },
  ],
  outputs: [{ id: 'default', type: 'Path', label: '', legacyDefault: true }],
  operations: [
    { id: 'left-right', active: ['path', 'center', 'keepOriginal'] },
    { id: 'up-down', active: ['path', 'center', 'keepOriginal'] },
    { id: 'angle', active: ['path', 'center', 'angle', 'keepOriginal'] },
  ],
  catalog: {
    categoryId: 'transform',
    labelJa: '鏡像',
    aliases: ['鏡像', '対称'],
    description: '鏡像 対称',
    creationRoute: 'add',
  },
};
