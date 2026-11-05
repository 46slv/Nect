import { importColorSlots, colorImportSchemaVersion } from './color-slots.js';
import { cloneDocument, deserialize, validate } from './core.js';
import {
  CURVE_POINT_REFERENCE_SCHEMA_VERSION,
  nodesHaveCurvePointReferences,
} from './curve-point-reference.js';
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
    throw Object.assign(Error('Missing source Composition'), {
      code: 'COMPOSITION_SOURCE_MISSING',
    });
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
  const occupied = collectIds(current);
  const copiedLayers = remapIds(selectedLayers, collectIds(selectedLayers), occupied);
  const colors = importColorSlots(current, source, copiedLayers, occupied);
  const references = collectAssetReferences(copiedLayers);
  const fonts = mergeAssetRegistry(references.fonts, source.fontAssets, current.fontAssets, 'font');
  const images = mergeAssetRegistry(
    references.images,
    source.imageAssets,
    current.imageAssets,
    'image',
  );
  remapAssetReferences(copiedLayers, fonts.mapping, images.mapping);
  const result = cloneDocument(current);
  result.layers.push(...copiedLayers);
  const importsPointReference = copiedLayers.some((layer) =>
    nodesHaveCurvePointReferences(layer.nodes),
  );
  const sourceForColors =
    source.schemaVersion === CURVE_POINT_REFERENCE_SCHEMA_VERSION && !importsPointReference
      ? { ...source, schemaVersion: CURVE_POINT_REFERENCE_SCHEMA_VERSION - 1 }
      : source;
  result.schemaVersion = Math.max(
    result.schemaVersion,
    colorImportSchemaVersion(sourceForColors, colors),
    importsPointReference ? CURVE_POINT_REFERENCE_SCHEMA_VERSION : 1,
  );
  if (colors.colorSlots !== undefined) result.colorSlots = colors.colorSlots;
  if (colors.keyColorRecipe) result.keyColorRecipe = colors.keyColorRecipe;
  result.fontAssets = fonts.assets;
  result.imageAssets = images.assets;
  validate(result);
  return result;
}
