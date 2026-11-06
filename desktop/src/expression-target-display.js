import { displayColorLabels as disambiguateLabels } from './color-display-labels.js';
import { boundedDisplayPart } from './shared-color-uses.js';

// Context is presentation only: references keep their complete layer/node IDs.
function artworkTargetDisplay(document, activeLayerId) {
  const layerParts = new Map(
      document.layers.map((layer) => [layer.id, boundedDisplayPart(layer.name)]),
    ),
    layerLabels = disambiguateLabels(
      document.layers.map((layer) => ({
        id: layer.id,
        label: layer.name.trim() ? layerParts.get(layer.id).text : 'Untitled',
      })),
    ),
    layers = new Map(document.layers.map((layer) => [layer.id, layer])),
    nodes = new Map(),
    nodeLabels = new Map(),
    nodeParts = new Map(),
    duplicateNodeIds = new Set(),
    contextSearchTerms = new Map(),
    fullNames = new Map(),
    normalizedLayers = new Map(),
    normalizedNodes = new Map(),
    rowSearch = new WeakMap();
  for (const layer of document.layers) {
    const counts = new Map(),
      names = new Map();
    for (const node of layer.nodes) {
      const name = node.name.normalize('NFC');
      names.set(node.id, name);
      counts.set(name, (counts.get(name) || 0) + 1);
      nodes.set(node.id, node);
      nodeParts.set(node.id, boundedDisplayPart(node.name));
    }
    for (const node of layer.nodes)
      if (counts.get(names.get(node.id)) > 1) duplicateNodeIds.add(node.id);
    for (const [id, label] of disambiguateLabels(
      layer.nodes.map((node) => ({
        id: node.id,
        label: node.name.trim() ? nodeParts.get(node.id).text : 'Untitled',
      })),
    ))
      nodeLabels.set(id, label);
    contextSearchTerms.set(layer.id, [layer.id, ...layerLabels.get(layer.id).split(/\s+/)]);
  }
  const contextFor = (target) =>
    target.layerId && target.layerId !== activeLayerId
      ? layerLabels.get(target.layerId) || target.layerName
      : '';
  const labelFor = (target) => {
    const part = nodeParts.get(target.nodeId);
    if (!part?.truncated && target.name.trim()) return target.label;
    const name = nodeLabels.get(target.nodeId) || 'Untitled';
    if (target.pointOrdinal) {
      const field = target.key.slice(target.key.lastIndexOf('.') + 1),
        handle = field === 'position' ? '' : field === 'handleA' ? 'Incoming · ' : 'Outgoing · ',
        component = target.component === 'length' ? 'Length' : target.component.toUpperCase();
      return `${name} · Point ${target.pointOrdinal} · ${handle}${component}`;
    }
    return `${name} · ${target.key}${target.component ? `.${target.component}` : ''}`;
  };
  const fullNameFor = (target, contextLabel) => {
    const layerId = target.layerId || activeLayerId,
      layer = layers.get(layerId),
      node = nodes.get(target.nodeId);
    if (
      !layer ||
      !node ||
      !(
        (contextLabel && layerParts.get(layerId)?.truncated) ||
        nodeParts.get(target.nodeId)?.truncated
      )
    )
      return undefined;
    if (!fullNames.has(target.nodeId))
      fullNames.set(target.nodeId, () => `${contextLabel ? `${layer.name} · ` : ''}${node.name}`);
    return fullNames.get(target.nodeId);
  };
  const normalize = (value) => value.normalize('NFKC').toLowerCase();
  return {
    completion(target) {
      const contextLabel = contextFor(target),
        label = labelFor(target),
        fullName = fullNameFor(target, contextLabel);
      return contextLabel || label !== target.label || fullName
        ? {
            ...target,
            label,
            ...(fullName ? { fullName } : {}),
            ...(contextLabel
              ? {
                  contextLabel,
                  contextSearchTerms: contextSearchTerms.get(target.layerId),
                  contextSearchText: layers.get(target.layerId)?.name,
                }
              : {}),
          }
        : target;
    },
    picker(target) {
      const layerId = target.layerId || activeLayerId,
        contextLabel = contextFor(target),
        duplicateNodeName = duplicateNodeIds.has(target.nodeId),
        fullName = fullNameFor(target, contextLabel);
      return {
        id: target.expression,
        label: contextLabel ? `${contextLabel} · ${labelFor(target)}` : labelFor(target),
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
        layerId,
        nodeId: target.nodeId,
        searchLayer: !!contextLabel,
        ...(fullName ? { fullName } : {}),
      };
    },
    filter(items, query) {
      const words = normalize(query.trim()).split(/\s+/).filter(Boolean),
        layerMatches = new Map(),
        nodeMatches = new Map();
      if (!words.length) return items;
      const matches = (id, source, normalized, cache) => {
        if (!cache.has(id)) {
          if (!normalized.has(id)) normalized.set(id, normalize(source));
          const text = normalized.get(id);
          cache.set(
            id,
            words.map((word) => text.includes(word)),
          );
        }
        return cache.get(id);
      };
      return items.filter((item) => {
        if (!rowSearch.has(item))
          rowSearch.set(
            item,
            normalize(`${item.label} ${item.detail || ''} ${item.searchText || ''}`),
          );
        const row = rowSearch.get(item),
          layer = layers.get(item.layerId),
          node = nodes.get(item.nodeId);
        return words.every(
          (word, index) =>
            row.includes(word) ||
            (item.searchLayer &&
              layer &&
              matches(item.layerId, layer.name, normalizedLayers, layerMatches)[index]) ||
            (node && matches(item.nodeId, node.name, normalizedNodes, nodeMatches)[index]),
        );
      });
    },
  };
}

