import { importColorSlots, colorImportSchemaVersion } from './color-slots.js';
import { validateCompositionProject } from './composition-project-model.js';
import { expressionReferencesForSchema } from './core.js';
import {
  CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
  CURVE_POINT_REFERENCE_SCHEMA_VERSION,
  CURVE_HANDLE_REFERENCE_SCHEMA_VERSION,
  nodesCurvePointReferenceVersion,
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

export function prepareProjectCompositionsImport(current, sourceProject, compositionIds) {
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

  for (const composition of sourceCompositions)
    for (const layer of composition.layers)
      for (const node of layer.nodes)
        for (const q of expressionParameters(node))
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

  const occupied = collectIds(current),
    compositions = sourceCompositions.map((item) =>
      remapIds(item, collectIds(item), occupied, sourceProject.schemaVersion),
    );
  const colors = importColorSlots(
    current,
    sourceProject,
    compositions.flatMap((composition) => composition.layers),
    occupied,
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
  let colorSourceVersion = sourceProject.schemaVersion;
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

export function prepareProjectCompositionImport(current, sourceProject, compositionId) {
  const result = prepareProjectCompositionsImport(current, sourceProject, [compositionId]);
  return { project: result.project, compositionId: result.compositionIds[0] };
}
