import { cloneDocument, deserialize, validate } from './core.js';
import {
  collectAssetReferences,
  collectIds,
  mergeAssetRegistry,
  remapAssetReferences,
  remapIds,
} from './project-import-remap.js';

export function prepareCompositionImport(current, sourceText, compositionId) {
  const source = deserialize(sourceText);
  const selected = source.layers.find((layer) => layer.id === compositionId);
  if (!selected)
    throw Object.assign(Error('Missing source Composition'), { code: 'COMPOSITION_SOURCE_MISSING' });
  const layers = new Map(source.layers.map((layer) => [layer.id, layer]));
  const needed = new Set();
  const visit = (id) => {
    if (needed.has(id)) return;
    const layer = layers.get(id);
    if (!layer)
      throw Object.assign(Error('Missing referenced Composition: ' + id), {
        code: 'COMPOSITION_REFERENCE_MISSING',
      });
    needed.add(id);
    for (const node of layer.nodes) {
      const reference = node.compositionReference?.compositionId;
      if (reference) visit(reference);
    }
  };
  visit(compositionId);
  const selectedLayers = [...needed].map((id) => layers.get(id));
  const copiedLayers = remapIds(selectedLayers, collectIds(selectedLayers), collectIds(current));
  const references = collectAssetReferences(copiedLayers);
  const fonts = mergeAssetRegistry(
    references.fonts,
    source.fontAssets,
    current.fontAssets,
    'font',
  );
  const images = mergeAssetRegistry(
    references.images,
    source.imageAssets,
    current.imageAssets,
    'image',
  );
  remapAssetReferences(copiedLayers, fonts.mapping, images.mapping);
  const result = cloneDocument(current);
  result.layers.push(...copiedLayers);
  result.schemaVersion = Math.max(result.schemaVersion, source.schemaVersion);
  result.fontAssets = fonts.assets;
  result.imageAssets = images.assets;
  validate(result);
  return result;
}
