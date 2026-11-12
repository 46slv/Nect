import { snapshotNodeDeclaration } from './node-foundation.js';
import { snapshotMacroDefinition } from './macro-package.js';

// Build a detached definition from exactly two selected literal Transforms.
// The caller must recheck its Project/Session envelope before registering it.
export function captureMacroDefinition(request) {
  const input = snapshotNodeDeclaration(request),
    selected = input.nodes,
    published = input.parameters;
  if (
    !Array.isArray(selected) ||
    selected.length !== 2 ||
    selected.some((item) => item.type !== 'Transform' || typeof item.id !== 'string') ||
    selected[0].id === selected[1].id
  )
    throw Error('Select two Transform nodes');
  if (!Array.isArray(published) || !published.length) throw Error('Publish at least one Scalar');

  const keys = ['path', 'anchor', 'offset', 'scale', 'rotation'];
  for (const item of selected) {
    if (item.bypassed || item.paintMode || item.locked)
      throw Error('Restore and unlock both Path Transforms');
    if (
      !item.params ||
      Object.keys(item.params).length !== keys.length ||
      keys.some((key) => !Object.hasOwn(item.params, key))
    )
      throw Error('Transform extensions are not supported by this Macro');
    for (const key of keys) {
      const descriptor = item.params[key],
        allowed =
          key === 'path'
            ? ['type', 'value', 'expression', 'locked', 'source']
            : ['type', 'value', 'expression', 'locked'];
      if (
        !descriptor ||
        Object.keys(descriptor).some((field) => !allowed.includes(field)) ||
        descriptor.type !==
          (key === 'path' ? 'Path' : ['anchor', 'offset'].includes(key) ? 'Vector2' : 'Scalar') ||
        descriptor.locked !== false ||
        descriptor.expression !== '' ||
        descriptor.sourcePort !== undefined ||
        (key === 'path' && descriptor.value !== null) ||
        (key === 'path' &&
          descriptor.source !== undefined &&
          (typeof descriptor.source !== 'string' || !descriptor.source)) ||
        (key !== 'path' && descriptor.source !== undefined)
      )
        throw Error('Macro Transform inputs must be unlocked literals');
    }
  }

  const [a, b] = selected,
    second = a.params.path.source === b.id ? a : b.params.path.source === a.id ? b : null,
    first = second === a ? b : a;
  if (!second || [first.id, second.id].includes(first.params.path.source))
    throw Error('Connect the two Transform nodes in order');

  const sourceIds = [first.id, second.id],
    internalIds = new Map(sourceIds.map((id, index) => [id, `step-${index + 1}`])),
    nodes = [first, second].map((item) => ({
      id: internalIds.get(item.id),
      type: 'Transform',
      params: Object.fromEntries(
        ['anchor', 'offset', 'scale', 'rotation'].map((key) => [key, item.params[key].value]),
      ),
    })),
    parameters = published.map((item) => {
      const node = selected.find((candidate) => candidate.id === item.nodeId);
      if (!node || !['scale', 'rotation'].includes(item.key))
        throw Error('Select a Transform Scalar to publish');
      return {
        id: item.id,
        label: item.label,
        nodeId: internalIds.get(node.id),
        key: item.key,
        type: 'Scalar',
        defaultValue: node.params[item.key].value,
      };
    }),
    definition = snapshotMacroDefinition({
      format: 'nect.macro',
      formatVersion: 1,
      id: input.id,
      revision: 1,
      name: input.name,
      input: { type: 'Path', nodeId: 'step-1', socket: 'path' },
      output: { type: 'Path', nodeId: 'step-2', port: null },
      nodes,
      edges: [
        {
          sourceNodeId: 'step-1',
          sourcePort: null,
          targetNodeId: 'step-2',
          targetSocket: 'path',
        },
      ],
      parameters,
    });
  return Object.freeze({ sourceIds: Object.freeze(sourceIds), definition });
}
