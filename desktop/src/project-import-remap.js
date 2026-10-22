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

export function remapIds(value, ids, occupied) {
  const mapping = new Map();
  for (const id of ids) {
    let next = id,
      index = 1;
    while (occupied.has(next)) next = id + '-import-' + index++;
    mapping.set(id, next);
    occupied.add(next);
  }
  const copy = structuredClone(value);
  walk(copy, (owner, key, current) => {
    if (
      typeof current === 'string' &&
      !NON_ID_KEYS.has(key) &&
      REF_KEYS.has(key) &&
      mapping.has(current)
    )
      owner[key] = mapping.get(current);
  });
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
