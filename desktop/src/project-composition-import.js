import {
  importColorSlots,
  colorImportSchemaVersion,
  readCaptureLiveColors,
} from './color-slots.js';
import { cloneDocument, deserialize, validate } from './core.js';
import {
  CURVE_POINT_REFERENCE_SCHEMA_VERSION,
  CURVE_HANDLE_REFERENCE_SCHEMA_VERSION,
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  nodesHaveQualifiedColorReferenceSyntax,
  nodesHaveCompositionPropertyReferenceSyntax,
  nodesCurvePointReferenceVersion,
  nodesHaveArtworkReferences,
  nodesHaveArtworkReferenceSyntax,
  artworkReferencesInLayers,
  reserveDanglingArtworkReferenceIds,
  assertNoImportedArtworkReferenceRebind,
} from './curve-point-reference.js';
import {
  collectAssetReferences,
  collectIds,
  mergeAssetRegistry,
  remapAssetReferences,
  remapIds,
} from './project-import-remap.js';

export function prepareCompositionImport(current, sourceText, compositionId, options = {}) {
  const captureLiveColors = readCaptureLiveColors(options);
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
    if (source.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
      for (const ref of artworkReferencesInLayers([layer]))
        if (layers.has(ref.layerId)) visit(ref.layerId);
  };
  visit(compositionId);
  const selectedLayers = [...needed].map((id) => layers.get(id));
  if (selectedLayers.some((layer) => nodesHaveQualifiedColorReferenceSyntax(layer.nodes))) {
    if (source.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
      throw Error('Import of qualified Color references is not supported');
    if (current.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
      throw Error(
        'Legacy opaque qualified Color reference text cannot be promoted to schemaVersion 64',
      );
  }
  if (selectedLayers.some((layer) => nodesHaveCompositionPropertyReferenceSyntax(layer.nodes))) {
    if (source.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION)
      throw Error('Import of Composition property references is not supported');
    if (current.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION)
      throw Error(
        'Legacy opaque Composition reference text cannot be promoted to schemaVersion 63',
      );
  }
  const importsArtworkReferences =
    source.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION &&
    selectedLayers.some((layer) => nodesHaveArtworkReferences(layer.nodes));
  if (
    importsArtworkReferences &&
    current.schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION &&
    current.layers.some((layer) => nodesHaveArtworkReferenceSyntax(layer.nodes))
  )
    throw Error('Legacy opaque Artwork reference text cannot be promoted to schemaVersion 62');
  if (source.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    assertNoImportedArtworkReferenceRebind(selectedLayers, current.layers);
  const occupied = collectIds(current);
  if (current.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    reserveDanglingArtworkReferenceIds(current.layers, occupied);
  const copiedLayers = remapIds(
    selectedLayers,
    collectIds(selectedLayers),
    occupied,
    source.schemaVersion,
  );
  const colors = importColorSlots(current, source, copiedLayers, occupied, captureLiveColors);
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
  let referenceVersion = 0;
  for (const layer of copiedLayers)
    referenceVersion = Math.max(
      referenceVersion,
      nodesCurvePointReferenceVersion(layer.nodes, source.schemaVersion),
    );
  if (importsArtworkReferences) referenceVersion = ARTWORK_REFERENCE_SCHEMA_VERSION;
  // Schema 64 adds only qualified Color references, already refused above.
  // Do not promote an older destination merely because unrelated source data is v64.
  let colorSourceVersion =
    source.schemaVersion === QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION
      ? ARTWORK_REFERENCE_SCHEMA_VERSION
      : source.schemaVersion;
  if (
    colorSourceVersion === ARTWORK_REFERENCE_SCHEMA_VERSION &&
    referenceVersion < ARTWORK_REFERENCE_SCHEMA_VERSION
  )
    colorSourceVersion--;
  if (
    colorSourceVersion === CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
    referenceVersion < CURVE_HANDLE_REFERENCE_SCHEMA_VERSION
  )
    colorSourceVersion--;
  if (
    colorSourceVersion === CURVE_POINT_REFERENCE_SCHEMA_VERSION &&
    referenceVersion < CURVE_POINT_REFERENCE_SCHEMA_VERSION
  )
    colorSourceVersion--;
  const sourceForColors = { ...source, schemaVersion: colorSourceVersion };
  result.schemaVersion = Math.max(
    result.schemaVersion,
    colorImportSchemaVersion(sourceForColors, colors),
    referenceVersion,
  );
  if (colors.colorSlots !== undefined) result.colorSlots = colors.colorSlots;
  if (colors.keyColorRecipe) result.keyColorRecipe = colors.keyColorRecipe;
  result.fontAssets = fonts.assets;
  result.imageAssets = images.assets;
  validate(result);
  return result;
}
