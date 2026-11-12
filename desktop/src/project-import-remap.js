import {
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
  compositionPropertyReferencesInProject,
  qualifiedColorReferencesInProject,
  componentCompositionReferencesInProject,
  curvePointReference,
  danglingExpressionIds,
  remapExpressionReferences,
  remapArtworkExpressionReferences,
  reserveDanglingArtworkReferenceIds,
} from './curve-point-reference.js';

const REF_KEYS = new Set([
  'id',
  'source',
  'nodeId',
  'compositionId',
  'output',
  'reference',
  'parent',
  'layerId',
  'targetId',
  'sourceId',
  'path',
  'contourId',
  'pointId',
]);
const NON_ID_KEYS = new Set(['name', 'label', 'text', 'data', 'mime', 'kind', 'type']);

function cloneJsonOccurrences(value) {
  if (Array.isArray(value)) return value.map(cloneJsonOccurrences);
  if (value && typeof value === 'object')
    return Object.fromEntries(
      Object.entries(value).map(([key, child]) => [key, cloneJsonOccurrences(child)]),
    );
  return value;
}

export function walk(value, fn, key = '', skip = () => false) {
  if (Array.isArray(value)) return value.forEach((item) => walk(item, fn, key, skip));
  if (!value || typeof value !== 'object') return;
  for (const [childKey, child] of Object.entries(value)) {
    if (skip(value, childKey)) continue;
    fn(value, childKey, child);
    walk(child, fn, childKey, skip);
  }
}

// Unowned metadata can contain fields that look like IDs or asset references.
// They remain literal even when an owned entity or asset has the same value.
const isOpaqueMetadata = (_owner, key) => key === 'metadata';
// A unit ID belongs to one Text node, even when the same spelling is a
// project entity ID or another Text's unit ID. Its keyed adjustments stay in
// that namespace; asset references inside it are handled separately below.
const isTextLocalIdentity = (owner, key) => owner.type === 'Text' && key === 'characterPositions';
const skipGlobalIds = (owner, key) =>
  isOpaqueMetadata(owner, key) || isTextLocalIdentity(owner, key);

// A Text-local ID or opaque field must not accidentally be the only thing
// preventing an imported node from reviving a missing same-layer reference.
function reserveDanglingIncomingReferences(layers, occupied) {
  const reserveMissing = (ids, owners) => {
    if (!Array.isArray(ids)) return;
    for (const id of ids) if (typeof id === 'string' && !owners.has(id)) occupied.add(id);
  };
  for (const layer of layers) {
    const nodeIds = new Set(layer.nodes.map((node) => node.id));
    for (const id of danglingExpressionIds(layer.nodes)) occupied.add(id);
    if (Array.isArray(layer.nodeGroups))
      for (const group of layer.nodeGroups) reserveMissing(group?.nodeIds, nodeIds);
    for (const node of layer.nodes) {
      const target = node.reference?.nodeId;
      if (node.type === 'Reference' && target && !nodeIds.has(target)) occupied.add(target);
      if (node.type !== 'Multi Path') continue;
      const points = Array.isArray(node.points) ? node.points : [];
      const contours = Array.isArray(node.contours) ? node.contours : [];
      const units = Array.isArray(node.units) ? node.units : [];
      const pointIds = new Set(points.map((point) => point?.id));
      const contourIds = new Set(contours.map((contour) => contour?.id));
      for (const contour of contours) reserveMissing(contour?.points, pointIds);
      for (const unit of units) reserveMissing(unit?.contours, contourIds);
    }
  }
}

// The three reference arrays have distinct owners. Only rewrite members that
// belong to that exact source Layer or Multi Path; unrelated arrays and missing
// references stay literal.
function remapIncomingReferenceArrays(sourceLayers, copiedLayers) {
  sourceLayers.forEach((sourceLayer, layerIndex) => {
    const copy = copiedLayers[layerIndex];
    const nodes = new Map(sourceLayer.nodes.map((node, index) => [node.id, copy.nodes[index].id]));
    if (Array.isArray(sourceLayer.nodeGroups))
      sourceLayer.nodeGroups.forEach((group, index) => {
        if (Array.isArray(group?.nodeIds))
          copy.nodeGroups[index].nodeIds = group.nodeIds.map((id) => nodes.get(id) ?? id);
      });
    sourceLayer.nodes.forEach((node, nodeIndex) => {
      if (node.type !== 'Multi Path') return;
      const copiedNode = copy.nodes[nodeIndex];
      const sourcePoints = Array.isArray(node.points) ? node.points : [];
      const sourceContours = Array.isArray(node.contours) ? node.contours : [];
      const sourceUnits = Array.isArray(node.units) ? node.units : [];
      const points = new Map(
        sourcePoints.map((point, index) => [point?.id, copiedNode.points[index]?.id]),
      );
      const contours = new Map(
        sourceContours.map((contour, index) => [contour?.id, copiedNode.contours[index]?.id]),
      );
      sourceContours.forEach((contour, index) => {
        if (Array.isArray(contour?.points))
          copiedNode.contours[index].points = contour.points.map((id) => points.get(id) ?? id);
      });
      sourceUnits.forEach((unit, index) => {
        if (Array.isArray(unit?.contours))
          copiedNode.units[index].contours = unit.contours.map((id) => contours.get(id) ?? id);
      });
    });
  });
}

