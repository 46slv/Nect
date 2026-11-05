// Direct authored bindings only; equal literals, expressions and downstream
// render effects are not evidence of a shared Color reference.
export function directSharedColorUses(project, slotId) {
  if (!project.colorSlots?.some((slot) => slot.id === slotId))
    throw Error('Shared color is no longer available');
  const uses = [];
  for (const composition of project.compositions)
    for (const layer of composition.layers)
      for (const node of layer.nodes)
        for (const [key, parameter] of Object.entries(node.params || {}))
          if (parameter?.type === 'Color' && parameter.colorSlotId === slotId)
            uses.push({
              compositionId: composition.id,
              layerId: layer.id,
              nodeId: node.id,
              key,
              label: `${composition.name} / ${layer.name} / ${node.name} / ${key}`,
            });

  // Keep visually identical paths distinguishable without treating any name
  // as identity. NFC comparison also catches equivalent Unicode spellings.
  const normalized = (value) => value.normalize('NFC');
  const counts = new Map();
  for (const use of uses)
    counts.set(normalized(use.label), (counts.get(normalized(use.label)) || 0) + 1);
  const occupied = new Set(
    uses
      .filter((use) => counts.get(normalized(use.label)) === 1)
      .map((use) => normalized(use.label)),
  );
  const nextIndex = new Map();
  for (const use of uses) {
    if (counts.get(normalized(use.label)) === 1) continue;
    const key = normalized(use.label);
    let index = nextIndex.get(key) || 1,
      candidate;
    do candidate = `${use.label} · ${index++}`;
    while (occupied.has(normalized(candidate)));
    nextIndex.set(key, index);
    use.label = candidate;
    occupied.add(normalized(candidate));
  }
  return uses;
}
