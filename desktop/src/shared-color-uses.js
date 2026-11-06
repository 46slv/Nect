// Direct authored bindings only; equal literals, expressions and downstream
// render effects are not evidence of a shared Color reference.
const graphemes =
  typeof Intl === 'object' && typeof Intl.Segmenter === 'function'
    ? new Intl.Segmenter(undefined, { granularity: 'grapheme' })
    : null;
export function boundedDisplayPart(value) {
  if (value.length <= 32) return { text: value, truncated: false };
  const sample = [];
  for (const character of value) {
    sample.push(character);
    if (sample.length === 33) break;
  }
  if (sample.length <= 32) return { text: value, truncated: false };
  // Segment only the bounded prefix and the next code point. If the boundary
  // cuts a grapheme, omit that whole cluster rather than showing a false name.
  if (!graphemes) return { text: '…', truncated: true };
  const prefix = sample.slice(0, 32).join('');
  let end = prefix.length;
  for (const part of graphemes.segment(prefix + sample[32]))
    if (part.index < end && part.index + part.segment.length > end) {
      end = part.index;
      break;
    }
  return { text: prefix.slice(0, end) + '…', truncated: true };
}

export function directSharedColorUses(project, slotId) {
  if (!project.colorSlots?.some((slot) => slot.id === slotId))
    throw Error('Shared color is no longer available');
  const uses = [];
  for (const composition of project.compositions) {
    const compositionName = boundedDisplayPart(composition.name);
    for (const layer of composition.layers) {
      const layerName = boundedDisplayPart(layer.name);
      for (const node of layer.nodes) {
        const nodeName = boundedDisplayPart(node.name);
        for (const [key, parameter] of Object.entries(node.params || {}))
          if (parameter?.type === 'Color' && parameter.colorSlotId === slotId) {
            const parameterName = boundedDisplayPart(key);
            uses.push({
              compositionId: composition.id,
              layerId: layer.id,
              nodeId: node.id,
              key,
              label: [compositionName, layerName, nodeName, parameterName]
                .map((part) => part.text)
                .join(' / '),
              ...([compositionName, layerName, nodeName, parameterName].some(
                (part) => part.truncated,
              )
                ? { fullNames: [composition.name, layer.name, node.name, key] }
                : {}),
            });
          }
      }
    }
  }

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
