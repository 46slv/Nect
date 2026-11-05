import { importColorSlots, colorImportSchemaVersion } from './color-slots.js';
import { validateCompositionProject } from './composition-project-model.js';
import {
  CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
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

  const occupied = collectIds(current),
    compositions = sourceCompositions.map((item) => remapIds(item, collectIds(item), occupied));
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
  const importsPointReference = compositions.some((composition) =>
    composition.layers.some((layer) => nodesHaveCurvePointReferences(layer.nodes)),
  );
  const sourceForColors =
    sourceProject.schemaVersion === CURVE_POINT_REFERENCE_SCHEMA_VERSION && !importsPointReference
      ? { ...sourceProject, schemaVersion: CURVE_POINT_REFERENCE_SCHEMA_VERSION - 1 }
      : sourceProject;
  project.schemaVersion = Math.max(
    current.schemaVersion,
    colorImportSchemaVersion(sourceForColors, colors),
    importsPointReference ? CURVE_POINT_REFERENCE_SCHEMA_VERSION : 1,
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
