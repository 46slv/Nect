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

export function walk(value, fn, key = '') {
  if (Array.isArray(value)) return value.forEach((item) => walk(item, fn, key));
  if (!value || typeof value !== 'object') return;
  for (const [childKey, child] of Object.entries(value)) {
    fn(value, childKey, child);
    walk(child, fn, childKey);
  }
}

export function collectIds(value) {
  const ids = new Set();
  walk(value, (_owner, key, current) => {
    if (key === 'id' && typeof current === 'string') ids.add(current);
  });
  return ids;
}

function remapLayerReferences(source, copy, mapping) {
  if (!Array.isArray(source?.nodes)) return;
  // ref() resolves only within its own layer. A colliding Composition/layer/
  // point ID or a node in another layer must not retarget a dangling expression.
  const nodes = new Map(
    source.nodes
      .filter((node) => mapping.has(node.id))
      .map((node) => [node.id, mapping.get(node.id)]),
  );
  // Group membership, like ref(), is local to this layer. Do not rewrite
  // arbitrary string arrays or turn dangling/non-node IDs into valid members.
  for (const group of Array.isArray(copy.nodeGroups) ? copy.nodeGroups : [])
    if (Array.isArray(group?.nodeIds))
      group.nodeIds = group.nodeIds.map((id) => nodes.get(id) ?? id);
  for (const node of copy.nodes) {
    const parameters = Object.values(node.params ?? {});
    for (const point of Array.isArray(node.points) ? node.points : [])
      parameters.push(point?.position, point?.handleA, point?.handleB);
    for (const edit of Array.isArray(node.edits) ? node.edits : [])
      parameters.push(...Object.values(edit?.params ?? {}));
    for (const parameter of parameters) {
      if (
        !['Scalar', 'Vector2', 'Color'].includes(parameter?.type) ||
        typeof parameter.expression !== 'string'
      )
        continue;
      // Same restricted quoted-ID matcher as duplicateNodes in core.js.
      // Replace only the first ref argument, preserving all authored syntax.
      parameter.expression = parameter.expression.replace(
        /\bref\s*\(\s*"([a-zA-Z0-9:_-]+)"/g,
        (match, id) => (nodes.has(id) ? match.replace(`"${id}"`, `"${nodes.get(id)}"`) : match),
      );
    }
  }
}

export function remapIds(value, ids, occupied) {
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
  walk(copy, (owner, key, current) => {
    if (
      typeof current === 'string' &&
      !NON_ID_KEYS.has(key) &&
      REF_KEYS.has(key) &&
      mapping.has(current)
    )
      owner[key] = mapping.get(current);
  });
  const sourceLayers = Array.isArray(value) ? value : value?.layers;
  const copiedLayers = Array.isArray(copy) ? copy : copy?.layers;
  if (Array.isArray(sourceLayers))
    sourceLayers.forEach((layer, index) =>
      remapLayerReferences(layer, copiedLayers[index], mapping),
    );
  return copy;
}

export function collectAssetReferences(value) {
  const fonts = new Set(),
    images = new Set();
  walk(value, (_owner, key, current) => {
    if (typeof current !== 'string' || !current) return;
    if (key === 'font' && current !== 'default') fonts.add(current);
    if (key === 'asset') images.add(current);
  });
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
  walk(value, (owner, key, current) => {
    if (typeof current !== 'string') return;
    const mapping = key === 'font' ? fontIds : key === 'asset' ? imageIds : null;
    if (mapping?.has(current)) owner[key] = mapping.get(current);
  });
  return value;
}