// Project context only affects presentation and search. Each expression keeps
// its complete Composition, Artwork and node identities.
export function expressionTargetDisplay(document, activeLayerId, options = {}) {
  const active = artworkTargetDisplay(document, activeLayerId),
    project = options.project,
    activeCompositionId = options.activeCompositionId;
  if (!project || !activeCompositionId) return active;
  const compositions = new Map(project.compositions.map((item) => [item.id, item])),
    parts = new Map(project.compositions.map((item) => [item.id, boundedDisplayPart(item.name)])),
    labels = disambiguateLabels(
      project.compositions.map((item) => ({
        id: item.id,
        label: item.name.trim() ? parts.get(item.id).text : 'Untitled',
      })),
    ),
    displays = new Map(),
    contexts = new Map(),
    fullNames = new Map(),
    normalized = new Map();
  const contextEntries = [];
  for (const composition of project.compositions) {
    const layers = composition.id === activeCompositionId ? document.layers : composition.layers;
    const layerLabels = disambiguateLabels(
      layers.map((layer) => ({
        id: layer.id,
        label: layer.name.trim() ? boundedDisplayPart(layer.name).text : 'Untitled',
      })),
    );
    for (const layer of layers) {
      if (composition.id === activeCompositionId && layer.id === activeLayerId) continue;
      contextEntries.push({
        id: layer.id,
        label:
          composition.id === activeCompositionId
            ? layerLabels.get(layer.id)
            : `${labels.get(composition.id)} · ${layerLabels.get(layer.id)}`,
      });
    }
  }
  const contextLabels = disambiguateLabels(contextEntries);
  function foreignState(compositionId) {
    const composition = compositions.get(compositionId);
    if (!composition || compositionId === activeCompositionId) return null;
    if (!displays.has(compositionId))
      displays.set(compositionId, {
        display: artworkTargetDisplay({ layers: composition.layers }, null),
        layers: new Map(composition.layers.map((layer) => [layer.id, layer])),
        nodes: new Map(
          composition.layers.flatMap((layer) => layer.nodes.map((node) => [node.id, node])),
        ),
      });
    return displays.get(compositionId);
  }
  function contextTerms(target, value) {
    const key = `${target.compositionId}\0${target.layerId}`;
    if (!contexts.has(key))
      contexts.set(key, [
        target.compositionId,
        ...labels.get(target.compositionId).split(/\s+/),
        ...contextLabels.get(target.layerId).split(/\s+/),
        ...(value.contextSearchTerms || []),
      ]);
    return contexts.get(key);
  }
  function fullName(target, state, value) {
    if (!parts.get(target.compositionId)?.truncated && !value.fullName) return undefined;
    if (!fullNames.has(target.nodeId)) {
      const composition = compositions.get(target.compositionId),
        layer = state.layers.get(target.layerId),
        node = state.nodes.get(target.nodeId);
      fullNames.set(target.nodeId, () => `${composition.name} · ${layer.name} · ${node.name}`);
    }
    return fullNames.get(target.nodeId);
  }
  return {
    completion(target) {
      const state = foreignState(target.compositionId);
      if (!state) return active.completion(target);
      const value = state.display.completion(target),
        name = fullName(target, state, value);
      return {
        ...value,
        contextLabel: contextLabels.get(target.layerId),
        contextSearchTerms: contextTerms(target, value),
        projectSearchText: compositions.get(target.compositionId).name,
        ...(name ? { fullName: name } : {}),
      };
    },
    picker(target) {
      const state = foreignState(target.compositionId);
      if (!state) return active.picker(target);
      const value = state.display.picker(target),
        nodeLabel = state.display.completion(target).label,
        name = fullName(target, state, value);
      return {
        ...value,
        label: `${contextLabels.get(target.layerId)} · ${nodeLabel}`,
        compositionId: target.compositionId,
        searchText: `${target.compositionId} ${value.searchText || ''}`,
        ...(name ? { fullName: name } : {}),
      };
    },
    filter(items, query) {
      const normalize = (value) => value.normalize('NFKC').toLowerCase(),
        words = normalize(query.trim()).split(/\s+/).filter(Boolean),
        groups = new Map(),
        matches = new Set();
      if (!words.length) return items;
      for (const item of items) {
        const id = item.compositionId || activeCompositionId;
        if (!groups.has(id)) groups.set(id, []);
        groups.get(id).push(item);
      }
      for (const [id, group] of groups) {
        const state = foreignState(id);
        if (!state) {
          for (const item of active.filter(group, query)) matches.add(item);
          continue;
        }
        if (!normalized.has(id)) normalized.set(id, normalize(compositions.get(id).name));
        const remaining = words.filter((word) => !normalized.get(id).includes(word));
        for (const item of state.display.filter(group, remaining.join(' '))) matches.add(item);
      }
      return items.filter((item) => matches.has(item));
    },
  };
}
