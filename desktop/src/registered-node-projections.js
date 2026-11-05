// Merge trusted projections at host-owned anchors, without per-type consumer branches.
export function placeRegistered(legacy, rows, location, project) {
  const output = [...legacy],
    ids = new Set(output.map(([id]) => id));
  if (ids.size !== output.length) throw Error('Duplicate legacy type');
  const pending = [...rows];
  while (pending.length) {
    let changed = false;
    for (let i = 0; i < pending.length; ) {
      const row = pending[i],
        id = row.resolved.declaration.typeId,
        anchor = row.placement[location];
      if (ids.has(id)) throw Error(`Duplicate projected type: ${id}`);
      if (anchor === null) {
        output.push([id, project(row)]);
      } else {
        if (typeof anchor !== 'string' || !anchor) throw Error('Invalid placement');
        const at = output.findIndex(([key]) => key === anchor);
        if (at < 0) {
          i++;
          continue;
        }
        output.splice(at + 1, 0, [id, project(row)]);
      }
      ids.add(id);
      pending.splice(i, 1);
      changed = true;
    }
    if (!changed) throw Error('Unresolved or cyclic placement');
  }
  return output;
}

// Internal startup validation: reuse of the same implementation functions is valid.
export function manifestBindings(rows) {
  const implementations = new Map();
  for (const row of rows) {
    if (!['public', 'private'].includes(row.exposure)) throw Error('Invalid built-in exposure');
    const id = row.declaration.implementationId;
    const existing = implementations.get(id);
    if (
      existing &&
      (existing.evaluate !== row.binding.evaluate || existing.project !== row.binding.project)
    )
      throw Error(`Conflicting built-in implementation: ${id}`);
    implementations.set(id, row.binding);
  }
  return implementations;
}
