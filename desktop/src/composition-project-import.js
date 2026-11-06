import {
  importColorSlots,
  colorImportSchemaVersion,
  readCaptureLiveColors,
} from './color-slots.js';
import {
  validateCompositionProject,
  assertNoLegacyArtworkReferencePromotion,
  assertNoLegacyHandleReferencePromotion,
} from './composition-project-model.js';
import { expressionReferencesForSchema, validate } from './core.js';
import {
  CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
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
  reserveDanglingArtworkReferenceIds,
  assertNoImportedArtworkReferenceRebind,
  nodesHaveLegacyCurveHandleReference,
  expressionParameters,
} from './curve-point-reference.js';
import {
  collectAssetReferences,
  collectIds,
  mergeAssetRegistry,
  remapAssetReferences,
  remapIds,
} from './project-import-remap.js';

export function prepareProjectCompositionsImport(
  current,
  sourceProject,
  compositionIds,
  options = {},
) {
  const captureLiveColors = readCaptureLiveColors(options);
  validateCompositionProject(current);
  validateCompositionProject(sourceProject);
  const selectedIds = compositionIds ?? sourceProject.compositions.map((item) => item.id);
  if (
    !Array.isArray(selectedIds) ||
    selectedIds.length < 1 ||
    selectedIds.some((id) => typeof id !== 'string') ||
    new Set(selectedIds).size !== selectedIds.length
  )
    throw Object.assign(Error('Invalid source Composition IDs'), {
      code: 'COMPOSITION_SOURCE_MISSING',
    });
  const requested = new Set(selectedIds),
    sourceCompositions = sourceProject.compositions.filter((item) => requested.has(item.id));
  if (sourceCompositions.length !== requested.size)
    throw Object.assign(Error('Missing source Composition'), {
      code: 'COMPOSITION_SOURCE_MISSING',
    });
  const sourceLayers = sourceCompositions.flatMap((composition) => composition.layers);
  const destinationLayers = current.compositions.flatMap((composition) => composition.layers);
  const hasQualifiedColorSyntax = sourceLayers.some((layer) =>
    nodesHaveQualifiedColorReferenceSyntax(layer.nodes),
  );
  if (
    hasQualifiedColorSyntax &&
    sourceProject.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION
  )
    throw Error('Import of qualified Color references is not supported');
  if (hasQualifiedColorSyntax && current.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
    throw Error(
      'Legacy opaque qualified Color reference text cannot be promoted to schemaVersion 64',
    );
  const hasCompositionSyntax = sourceLayers.some((layer) =>
    nodesHaveCompositionPropertyReferenceSyntax(layer.nodes),
  );
  if (
    hasCompositionSyntax &&
    sourceProject.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION
  )
    throw Error('Import of Composition property references is not supported');
  if (
    hasCompositionSyntax &&
    current.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION
  )
    throw Error('Legacy opaque Composition reference text cannot be promoted to schemaVersion 63');
  const importsArtworkReferences =
    sourceProject.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION &&
    sourceLayers.some((layer) => nodesHaveArtworkReferences(layer.nodes));
  if (importsArtworkReferences) assertNoLegacyArtworkReferencePromotion(current);
  if (
    sourceProject.schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION &&
    current.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION &&
    sourceLayers.some((layer) => nodesHaveArtworkReferenceSyntax(layer.nodes))
  )
    throw Error('Legacy opaque Artwork reference text cannot be promoted to schemaVersion 62');
  if (sourceProject.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    assertNoImportedArtworkReferenceRebind(sourceLayers, destinationLayers, {
      outerComposition: true,
    });

  if (captureLiveColors) {
    // Structured cloning omits non-enumerable properties. A direct JS caller
    // must not lose a valid Color binding and silently import its fallback.
    for (const composition of sourceCompositions)
      for (const layer of composition.layers)
        for (const node of layer.nodes)
          for (const q of Object.values(node.params || {})) {
            if (q?.type !== 'Color') continue;
            const binding = Object.getOwnPropertyDescriptor(q, 'colorSlotId');
            if (binding && !binding.enumerable)
              throw Error('Unsupported non-enumerable Color binding');
          }
    // The outer project codec validates binding IDs, but not every Color
    // fallback. Validate each selected source Document before Capture can
    // replace a malformed fallback with a valid resolved literal.
    const fields = { ...sourceProject };
    delete fields.compositionSchemaVersion;
    delete fields.compositions;
    // Keep one detached copy of shared assets across projections. The core
    // validator freezes verified asset records and caches image validation by
    // identity; cloning those records per Composition repeats PNG decoding.
    const projectFields = structuredClone(fields);
    for (const source of sourceCompositions) {
      const composition = structuredClone(source);
      validate({
        ...projectFields,
        width: composition.width,
        height: composition.height,
        ...(composition.editor === undefined ? {} : { editor: composition.editor }),
        layers: composition.layers,
      });
    }
  }

  for (const composition of sourceCompositions)
    for (const layer of composition.layers)
      for (const node of layer.nodes)
        for (const q of expressionParameters(node, { includeUnsupported: true }))
          expressionReferencesForSchema(q, sourceProject.schemaVersion, { preserveOpaque: true });

  if (
    sourceProject.schemaVersion < CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
    current.schemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
    sourceCompositions.some((composition) =>
      composition.layers.some((layer) => nodesHaveLegacyCurveHandleReference(layer.nodes)),
    )
  )
    throw Error(
      'Legacy unresolved Curve handle expression cannot be imported into schemaVersion 61',
    );

  const occupied = collectIds(current);
  if (current.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    reserveDanglingArtworkReferenceIds(destinationLayers, occupied);
  // Qualified identities are Composition-scoped and must be reserved across
  // the whole selection. Legacy ref IDs keep their original layer scope.
  if (sourceProject.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
    reserveDanglingArtworkReferenceIds(sourceLayers, occupied);
  const compositions = sourceCompositions.map((item) => {
    // Local dangling refs must protect this Composition's allocation without
    // reserving their IDs in unrelated Compositions imported afterwards.
    const localOccupied = new Set(occupied);
    const copy = remapIds(item, collectIds(item), localOccupied, sourceProject.schemaVersion);
    for (const id of collectIds(copy)) occupied.add(id);
    return copy;
  });
  const colors = importColorSlots(
    current,
    sourceProject,
    compositions.flatMap((composition) => composition.layers),
    occupied,
    captureLiveColors,
  );
  const references = collectAssetReferences(compositions);
  const fonts = mergeAssetRegistry(
    references.fonts,
    sourceProject.fontAssets,
    current.fontAssets,
    'font',
  );
  const images = mergeAssetRegistry(
    references.images,
    sourceProject.imageAssets,
    current.imageAssets,
    'image',
  );
  remapAssetReferences(compositions, fonts.mapping, images.mapping);

  const project = structuredClone(current);
  let referenceVersion = 0;
  for (const composition of compositions)
    for (const layer of composition.layers)
      referenceVersion = Math.max(
        referenceVersion,
        nodesCurvePointReferenceVersion(layer.nodes, sourceProject.schemaVersion),
      );
  if (importsArtworkReferences) referenceVersion = ARTWORK_REFERENCE_SCHEMA_VERSION;
  let colorSourceVersion = Math.min(sourceProject.schemaVersion, ARTWORK_REFERENCE_SCHEMA_VERSION);
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
  const sourceForColors = { ...sourceProject, schemaVersion: colorSourceVersion };
  project.schemaVersion = Math.max(
    current.schemaVersion,
    colorImportSchemaVersion(sourceForColors, colors),
    referenceVersion,
  );
  if (project.schemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION)
    assertNoLegacyHandleReferencePromotion(current);
  if (project.schemaVersion >= CURVE_POINT_REFERENCE_SCHEMA_VERSION)
    project.compositionSchemaVersion = CURVE_POINT_COMPOSITION_SCHEMA_VERSION;
  if (colors.colorSlots !== undefined) project.colorSlots = colors.colorSlots;
  if (colors.keyColorRecipe) project.keyColorRecipe = colors.keyColorRecipe;
  project.fontAssets = fonts.assets;
  project.imageAssets = images.assets;
  project.compositions.push(...compositions);
  validateCompositionProject(project);
  return { project, compositionIds: compositions.map((item) => item.id) };
}

export function prepareProjectCompositionImport(current, sourceProject, compositionId, options) {
  const result = prepareProjectCompositionsImport(current, sourceProject, [compositionId], options);
  return { project: result.project, compositionId: result.compositionIds[0] };
}
