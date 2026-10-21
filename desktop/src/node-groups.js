// Graph organization only. Groups never participate in evaluation or transforms.
export const NODE_GROUP_SCHEMA_VERSION = 44;
export const nodeGroupCommands = new Set([
  'create-node-group',
  'rename-node-group',
  'add-to-node-group',
  'move-to-node-group',
  'remove-from-node-group',
  'ungroup-nodes',
]);

function groupName(value) {
  if (typeof value !== 'string' || !value.trim() || value.trim().length > 100)
    throw Error('Group name must contain 1 to 100 characters');
  return value.trim();
}

export function validateNodeGroups(layer, version, registerId) {
  if (layer.nodeGroups === undefined) return;
  if (version < NODE_GROUP_SCHEMA_VERSION) throw Error('Node groups require schemaVersion 44');
  if (!Array.isArray(layer.nodeGroups) || layer.nodeGroups.length > 2000)
    throw Error('Invalid node groups');
  const nodes = new Set(layer.nodes.map((n) => n.id)),
    assigned = new Set();
  for (const group of layer.nodeGroups) {
    if (
      !group ||
      typeof group !== 'object' ||
      Object.keys(group).some((key) => !['id', 'name', 'nodeIds'].includes(key))
    )
      throw Error('Invalid node group');
    registerId(group.id);
    if (groupName(group.name) !== group.name) throw Error('Invalid group name');
    if (!Array.isArray(group.nodeIds) || group.nodeIds.length > 2000)
      throw Error('Invalid group members');
    for (const id of group.nodeIds) {
      if (!nodes.has(id)) throw Error('Missing group member');
      if (assigned.has(id)) throw Error('A node can belong to only one group');
      assigned.add(id);
    }
  }
}

// Mutates only the Session's private candidate, after checking all selected IDs.
export function applyNodeGroupCommand(doc, layer, type, payload, freshId) {
  const groups = layer.nodeGroups || [],
    group = groups.find((g) => g.id === payload.groupId);
  if (type !== 'create-node-group' && !group) throw Error('Missing node group');
  if (type === 'rename-node-group') group.name = groupName(payload.name);
  else if (type === 'ungroup-nodes') layer.nodeGroups = groups.filter((g) => g !== group);
  else {
    const ids = payload.nodeIds;
    if (
      !Array.isArray(ids) ||
      !ids.length ||
      new Set(ids).size !== ids.length ||
      ids.some((id) => !layer.nodes.some((n) => n.id === id))
    )
      throw Error('Select distinct current nodes in this composition');
    const selected = new Set(ids),
      other = type === 'create-node-group' ? groups : groups.filter((g) => g !== group);
    if (
      ['create-node-group', 'add-to-node-group'].includes(type) &&
      other.some((g) => g.nodeIds.some((id) => selected.has(id)))
    )
      throw Error('Selected nodes already belong to another group; use Move into group');
    if (type === 'create-node-group') {
      const created = { id: freshId(), name: groupName(payload.name), nodeIds: [...ids] };
      layer.nodeGroups = [...groups, created];
      doc.schemaVersion = Math.max(doc.schemaVersion, NODE_GROUP_SCHEMA_VERSION);
      return { groupId: created.id };
    }
    if (type === 'remove-from-node-group')
      group.nodeIds = group.nodeIds.filter((id) => !selected.has(id));
    else {
      if (type === 'move-to-node-group')
        for (const previous of other)
          previous.nodeIds = previous.nodeIds.filter((id) => !selected.has(id));
      group.nodeIds = [...new Set([...group.nodeIds, ...ids])];
    }
  }
  return { groupId: group.id };
}

export function removeDeletedGroupMembers(layer, removed) {
  for (const group of layer.nodeGroups || [])
    group.nodeIds = group.nodeIds.filter((id) => !removed.has(id));
}