export function reserveProjectImportReferences(project, occupied) {
  // A Project import adds whole outer Compositions. Bare/local references in
  // existing Compositions cannot bind to their nodes; only qualified project
  // references may cross that boundary.
  const references = [];
  if (project.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION)
    references.push(...compositionPropertyReferencesInProject(project));
  if (project.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
    references.push(...qualifiedColorReferencesInProject(project));
  if (project.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION)
    references.push(...componentCompositionReferencesInProject(project));
  for (const ref of references) {
    if (ref.compositionId) occupied.add(ref.compositionId);
    if (ref.layerId) occupied.add(ref.layerId);
    if (ref.nodeId) occupied.add(ref.nodeId);
    const point = curvePointReference(ref.key, ref.component);
    if (point) occupied.add(point.id);
  }
}

export function collectIds(value) {
  const ids = new Set();
  walk(
    value,
    (_owner, key, current) => {
      if (key === 'id' && typeof current === 'string') ids.add(current);
    },
    '',
    skipGlobalIds,
  );
  return ids;
}

export function remapIds(value, ids, occupied, sourceSchemaVersion) {
  const sourceLayers = Array.isArray(value) ? value : value?.layers;
  if (Array.isArray(sourceLayers)) reserveDanglingIncomingReferences(sourceLayers, occupied);
  if (Array.isArray(sourceLayers) && sourceSchemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    reserveDanglingArtworkReferenceIds(sourceLayers, occupied);
  const mapping = new Map();
  for (const id of ids) {
    let next = id,
      index = 1;
    while (occupied.has(next)) next = id + '-import-' + index++;
    mapping.set(id, next);
    occupied.add(next);
  }
  // Callers already validate a JSON tree. Copy each occurrence independently:
  // shared JS descriptors must not retarget metadata or another layer by alias.
  const copy = cloneJsonOccurrences(value);
  walk(
    copy,
    (owner, key, current) => {
      if (
        typeof current === 'string' &&
        !NON_ID_KEYS.has(key) &&
        REF_KEYS.has(key) &&
        mapping.has(current)
      )
        owner[key] = mapping.get(current);
    },
    '',
    skipGlobalIds,
  );
  const copiedLayers = Array.isArray(copy) ? copy : copy?.layers;
  if (Array.isArray(sourceLayers)) remapIncomingReferenceArrays(sourceLayers, copiedLayers);
  if (Array.isArray(sourceLayers))
    sourceLayers.forEach((layer, index) =>
      remapExpressionReferences(
        layer.nodes || [],
        copiedLayers[index].nodes || [],
        sourceSchemaVersion,
      ),
    );
  if (Array.isArray(sourceLayers) && sourceSchemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    remapArtworkExpressionReferences(sourceLayers, copiedLayers);
  return copy;
}

export function collectAssetReferences(value) {
  const fonts = new Set(),
    images = new Set();
  walk(
    value,
    (_owner, key, current) => {
      if (typeof current !== 'string' || !current) return;
      if (key === 'font' && current !== 'default') fonts.add(current);
      if (key === 'asset') images.add(current);
    },
    '',
    isOpaqueMetadata,
  );
  return { fonts, images };
}

export function mergeAssetRegistry(references, sourceAssets, destinationAssets, label) {
  const assets = structuredClone(destinationAssets || {}),
    mapping = new Map(),
    occupied = new Set(Object.keys(assets));
  for (const id of references) {
    const sourceAsset = sourceAssets?.[id];
    if (!sourceAsset || typeof sourceAsset !== 'object')
      throw Object.assign(Error('Missing ' + label + ' asset: ' + id), {
        code: 'IMPORT_ASSET_MISSING',
      });
    let target = id;
    if (
      Object.hasOwn(assets, target) &&
      JSON.stringify(assets[target]) !== JSON.stringify(sourceAsset)
    ) {
      let index = 1;
      while (occupied.has(target)) target = id + '-import-' + index++;
    }
    if (!Object.hasOwn(assets, target))
      Object.defineProperty(assets, target, {
        value: structuredClone(sourceAsset),
        enumerable: true,
        writable: true,
        configurable: true,
      });
    occupied.add(target);
    mapping.set(id, target);
  }
  return { assets, mapping };
}

export function remapAssetReferences(value, fontIds, imageIds) {
  walk(
    value,
    (owner, key, current) => {
      if (typeof current !== 'string') return;
      const mapping = key === 'font' ? fontIds : key === 'asset' ? imageIds : null;
      if (mapping?.has(current)) owner[key] = mapping.get(current);
    },
    '',
    isOpaqueMetadata,
  );
  return value;
}
