import { displayColorLabels as disambiguateLabels } from './color-display-labels.js';

// Context is presentation only: references keep their complete layer/node IDs.
export function expressionTargetDisplay(document, activeLayerId) {
  const layerLabels = disambiguateLabels(
      document.layers.map((layer) => ({ id: layer.id, label: layer.name })),
    ),
    duplicateNodeIds = new Set(),
    contextSearchTerms = new Map();
  for (const layer of document.layers) {
    const counts = new Map(),
      names = new Map();
    for (const node of layer.nodes) {
      const name = node.name.normalize('NFC');
      names.set(node.id, name);
      counts.set(name, (counts.get(name) || 0) + 1);
    }
    for (const node of layer.nodes)
      if (counts.get(names.get(node.id)) > 1) duplicateNodeIds.add(node.id);
    contextSearchTerms.set(layer.id, [layer.id, ...layerLabels.get(layer.id).split(/\s+/)]);
  }
  const contextFor = (target) =>
    target.layerId && target.layerId !== activeLayerId
      ? layerLabels.get(target.layerId) || target.layerName
      : '';
  return {
    completion(target) {
      const contextLabel = contextFor(target);
      return contextLabel
        ? { ...target, contextLabel, contextSearchTerms: contextSearchTerms.get(target.layerId) }
        : target;
    },
    picker(target) {
      const contextLabel = contextFor(target),
        duplicateNodeName = duplicateNodeIds.has(target.nodeId);
      return {
        id: target.expression,
        label: contextLabel ? `${contextLabel} · ${target.label}` : target.label,
        pointOrdinal: target.pointOrdinal,
        detail: target.pointOrdinal
          ? duplicateNodeName
            ? target.nodeId
            : ''
          : contextLabel
            ? target.nodeId
            : `${target.nodeId} · ${target.expression}`,
        searchText: contextLabel
          ? `${target.layerId} ${target.nodeId} ${target.key}`
          : target.pointOrdinal
            ? `${target.nodeId} ${target.key}`
            : '',
      };
    },
  };
}
