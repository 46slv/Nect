import { cloneDocument, deserialize, validate } from './core.js';

const REF_KEYS = new Set([
  'id', 'source', 'nodeId', 'compositionId', 'asset', 'output', 'reference',
  'parent', 'layerId', 'targetId', 'sourceId', 'path', 'contourId', 'pointId',
]);
const NON_ID_KEYS = new Set(['name', 'label', 'text', 'data', 'mime', 'kind', 'type']);
const walk = (value, fn, key = '') => {
  if (Array.isArray(value)) return value.forEach((v) => walk(v, fn, key));
  if (!value || typeof value !== 'object') return;
  for (const [k, v] of Object.entries(value)) { fn(value, k, v); walk(v, fn, k); }
};
const collectIds = (value) => {
  const ids = new Set();
  walk(value, (owner, key, current) => { if (key === 'id' && typeof current === 'string') ids.add(current); });
  return ids;
};
const remap = (value, ids, occupied) => {
  const mapping = new Map();
  for (const id of ids) {
    let next = id, i = 1;
    while (occupied.has(next)) next = `${id}-import-${i++}`;
    mapping.set(id, next); occupied.add(next);
  }
  const copy = structuredClone(value);
  walk(copy, (owner, key, current) => {
    if (typeof current === 'string' && !NON_ID_KEYS.has(key) && REF_KEYS.has(key) && mapping.has(current))
      owner[key] = mapping.get(current);
  });
  return copy;
};
export function prepareCompositionImport(current, sourceText, compositionId) {
  const source = deserialize(sourceText);
  const selected = source.layers.find((layer) => layer.id === compositionId);
  if (!selected) throw Object.assign(Error('Missing source Composition'), { code: 'COMPOSITION_SOURCE_MISSING' });
  const layers = new Map(source.layers.map((layer) => [layer.id, layer]));
  const needed = new Set();
  const visit = (id) => {
    if (needed.has(id)) return;
    const layer = layers.get(id);
    if (!layer) throw Object.assign(Error(`Missing referenced Composition: ${id}`), { code: 'COMPOSITION_REFERENCE_MISSING' });
    needed.add(id);
    for (const node of layer.nodes) { const ref = node.compositionReference?.compositionId; if (ref) visit(ref); }
  };
  visit(compositionId);
  const selectedLayers = [...needed].map((id) => layers.get(id));
  const copiedLayers = remap(selectedLayers, collectIds(selectedLayers), collectIds(current));
  const result = cloneDocument(current);
  result.layers.push(...copiedLayers);
  for (const layer of copiedLayers) for (const node of layer.nodes) {
    if (node.type === 'Image' && node.asset) {
      const asset = source.imageAssets?.[node.asset];
      if (!asset) throw Object.assign(Error(`Missing image asset: ${node.asset}`), { code: 'IMPORT_ASSET_MISSING' });
      result.imageAssets ??= {}; result.imageAssets[node.asset] ??= structuredClone(asset);
    }
  }
  validate(result);
  return result;
}
