function ids(value) {
  if (!Array.isArray(value) && !(value instanceof Set)) return [];
  return [...value].filter((id) => typeof id === 'string');
}

function pair(value, fallback, nonNegative = false) {
  if (
    !Array.isArray(value) ||
    value.length !== 2 ||
    !value.every((item) => Number.isFinite(item) && (!nonNegative || item >= 0))
  )
    return [...fallback];
  return [...value];
}

function positive(value, fallback) {
  return Number.isFinite(value) && value > 0 ? value : fallback;
}

function viewerState(value, document, fallback) {
  const layers = new Map(document.layers.map((layer) => [layer.id, layer]));
  return {
    mode: value?.mode === 'viewers' ? 'viewers' : fallback.mode,
    slots: fallback.slots.map((base, index) => {
      const slot = value?.slots?.[index],
        binding = slot?.binding,
        layer = layers.get(binding?.layerId),
        node = layer?.nodes.find((entry) => entry.id === binding?.nodeId),
        validBinding =
          layer && node && typeof binding.port === 'string' && binding.port.length > 0,
        view = slot?.view;
      return {
        binding: validBinding ? structuredClone(binding) : null,
        view:
          Array.isArray(view) &&
          view.length === 4 &&
          view.every(Number.isFinite) &&
          view[2] > 0 &&
          view[3] > 0
            ? [...view]
            : base.view === null
              ? null
              : structuredClone(base.view),
      };
    }),
  };
}

export function sanitizeCompositionViewState(value, document, defaults) {
  const base = structuredClone(defaults);
  if (!value || !Array.isArray(document?.layers)) return base;
  const layers = document.layers,
    layerIds = new Set(layers.map((layer) => layer.id)),
    activeLayer = layerIds.has(value.activeLayer) ? value.activeLayer : base.activeLayer,
    active = layers.find((layer) => layer.id === activeLayer) || layers[0],
    nodeIds = new Set(active?.nodes.map((node) => node.id) || []),
    selection = [...new Set(ids(value.selection).filter((id) => nodeIds.has(id)))],
    selected = nodeIds.has(value.selected)
      ? value.selected
      : selection.at(-1) || null,
    artworkSelection = [...new Set(ids(value.artworkSelection).filter((id) => layerIds.has(id)))],
    artworkTarget = layerIds.has(value.artworkTarget)
      ? value.artworkTarget
      : artworkSelection.at(-1) || null,
    pointNode = active?.nodes.find((node) => node.id === value.pointSelection?.nodeId),
    pointIds = new Set(pointNode?.points?.map((point) => point.id) || []),
    validPointNode = Array.isArray(pointNode?.points),
    pointSelection = validPointNode
      ? {
          nodeId: pointNode.id,
          ids: [...new Set(ids(value.pointSelection.ids).filter((id) => pointIds.has(id)))],
        }
      : { nodeId: null, ids: [] },
    pointFocus =
      validPointNode &&
      value.pointFocus?.nodeId === pointNode.id &&
      pointIds.has(value.pointFocus.pointId)
        ? { nodeId: pointNode.id, pointId: value.pointFocus.pointId }
        : null;
  if (artworkTarget && !artworkSelection.includes(artworkTarget))
    artworkSelection.push(artworkTarget);
  const guideIds = new Set(defaults.guideIds || []);
  return {
    activeLayer,
    selected: artworkTarget ? null : selected,
    selection: artworkTarget ? [] : selection,
    artworkTarget,
    artworkSelection,
    pointSelection,
    pointFocus,
    selectedGuide: guideIds.has(value.selectedGuide) ? value.selectedGuide : null,
    zoom: positive(value.zoom, base.zoom),
    canvasFit: typeof value.canvasFit === 'boolean' ? value.canvasFit : base.canvasFit,
    pan: pair(value.pan, base.pan),
    graphZoom: positive(value.graphZoom, base.graphZoom),
    graphScroll: pair(value.graphScroll, base.graphScroll, true),
    tool: typeof value.tool === 'string' ? value.tool : base.tool,
    viewerState: viewerState(value.viewerState, document, base.viewerState),
  };
}

export function createCompositionViewStateStore() {
  const states = new Map();
  return {
    save(compositionId, state) {
      if (typeof compositionId !== 'string' || !compositionId)
        throw Error('Composition ID is required for view state');
      states.set(compositionId, structuredClone(state));
    },
    restore(compositionId, document, defaults) {
      return sanitizeCompositionViewState(states.get(compositionId), document, defaults);
    },
    prune(compositions) {
      const ids = new Set(compositions.map((composition) => composition.id));
      for (const id of states.keys()) if (!ids.has(id)) states.delete(id);
    },
    forget(compositionId) {
      states.delete(compositionId);
    },
    clear() {
      states.clear();
    },
    get size() {
      return states.size;
    },
  };
}