import { expression, qualifiedReferencesForCodec } from './expression-parser.js';
import {
  validateCompositionProject,
  assertNoLegacyCompositionPropertyReferencePromotion,
  assertNoLegacyCompositionPropertyDocumentPromotion,
  assertNoLegacyQualifiedColorReferencePromotion,
  assertNoLegacyQualifiedColorDocumentPromotion,
  assertNoLegacyComponentCompositionReferencePromotion,
  assertNoLegacyComponentCompositionDocumentPromotion,
  assertNoLegacyArtworkReferencePromotion,
  assertNoLegacyArtworkDocumentPromotion,
  assertNoLegacyHandleReferencePromotion,
  assertNoLegacyHandleDocumentPromotion,
} from './composition-project-model.js';
export { expression } from './expression-parser.js';
import { JOIN_SLOT_LIMIT, validJoinSlotOrder } from './join-slots-protocol.js';
import {
  CURVE_POINT_REFERENCE_SCHEMA_VERSION,
  CURVE_HANDLE_REFERENCE_SCHEMA_VERSION,
  ARTWORK_REFERENCE_SCHEMA_VERSION,
  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
  hasComponentCompositionReferenceSyntax,
  nodesHaveComponentCompositionReferenceSyntax,
  componentCompositionReferences,
  componentCompositionReferencesInProject,
  clipboardReferenceSchemaVersion,
  hasQualifiedColorReferenceSyntax,
  nodesHaveQualifiedColorReferenceSyntax,
  qualifiedColorReferences,
  qualifiedColorReferencesInProject,
  allExpressionParameters,
  CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
  compositionPropertyReferences,
  compositionPropertyReferencesInProject,
  hasCompositionPropertyReferenceSyntax,
  nodesHaveCompositionPropertyReferenceSyntax,
  artworkExpressionReferences,
  artworkReferencesInLayers,
  hasArtworkReferenceSyntax,
  nodesHaveArtworkReferences,
  nodesHaveArtworkReferenceSyntax,
  nodesHaveLegacyCurveHandleReference,
  assertNoAccidentalCurvePointRebind,
  assertNoExternalPointReferenceRebind,
  curvePointReference,
  curvePointReferenceOwners,
  danglingExpressionIds,
  remapExpressionReferences,
} from './curve-point-reference.js';
export { JOIN_SLOT_LIMIT } from './join-slots-protocol.js';
import { contourGroups } from './path-groups.js';
import { signature } from './path-signature.js';
export { signature } from './path-signature.js';
import { builtinNodeRegistry, registeredNodes } from './builtin-node-registry.js';
import { placeRegistered } from './registered-node-projections.js';
import { isColor, isVector2 } from './node-value-validation.js';
import { nectOutputPort } from './nect-node-host.js';
import {
  KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION,
  LIVE_KEY_COLOR_RECIPE_SCHEMA_VERSION,
  isLiveKeyColorRecipe,
} from './key-color-recipe-document.js';
import {
  COLOR_SLOTS_SCHEMA_VERSION,
  COLOR_DESIGN_SCHEMA_VERSION,
  HARMONY_DESIGN_SCHEMA_VERSION,
  CHROMA_OVERRIDE_SCHEMA_VERSION,
  colorSlotCommands,
  validateColorSlots,
  validateColorSlotBinding,
  resolveColorSlot,
  applyColorSlotCommand,
} from './color-slots.js';
import {
  insertSVGAsset,
  validateSVGAsset,
  validateAssetProvenance,
  SVG_ASSET_SCHEMA_VERSION,
} from './svg-asset.js';
import { resolvePenEndpoint, penContourIds, insertContinuationPoint } from './pen-continuation.js';
import {
  NODE_GROUP_SCHEMA_VERSION,
  nodeGroupCommands,
  validateNodeGroups,
  applyNodeGroupCommand,
  removeDeletedGroupMembers,
} from './node-groups.js';
import { appendSVGPart } from './svg-part.js';
import { svgRootAttributes } from './artboard-units.js';
import { formatSvgExtent, formatSvgNumber, validateSvgPrecision } from './svg-number-format.js';
import { OPENTYPE_SCHEMA_VERSION, validateTextFeatures } from './opentype-features.js';
import {
  imageAssetFor,
  keyedImage,
  ditheredImage,
  keyerSettings,
  MAX_ACTIVE_KEYERS,
} from './image-keyer.js';
import { MAX_ACTIVE_IMAGE_DITHERS } from './image-dither.js';
import {
  maskAppearance,
  maskFeather,
  featherMaskSVG,
  imageAlphaMaskSVG,
  MASK_FEATHER_SCHEMA_VERSION,
  IMAGE_MASK_SCHEMA_VERSION,
  MASK_LUMINANCE_SCHEMA_VERSION,
} from './appearance-mask.js';
import { layerIsVisible } from './layer-visibility.js';
import {
  validateCompositionReference,
  assertCompositionAcyclic,
  isPaintedGroup,
  paintedGroup,
  groupGeometry,
} from './composition-reference.js';
export { compositionReferenceTargets } from './composition-reference.js';
import { splitPathSegment } from './path-point-insertion.js';
import { osOutlineGeometry } from './os-font-outline.js';
import { validateOsFontText, osFontTextPath } from './os-font-text.js';
import { desktopTextIntent, desktopOsReference, desktopFontTextPath } from './desktop-font-text.js';
import { PRIVATE_FONT_SCHEMA_VERSION } from './private-font-source.js';
import {
  createVariableDocumentPreparer as createVariableBridge,
  assertVariableDocumentReady,
  validateVariableText,
  VARIABLE_KIND,
} from './variable-font/document-bridge.js';
import { canonicalAxes, requireThat as variableCheck } from './variable-font/contract.js';
import { validateCharacterArrangement } from './character-arrangement.js';
import {
  colorExpression,
  usesColorOperations,
  COLOR_OPERATIONS_SCHEMA_VERSION,
  COLOR_OPERATIONS_VERSION,
} from './color-operations.js';
import { validateTonePower } from './luminance-tone.js';
import {
  DITHER_SCHEMA_VERSION,
  IMAGE_DITHER_SCHEMA_VERSION,
  imageDitherTypes,
  isImageDither,
  isDither,
  hasDotControls,
  readDotSettings,
  dotFingerprint,
  migrateDither,
} from './dither-controls.js';
import {
  luminanceInstanceSettings,
  luminanceInstancesGeometry,
  selectLuminanceCenters,
} from './luminance-instances.js';
import { validateInstancePath } from './instance-points-geometry.js';
import { polarSettings } from './polar-transform.js';
import { dashIntervals } from './dash-intervals.js';
import {
  repeatGeometry,
  repeaterSettings,
  assertRepeaterControls,
  REPEATER_KEYS,
  REPEATER_SCHEMA_VERSION,
  REPEATER_ALTERNATE_SCHEMA_VERSION,
  REPEATER_FILL_SCHEMA_VERSION,
  REPEATER_FILL_DEFAULT,
  hasRepeaterFillAuthorship,
  REPEATER_STROKE_SCHEMA_VERSION,
  REPEATER_STROKE_DEFAULT,
  hasRepeaterStrokeAuthorship,
  assertRepeaterRoute,
} from './repeater.js';
import { setClosureGeometry } from './set-closure-geometry.js';
import { instancePointsGeometry } from './instance-points-geometry.js';
import { sampleLuminanceGrid } from './luminance-field.js';
import { sampleImageLuminanceGrid } from './image-luminance-field.js';
import { generateLuminanceMarks } from './luminance-marks.js';
import { luminanceToPoints } from './luminance-points.js';
import { prepareLuminanceMask, clipLuminanceMarks } from './luminance-mask.js';
export { hitPathSegment } from './path-point-insertion.js';
export { pointControlTargets, hitPointControl } from './point-controls.js';
export { scrubNumericValues } from './numeric-scrub.js';
export { textEditGeometry, nearestTextCaret } from './canvas-text-edit.js';
export { resolveNodeOutputTargets } from './node-output-alignment.js';
export { constrainAxisDrag } from './axis-drag.js';
export { objectResizeHandles, resizedObjectValue } from './object-resize.js';
import { validateImageAssets, imageURL, decodeImage, pngSize } from './images.js';
import {
  appendArtwork,
  createPenArtworkDraft,
  appendPenArtwork,
  appendBlankComposition,
  duplicateArtworkLayer,
  duplicateArtworkLayers,
  duplicateLayerGeneric,
} from './artwork-creation.js';
import { removeArtworkLayer, removeArtworkLayers } from './artwork-deletion.js';
import { moveArtworkLayer, moveArtworkLayerBefore } from './artwork-ordering.js';
import { resolveArtworkMoveSelection, translateArtworkTargets } from './artwork-multi-move.js';
import { alignArtworkSelection } from './artwork-alignment.js';
import { planNodeOutputAlignment } from './node-output-alignment.js';
import { planPointAlignment } from './point-alignment.js';
import { planPointDistribution, planPointSpacing } from './point-distribution.js';
import { distributeArtworkSelection } from './artwork-distribution.js';
import { distributeArtworkGaps } from './artwork-gap-distribution.js';
import { importSVGTree } from './svg-import.js';
import { prepareCompositionImport } from './project-composition-import.js';
import {
  validateLayerFolders,
  createLayerFolder,
  renameLayerFolder,
  addLayersToFolder,
  moveLayersToFolder,
  removeLayersFromFolder,
  dissolveLayerFolder,
} from './layer-folders.js';
import {
  GRID_ORIGIN_SCHEMA_VERSION,
  GRID_ANGLE_SCHEMA_VERSION,
  validateGuides,
  guideDefaults,
  setLayoutGrid,
} from './guides.js';
import { createLinearGradientFill, renderLinearGradientFill } from './linear-gradient.js';
import { createRadialGradientFill, renderRadialGradientFill } from './radial-gradient.js';
import {
  ANGULAR_KEYS,
  createAngularGradientFill,
  renderAngularGradientFill,
} from './angular-gradient.js';
export {
  guideDefaults,
  guideSettings,
  gridAnchors,
  resolveGridOrigin,
  resolveGridBasis,
  gridLineLayout,
  snapTranslation,
  collectSnapLayerBounds,
  layoutGridDefaults,
  layoutGridLines,
  setLayoutGrid,
} from './guides.js';
// The full build initializes PaperScript with new Function, which native CSP blocks.
// Geometry needs only the core build; keep script-src free of unsafe-eval.
import paper from 'paper/dist/paper-core.js';
import {
  validateFontAssets,
  resolveFonts,
  makeFontAsset,
  MAX_FONT_BYTES,
  MAX_PROJECT_BYTES,
  utf8ByteLength,
} from './fonts.js';
import { layoutText, validateLeading } from './text-layout.js';
import { validateTextWrap, withWrapExtent } from './text-wrap.js';
import { validateTextPathAttachment, validateTextPathMode, projectTextPath } from './text-path.js';
import { offsetClosedPath, OFFSET_EDITOR_LIMITS, OffsetError } from './closed-offset.js';
import { offsetGroupedClosedPathDetailed } from './grouped-closed-offset.js';
export { OFFSET_EDITOR_LIMITS } from './closed-offset.js';
import { fingerprint } from './fingerprint.js';
import {
  createCharacterPositions,
  validateCharacterPositions,
  reconcileCharacterEdit,
  normalizeCharacterText,
  setCharacterStyles,
  setCharacterOffsets,
  setCharacterTransforms,
  resetCharacterOffsets,
  resetCharacterTransforms,
  resetCharacterAdjustments,
  setCharacterKerningPairs,
  characterPairFromIds,
  characterIdsInRange,
} from './character-identity.js';
import { TextDraft } from './text-draft.js';
import {
  isTransform,
  transformExtensions,
  transformParameter,
  transformValues,
  transformVector,
  repositionTransformAnchor,
  anchorFromOutputDelta,
} from './transform-affine.js';
export {
  isTransform,
  transformExtensions,
  transformParameter,
  anchorFromOutputDelta,
} from './transform-affine.js';
import { identityMatrix, characterTransform, routeCharacters } from './character-geometry.js';
export * from './character-identity.js';
export * from './character-geometry.js';
export * from './transform-handles.js';
const scope = new paper.PaperScope();
scope.setup(new scope.Size(1200, 800));
export const clone = (x) => structuredClone(x);
// Font assets are immutable and shared across snapshots; copying large base64 strings
// for every keystroke would turn a 20 MB font into gigabytes of undo history.
// An explicit, snapshot-scoped evaluation binding; never part of native JSON.
const projectDocumentContexts = new WeakMap();
// An externally rebound live or retained History root invalidates copied Color
// references even if an older binding is later restored. The record owns no
// Session or strong references to its History roots.
const projectContextOwners = new WeakMap();
function markExternalProjectContextWrite(document) {
  const record = projectContextOwners.get(document);
  if (record?.members.has(document)) record.epoch++;
}
function prepareProjectDocumentContext(document, project, compositionId) {
  if (!project?.compositions?.some((item) => item.id === compositionId))
    throw Error('Unknown active Composition ID');
  // Project History replaces root fields in place. Capture its immutable child
  // snapshots behind a fresh root so older display/gesture views stay stable.
  return {
    project: { ...project },
    activeCompositionId: compositionId,
    activeDocument: document,
    normalizedCompositions: new Map(),
    clipboardIdentity: Object.freeze({}),
  };
}
export function bindProjectDocumentContext(document, project, compositionId) {
  const binding = projectDocumentContexts.get(document),
    owner = projectContextOwners.get(document),
    epoch = owner?.epoch,
    members = owner?.members,
    next = prepareProjectDocumentContext(document, project, compositionId);
  if (
    projectDocumentContexts.get(document) !== binding ||
    projectContextOwners.get(document) !== owner ||
    (owner && (owner.epoch !== epoch || owner.members !== members))
  )
    throw Error('STALE_REVISION: project context changed');
  markExternalProjectContextWrite(document);
  projectDocumentContexts.set(document, next);
  return document;
}
export function inheritProjectDocumentContext(source, target) {
  const binding = projectDocumentContexts.get(source);
  if (binding) {
    markExternalProjectContextWrite(target);
    projectDocumentContexts.set(
      target,
      binding.activeDocument === source ? { ...binding, activeDocument: target } : binding,
    );
  }
  return target;
}

// A Canvas-only move must not change another branch or Composition through a
// reference to the route. Inspect the current active candidate plus the bound
// project's other Compositions without exposing its private snapshot.
export function hasExternalArtworkMoveConsumer(doc, layerId, nodeIds, allowedEdges) {
  const protectedIds = new Set(nodeIds);
  const binding = projectDocumentContexts.get(doc);
  const groups = binding
    ? binding.project.compositions.map((composition) => ({
        id: composition.id,
        layers: composition.id === binding.activeCompositionId ? doc.layers : composition.layers,
      }))
    : [{ id: null, layers: doc.layers }];
  function* authoredInputs(n) {
    yield* Object.entries(n.params || {});
    for (const point of n.points || [])
      for (const key of ['position', 'handleA', 'handleB'])
        yield [`${point.id}.${key}`, point[key]];
    for (const edit of n.edits || [])
      for (const [key, q] of Object.entries(edit.params || {}))
        yield [`remap.${edit.pointId}.${key}`, q];
  }
  try {
    for (const group of groups)
      for (const layer of group.layers)
        for (const n of layer.nodes) {
          if (
            layer.id !== layerId &&
            ['Composition Reference', 'SVG'].includes(n.type) &&
            n.compositionReference?.compositionId === layerId
          )
            return true;
          if (n.type === 'Reference' && protectedIds.has(n.reference?.nodeId)) return true;
          if (n.type === 'Text' && protectedIds.has(n.pathAttachment?.nodeId)) return true;
          for (const [key, q] of authoredInputs(n)) {
            if (
              !q ||
              !['Scalar', 'Vector2', 'Color', 'Path', 'PaintedPath', 'Image', 'Boolean'].includes(
                q.type,
              )
            )
              continue;
            if (protectedIds.has(q.source)) {
              if (
                layer.id !== layerId ||
                !allowedEdges.some(
                  (edge) => edge.source === q.source && edge.nodeId === n.id && edge.key === key,
                )
              )
                return true;
            }
            if (!q.expression) continue;
            for (const ref of expressionReferencesForSchema(q, doc.schemaVersion, {
              preserveOpaque: true,
            })) {
              if (!protectedIds.has(ref.nodeId)) continue;
              if (ref.compositionId && binding && ref.compositionId !== binding.activeCompositionId)
                continue;
              if (ref.layerId ? ref.layerId === layerId : layer.id === layerId) return true;
            }
          }
        }
  } catch {
    // A failed inspection cannot establish that the route is exclusive.
    return true;
  }
  return false;
}
// A v65 edit crosses all earlier syntax boundaries too. A standalone bound
// Session and an isolated preview must check the same complete project as the
// Composition owner before evaluating a promoted candidate.
function assertSafeComponentCompositionPromotion(document) {
  assertNoLegacyComponentCompositionDocumentPromotion(document);
  assertNoLegacyQualifiedColorDocumentPromotion(document);
  assertNoLegacyCompositionPropertyDocumentPromotion(document);
  assertNoLegacyArtworkDocumentPromotion(document);
  assertNoLegacyHandleDocumentPromotion(document);
  const binding = projectDocumentContexts.get(document);
  if (!binding) return;
  assertNoLegacyComponentCompositionReferencePromotion(binding.project);
  assertNoLegacyQualifiedColorReferencePromotion(binding.project);
  assertNoLegacyCompositionPropertyReferencePromotion(binding.project);
  assertNoLegacyArtworkReferencePromotion(binding.project);
  assertNoLegacyHandleReferencePromotion(binding.project);
}
// Normalize in the source project's original schema, just as an active
// CommandSession does. Cache one detached projection per bound snapshot so
// migrations that allocate IDs are stable across evaluation and commit.
function normalizedProjectSource(binding, compositionId) {
  if (binding.normalizedCompositions.has(compositionId))
    return binding.normalizedCompositions.get(compositionId);
  const source = binding.project.compositions.find((item) => item.id === compositionId);
  if (!source) return null;
  const { compositionSchemaVersion, compositions, ...shared } = binding.project;
  const document = cloneDocument({
    ...shared,
    width: source.width,
    height: source.height,
    ...(source.editor === undefined ? {} : { editor: source.editor }),
    layers: source.layers,
  });
  validate(document);
  binding.normalizedCompositions.set(compositionId, document);
  return document;
}

// Owner promotion uses exactly the same migrated IDs as the read that preceded
// candidate admission. The clone prevents validation from mutating the cache.
export function normalizedProjectSourceForDocument(document, compositionId) {
  const binding = projectDocumentContexts.get(document);
  if (!binding) throw Error('Composition property reference requires project context');
  const source = normalizedProjectSource(binding, compositionId);
  return source && cloneDocument(source);
}

function projectFrameDocument(binding, compositionId, context) {
  if (!context.projectDocuments) context.projectDocuments = new Map();
  if (compositionId === binding.activeCompositionId) return binding.activeDocument;
  if (context.projectDocuments.has(compositionId))
    return context.projectDocuments.get(compositionId);
  const source = normalizedProjectSource(binding, compositionId);
  if (!source) return null;
  const { width, height, editor, layers, ...shared } = binding.activeDocument;
  const doc = {
    ...shared,
    width: source.width,
    height: source.height,
    ...(source.editor === undefined ? {} : { editor: source.editor }),
    layers: source.layers,
  };
  projectDocumentContexts.set(doc, binding);
  context.projectDocuments.set(compositionId, doc);
  return doc;
}
export function cloneDocument(doc) {
  const { fontAssets, imageAssets, ...model } = doc;
  const copy = clone(model);
  if (fontAssets) {
    for (const asset of Object.values(fontAssets)) freezeDocumentView(asset);
    copy.fontAssets = { ...fontAssets };
  }
  if (imageAssets) {
    for (const asset of Object.values(imageAssets)) freezeDocumentView(asset);
    copy.imageAssets = { ...imageAssets };
  }
  return inheritProjectDocumentContext(doc, copy);
}
export function sameDocument(a, b) {
  const { fontAssets: af = {}, imageAssets: ai = {}, ...am } = a;
  const { fontAssets: bf = {}, imageAssets: bi = {}, ...bm } = b;
  const sameAssets = (left, right) =>
    Object.keys(left).length === Object.keys(right).length &&
    Object.entries(left).every(([id, asset]) => {
      const other = right[id];
      return (
        other &&
        (asset === other ||
          (Object.keys(asset).length === Object.keys(other).length &&
            Object.entries(asset).every(([key, value]) => other[key] === value)))
      );
    });
  return sameAssets(af, bf) && sameAssets(ai, bi) && JSON.stringify(am) === JSON.stringify(bm);
}
export const uid = (prefix = 'id') => `${prefix}-${globalThis.crypto.randomUUID()}`;
const p = (type, value) => ({ type, value, expression: '', locked: false });
const legacyDefinitions = {
  Image: { position: p('Vector2', [100, 100]), size: p('Vector2', [320, 240]) },
  'Image Output': { path: p('Image', null) },
  Keyer: {
    path: p('Image', null),
    keyColor: p('Color', { hex: '#00ff00', alpha: 1 }),
    tolerance: p('Scalar', 0.1),
    softness: p('Scalar', 0.05),
  },
  Mask: { path: p('PaintedPath', null), mask: p('Path', null), invert: p('Boolean', false) },
  Circle: { center: p('Vector2', [350, 250]), radius: p('Scalar', 120) },
  Ellipse: { center: p('Vector2', [350, 250]), size: p('Vector2', [240, 160]) },
  Rectangle: { center: p('Vector2', [460, 250]), size: p('Vector2', [180, 220]) },
  Polygon: {
    center: p('Vector2', [350, 250]),
    sides: p('Scalar', 6),
    radius: p('Scalar', 120),
    rotation: p('Scalar', 0),
  },
  Star: {
    center: p('Vector2', [350, 250]),
    points: p('Scalar', 5),
    outerRadius: p('Scalar', 120),
    innerRadius: p('Scalar', 60),
    rotation: p('Scalar', 0),
  },
  Curve: { closed: p('Boolean', true) },
  'Multi Path': {},
  Boolean: { a: p('Path', null), b: p('Path', null) },
  'Convert to Path': { path: p('Path', null) },
  'Closed Path Offset': {
    path: p('Path', null),
    amount: p('Scalar', 10),
    miterLimit: p('Scalar', 4),
  },
  'Path Remap': { path: p('Path', null), offset: p('Vector2', [0, 0]) },
  Transform: {
    path: p('Path', null),
    anchor: p('Vector2', [0, 0]),
    offset: p('Vector2', [0, 0]),
    scale: p('Scalar', 1),
    rotation: p('Scalar', 0),
  },
  'Vector Transform': {
    vector: p('Vector2', [0, 0]),
    anchor: p('Vector2', [0, 0]),
    offset: p('Vector2', [0, 0]),
    scale: p('Scalar', 1),
    rotation: p('Scalar', 0),
  },
  'Dash Path': {
    path: p('Path', null),
    dashLength: p('Scalar', 12),
    gapLength: p('Scalar', 6),
    offset: p('Scalar', 0),
  },
  'Trim Path': {
    path: p('Path', null),
    start: p('Scalar', 0),
    end: p('Scalar', 1),
  },
  'Set Path Start': {
    path: p('Path', null),
    start: p('Scalar', 0),
  },
  'Set Closure': { path: p('Path', null), closed: p('Boolean', true) },
  'Subdivide Path': { path: p('Path', null), segments: p('Scalar', 2) },
  'Resample Path': {
    path: p('Path', null),
    count: p('Scalar', 32),
    spacing: p('Scalar', 24),
  },
  'Geometry Repeater': {
    path: p('Path', null),
    count: p('Scalar', 6),
    step: p('Vector2', [60, 0]),
    center: p('Vector2', [480, 270]),
    startAngle: p('Scalar', 0),
    angle: p('Scalar', 360),
    offset: p('Scalar', 0),
    anchor: p('Vector2', [0, 0]),
    position: p('Vector2', [60, 0]),
    scale: p('Vector2', [100, 100]),
    alternateScale: p('Scalar', 100),
    alternateFill: p('Boolean', false),
    alternateColor: p('Color', { ...REPEATER_FILL_DEFAULT }),
    alternateStroke: p('Boolean', false),
    alternateStrokeColor: p('Color', { ...REPEATER_STROKE_DEFAULT }),
    rotation: p('Scalar', 0),
    startOpacity: p('Scalar', 100),
    endOpacity: p('Scalar', 100),
    copiesAbove: p('Boolean', true),
  },
  'Luminance Instances': {
    path: p('PaintedPath', null),
    motif: p('Path', null),
    pitch: p('Scalar', 32),
    invert: p('Boolean', false),
    gridOrigin: p('Vector2', [0, 0]),
    tonePower: p('Scalar', 1),
    minScale: p('Scalar', 0.2),
    maxScale: p('Scalar', 1),
    minRotation: p('Scalar', 0),
    maxRotation: p('Scalar', 90),
  },
  'Luminance to Points': {
    path: p('PaintedPath', null),
    pitch: p('Scalar', 32),
    invert: p('Boolean', false),
    gridOrigin: p('Vector2', [0, 0]),
    tonePower: p('Scalar', 1),
    minScale: p('Scalar', 0),
    maxScale: p('Scalar', 1),
    minRotation: p('Scalar', 0),
    maxRotation: p('Scalar', 0),
  },
  'Instance on Points': {
    path: p('Path', null),
    points: p('Path', null),
    alignTangent: p('Boolean', false),
  },
  Reference: {},
  SVG: { position: p('Vector2', [0, 0]) },
  'Composition Reference': { position: p('Vector2', [0, 0]) },
  Fill: { path: p('PaintedPath', null), color: p('Color', { hex: '#36434e', alpha: 1 }) },
  'Linear Gradient Fill': {
    path: p('PaintedPath', null),
    start: p('Vector2', [200, 250]),
    end: p('Vector2', [500, 250]),
    startColor: p('Color', { hex: '#ff6b35', alpha: 1 }),
    endColor: p('Color', { hex: '#6554c0', alpha: 1 }),
  },
  'Radial Gradient Fill': {
    path: p('PaintedPath', null),
    center: p('Vector2', [350, 250]),
    radius: p('Scalar', 150),
    startColor: p('Color', { hex: '#fff3b0', alpha: 1 }),
    endColor: p('Color', { hex: '#d94862', alpha: 1 }),
  },
  'Angular Gradient Fill': {
    path: p('PaintedPath', null),
    center: p('Vector2', [480, 270]),
    angle: p('Scalar', -90),
    clockwise: p('Boolean', true),
    startColor: p('Color', { hex: '#ff0000', alpha: 1 }),
    color1: p('Color', { hex: '#00ff00', alpha: 1 }),
    position1: p('Scalar', 1 / 3),
    color2: p('Color', { hex: '#0000ff', alpha: 1 }),
    position2: p('Scalar', 2 / 3),
    endColor: p('Color', { hex: '#ff0000', alpha: 1 }),
  },
  Halftone: {
    path: p('PaintedPath', null),
    pitch: p('Scalar', 16),
    dotSize: p('Scalar', 16),
    dotSides: p('Scalar', 4),
    dotRotation: p('Scalar', 0),
    starInnerRatio: p('Scalar', 0.25),
    invert: p('Boolean', false),
    gridOrigin: p('Vector2', [0, 0]),
    tonePower: p('Scalar', 1),
  },
  'Ordered Dither': {
    path: p('PaintedPath', null),
    pitch: p('Scalar', 16),
    dotSize: p('Scalar', 16),
    dotSides: p('Scalar', 4),
    dotRotation: p('Scalar', 0),
    starInnerRatio: p('Scalar', 0.25),
    invert: p('Boolean', false),
    gridOrigin: p('Vector2', [0, 0]),
    tonePower: p('Scalar', 1),
  },
  'Error Diffusion Dither': {
    path: p('PaintedPath', null),
    pitch: p('Scalar', 16),
    dotSize: p('Scalar', 16),
    dotSides: p('Scalar', 4),
    dotRotation: p('Scalar', 0),
    starInnerRatio: p('Scalar', 0.25),
    invert: p('Boolean', false),
    gridOrigin: p('Vector2', [0, 0]),
    tonePower: p('Scalar', 1),
  },
  'Line Screen': {
    path: p('PaintedPath', null),
    pitch: p('Scalar', 16),
    invert: p('Boolean', false),
    gridOrigin: p('Vector2', [0, 0]),
    tonePower: p('Scalar', 1),
  },
  Stroke: {
    path: p('PaintedPath', null),
    color: p('Color', { hex: '#36434e', alpha: 1 }),
    width: p('Scalar', 2),
    miterLimit: p('Scalar', 4),
    dashOffset: p('Scalar', 0),
  },
  Output: { path: p('PaintedPath', null) },
  Text: {
    position: p('Vector2', [200, 300]),
    size: p('Scalar', 100),
    letterSpacing: p('Scalar', 0),
    lineSpacing: p('Scalar', 1.2),
  },
  'Text to Path': { path: p('Path', null) },
  'Imported Path': {},
};
export const definitions = Object.fromEntries(
  placeRegistered(Object.entries(legacyDefinitions), registeredNodes, 'definitionAfter', (row) =>
    Object.fromEntries(
      row.resolved.declaration.parameters.map(({ id, type, default: value }) => [
        id,
        p(type, value),
      ]),
    ),
  ),
);
for (const type of imageDitherTypes) {
  if (Object.hasOwn(definitions, type)) throw Error(`Duplicate projected type: ${type}`);
  const shapeType = type.replace(/ Image$/, '');
  definitions[type] = { ...structuredClone(definitions[shapeType]), path: p('Image', null) };
}
export function closedOffsetSettings(read) {
  const amount = read('amount'),
    miterLimit = read('miterLimit');
  if (!Number.isFinite(amount) || (amount !== 0 && Math.abs(amount) < 0.0001))
    throw new OffsetError(
      'OFFSET_AMOUNT',
      'Offset amount must be zero or at least 0.0001 px in magnitude',
    );
  if (!Number.isFinite(miterLimit) || miterLimit < 1 || miterLimit > 100)
    throw new OffsetError('OFFSET_OPTIONS', 'Offset miter limit must be between 1 and 100');
  return { amount, miterLimit };
}
export function linearGradientSettings(read) {
  return createLinearGradientFill(
    Object.fromEntries(['start', 'end', 'startColor', 'endColor'].map((key) => [key, read(key)])),
  );
}
export function angularGradientSettings(read) {
  return createAngularGradientFill(Object.fromEntries(ANGULAR_KEYS.map((key) => [key, read(key)])));
}
export function radialGradientSettings(read) {
  return createRadialGradientFill(
    Object.fromEntries(
      ['center', 'radius', 'startColor', 'endColor'].map((key) => [key, read(key)]),
    ),
  );
}
export function luminanceModifierSettings(read) {
  const pitch = read('pitch'),
    invert = read('invert'),
    gridOrigin = read('gridOrigin'),
    tonePower = validateTonePower(read('tonePower'));
  if (!Number.isFinite(pitch) || pitch < 1 || pitch > 1024)
    throw Error('Luminance pitch must be from 1 to 1024 px');
  if (typeof invert !== 'boolean') throw Error('Luminance invert must be boolean');
  if (
    !Array.isArray(gridOrigin) ||
    gridOrigin.length !== 2 ||
    !gridOrigin.every((value) => Number.isFinite(value) && Math.abs(value) <= 100000)
  )
    throw Error('Luminance grid origin components must be finite and within ±100000 px');
  return { pitch, invert, gridOrigin, tonePower };
}
// Keep count validation shared by JSON/direct input and resolved expression/socket input.
export function radialSettings(type, read) {
  const key = type === 'Polygon' ? 'sides' : 'points',
    count = read(key);
  if (!Number.isInteger(count) || count < 3 || count > 256)
    throw Error(`${type} ${key} must be an integer from 3 to 256`);
  const outer = read(type === 'Polygon' ? 'radius' : 'outerRadius'),
    inner = type === 'Star' ? read('innerRadius') : outer;
  if (![outer, inner].every((v) => Number.isFinite(v) && v > 0 && v <= 100000))
    throw Error('Radii must be positive and at most 100000 px');
  if (inner > outer) throw Error('Star innerRadius must not exceed outerRadius');
  return { count, outer, inner };
}
function radialPath(n, read) {
  const { count, outer, inner } = radialSettings(n.type, read),
    center = read('center'),
    rotation = read('rotation') % 360,
    total = n.type === 'Star' ? count * 2 : count,
    identity = `${n.id}:${n.type}:${count}`;
  return {
    contours: [
      {
        id: identity,
        closed: true,
        points: Array.from({ length: total }, (_, i) => {
          const angle = ((rotation - 90 + (i * 360) / total) * Math.PI) / 180,
            radius = n.type === 'Star' && i % 2 ? inner : outer,
            position = [center[0] + radius * Math.cos(angle), center[1] + radius * Math.sin(angle)];
          assertValue('Vector2', position);
          return { id: `${identity}:${i}`, position, handleA: [0, 0], handleB: [0, 0] };
        }),
      },
    ],
  };
}
export function parseStrokeDash(value) {
  const raw = value.trim();
  if (!raw || raw === 'none') return [];
  if (/^,|,$|,\s*,/.test(raw)) throw Error('Invalid dash pattern');
  const tokens = raw.split(/[\s,]+/);
  if (
    tokens.length > 32 ||
    tokens.some((t) => !/^[+]?(?:\d*\.)?\d+(?:e[+-]?\d+)?(?:px)?$/i.test(t))
  )
    throw Error('Dash pattern needs up to 32 nonnegative pixel lengths');
  const dash = tokens.map((t) => Number(t.replace(/px$/i, '')));
  validateStrokeStyle({ strokeDash: dash });
  return dash;
}
export function validateStrokeStyle(n) {
  if (n.strokeCap !== undefined && !['butt', 'round', 'square'].includes(n.strokeCap))
    throw Error('Invalid stroke cap');
  if (n.strokeJoin !== undefined && !['miter', 'round', 'bevel'].includes(n.strokeJoin))
    throw Error('Invalid stroke join');
  const dash = n.strokeDash === undefined ? [] : n.strokeDash;
  if (
    !Array.isArray(dash) ||
    dash.length > 32 ||
    dash.some((v) => !Number.isFinite(v) || v < 0 || v > 100000)
  )
    throw Error('Dash pattern needs up to 32 lengths from 0 to 100000 px');
  return { cap: n.strokeCap ?? 'butt', join: n.strokeJoin ?? 'miter', dash: [...dash] };
}
export function point(x, y) {
  return {
    id: uid('point'),
    position: p('Vector2', [x, y]),
    handleA: p('Vector2', [0, 0]),
    handleB: p('Vector2', [0, 0]),
    handleType: 'corner',
  };
}
export function node(type, x = 40, y = 40) {
  if (!definitions[type]) throw Error('Unknown node type');
  return {
    id: uid('node'),
    type,
    name: builtinNodeRegistry.get(type)?.declaration.factoryName ?? type,
    position: [x, y],
    width: 264,
    collapsed: false,
    params: clone(definitions[type]),
    ...(builtinNodeRegistry.get(type)?.initialFields() ?? {}),
    points: type === 'Curve' ? [point(250, 150), point(450, 150), point(350, 350)] : [],
    operation: isDither(type)
      ? 'square'
      : type === 'Resample Path'
        ? 'count'
        : type === 'Line Screen'
          ? 'horizontal'
          : type === 'Closed Path Offset'
            ? 'miter'
            : type === 'Geometry Repeater'
              ? 'linear'
              : builtinNodeRegistry.get(type)?.declaration.defaultOperation
                ? builtinNodeRegistry.get(type).declaration.defaultOperation
                : 'subtract',
    edits: [],
    text: 'Nect',
    font: 'default',
    ...(type === 'Text'
      ? {
          writingMode: 'horizontal-tb',
          alignment: 'start',
          kerning: true,
          referenceBounds: 'visible',
          crossAlignment: 'origin',
        }
      : {}),
    rawPath: '',
    ...(type === 'Multi Path' ? { contours: [], units: [] } : {}),
    ...(type === 'Image' ? { asset: '' } : {}),
    ...(type === 'Vector' ? { canvasPositionHandle: false } : {}),
    ...(type === 'Reference' ? { reference: null } : {}),
    ...(['Composition Reference', 'SVG'].includes(type) ? { compositionReference: null } : {}),
  };
}
export function layer(name = 'Composition 1') {
  let a = node('Circle', 40, 50),
    b = node('Rectangle', 40, 460),
    op = node('Boolean', 330, 50),
    remap = node('Path Remap', 620, 50),
    paint = node('Fill', 920, 50),
    out = node('Output', 1220, 50);
  op.params.a.source = a.id;
  op.params.b.source = b.id;
  remap.params.path.source = op.id;
  paint.params.path.source = remap.id;
  out.params.path.source = paint.id;
  return {
    id: uid('layer'),
    name,
    visible: true,
    locked: false,
    fill: '#36434e',
    nodes: [a, b, op, remap, paint, out],
    output: out.id,
  };
}
export function document() {
  return {
    schemaVersion: 24,
    name: 'Untitled composition',
    width: 960,
    height: 540,
    layers: [layer()],
  };
}
// Keep document()/layer() as the existing explicit Boolean demo factories.
export function blankLayer(name = 'Composition 1') {
  const output = node('Output', 40, 50);
  return {
    id: uid('layer'),
    name,
    visible: true,
    locked: false,
    fill: '#36434e',
    allowEmptyOutput: true,
    nodes: [output],
    output: output.id,
  };
}
export function blankDocument() {
  return {
    schemaVersion: 27,
    name: 'Untitled composition',
    editor: { ...guideDefaults(), gridAnchor: 'center' },
    width: 960,
    height: 540,
    layers: [blankLayer()],
  };
}
// An intentional empty terminal, not a fallback for disconnected graph inputs.
export function hasEmptyOutput(layer, output = layer.nodes?.find((n) => n.id === layer.output)) {
  const q = output?.params?.path;
  return (
    layer.allowEmptyOutput === true &&
    output?.id === layer.output &&
    layer.nodes.includes(output) &&
    output.type === 'Output' &&
    q?.type === 'PaintedPath' &&
    q.value === null &&
    q.source === undefined &&
    q.sourcePort === undefined &&
    q.expression === ''
  );
}
export function outputPorts(n) {
  const registered = builtinNodeRegistry.get(n.type);
  if (registered)
    return registered.declaration.outputs.map(({ id, label, legacyDefault, type }) => {
      const port = nectOutputPort({ id, legacyDefault });
      return { id: port, label, type: legacyDefault ? outputType(n, port) : type };
    });
  return n.type === 'Text'
    ? [
        { id: undefined, label: 'Path', type: 'Path' },
        { id: 'position', label: 'Position', type: 'Vector2' },
      ]
    : [{ id: undefined, label: '', type: outputType(n) }];
}
export function outputType(n, port) {
  if (['Composition Reference', 'SVG'].includes(n.type) || n.paintMode)
    return port === undefined ? 'PaintedPath' : undefined;
  const registered = builtinNodeRegistry.get(n.type);
  if (registered)
    return registered
      .interface(n.operation, registered.hasShape ? registered.snapshot(n) : undefined)
      .outputs.find((output) => nectOutputPort(output) === port)?.type;
  if (n.type === 'Text' && port === 'position') return 'Vector2';
  if (port !== undefined) return undefined;
  if (
    [
      'Fill',
      'Linear Gradient Fill',
      'Radial Gradient Fill',
      'Angular Gradient Fill',
      'Halftone',
      'Ordered Dither',
      'Error Diffusion Dither',
      'Line Screen',
      'Stroke',
      'Output',
    ].includes(n.type)
  )
    return 'PaintedPath';
  if (
    n.type === 'Image' ||
    n.type === 'Image Output' ||
    n.type === 'Keyer' ||
    isImageDither(n.type)
  )
    return 'Image';
  if (n.type === 'Mask') return 'PaintedPath';
  return ['Reference', 'Vector Transform'].includes(n.type) ? 'Vector2' : 'Path';
}
// Deliberate identity contracts. Never guess the first socket of a new node type.
const bypassInputs = Object.freeze({
  ...Object.fromEntries(
    registeredNodes
      .filter((row) => row.resolved.declaration.bypassInput !== undefined)
      .map((row) => [row.resolved.declaration.typeId, row.resolved.declaration.bypassInput]),
  ),
  Keyer: 'path',
  'Ordered Dither Image': 'path',
  'Error Diffusion Dither Image': 'path',
  Mask: 'path',
  Boolean: 'a',
  Transform: 'path',
  'Resample Path': 'path',
  'Trim Path': 'path',
  'Set Path Start': 'path',
  'Set Closure': 'path',
  'Subdivide Path': 'path',
  'Dash Path': 'path',
  'Vector Transform': 'vector',
  'Path Remap': 'path',
  'Closed Path Offset': 'path',
  'Geometry Repeater': 'path',
  'Instance on Points': 'path',
  'Luminance Instances': 'motif',
  'Convert to Path': 'path',
  'Text to Path': 'path',
  Fill: 'path',
  'Linear Gradient Fill': 'path',
  'Radial Gradient Fill': 'path',
  'Angular Gradient Fill': 'path',
  Halftone: 'path',
  'Ordered Dither': 'path',
  'Error Diffusion Dither': 'path',
  'Line Screen': 'path',
  Stroke: 'path',
});
export function bypassContract(n, layer) {
  const input = bypassInputs[n?.type];
  if (!input)
    return {
      supported: false,
      reason: 'This node has no bypassable effect with a same-type identity input.',
    };
  const type = outputType(n),
    parameterType = n.params?.[input]?.type;
  const source = layer?.nodes?.find((item) => item.id === n.params?.[input]?.source),
    sourceParameter = n.params?.[input];
  if (
    ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'].includes(n.type) &&
    source &&
    outputType(source, sourceParameter?.sourcePort) === 'Image'
  )
    return {
      supported: false,
      reason:
        'Image luminance inputs must be converted to marks; they cannot be bypassed as PaintedPath.',
    };
  if (
    n.type === 'Convert to Path' &&
    source &&
    outputType(source, sourceParameter?.sourcePort) === 'PaintedPath'
  )
    return {
      supported: false,
      reason:
        'Convert to Path changes PaintedPath into unpainted Path geometry and cannot be bypassed.',
    };
  if (parameterType !== type)
    return {
      supported: false,
      reason: 'This operation changes the input type; it cannot pass through unchanged.',
    };
  return {
    supported: true,
    input,
    type,
    reason: `Pass ${input === 'a' ? 'A' : input} through unchanged; keep all settings and connections.`,
  };
}
function bypassInput(n, layer) {
  if (!n.bypassed) return null;
  const contract = bypassContract(n, layer);
  if (!contract.supported) throw Error(`Cannot bypass ${n.type}: ${contract.reason}`);
  return contract.input;
}
function evaluatedParameters(n) {
  const input = bypassInput(n);
  if (input) return [[input, n.params[input]]];
  if (n.type === 'Instance on Points' && (!n.params.path.source || !n.params.points.source))
    return [];
  return activeParameters(n);
}
export function setNodeBypassed(layer, nodeId, bypassed) {
  const n = layer.nodes.find((n) => n.id === nodeId);
  if (!n || typeof bypassed !== 'boolean') throw Error('Invalid bypass target or state');
  if (layer.locked) throw Error('Composition is locked');
  if (bypassed && !bypassContract(n, layer).supported) throw Error(bypassContract(n, layer).reason);
  assertAcyclic(layer);
  if (bypassed) n.bypassed = true;
  else delete n.bypassed;
}
// Reference uses an explicit public-port ID. Existing default sockets remain unchanged.
const referencePort = (target) => (target.portId === 'output' ? undefined : target.portId);
export function vectorReferenceTargets(layer, referenceId) {
  return layer.nodes
    .filter((n) => n.id !== referenceId)
    .flatMap((n) =>
      outputPorts(n)
        .filter((port) => port.type === 'Vector2')
        .map((port) => ({
          nodeId: n.id,
          portId: port.id ?? 'output',
          name: n.name,
          label: port.label || 'Vector2',
        })),
    );
}
function referenceDependency(n) {
  return n.type === 'Reference' && n.reference
    ? { source: n.reference.nodeId, sourcePort: referencePort(n.reference) }
    : null;
}
function parameterDependencies(entries, connected = null) {
  return entries.flatMap(([, q]) => {
    if (q.source) {
      connected?.add(q);
      return [q];
    }
    return q.expression
      ? parameterExpressionUses(q).map((ref) => ({
          source: ref.nodeId,
          ...(ref.layerId ? { layerId: ref.layerId } : {}),
          ...(ref.compositionId ? { compositionId: ref.compositionId } : {}),
          ...(ref.qualifier ? { qualifier: ref.qualifier } : {}),
          ...(ref.componentComposition ? { componentComposition: true } : {}),
          parameterKey: ref.key,
          component: ref.component,
          ...(q.type === 'Color' ? { parameterType: ref.type } : {}),
        }))
      : [];
  });
}
function collectNodeDependencies(n, activeOnly = false, connected = null) {
  const deps = parameterDependencies(
    activeOnly ? evaluatedParameters(n) : parameters(n),
    connected,
  );
  const reference = referenceDependency(n);
  if (reference) deps.push(reference);
  if (n.type === 'Text' && n.pathAttachment) deps.push({ source: n.pathAttachment.nodeId });
  return deps;
}
export function nodeDependencies(n, activeOnly = false) {
  return collectNodeDependencies(n, activeOnly);
}
// Reuse the graph's parameter/reference/expression dependency contract. Inactive
// edges are intentionally included: retained edits must remain safe on re-enable.
export function insertionRemapDependents(layer, nodeId) {
  const downstream = new Set([nodeId]);
  let changed = true;
  while (changed) {
    changed = false;
    for (const n of layer.nodes)
      if (!downstream.has(n.id) && nodeDependencies(n).some((d) => downstream.has(d.source))) {
        downstream.add(n.id);
        changed = true;
      }
  }
  return layer.nodes.filter(
    (n) => downstream.has(n.id) && n.type === 'Path Remap' && n.edits.length,
  );
}
function isMissingReferenceTarget(error) {
  return [
    'REFERENCE_TARGET_UNSET',
    'REFERENCE_TARGET_MISSING',
    'EXPRESSION_REFERENCE_MISSING',
  ].includes(error.code);
}
function assertTransformControls(evaluation, n) {
  try {
    evaluation.transformValues(n.id);
    return;
  } catch (error) {
    if (!isMissingReferenceTarget(error)) throw error;
  }
  // A missing reference prevents matrix construction. Still validate each
  // independently resolved control rather than blocking edits to other fields.
  const read = (key) => {
    try {
      return evaluation.parameterValue(n.id, key);
    } catch (error) {
      if (!isMissingReferenceTarget(error)) throw error;
      return undefined;
    }
  };
  for (const key of ['anchor', 'scale', 'offset', 'rotation', 'position', 'skewAxis'])
    if (n.params[key]) read(key);
  const uniform = n.params.uniformScale ? read('uniformScale') : true;
  if (uniform === false && n.params.scaleWidth) read('scaleWidth');
  const skew = n.params.skew ? read('skew') : 0;
  if (skew !== undefined && Math.abs(skew) >= 89.9)
    throw Error('Skew must be between -89.9 and 89.9 degrees');
}
export function referenceTarget(layer, n) {
  if (n?.type !== 'Reference') throw Error('Select a Reference node');
  if (!n.reference)
    throw Object.assign(Error('Reference target is not selected'), {
      code: 'REFERENCE_TARGET_UNSET',
    });
  const target = layer.nodes.find((x) => x.id === n.reference.nodeId);
  if (!target)
    throw Object.assign(Error(`Reference target is missing: ${n.reference.nodeId}`), {
      code: 'REFERENCE_TARGET_MISSING',
    });
  if (outputType(target, referencePort(n.reference)) !== 'Vector2')
    throw Error('Reference target must be a public Vector2 output');
  return target;
}
function assertAcyclic(layer) {
  const byId = new Map(layer.nodes.map((n) => [n.id, n])),
    seen = new Set(),
    active = new Set();
  function visit(id) {
    if (active.has(id)) throw Error('Connection would create a cycle');
    if (seen.has(id)) return;
    if (active.size >= 256) throw Error('Graph depth exceeds 256 nodes');
    const n = byId.get(id);
    // Deleted Reference targets remain explicitly dangling and can be saved/undone.
    if (!n) return;
    active.add(id);
    // Socket descriptors can carry arbitrary authored metadata, including a
    // field named layerId. Only parsed expression edges are qualified.
    const connected = new Set();
    for (const dep of collectNodeDependencies(n, false, connected)) {
      if (dep.layerId && !connected.has(dep)) continue;
      if (
        !byId.has(dep.source) &&
        n.type !== 'Reference' &&
        (!dep.parameterKey || connected.has(dep))
      )
        throw Error('Missing source node');
      visit(dep.source);
    }
    active.delete(id);
    seen.add(id);
  }
  for (const n of layer.nodes) visit(n.id);
}
// A parameter read does not read its node's output. In particular, reading a
// Composition Reference's Position must not evaluate its referenced output,
// and Text's Position port must not evaluate its path or outline.
function documentDependencyGraph(doc) {
  const layers = new Map(doc.layers.map((layer) => [layer.id, layer]));
  const nodes = new Map(
    doc.layers.map((layer) => [layer.id, new Map(layer.nodes.map((n) => [n.id, n]))]),
  );
  // One graph describes one immutable candidate. Preserve parameter()'s first
  // matching entry, including virtual descriptors, without rebuilding the
  // entire Curve point list for every visited property.
  const parameterIndexes = new WeakMap();
  function graphParameter(n, key) {
    let indexed = parameterIndexes.get(n);
    if (!indexed) {
      indexed = new Map();
      for (const [name, value] of parameters(n)) if (!indexed.has(name)) indexed.set(name, value);
      parameterIndexes.set(n, indexed);
    }
    return indexed.get(key) || parameterFallback(n, key);
  }
  const output = (layerId, nodeId, port) => ({ layerId, nodeId, port });
  const property = (layerId, nodeId, key) => ({ layerId, nodeId, key });
  const keyOf = (v) =>
    `${v.layerId}\0${v.nodeId}\0${v.key === undefined ? `out:${v.port || ''}` : `param:${v.key}`}`;
  function edges(v) {
    const layer = layers.get(v.layerId),
      n = nodes.get(v.layerId)?.get(v.nodeId);
    if (!n) return [];
    if (v.key !== undefined) {
      // Legacy virtual controls evaluate their backing property. Keep the
      // edge explicit so editing that property reaches external consumers.
      if (isTransform(n) && !n.params[v.key] && v.key === 'position')
        return [property(layer.id, n.id, 'anchor')];
      if (isTransform(n) && !n.params[v.key] && v.key === 'scaleWidth')
        return [property(layer.id, n.id, 'scale')];
      const q = graphParameter(n, v.key);
      if (!q) return [];
      if (q.source) return [output(layer.id, q.source, q.sourcePort)];
      if (!q.expression) return [];
      return parameterExpressionUses(q).map((ref) =>
        property(ref.layerId || layer.id, ref.nodeId, ref.key),
      );
    }
    const positionOnly = n.type === 'Text' && v.port === 'position';
    const entries = positionOnly ? [['position', n.params.position]] : evaluatedParameters(n);
    const result = entries.map(([key]) => property(layer.id, n.id, key));
    if (!positionOnly) {
      const reference = referenceDependency(n);
      if (reference) result.push(output(layer.id, reference.source, reference.sourcePort));
      if (n.type === 'Text' && n.pathAttachment)
        result.push(output(layer.id, n.pathAttachment.nodeId));
      if (['Composition Reference', 'SVG'].includes(n.type) && n.compositionReference) {
        const target = layers.get(n.compositionReference.compositionId);
        if (target) result.push(output(target.id, target.output));
      }
    }
    return result;
  }
  return { layers, nodes, output, property, keyOf, edges, parameter: graphParameter };
}
function assertDocumentReferenceAcyclic(doc) {
  const graph = documentDependencyGraph(doc);
  const seen = new Set(),
    active = new Set();
  function visit(vertex) {
    const key = graph.keyOf(vertex);
    if (active.has(key)) throw Error('Connection would create a cycle across Artwork layers');
    if (seen.has(key)) return;
    if (!graph.nodes.get(vertex.layerId)?.has(vertex.nodeId)) return;
    // Graph traversal alternates output and parameter vertices. The runtime's
    // separate 512-step safety budget must not become a 256-node limit here.
    if (active.size >= 1024) throw Error('Graph depth exceeds the supported limit');
    active.add(key);
    for (const dep of graph.edges(vertex)) visit(dep);
    active.delete(key);
    seen.add(key);
  }
  for (const layer of doc.layers)
    for (const n of layer.nodes) {
      visit(graph.output(layer.id, n.id));
      if (n.type === 'Text') visit(graph.output(layer.id, n.id, 'position'));
      for (const [key] of parameters(n)) visit(graph.property(layer.id, n.id, key));
    }
}
export function setReference(layer, nodeId, target) {
  const n = layer.nodes.find((n) => n.id === nodeId);
  if (n?.type !== 'Reference') throw Error('Select a Reference node');
  const old = n.reference;
  n.reference = target === null ? null : { ...target };
  try {
    validateReference(n);
    if (target !== null) referenceTarget(layer, n);
    assertAcyclic(layer);
  } catch (e) {
    n.reference = old;
    throw e;
  }
}
const nodeIdPattern = /^[a-zA-Z0-9][a-zA-Z0-9:_-]{0,180}$/;
function validateReference(n) {
  if (n.type !== 'Reference') {
    if (n.reference !== undefined) throw Error('Reference target requires a Reference node');
    return;
  }
  const r = n.reference;
  if (r === null) return;
  if (
    !r ||
    typeof r !== 'object' ||
    Array.isArray(r) ||
    Object.keys(r).length !== 2 ||
    typeof r.nodeId !== 'string' ||
    !nodeIdPattern.test(r.nodeId) ||
    !['output', 'position'].includes(r.portId)
  )
    throw Error('Invalid Reference target');
}
// Duplicate a selected group as one operation. Internal node dependencies are
// remapped; dependencies outside the group, including dangling IDs, stay intact.
export function duplicateNodes(layer, ids, { append = true } = {}) {
  const wanted = new Set(ids),
    originals = layer.nodes.filter((n) => wanted.has(n.id));
  if (originals.length !== wanted.size || !originals.length)
    throw Error('Missing nodes to duplicate');
  if ((append ? layer.nodes.length : 0) + originals.length > 2000) throw Error('Too many nodes');
  if (
    originals.some(
      (n) => n.type === 'Path Remap' && n.edits.length && wanted.has(n.params.path.source),
    )
  )
    throw Error(
      'Cannot duplicate an edited Path Remap with its source; duplicate it individually to retain point bindings',
    );
  const occupied = new Set([
    ...layer.nodes.flatMap((n) => [n.id, ...n.points.map((pt) => pt.id)]),
    ...danglingExpressionIds(layer.nodes),
  ]);
  const fresh = (prefix) => {
    const id = uid(prefix);
    if (occupied.has(id)) throw Error('Duplicate node or point ID collision');
    occupied.add(id);
    return id;
  };
  const remap = new Map(originals.map((n) => [n.id, fresh('node')]));
  const copies = originals.map((original) => {
    const copy = clone(original);
    copy.id = remap.get(original.id);
    copy.name += ' copy';
    copy.position = copy.position.map((v) => v + 30);
    const pointIds = new Map(copy.points.map((pt) => [pt.id, fresh('point')]));
    for (const pt of copy.points) pt.id = pointIds.get(pt.id);
    if (copy.type === 'Multi Path') {
      const contourIds = new Map(copy.contours.map((c, i) => [c.id, `${copy.id}:c${i}`]));
      copy.contours = copy.contours.map((c) => ({
        ...c,
        id: contourIds.get(c.id),
        points: c.points.map((id) => pointIds.get(id)),
      }));
      copy.units = copy.units.map((u, i) => ({
        id: `${copy.id}:u${i}`,
        contours: u.contours.map((id) => contourIds.get(id)),
      }));
    }
    for (const [, q] of parameters(copy)) {
      if (remap.has(q.source)) q.source = remap.get(q.source);
    }
    if (copy.pathAttachment && remap.has(copy.pathAttachment.nodeId)) {
      // Only remap a contour when the entire selected reference route reaches a
      // duplicated generator. A copied Curve outside an unselected Transform
      // must not retarget a Text copy that still reads that original Transform.
      let source = layer.nodes.find((n) => n.id === copy.pathAttachment.nodeId);
      const visited = new Set();
      while (source && remap.has(source.id) && !visited.has(source.id)) {
        visited.add(source.id);
        const identityInput = bypassInput(source);
        if (
          identityInput ||
          ['Transform', 'Path Remap', 'Convert to Path', 'Closed Path Offset'].includes(source.type)
        ) {
          source = layer.nodes.find((n) => n.id === source.params[identityInput || 'path'].source);
          continue;
        }
        const contour = copy.pathAttachment.contourId,
          prefix =
            source.type === 'Curve'
              ? source.id
              : ['Circle', 'Ellipse', 'Rectangle'].includes(source.type)
                ? source.id + ':c0'
                : ['Polygon', 'Star'].includes(source.type)
                  ? source.id + ':' + source.type + ':'
                  : null;
        if (
          prefix &&
          (contour === prefix ||
            (['Polygon', 'Star'].includes(source.type) && contour.startsWith(prefix)))
        )
          copy.pathAttachment.contourId = remap.get(source.id) + contour.slice(source.id.length);
        break;
      }
      copy.pathAttachment.nodeId = remap.get(copy.pathAttachment.nodeId);
    }
    if (copy.reference && remap.has(copy.reference.nodeId))
      copy.reference.nodeId = remap.get(copy.reference.nodeId);
    return copy;
  });
  remapExpressionReferences(originals, copies);
  if (append) layer.nodes.push(...copies);
  return copies;
}

const NODE_PRESET_SCHEMA_VERSION = 39;
const BOOLEAN_INPUT_SCHEMA_VERSION = 40;
export const booleanPendingInputKey = 'input_pending';
function presetName(name) {
  if (typeof name !== 'string' || !name.trim() || name.trim().length > 100)
    throw Error('Preset name must contain 1–100 characters');
  return name.trim();
}
// Saved nodes live in a separate identity namespace. Reuse all ordinary graph
// validation, including retained dependencies, without installing them as artwork.
function validateNodePreset(doc, preset) {
  if (!Array.isArray(preset.nodes) || !preset.nodes.length || preset.nodes.length >= 2000)
    throw Error('Preset must contain 1–1999 source/effect nodes');
  if (nodesHaveArtworkReferences(preset.nodes))
    throw Error('Node presets do not support Artwork references');
  if (
    doc.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
    nodesHaveComponentCompositionReferenceSyntax(preset.nodes)
  )
    throw Error('Node presets do not support Component Composition references');
  if (
    doc.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
    nodesHaveQualifiedColorReferenceSyntax(preset.nodes)
  )
    throw Error('Node presets do not support qualified Color references');
  if (
    doc.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
    nodesHaveCompositionPropertyReferenceSyntax(preset.nodes)
  )
    throw Error('Node presets do not support Composition property references');
  const ids = new Set(preset.nodes.map((n) => n.id));
  for (const n of preset.nodes) {
    if (['Output', 'Image Output', 'Composition Reference', 'SVG'].includes(n.type))
      throw Error('Preset cannot contain Output or Composition Reference nodes');
    if (nodeDependencies(n).some((d) => !ids.has(d.source)))
      throw Error('Preset has an external node reference; select its sources too');
  }
  const fontIds = new Set(
    preset.nodes
      .filter((n) => n.type === 'Text')
      .flatMap((n) => [
        n.font,
        ...Object.values(n.characterPositions?.styles || {}).map((s) => s.font),
      ])
      .filter((id) => id && id !== 'default'),
  );
  const imageIds = new Set(preset.nodes.filter((n) => n.type === 'Image').map((n) => n.asset));
  for (const [key, required] of [
    ['fontAssets', fontIds],
    ['imageAssets', imageIds],
  ]) {
    const assets = preset[key];
    if (
      !assets ||
      typeof assets !== 'object' ||
      Array.isArray(assets) ||
      Object.keys(assets).length !== required.size
    )
      throw Error('Invalid preset assets');
    for (const id of required)
      if (
        !assets[id] ||
        !doc[key]?.[id] ||
        JSON.stringify(assets[id]) !== JSON.stringify(doc[key][id])
      )
        throw Error('Preset asset is missing or changed in this Document');
  }
  const layer = blankLayer('Preset validation');
  layer.nodes.push(...clone(preset.nodes));
  const isolated = {
    // Detached nodes retain the source Document's supported feature floor.
    schemaVersion: Math.max(NODE_PRESET_SCHEMA_VERSION, doc.schemaVersion),
    name: 'Preset validation',
    width: doc.width,
    height: doc.height,
    layers: [layer],
    fontAssets: preset.fontAssets,
    imageAssets: preset.imageAssets,
    ...(doc.colorSlots === undefined ? {} : { colorSlots: clone(doc.colorSlots) }),
  };
  validate(isolated);
  // Detect the existing edited-Remap/source refusal before saving unusable data.
  duplicateNodes({ nodes: preset.nodes }, [...ids], { append: false });
}
function captureNodePreset(doc, layer, nodeIds, name, id) {
  if (
    !Array.isArray(nodeIds) ||
    !nodeIds.length ||
    new Set(nodeIds).size !== nodeIds.length ||
    nodeIds.some((id) => !layer.nodes.some((n) => n.id === id))
  )
    throw Error('Select distinct nodes in this composition');
  const nodes = clone(layer.nodes.filter((n) => nodeIds.includes(n.id)));
  const selected = new Set(nodeIds);
  if (
    curvePointReferenceOwners(nodes).some(
      (id) =>
        !selected.has(id) && layer.nodes.some((node) => node.id === id && node.type === 'Curve'),
    )
  )
    throw Error('Preset has an external node reference; select its sources too');
  const fonts = new Set(
    nodes
      .filter((n) => n.type === 'Text')
      .flatMap((n) => [
        n.font,
        ...Object.values(n.characterPositions?.styles || {}).map((s) => s.font),
      ])
      .filter((id) => id && id !== 'default'),
  );
  const images = new Set(nodes.filter((n) => n.type === 'Image').map((n) => n.asset));
  const preset = {
    id,
    name: presetName(name),
    nodes,
    fontAssets: clone(Object.fromEntries([...fonts].map((id) => [id, doc.fontAssets?.[id]]))),
    imageAssets: clone(Object.fromEntries([...images].map((id) => [id, doc.imageAssets?.[id]]))),
  };
  validateNodePreset(doc, preset);
  return preset;
}
export function socketCompatible(sourceType, targetType) {
  return sourceType === targetType || (sourceType === 'Path' && targetType === 'PaintedPath');
}
const imageLuminanceTargets = new Set([
  'Luminance to Points',
  'Luminance Instances',
  'Halftone',
  'Ordered Dither',
  'Line Screen',
  'Error Diffusion Dither',
]);
export function socketCompatibleFor(source, target, key, sourcePort) {
  const q = target && parameter(target, key),
    sourceType = source && outputType(source, sourcePort);
  return (
    !!q &&
    (socketCompatible(sourceType, q.type) ||
      (sourceType === 'PaintedPath' &&
        ((target.type === 'Transform' && key === 'path') || target.type === 'Join Geometry')) ||
      (sourceType === 'PaintedPath' &&
        target.type === 'Convert to Path' &&
        key === 'path' &&
        q.type === 'Path') ||
      (sourceType === 'Image' &&
        sourcePort === undefined &&
        (imageLuminanceTargets.has(target.type) || target.type === 'Mask') &&
        key === 'path' &&
        q.type === 'PaintedPath') ||
      (sourceType === 'Image' &&
        sourcePort === undefined &&
        target.type === 'Mask' &&
        key === 'mask' &&
        q.type === 'Path'))
  );
}
function isImageMask(n, byId) {
  return (
    n.type === 'Mask' &&
    n.params.mask.source &&
    byId.has(n.params.mask.source) &&
    outputType(byId.get(n.params.mask.source), n.params.mask.sourcePort) === 'Image'
  );
}
function hasImageMask(layer) {
  const byId = new Map(layer.nodes.map((n) => [n.id, n]));
  return layer.nodes.some((n) => isImageMask(n, byId));
}
export const joinInputKeys = (n) => n.joinInputOrder ?? ['a', 'b'];
export const booleanInputKeys = (n) => n.booleanInputOrder ?? ['a', 'b'];
// Only command/migration paths call these; reads never allocate or renumber sockets.
function normalizeJoinInputs(n, compact = false, migration = false) {
  if (n.type !== 'Join Geometry') return;
  n.joinInputOrder ??= ['a', 'b'];
  if (compact) {
    const last = n.joinInputOrder.at(-1);
    n.joinInputOrder = n.joinInputOrder.filter((key) => {
      if (
        key === 'a' ||
        key === 'b' ||
        key === last ||
        n.params[key].source ||
        n.params[key].locked
      )
        return true;
      delete n.params[key];
      return false;
    });
  }
  const last = n.params[n.joinInputOrder.at(-1)];
  if ((last.source || last.locked) && n.joinInputOrder.length < JOIN_SLOT_LIMIT) {
    const key = migration ? 'input_legacy_spare' : `input_${uid('slot')}`;
    n.params[key] = p(n.paintMode ? 'PaintedPath' : 'Path', null);
    n.joinInputOrder.push(key);
  }
}
function normalizeBooleanInputs(n, compact = false) {
  // A/B remain the complete legacy representation. Persist the expanded order only
  // after the spare socket is explicitly used or locked.
  if (n.type !== 'Boolean' || !n.booleanInputOrder) return;
  let keys = n.booleanInputOrder;
  if (compact) {
    const last = keys.at(-1);
    keys = keys.filter((key) => {
      if (
        key === 'a' ||
        key === 'b' ||
        key === last ||
        n.params[key].source ||
        n.params[key].locked
      )
        return true;
      delete n.params[key];
      return false;
    });
  }
  const last = n.params[keys.at(-1)];
  if (last && (last.source || last.locked) && keys.length < JOIN_SLOT_LIMIT) {
    const key = 'input_' + uid('slot');
    n.params[key] = p('Path', null);
    n.booleanInputOrder = [...keys, key];
  } else n.booleanInputOrder = keys;
}
export function parameters(n) {
  const entries = builtinNodeRegistry.get(n.type)?.hasShape
    ? builtinNodeRegistry
        .get(n.type)
        .snapshot(n)
        .slots.map(({ id }) => [id, n.params[id]])
    : n.type === 'Boolean'
      ? booleanInputKeys(n).map((key) => [key, n.params[key]])
      : Object.entries(n.params);
  if (n.type === 'Mask' && !Object.hasOwn(n.params, 'feather'))
    entries.push(['feather', p('Scalar', 0)]);
  if (n.type === 'Mask' && !Object.hasOwn(n.params, 'luminance'))
    entries.push(['luminance', p('Boolean', false)]);
  for (const pt of n.points)
    for (const key of ['position', 'handleA', 'handleB'])
      entries.push([`${pt.id}.${key}`, pt[key]]);
  for (const edit of n.edits)
    if (edit.params)
      for (const [key, value] of Object.entries(edit.params))
        entries.push([`remap.${edit.pointId}.${key}`, value]);
  return entries;
}
export const operationOptions = {
  Halftone: ['subtract', 'square', 'circle', 'polygon', 'star'],
  'Ordered Dither': ['square', 'circle', 'polygon', 'star'],
  'Error Diffusion Dither': ['square', 'circle', 'polygon', 'star'],
  'Ordered Dither Image': ['square', 'circle', 'polygon', 'star'],
  'Error Diffusion Dither Image': ['square', 'circle', 'polygon', 'star'],
  'Closed Path Offset': ['miter', 'bevel', 'round'],
  'Geometry Repeater': ['linear', 'radial-fit', 'radial-step', 'ae-2d'],
  'Resample Path': ['count', 'spacing'],
  'Line Screen': ['horizontal', 'vertical'],
  Boolean: ['union', 'subtract', 'intersect', 'difference'],
  ...Object.fromEntries(
    registeredNodes
      .filter((row) => row.resolved.declaration.operations)
      .map((row) => [
        row.resolved.declaration.typeId,
        row.resolved.declaration.operations.map(({ id }) => id),
      ]),
  ),
};
export function activeParameters(n) {
  const entries = parameters(n);
  if (n.type === 'Pucker & Bloat' && n.params.autoCenter.value && !n.params.autoCenter.source)
    return entries.filter(([key]) => key !== 'center');
  if (n.type === 'Geometry Repeater')
    return entries.filter(([key]) =>
      (n.operation === 'ae-2d'
        ? ['path', 'count', ...REPEATER_KEYS]
        : n.operation === 'linear'
          ? ['path', 'count', 'step']
          : ['path', 'count', 'center', 'startAngle', 'angle']
      ).includes(key),
    );
  if (n.type === 'Resample Path')
    return entries.filter(
      ([key]) => key === 'path' || key === (n.operation === 'spacing' ? 'spacing' : 'count'),
    );

  if (
    isTransform(n) &&
    (!n.params.uniformScale ||
      (n.params.uniformScale.value === true &&
        !n.params.uniformScale.source &&
        !n.params.uniformScale.expression))
  )
    return entries.filter(([key]) => key !== 'scaleWidth');
  const registered = builtinNodeRegistry.get(n.type);
  if (!registered?.declaration.operations) return entries;
  const active = registered.interface(n.operation).active;
  return entries.filter(([key]) => active.includes(key));
}
export function setOperation(layer, nodeId, operation) {
  const n = layer.nodes.find((n) => n.id === nodeId);
  if (!n || !operationOptions[n.type]?.includes(operation)) throw Error('Invalid operation');
  if (n.bypassed && !bypassContract({ ...n, operation }).supported)
    throw Error('Restore this node before choosing an operation that changes its output type');
  for (const target of layer.nodes)
    for (const [, q] of parameters(target))
      if (
        q.source === nodeId &&
        !socketCompatible(outputType({ ...n, operation }, q.sourcePort), q.type)
      )
        throw Error('Operation would break a connected socket');
  for (const target of layer.nodes)
    if (
      target.type === 'Reference' &&
      target.reference?.nodeId === nodeId &&
      outputType({ ...n, operation }, referencePort(target.reference)) !== 'Vector2'
    )
      throw Error('Operation would break a Reference Vector2 output');
  n.operation = operation;
}
// Evaluator-owned identities only: authored data and caller-supplied alias fields never
// grant compatibility. The second identity reproduces the empty-expression-field era
// from the same geometry, not from a second geometry evaluation.
const evaluationFingerprints = new WeakMap();
const evaluatedPaintSlots = new WeakMap();
function setEvaluatedPaintSlot(value, paint, slotId) {
  const previous = evaluatedPaintSlots.get(value);
  if (slotId !== null || previous) evaluatedPaintSlots.set(value, { ...previous, [paint]: slotId });
}
function evaluatedFingerprint(value, currentEra = false) {
  const identity = value && evaluationFingerprints.get(value);
  return currentEra && identity && identity.canonical === value.fingerprint
    ? identity.current
    : value?.fingerprint;
}
export function remapFingerprintMatches(input, retainedFingerprint) {
  return (
    retainedFingerprint === input.fingerprint ||
    retainedFingerprint === evaluatedFingerprint(input, true)
  );
}
function copyEvaluationIdentity(source, result) {
  if (source && evaluatedPaintSlots.has(source))
    evaluatedPaintSlots.set(result, { ...evaluatedPaintSlots.get(source) });
  const identity = source && evaluationFingerprints.get(source);
  if (
    identity &&
    identity.canonical === source.fingerprint &&
    result.fingerprint === source.fingerprint
  )
    evaluationFingerprints.set(result, identity);
  if (isPaintedGroup(source) && isPaintedGroup(result))
    source.children.forEach((child, i) => copyEvaluationIdentity(child, result.children[i]));
  return result;
}
const cloneEvaluated = (value) => copyEvaluationIdentity(value, clone(value));
function evaluatedPaintedGroup(values, options) {
  const result = paintedGroup(values, options);
  values.forEach((value, i) => copyEvaluationIdentity(value, result.children[i]));
  return result;
}
function setEvaluationFingerprint(result, preimage) {
  result.fingerprint = fingerprint(JSON.stringify(preimage(false)));
  const current = fingerprint(
    JSON.stringify(preimage(true), function (key, value) {
      return key === 'fingerprint' ? evaluatedFingerprint(this, true) : value;
    }),
  );
  if (current !== result.fingerprint)
    evaluationFingerprints.set(result, { canonical: result.fingerprint, current });
  else evaluationFingerprints.delete(result);
}
export function enableRemapPoint(n, input, pointId) {
  if (
    n.type !== 'Path Remap' ||
    !input.contours.some((c) => c.points.some((p) => p.id === pointId))
  )
    throw Error('Missing remap target');
  let edit = n.edits.find((e) => e.pointId === pointId);
  if (!edit) {
    edit = { pointId, ...(input.fingerprint ? { fingerprint: input.fingerprint } : {}) };
    n.edits.push(edit);
  }
  if (!edit.params) {
    edit.params = {};
    for (const key of ['position', 'handleA', 'handleB']) {
      edit.params[key] = p('Vector2', edit[key] || [0, 0]);
      delete edit[key];
    }
  }
  return edit;
}
export function rebindRemapPoint(n, input, pointId) {
  const edit = n.edits.find((e) => e.pointId === pointId);
  if (!edit || !input.contours.some((c) => c.points.some((p) => p.id === pointId)))
    throw Error('Missing remap target');
  if (input.fingerprint) edit.fingerprint = input.fingerprint;
  else delete edit.fingerprint;
  return edit;
}
export function translateRemapPoints(n, input, pointIds, delta) {
  assertValue('Vector2', delta);
  if (!Array.isArray(pointIds) || !pointIds.length) throw Error('Select at least one point');
  const draft = clone(n);
  for (const pointId of new Set(pointIds)) {
    const existing = draft.edits.find((e) => e.pointId === pointId);
    if (existing && input.unstable && !remapFingerprintMatches(input, existing.fingerprint))
      throw Error('Selected edit is suspended; rebind explicitly first');
    const edit = enableRemapPoint(draft, input, pointId);
    const q = edit.params.position;
    if (q.locked || q.source || q.expression) throw Error('Selected position is locked or driven');
    q.value = q.value.map((v, i) => v + delta[i]);
    assertValue('Vector2', q.value);
  }
  n.edits = draft.edits;
}
export function parameter(n, key) {
  const actual = parameters(n).find(([k]) => k === key)?.[1];
  if (actual) return actual;
  return parameterFallback(n, key);
}
function parameterFallback(n, key) {
  if (
    n.type === 'Boolean' &&
    key === booleanPendingInputKey &&
    !n.booleanInputOrder &&
    (n.params.b?.source || n.params.b?.locked)
  )
    return p('Path', null);
  return transformParameter(n, key);
}
// Only bare value is new syntax; a quoted parameter key named "value" is not.
function usesBaseValue(source) {
  return /\bvalue\b/.test(source.replace(/"[^"]*"/g, ''));
}
export const COLOR_EXPRESSION_SCHEMA_VERSION = 42;
function colorExpressionSchemaVersion(source) {
  return usesColorOperations(source)
    ? COLOR_OPERATIONS_SCHEMA_VERSION
    : COLOR_EXPRESSION_SCHEMA_VERSION;
}
function colorReference(source, resolveReference) {
  return expression(
    source,
    (ref) => {
      if (ref.component !== undefined || (ref.layerId && !ref.qualifier))
        throw Error('Expression reference requires a Color parameter');
      return resolveReference(ref, 'Color');
    },
    true,
  );
}
function parameterExpressionUses(q) {
  if (q.type !== 'Color')
    return expressionReferences(q.expression).map((ref) => ({ ...ref, type: 'Scalar' }));
  const refs = [],
    dummy = { hex: '#000000', alpha: 1 };
  if (!usesColorOperations(q.expression)) {
    parameterExpression(
      q,
      (ref, type) => {
        refs.push({ ...ref, type });
        return dummy;
      },
      dummy,
    );
    return refs;
  }
  colorExpression(q.expression, dummy, {
    reference: (source) =>
      colorReference(source, (ref, type) => {
        refs.push({ ...ref, type });
        return dummy;
      }),
    scalar: (source) => {
      if (usesBaseValue(source)) throw Error('Color value cannot be used as a Scalar');
      const before = refs.length;
      expression(
        source,
        (ref) => {
          if (ref.qualifier) throw Error('Qualified Color references require a Color expression');
          refs.push({ ...ref, type: 'Scalar' });
          return 0;
        },
        true,
      );
      // References are typed below; their actual range is checked at evaluation/Apply.
      return refs.length === before ? expression(source) : 0.5;
    },
  });
  return refs;
}
// Validate against the source Document's grammar before a wrapped import can
// raise its version. Missing owners do not make new syntax legal in old files.
export function expressionReferencesForSchema(q, schemaVersion, { preserveOpaque = false } = {}) {
  if (
    preserveOpaque &&
    schemaVersion < COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
    hasComponentCompositionReferenceSyntax(q.expression)
  )
    return [];
  if (
    schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
    hasComponentCompositionReferenceSyntax(q.expression)
  )
    qualifiedReferencesForCodec(q);
  if (
    preserveOpaque &&
    schemaVersion < QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
    hasQualifiedColorReferenceSyntax(q.expression)
  )
    return [];
  if (
    preserveOpaque &&
    schemaVersion < COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
    hasCompositionPropertyReferenceSyntax(q.expression)
  )
    return [];
  if (
    preserveOpaque &&
    schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION &&
    (hasArtworkReferenceSyntax(q.expression) || hasCompositionPropertyReferenceSyntax(q.expression))
  )
    return [];
  let refs;
  try {
    refs = parameterExpressionUses(q);
  } catch (error) {
    // The wrapper importer historically retained expressions that neither the
    // old nor the new parser can understand. Keep those bytes opaque.
    if (preserveOpaque && error.code !== 'EXPRESSION_INVALID_LENGTH_REFERENCE') return [];
    throw error;
  }
  if (
    refs.some((ref) => ref.componentComposition) &&
    schemaVersion < COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION
  )
    throw Error(
      `Component Composition references require schemaVersion ${COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION}`,
    );
  if (
    refs.some((ref) => ref.componentComposition && ref.type !== 'Scalar') ||
    (refs.some((ref) => ref.componentComposition) &&
      !['Scalar', 'Vector2', 'Color'].includes(q.type))
  )
    throw Error('Component Composition references require a numeric expression');
  if (refs.some((ref) => ref.qualifier) && schemaVersion < QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
    throw Error(
      `Qualified Color references require schemaVersion ${QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION}`,
    );
  if (refs.some((ref) => ref.qualifier && (q.type !== 'Color' || ref.type !== 'Color')))
    throw Error('Qualified Color references require a Color expression');
  if (
    schemaVersion < COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
    refs.some((ref) => ref.compositionId && !ref.qualifier && !ref.componentComposition)
  )
    throw Error(
      `Composition property references require schemaVersion ${COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION}`,
    );
  if (
    refs.some((ref) => ref.compositionId && !ref.qualifier && !ref.componentComposition) &&
    !['Scalar', 'Color'].includes(q.type)
  )
    throw Error('Composition references require a Scalar expression');
  if (
    refs.some(
      (ref) =>
        ref.compositionId &&
        !ref.qualifier &&
        !ref.componentComposition &&
        (ref.type !== 'Scalar' || ref.component !== undefined),
    )
  )
    throw Error('Composition references require a Scalar property');
  if (schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION && refs.some((ref) => ref.layerId))
    throw Error(`Artwork references require schemaVersion ${ARTWORK_REFERENCE_SCHEMA_VERSION}`);
  if (!['Scalar', 'Vector2', 'Color'].includes(q.type) && refs.some((ref) => ref.layerId))
    throw Error('Artwork reference requires a numeric expression parameter');
  if (refs.some((ref) => ref.layerId && !ref.qualifier && ref.type === 'Color'))
    throw Error('Artwork references require a numeric parameter');
  if (
    schemaVersion < CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
    refs.some((ref) => ref.component === 'length')
  )
    throw Error(
      `Curve handle length references require schemaVersion ${CURVE_HANDLE_REFERENCE_SCHEMA_VERSION}`,
    );
  if (
    !['Scalar', 'Vector2', 'Color'].includes(q.type) &&
    refs.some((ref) => ref.component === 'length')
  )
    throw Error('Curve handle length requires a numeric expression parameter');
  return refs;
}
function parameterBaseValue(layer, n, key, q) {
  return n.type === 'Fill' && key === 'color' && q.value === null
    ? { hex: layer.fill, alpha: 1 }
    : q.value;
}
function parameterExpression(q, resolveReference, baseValue = q.value) {
  if (q.type === 'Color') {
    if (q.expression.length > 4096) throw Error('Expression exceeds 4096 characters');
    const source = q.expression.trim();
    let value;
    if (source === 'value') value = baseValue;
    else if (/^ref\s*\([^()]*\)$/.test(source)) value = colorReference(source, resolveReference);
    else if (usesColorOperations(source))
      value = colorExpression(source, baseValue, {
        reference: (text) => colorReference(text, resolveReference),
        scalar: (text) =>
          expression(text, (ref) => resolveReference(ref, 'Scalar'), false, baseValue),
      });
    else throw Error('Color expression requires value, ref, mix or alpha');
    assertValue('Color', value);
    return clone(value);
  }
  return q.type === 'Vector2'
    ? q.expression
        .split(';')
        .map((part, axis) => expression(part, resolveReference, false, q.value[axis]))
    : expression(q.expression, resolveReference, false, q.value);
}
export function previewColorExpression(
  layer,
  nodeId,
  key,
  source,
  fonts = {},
  imageAssets = {},
  documentContext = null,
) {
  const draft = clone(layer),
    n = draft.nodes.find((n) => n.id === nodeId),
    q = n && parameter(n, key);
  if (!q || q.type !== 'Color' || typeof source !== 'string')
    throw Error('Select a Color parameter');
  q.expression = source;
  assertAcyclic(draft);
  const refs = source ? parameterExpressionUses(q) : [];
  if (refs.some((ref) => !ref.compositionId && ref.layerId === layer.id))
    throw Error('Artwork reference requires a different layer');
  const qualified = refs.some((ref) => ref.layerId);
  let previewDocument = documentContext;
  if (qualified && documentContext) {
    if (
      documentContext.schemaVersion < CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
      (documentContext.layers.some((item) => nodesHaveLegacyCurveHandleReference(item.nodes)) ||
        (documentContext.nodePresets || []).some((preset) =>
          nodesHaveLegacyCurveHandleReference(preset.nodes || []),
        ))
    )
      throw Error(
        'Legacy unresolved Curve handle expression cannot be promoted to schemaVersion 61',
      );
    if (
      documentContext.schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION &&
      (documentContext.layers.some((item) => nodesHaveArtworkReferenceSyntax(item.nodes)) ||
        (documentContext.nodePresets || []).some((preset) =>
          nodesHaveArtworkReferenceSyntax(preset.nodes || []),
        ))
    )
      throw Error('Legacy opaque Artwork reference text cannot be promoted to schemaVersion 62');
    previewDocument = inheritProjectDocumentContext(documentContext, clone(documentContext));
    const hasQualifiedColor = refs.some((ref) => ref.qualifier);
    const hasComponentComposition = refs.some((ref) => ref.componentComposition);
    if (hasQualifiedColor) assertNoLegacyQualifiedColorDocumentPromotion(documentContext);
    if (hasComponentComposition) assertSafeComponentCompositionPromotion(documentContext);
    previewDocument.schemaVersion = Math.max(
      documentContext.schemaVersion,
      hasComponentComposition
        ? COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION
        : hasQualifiedColor
          ? QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION
          : refs.some((ref) => ref.compositionId)
            ? COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION
            : ARTWORK_REFERENCE_SCHEMA_VERSION,
    );
    previewDocument.layers = previewDocument.layers.map((item) =>
      item.id === draft.id ? draft : item,
    );
    assertDocumentReferenceAcyclic(previewDocument);
    if (refs.some((ref) => ref.compositionId || ref.qualifier)) {
      const binding = projectDocumentContexts.get(previewDocument);
      if (!binding && refs.some((ref) => ref.compositionId))
        throw Error('Composition property reference requires project context');
      if (binding) {
        if (hasQualifiedColor) assertNoLegacyQualifiedColorReferencePromotion(binding.project);
        assertNoLegacyCompositionPropertyReferencePromotion(binding.project);
        const candidate = {
          ...binding.project,
          schemaVersion: previewDocument.schemaVersion,
          compositionSchemaVersion: CURVE_POINT_COMPOSITION_SCHEMA_VERSION,
          compositions: binding.project.compositions.map((item) => {
            if (item.id === binding.activeCompositionId)
              return { ...item, layers: previewDocument.layers };
            // Preview remains ephemeral, but must evaluate old source Documents
            // with their original migration just like the actual promotion.
            if (binding.project.schemaVersion < COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) {
              const migrated = normalizedProjectSource(binding, item.id);
              const next = {
                ...item,
                width: migrated.width,
                height: migrated.height,
                layers: migrated.layers,
              };
              if (migrated.editor === undefined) delete next.editor;
              else next.editor = migrated.editor;
              return next;
            }
            return item;
          }),
        };
        validateCompositionProject(candidate);
        assertProjectReferenceTransition(binding.project, candidate, fonts);
      }
    }
  }
  const evaluation = evaluate(draft, fonts, imageAssets, previewDocument);
  const resolve = (ref, type = 'Scalar') => evaluation.resolveReference(ref, type);
  return source
    ? parameterExpression(q, resolve, parameterBaseValue(draft, n, key, q))
    : clone(parameterBaseValue(draft, n, key, q));
}
// IDs are deliberately restricted: no JavaScript, object access, or string evaluation.
function parameterReferenceText(nodeId, key, component) {
  return `ref(${JSON.stringify(nodeId)}, ${JSON.stringify(key)}${component ? `, ${JSON.stringify(component)}` : ''})`;
}
export function parameterReferenceExpression(nodeId, key, component) {
  const text = parameterReferenceText(nodeId, key, component);
  expression(text, () => 0, true);
  return text;
}
export function compositionParameterReferenceExpression(compositionId, layerId, nodeId, key) {
  const text = `ref(composition, ${JSON.stringify(compositionId)}, ${JSON.stringify(layerId)}, ${JSON.stringify(nodeId)}, ${JSON.stringify(key)})`;
  expression(text, () => 0, true);
  return text;
}
export function componentCompositionParameterReferenceExpression(
  compositionId,
  layerId,
  nodeId,
  key,
  component,
) {
  const text = `ref(componentComposition, ${JSON.stringify(compositionId)}, ${JSON.stringify(layerId)}, ${JSON.stringify(nodeId)}, ${JSON.stringify(key)}, ${JSON.stringify(component)})`;
  expression(text, () => 0, true);
  return text;
}
export function colorArtworkParameterReferenceExpression(layerId, nodeId, key) {
  const text = `ref(colorArtwork, ${JSON.stringify(layerId)}, ${JSON.stringify(nodeId)}, ${JSON.stringify(key)})`;
  expression(text, () => 0, true);
  return text;
}
export function colorCompositionParameterReferenceExpression(compositionId, layerId, nodeId, key) {
  const text = `ref(colorComposition, ${JSON.stringify(compositionId)}, ${JSON.stringify(layerId)}, ${JSON.stringify(nodeId)}, ${JSON.stringify(key)})`;
  expression(text, () => 0, true);
  return text;
}
export function artworkParameterReferenceExpression(layerId, nodeId, key, component) {
  const text = `ref(artwork, ${JSON.stringify(layerId)}, ${JSON.stringify(nodeId)}, ${JSON.stringify(key)}${component ? `, ${JSON.stringify(component)}` : ''})`;
  expression(text, () => 0, true);
  return text;
}
export function expressionReferences(source) {
  // Existing dormant literal expressions keep their prior import semantics.
  if (!/\bref\b/.test(source)) return [];
  if (usesColorOperations(source))
    return parameterExpressionUses({ type: 'Color', expression: source }).map(
      ({ type, ...ref }) => ref,
    );
  const refs = [];
  for (const part of source.split(';'))
    expression(
      part,
      (ref) => {
        refs.push(ref);
        return 0;
      },
      true,
    );
  return refs;
}
function referenceParameter(layer, ref, type = 'Scalar', index = null) {
  if (ref.layerId && ((ref.qualifier && type !== 'Color') || (!ref.qualifier && type !== 'Scalar')))
    throw Error(
      ref.qualifier
        ? 'Qualified Color references require a Color parameter'
        : 'Artwork references require a numeric parameter',
    );
  const target = index ? index.nodes.get(ref.nodeId) : layer.nodes.find((n) => n.id === ref.nodeId);
  if (!target)
    throw Object.assign(Error(`Expression reference target is missing: ${ref.nodeId}`), {
      code: 'EXPRESSION_REFERENCE_MISSING',
    });
  const pointRef = curvePointReference(ref.key, ref.component);
  if (pointRef && target.type === 'Curve') {
    if (type !== 'Scalar') throw Error('Curve point reference requires a Scalar component');
    const point = index
      ? index.points.get(target.id)?.get(pointRef.id)
      : target.points.find((item) => item.id === pointRef.id);
    if (!point)
      throw Object.assign(Error(`Expression reference point is missing: ${pointRef.id}`), {
        code: 'EXPRESSION_REFERENCE_MISSING',
      });
    if (point[pointRef.field]?.type !== 'Vector2') throw Error('Invalid Curve point parameter');
    return target;
  }
  const q = Object.hasOwn(target.params, ref.key)
    ? target.params[ref.key]
    : transformParameter(target, ref.key);
  if (type === 'Color') {
    if (!q || q.type !== 'Color' || ref.component !== undefined)
      throw Error('Expression reference requires a Color parameter');
    return target;
  }
  if (
    !q ||
    (ref.component === undefined
      ? q.type !== 'Scalar'
      : !['x', 'y'].includes(ref.component) || q.type !== 'Vector2')
  )
    throw Error('Expression reference requires a Scalar parameter or a Vector2 component');
  return target;
}

function colorArtworkClipboardReferences(nodes, schemaVersion) {
  const references = [];
  for (const node of nodes)
    for (const q of allExpressionParameters(node)) {
      if (!hasQualifiedColorReferenceSyntax(q.expression)) continue;
      if (schemaVersion < QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION || q.type !== 'Color')
        throw Error('Copying opaque or unsupported qualified Color references is not supported');
      const parsed = expressionReferencesForSchema(q, schemaVersion).filter((ref) => ref.qualifier);
      if (
        !parsed.length ||
        parsed.some((ref) => ref.qualifier !== 'colorArtwork' || ref.type !== 'Color')
      )
        throw Error('Copying Composition Color references is not supported');
      references.push(...parsed);
    }
  return references;
}

// Clipboard references preserve their original foreign targets only while each
// target remains present. Once absent, a later reuse of the same ID is not proof
// that it is the object copied by the user.
export function artworkClipboardTargetsIntact(doc, clipboard) {
  if (!doc.layers.some((layer) => layer.id === clipboard.sourceLayerId)) return false;
  const layers = new Map(doc.layers.map((layer) => [layer.id, layer])),
    indexes = new Map();
  for (const ref of clipboard.artworkReferences) {
    const layer = layers.get(ref.layerId);
    if (!layer || layer.id === clipboard.sourceLayerId) return false;
    let index = indexes.get(layer.id);
    if (!index) {
      index = { nodes: new Map(layer.nodes.map((node) => [node.id, node])), points: new Map() };
      indexes.set(layer.id, index);
    }
    const owner = index.nodes.get(ref.nodeId);
    if (!owner || owner.type !== ref.ownerType) return false;
    if (
      owner.type === 'Curve' &&
      curvePointReference(ref.key, ref.component) &&
      !index.points.has(owner.id)
    )
      index.points.set(owner.id, new Map(owner.points.map((point) => [point.id, point])));
    try {
      referenceParameter(layer, ref, ref.qualifier === 'colorArtwork' ? 'Color' : 'Scalar', index);
    } catch {
      return false;
    }
  }
  return true;
}
function projectReferenceValue(ref, value) {
  if (ref.component === undefined) return value;
  const projected =
    ref.component === 'length'
      ? Math.hypot(value[0], value[1])
      : value[ref.component === 'x' ? 0 : 1];
  assertValue('Scalar', projected);
  return projected;
}
export function expressionReferenceTargets(layer, nodeId, type = 'Scalar') {
  const reaches = (id, seen = new Set()) => {
    if (id === nodeId) return true;
    if (seen.has(id)) return false;
    seen.add(id);
    const n = layer.nodes.find((n) => n.id === id);
    return n ? nodeDependencies(n).some((d) => reaches(d.source, seen)) : false;
  };
  return layer.nodes
    .filter((n) => !reaches(n.id))
    .flatMap((n) =>
      [
        ...Object.entries(n.params).map(([key, q]) => [key, q, false]),
        ...(n.type === 'Curve' && type !== 'Color'
          ? n.points.flatMap((pt, index) =>
              ['position', 'handleA', 'handleB'].map((field) => [
                `${pt.id}.${field}`,
                pt[field],
                field,
                index + 1,
              ]),
            )
          : []),
      ].flatMap(([key, q, point, pointOrdinal]) =>
        (type === 'Color'
          ? q.type === 'Color'
            ? [undefined]
            : []
          : q.type === 'Scalar'
            ? [undefined]
            : q.type === 'Vector2'
              ? point && point !== 'position'
                ? ['x', 'y', 'length']
                : ['x', 'y']
              : []
        ).map((component) => ({
          nodeId: n.id,
          name: n.name,
          key,
          component,
          type: type === 'Color' ? 'Color' : 'Scalar',
          ...(point ? { pointOrdinal } : {}),
          label: point
            ? `${n.name} · Point ${pointOrdinal} · ${point === 'position' ? '' : point === 'handleA' ? 'Incoming · ' : 'Outgoing · '}${component === 'length' ? 'Length' : component.toUpperCase()}`
            : `${n.name} · ${key}${component ? `.${component}` : ''}`,
          expression:
            point && nodeIdPattern.test(n.id) && curvePointReference(key, component)
              ? parameterReferenceText(n.id, key, component)
              : parameterReferenceExpression(n.id, key, component),
        })),
      ),
    );
}
export function expressionReferenceTargetsForDocument(
  doc,
  layerId,
  nodeId,
  type = 'Scalar',
  targetKey,
) {
  const layer = doc?.layers?.find((item) => item.id === layerId);
  if (!layer || !layer.nodes.some((n) => n.id === nodeId)) return [];
  const graph = documentDependencyGraph(doc);
  const reachesTarget = (vertex, seen = new Set()) => {
    if (
      vertex.layerId === layerId &&
      vertex.nodeId === nodeId &&
      (targetKey === undefined || vertex.key === targetKey)
    )
      return true;
    const key = graph.keyOf(vertex);
    if (seen.has(key)) return false;
    seen.add(key);
    return graph.edges(vertex).some((dep) => reachesTarget(dep, seen));
  };
  const candidates = new Map();
  const reachesFromCandidate = (vertex) => {
    const key = graph.keyOf(vertex);
    if (!candidates.has(key)) candidates.set(key, reachesTarget(vertex));
    return candidates.get(key);
  };
  const local = expressionReferenceTargets(layer, nodeId, type)
    .filter((target) => !reachesFromCandidate(graph.property(layer.id, target.nodeId, target.key)))
    .map((target) => ({
      ...target,
      layerId: layer.id,
      layerName: layer.name,
    }));
  return local.concat(
    doc.layers
      .filter((item) => item.id !== layerId)
      .flatMap((sourceLayer) =>
        expressionReferenceTargets(sourceLayer, null, type)
          .filter(
            (target) =>
              !reachesFromCandidate(graph.property(sourceLayer.id, target.nodeId, target.key)),
          )
          .map((target) => ({
            ...target,
            layerId: sourceLayer.id,
            layerName: sourceLayer.name,
            expression:
              type === 'Color'
                ? colorArtworkParameterReferenceExpression(
                    sourceLayer.id,
                    target.nodeId,
                    target.key,
                  )
                : artworkParameterReferenceExpression(
                    sourceLayer.id,
                    target.nodeId,
                    target.key,
                    target.component,
                  ),
          })),
      ),
  );
}
// Enumerate from one project graph, so candidates that would close a mixed
// cross-Composition cycle are excluded without changing any authored state.
export function expressionReferenceTargetsForProject(
  project,
  compositionId,
  layerId,
  nodeId,
  type = 'Scalar',
  targetKey,
) {
  const composition = project?.compositions?.find((item) => item.id === compositionId);
  if (!composition) return [];
  const document = projectAsReferenceDocument(project, { normalize: true });
  const binding = projectDocumentContexts.get(document);
  const normalizedComposition = normalizedProjectSource(binding, compositionId);
  const graph = documentDependencyGraph(document);
  const reaches = (vertex, seen = new Set()) => {
    if (
      vertex.layerId === layerId &&
      vertex.nodeId === nodeId &&
      (targetKey === undefined || vertex.key === targetKey)
    )
      return true;
    const key = graph.keyOf(vertex);
    if (seen.has(key)) return false;
    seen.add(key);
    return graph.edges(vertex).some((dep) => reaches(dep, seen));
  };
  const localDoc = { ...document, layers: normalizedComposition.layers };
  const local = expressionReferenceTargetsForDocument(localDoc, layerId, nodeId, type, targetKey)
    .filter((target) => !reaches(graph.property(target.layerId, target.nodeId, target.key)))
    .map((target) => ({ ...target, compositionId }));
  const targetNode = normalizedComposition.layers
    .find((item) => item.id === layerId)
    ?.nodes.find((item) => item.id === nodeId);
  const destination =
    targetKey === undefined ? null : targetNode && parameter(targetNode, targetKey);
  if (
    (type === 'Color' && destination && destination.type !== 'Color') ||
    (destination && !['Scalar', 'Vector2', 'Color'].includes(destination.type))
  )
    return local;
  return local.concat(
    project.compositions
      .filter((item) => item.id !== compositionId)
      .flatMap((source) =>
        normalizedProjectSource(binding, source.id).layers.flatMap((sourceLayer) =>
          expressionReferenceTargets(sourceLayer, null, type)
            .filter(
              (target) =>
                source.layers
                  .find((item) => item.id === sourceLayer.id)
                  ?.nodes.some((item) => item.id === target.nodeId) &&
                !(destination?.type === 'Vector2' && !target.component) &&
                graph.parameter(
                  sourceLayer.nodes.find((n) => n.id === target.nodeId),
                  target.key,
                )?.type ===
                  (type === 'Color' ? 'Color' : target.component ? 'Vector2' : 'Scalar') &&
                !reaches(graph.property(sourceLayer.id, target.nodeId, target.key)),
            )
            .map((target) => ({
              ...target,
              compositionId: source.id,
              layerId: sourceLayer.id,
              layerName: sourceLayer.name,
              expression:
                type === 'Color'
                  ? colorCompositionParameterReferenceExpression(
                      source.id,
                      sourceLayer.id,
                      target.nodeId,
                      target.key,
                    )
                  : target.component
                    ? componentCompositionParameterReferenceExpression(
                        source.id,
                        sourceLayer.id,
                        target.nodeId,
                        target.key,
                        target.component,
                      )
                    : compositionParameterReferenceExpression(
                        source.id,
                        sourceLayer.id,
                        target.nodeId,
                        target.key,
                      ),
            })),
        ),
      ),
  );
}

function assertValue(type, value) {
  if (type === 'Color' && !isColor(value))
    throw Error('Expected Color with hex #RRGGBB and alpha from 0 to 1');
  if (type === 'Scalar' && !Number.isFinite(value)) throw Error('Expected finite Scalar');
  if (type === 'Vector2' && !isVector2(value)) throw Error('Expected Vector2');
  if (type === 'Boolean' && typeof value !== 'boolean') throw Error('Expected Boolean');
}
function toPaper(path) {
  let compound = new scope.CompoundPath({ insert: false });
  for (const contour of path.contours) {
    let child = new scope.Path({ insert: false, closed: contour.closed });
    for (const pt of contour.points)
      child.add(new scope.Segment(pt.position, pt.handleA, pt.handleB));
    compound.addChild(child);
  }
  return compound;
}
function booleanOperand(path) {
  if (!path.contours.some((c) => c.group !== undefined)) return toPaper(path);
  let result = null;
  for (const contours of contourGroups(path)) {
    const item = toPaper({ contours });
    if (!result) {
      result = item;
      continue;
    }
    const union = result.unite(item, { insert: false });
    result.remove();
    item.remove();
    result = union;
  }
  return result || toPaper({ contours: [] });
}
function fromPaper(item, prefix) {
  let children = item.children || [item];
  return {
    contours: children.map((c, ci) => ({
      id: `${prefix}:c${ci}`,
      closed: c.closed,
      points: c.segments.map((s, si) => ({
        id: `${prefix}:c${ci}:p${si}`,
        position: [s.point.x, s.point.y],
        handleA: [s.handleIn.x, s.handleIn.y],
        handleB: [s.handleOut.x, s.handleOut.y],
      })),
    })),
  };
}

// Paper's getTimeAt() uses an absolute 1e-12 length tolerance. Normalize very
// small curves around their first control point before finding t, then use that
// same t to take the exact subcurve from the original curve.
function curveTimeAtDistance(curve, distance, length) {
  if (distance <= 0) return 0;
  if (distance >= length) return 1;
  if (length >= 1e-6) return curve.getTimeAt(distance);
  const values = curve.values,
    scale = 1 / length,
    originX = values[0],
    originY = values[1],
    normalized = values.map((value, index) => (value - (index % 2 ? originY : originX)) * scale),
    scaledCurve = new scope.Curve(normalized),
    time = scaledCurve.getTimeAt(distance * scale);
  return Number.isFinite(time) ? Math.min(1, Math.max(0, time)) : time;
}

function trimPathGeometry(source, { start, end, prefix }) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (!Number.isFinite(start) || start < 0 || start > 1)
    throw Error('Trim Path start must be from 0 to 1');
  if (!Number.isFinite(end) || end < 0 || end > 1) throw Error('Trim Path end must be from 0 to 1');
  if (start > end) throw Error('Trim Path start must not exceed end');
  if (typeof prefix !== 'string' || !prefix) throw Error('Trim Path needs a nonempty ID prefix');

  let inputPoints = 0;
  for (const contour of source.contours) {
    if (!Array.isArray(contour?.points)) throw Error('Trim Path needs valid contours');
    inputPoints += contour.points.length;
    if (inputPoints > 100000) throw Error('Trim Path exceeds geometry budget');
  }
  if (source.contours.length > 10000) throw Error('Trim Path exceeds geometry budget');

  const copyPoint = (point, contourIndex, pointIndex) => {
    const position = [...point.position],
      handleA = [...point.handleA],
      handleB = [...point.handleB];
    assertValue('Vector2', position);
    assertValue('Vector2', handleA);
    assertValue('Vector2', handleB);
    return {
      ...point,
      id: `${prefix}:c${contourIndex}:p${pointIndex}`,
      position,
      handleA,
      handleB,
    };
  };
  const copyContour = (contour, contourIndex, closed = contour.closed) => ({
    ...contour,
    id: `${prefix}:c${contourIndex}`,
    closed,
    points: contour.points.map((point, pointIndex) => copyPoint(point, contourIndex, pointIndex)),
  });

  if (start === end) return { ...source, contours: [] };
  if (start === 0 && end === 1)
    return {
      ...source,
      contours: source.contours.map((contour, index) => copyContour(contour, index)),
    };

  const item = toPaper(source);
  try {
    let outputPointCount = 0;
    const contours = source.contours.map((contour, contourIndex) => {
      const path = item.children[contourIndex],
        length = path.length;
      if (!Number.isFinite(length) || length < 0)
        throw Error('Trim Path exceeds finite path length');

      // Arc length cannot distinguish positions on a degenerate contour. Keep
      // its finite points as an open contour for any non-empty partial range.
      if (length === 0) {
        outputPointCount += contour.points.length;
        if (outputPointCount > 100000) throw Error('Trim Path exceeds geometry budget');
        return copyContour(contour, contourIndex, false);
      }

      const from = length * start,
        to = length * end,
        output = [];
      if (![from, to].every(Number.isFinite)) throw Error('Trim Path exceeds finite path length');
      const append = (values) => {
        if (!Array.isArray(values) || values.length !== 8 || !values.every(Number.isFinite))
          throw Error('Trim Path exceeds finite coordinates');
        const addedPoints = output.length ? 1 : 2;
        if (outputPointCount + addedPoints > 100000)
          throw Error('Trim Path exceeds geometry budget');
        const startPoint = [values[0], values[1]],
          control1 = [values[2], values[3]],
          control2 = [values[4], values[5]],
          endPoint = [values[6], values[7]];
        if (!output.length) {
          output.push({
            id: `${prefix}:c${contourIndex}:p0`,
            position: startPoint,
            handleA: [0, 0],
            handleB: [control1[0] - startPoint[0], control1[1] - startPoint[1]],
          });
        } else {
          const previous = output[output.length - 1];
          previous.handleB = [
            control1[0] - previous.position[0],
            control1[1] - previous.position[1],
          ];
        }
        const index = output.length;
        output.push({
          id: `${prefix}:c${contourIndex}:p${index}`,
          position: endPoint,
          handleA: [control2[0] - endPoint[0], control2[1] - endPoint[1]],
          handleB: [0, 0],
        });
        outputPointCount += addedPoints;
      };
      let cursor = 0;
      for (const curve of path.curves) {
        const curveLength = curve.length,
          next = cursor + curveLength;
        if (!Number.isFinite(curveLength) || !Number.isFinite(next))
          throw Error('Trim Path exceeds finite path length');
        if (curveLength > 0) {
          const localStart = Math.max(0, from - cursor),
            localEnd = Math.min(curveLength, to - cursor);
          if (localEnd > localStart) {
            const t0 = curveTimeAtDistance(curve, localStart, curveLength),
              t1 = curveTimeAtDistance(curve, localEnd, curveLength);
            if (!Number.isFinite(t0) || !Number.isFinite(t1) || t0 < 0 || t1 > 1 || t0 > t1)
              throw Error('Trim Path could not locate a finite curve segment');
            if (t1 > t0) append(curve.getPart(t0, t1).values);
          }
        }
        cursor = next;
      }
      if (output.length > 100000) throw Error('Trim Path exceeds geometry budget');
      for (const point of output) {
        assertValue('Vector2', point.position);
        assertValue('Vector2', point.handleA);
        assertValue('Vector2', point.handleB);
      }
      return {
        ...contour,
        id: `${prefix}:c${contourIndex}`,
        closed: false,
        points: output,
      };
    });
    if (contours.reduce((sum, contour) => sum + contour.points.length, 0) > 100000)
      throw Error('Trim Path exceeds geometry budget');
    return { ...source, contours };
  } finally {
    item.remove();
  }
}

function setPathStartGeometry(source, { start, prefix }) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (!Number.isFinite(start) || start < 0 || start > 1)
    throw Error('Set Path Start start must be from 0 to 1');
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Set Path Start needs a nonempty ID prefix');
  if (source.contours.length > 10000) throw Error('Set Path Start exceeds geometry budget');

  let inputPointCount = 0;
  for (const contour of source.contours) {
    if (!Array.isArray(contour?.points) || typeof contour.closed !== 'boolean')
      throw Error('Set Path Start needs valid contours');
    inputPointCount += contour.points.length;
    if (inputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
    for (const point of contour.points) {
      if (!point) throw Error('Set Path Start needs valid points');
      assertValue('Vector2', point.position);
      assertValue('Vector2', point.handleA);
      assertValue('Vector2', point.handleB);
    }
  }

  const copyPoint = (point, contourIndex, pointIndex) => ({
      ...point,
      id: `${prefix}:c${contourIndex}:p${pointIndex}`,
      position: [...point.position],
      handleA: [...point.handleA],
      handleB: [...point.handleB],
    }),
    copyContour = (contour, contourIndex, points = contour.points) => ({
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      // Open paths have no seam to move. They intentionally pass through with
      // their point order, curve shape, groups and arbitrary source metadata intact.
      points: points.map((point, pointIndex) => copyPoint(point, contourIndex, pointIndex)),
    });

  // Both ends are geometry identities. Still give the modifier's result its
  // own deterministic IDs, like the other derived-path modifiers.
  if (start === 0 || start === 1)
    return {
      ...source,
      contours: source.contours.map((contour, contourIndex) => copyContour(contour, contourIndex)),
    };

  if (!source.contours.some((contour) => contour.closed && contour.points.length))
    return {
      ...source,
      contours: source.contours.map((contour, contourIndex) => copyContour(contour, contourIndex)),
    };

  const item = toPaper(source);
  try {
    if (!Array.isArray(item.children) || item.children.length !== source.contours.length)
      throw Error('Set Path Start could not resolve contour geometry');
    let outputPointCount = 0;
    const contours = source.contours.map((contour, contourIndex) => {
      const count = contour.points.length,
        path = item.children[contourIndex];
      if (!contour.closed || count === 0) {
        outputPointCount += count;
        if (outputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
        return copyContour(contour, contourIndex);
      }

      const length = path.length;
      if (!Number.isFinite(length) || length < 0)
        throw Error('Set Path Start exceeds finite path length');
      // No arc-length position can distinguish a zero-length closed contour.
      // Keep its original closure and finite points for every seam value.
      if (length === 0) {
        outputPointCount += count;
        if (outputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
        return copyContour(contour, contourIndex);
      }
      if (!path.curves || path.curves.length !== count)
        throw Error('Set Path Start could not resolve closed contour edges');

      const distance = start * length,
        tolerance = length * Number.EPSILON * 16;
      if (!Number.isFinite(distance)) throw Error('Set Path Start exceeds finite path length');

      // An exact source-vertex seam needs no split and must not acquire a
      // duplicate knot. Resolve a boundary before looking for an interior edge.
      let cursor = 0,
        vertexIndex = null,
        split = null;
      for (let edgeIndex = 0; edgeIndex < path.curves.length; edgeIndex++) {
        const curve = path.curves[edgeIndex],
          curveLength = curve.length,
          next = cursor + curveLength;
        if (!Number.isFinite(curveLength) || curveLength < 0 || !Number.isFinite(next))
          throw Error('Set Path Start exceeds finite path length');
        if (Math.abs(distance - cursor) <= tolerance) {
          vertexIndex = edgeIndex;
          break;
        }
        if (Math.abs(distance - next) <= tolerance) {
          vertexIndex = (edgeIndex + 1) % count;
          break;
        }
        if (curveLength > 0 && distance > cursor && distance < next) {
          const time = curveTimeAtDistance(curve, distance - cursor, curveLength);
          if (!Number.isFinite(time) || time <= 0 || time >= 1)
            throw Error('Set Path Start could not represent an interior seam');
          const tail = curve.getPart(time, 1)?.values,
            head = curve.getPart(0, time)?.values;
          if (
            !Array.isArray(tail) ||
            tail.length !== 8 ||
            !tail.every(Number.isFinite) ||
            !Array.isArray(head) ||
            head.length !== 8 ||
            !head.every(Number.isFinite)
          )
            throw Error('Set Path Start could not represent an interior seam');
          const position = tail.slice(0, 2),
            handleB = [tail[2] - position[0], tail[3] - position[1]],
            handleA = [head[4] - position[0], head[5] - position[1]];
          if (![...position, ...handleA, ...handleB].every(Number.isFinite))
            throw Error('Set Path Start could not represent an interior seam');
          split = { position, handleA, handleB, edgeIndex, tail, head };
          break;
        }
        cursor = next;
      }

      let points;
      if (vertexIndex !== null) {
        points = Array.from(
          { length: count },
          (_, offset) => contour.points[(vertexIndex + offset) % count],
        );
      } else if (split) {
        const seamSource = contour.points[split.edgeIndex];
        const tailEndIndex = (split.edgeIndex + 1) % count,
          tailEnd = contour.points[tailEndIndex],
          headStart = seamSource,
          tailEndHandleA = [split.tail[4] - split.tail[6], split.tail[5] - split.tail[7]],
          headStartHandleB = [split.head[2] - split.head[0], split.head[3] - split.head[1]];
        points =
          count === 1
            ? [
                {
                  ...seamSource,
                  position: split.position,
                  handleA: split.handleA,
                  handleB: split.handleB,
                },
                {
                  ...tailEnd,
                  handleA: tailEndHandleA,
                  handleB: headStartHandleB,
                },
              ]
            : [
                {
                  ...seamSource,
                  position: split.position,
                  handleA: split.handleA,
                  handleB: split.handleB,
                },
                {
                  ...tailEnd,
                  handleA: tailEndHandleA,
                },
                ...Array.from(
                  { length: count - 2 },
                  (_, offset) => contour.points[(split.edgeIndex + 2 + offset) % count],
                ),
                {
                  ...headStart,
                  handleB: headStartHandleB,
                },
              ];
      } else {
        throw Error('Set Path Start could not locate a finite seam');
      }

      outputPointCount += points.length;
      if (outputPointCount > 100000) throw Error('Set Path Start exceeds geometry budget');
      const remapped = copyContour(contour, contourIndex, points);
      for (const point of remapped.points) {
        assertValue('Vector2', point.position);
        assertValue('Vector2', point.handleA);
        assertValue('Vector2', point.handleB);
      }
      return remapped;
    });
    if (contours.reduce((sum, contour) => sum + contour.points.length, 0) > 100000)
      throw Error('Set Path Start exceeds geometry budget');
    return { ...source, contours };
  } finally {
    item.remove();
  }
}

function subdividePathGeometry(source, { segments, prefix }) {
  if (!source || !Array.isArray(source.contours)) throw Error('Connect a Path input');
  if (!Number.isInteger(segments) || segments < 1 || segments > 32)
    throw Error('Subdivide segments must be an integer from 1 to 32');
  if (typeof prefix !== 'string' || !prefix)
    throw Error('Subdivide Path needs a nonempty ID prefix');
  if (source.contours.length > 10000) throw Error('Subdivide Path exceeds geometry budget');

  let inputPoints = 0,
    outputPoints = 0;
  for (const contour of source.contours) {
    if (!Array.isArray(contour?.points) || typeof contour.closed !== 'boolean')
      throw Error('Subdivide Path needs valid contours');
    inputPoints += contour.points.length;
    if (inputPoints > 100000) throw Error('Subdivide Path exceeds geometry budget');
    const count = contour.points.length;
    outputPoints +=
      count === 0 ? 0 : contour.closed ? count * segments : 1 + (count - 1) * segments;
    if (outputPoints > 100000) throw Error('Subdivide Path exceeds geometry budget');
    for (const point of contour.points) {
      if (
        !point ||
        !Array.isArray(point.position) ||
        !Array.isArray(point.handleA) ||
        !Array.isArray(point.handleB)
      )
        throw Error('Subdivide Path needs valid points');
      assertValue('Vector2', point.position);
      assertValue('Vector2', point.handleA);
      assertValue('Vector2', point.handleB);
    }
  }

  const copy = () => ({
    ...source,
    contours: source.contours.map((contour, contourIndex) => ({
      ...contour,
      id: `${prefix}:c${contourIndex}`,
      points: contour.points.map((point, pointIndex) => ({
        ...point,
        id: `${prefix}:c${contourIndex}:p${pointIndex}`,
        position: [...point.position],
        handleA: [...point.handleA],
        handleB: [...point.handleB],
      })),
    })),
  });
  if (segments === 1) return copy();

  const item = toPaper(source);
  try {
    const contours = source.contours.map((contour, contourIndex) => {
      const sourceCount = contour.points.length,
        path = item.children[contourIndex],
        edgeCount = contour.closed ? sourceCount : Math.max(0, sourceCount - 1);
      if (edgeCount === 0) {
        const points = contour.points.map((point, pointIndex) => ({
          ...point,
          id: `${prefix}:c${contourIndex}:p${pointIndex}`,
          position: [...point.position],
          handleA: [...point.handleA],
          handleB: [...point.handleB],
        }));
        return { ...contour, id: `${prefix}:c${contourIndex}`, points };
      }
      if (path.curves.length !== edgeCount)
        throw Error('Subdivide Path could not resolve curve edges');
      const points = [];
      for (let edgeIndex = 0; edgeIndex < edgeCount; edgeIndex++) {
        const curve = path.curves[edgeIndex],
          sourceStart = contour.points[edgeIndex],
          sourceEnd = contour.points[(edgeIndex + 1) % sourceCount],
          keepLineHandles =
            sourceStart.handleB[0] === 0 &&
            sourceStart.handleB[1] === 0 &&
            sourceEnd.handleA[0] === 0 &&
            sourceEnd.handleA[1] === 0;
        for (let partIndex = 0; partIndex < segments; partIndex++) {
          const values = curve.getPart(partIndex / segments, (partIndex + 1) / segments).values;
          if (!Array.isArray(values) || values.length !== 8 || !values.every(Number.isFinite))
            throw Error('Subdivide Path exceeds finite coordinates');
          const start = [values[0], values[1]],
            control1 = [values[2], values[3]],
            control2 = [values[4], values[5]],
            end = [values[6], values[7]],
            startHandle = keepLineHandles
              ? [0, 0]
              : [control1[0] - start[0], control1[1] - start[1]],
            endHandle = keepLineHandles ? [0, 0] : [control2[0] - end[0], control2[1] - end[1]];
          let startPoint;
          if (points.length === 0) {
            startPoint = {
              ...sourceStart,
              id: `${prefix}:c${contourIndex}:p0`,
              position: start,
              handleA: [...sourceStart.handleA],
              handleB: startHandle,
            };
            points.push(startPoint);
          } else {
            startPoint = points[points.length - 1];
            startPoint.handleB = startHandle;
          }

          const isClosedSeam =
            contour.closed && edgeIndex === edgeCount - 1 && partIndex === segments - 1;
          if (isClosedSeam) {
            points[0].handleA = endHandle;
          } else {
            const pointIndex = points.length;
            points.push({
              ...(partIndex === segments - 1 ? sourceEnd : {}),
              id: `${prefix}:c${contourIndex}:p${pointIndex}`,
              position: end,
              handleA: endHandle,
              handleB:
                !contour.closed && edgeIndex === edgeCount - 1 && partIndex === segments - 1
                  ? [...sourceEnd.handleB]
                  : [0, 0],
            });
          }
        }
      }
      for (const point of points) {
        assertValue('Vector2', point.position);
        assertValue('Vector2', point.handleA);
        assertValue('Vector2', point.handleB);
      }
      return { ...contour, id: `${prefix}:c${contourIndex}`, points };
    });
    if (contours.reduce((sum, contour) => sum + contour.points.length, 0) !== outputPoints)
      throw Error('Subdivide Path exceeded its geometry estimate');
    return { ...source, contours };
  } finally {
    item.remove();
  }
}

function dashPathGeometry(source, options) {
  dashIntervals(0, options);
  if (!source?.contours) throw Error('Connect a Path input');
  if (
    source.contours.length > 128 ||
    source.contours.reduce((sum, c) => sum + c.points.length, 0) > 2048
  )
    throw Error('Dash Path exceeds input geometry budget');
  if (options.gapLength === 0) return clone(source);
  const item = toPaper(source),
    contours = [];
  let segments = 0,
    work = 0,
    points = 0;
  try {
    for (const [ci, c] of source.contours.entries()) {
      const length = item.children[ci].length;
      const intervals = dashIntervals(length, options);
      segments += intervals.length;
      work += intervals.length * c.points.length;
      if (segments > 512 || work > 200000) throw Error('Dash Path exceeds segment/work budget');
      let pieces = intervals.map(([a, b], i) => {
        const piece = trimPathGeometry(
          { contours: [c] },
          {
            start: a / length,
            end: b / length,
            prefix: `${options.prefix}:c${ci}:d${i}`,
          },
        ).contours[0];
        if (!piece?.points.length)
          throw Error('Dash Path interval is too small at this coordinate scale');
        return piece;
      });
      // A dash crossing a closed contour's stored seam remains one open segment.
      if (
        c.closed &&
        pieces.length > 1 &&
        intervals[0][0] === 0 &&
        intervals.at(-1)[1] === length
      ) {
        const first = pieces[0],
          last = pieces.at(-1);
        last.points.at(-1).handleB = [...first.points[0].handleB];
        last.points.push(...first.points.slice(1));
        pieces = [last, ...pieces.slice(1, -1)];
      }
      for (const [di, piece] of pieces.entries()) {
        points += piece.points.length;
        if (points > 100000) throw Error('Dash Path exceeds output geometry budget');
        piece.id = `${options.prefix}:c${ci}:d${di}`;
        piece.points.forEach((p, pi) => {
          p.id = `${piece.id}:p${pi}`;
        });
        contours.push(piece);
      }
    }
    return { contours };
  } finally {
    item.remove();
  }
}

export function parsePath(data, prefix = 'imported') {
  if (
    typeof data !== 'string' ||
    data.length > 100000 ||
    !/^[MmLlHhVvCcSsQqTtAaZzEe0-9+.,\s-]*$/.test(data)
  )
    throw Error('Unsupported SVG path data');
  const item = new scope.CompoundPath({ pathData: data, insert: false });
  try {
    const result = fromPaper(item, prefix);
    for (const c of result.contours)
      for (const pt of c.points)
        for (const k of ['position', 'handleA', 'handleB']) assertValue('Vector2', pt[k]);
    return result;
  } finally {
    item.remove();
  }
}
export function pathData(path, precision) {
  const obj = toPaper(path);
  try {
    if (precision === undefined) return obj.pathData;
    for (const source of obj.children) {
      const roundedData = source.getPathData(null, precision),
        rounded = new scope.Path({ pathData: roundedData, insert: false });
      try {
        const before = source.bounds,
          after = rounded.bounds;
        if (
          (before.width > 0 && after.width === 0) ||
          (before.height > 0 && after.height === 0) ||
          (source.closed && Math.abs(source.area) > 0 && Math.abs(rounded.area) === 0)
        )
          throw Error(
            `SVG precision ${precision} decimals collapses small geometry; choose a higher precision or Default`,
          );
      } finally {
        rounded.remove();
      }
    }
    return obj.getPathData(null, precision);
  } finally {
    obj.remove();
  }
}
export function pathBounds(path) {
  if (path.kind === 'image') {
    const [x, y] = path.position,
      [w, h] = path.size;
    return [
      [x, x + w / 2, x + w],
      [y, y + h / 2, y + h],
    ];
  }
  const obj = toPaper(path);
  const { left, right, top, bottom } = obj.bounds;
  obj.remove();
  return [
    [left, (left + right) / 2, right],
    [top, (top + bottom) / 2, bottom],
  ];
}
export function pathArea(path) {
  const obj = booleanOperand(path);
  obj.reorient(true, true);
  const area = Math.abs(obj.area);
  obj.remove();
  return area;
}
const evaluationCaches = new WeakMap();
let revision = 0;
export function evaluate(
  layer,
  fonts = {},
  imageAssets = {},
  documentContext = null,
  compositionContext = null,
  lazy = false,
) {
  const context = compositionContext || {
    frames: new Map(),
    activeNodes: new Set(),
    readingParameters: new Set(),
    depth: 0,
  };
  const frameFor = (target, targetDocument = documentContext) =>
    context.frames.get(target.id) ||
    evaluate(target, fonts, imageAssets, targetDocument, context, true);
  function referencedOutput(n) {
    const targetId = n.compositionReference?.compositionId;
    if (!targetId)
      throw Object.assign(Error('Choose a source Composition'), {
        code: 'COMPOSITION_TARGET_UNSET',
      });
    if (!documentContext) throw Error('Composition reference requires its document');
    if (targetId === layer.id) throw Error('Composition reference cycle detected');
    const target = documentContext.layers.find((l) => l.id === targetId);
    if (!target)
      throw Object.assign(Error(`Missing Composition: ${targetId}`), {
        code: 'COMPOSITION_TARGET_MISSING',
      });
    const result = frameFor(target);
    result.ensureOutput();
    if (result.error)
      throw Object.assign(Error(`${target.name}: ${result.error}`), { code: result.errorCode });
    warnings.push(...result.warnings.map((message) => `${target.name}: ${message}`));
    return result;
  }
  let cache = evaluationCaches.get(layer);
  if (!cache) {
    cache = new Map();
    evaluationCaches.set(layer, cache);
  }
  const values = new Map(),
    warnings = [],
    active = context.activeNodes,
    layerActive = new Set(),
    versions = new Map(),
    textLayouts = new Map();
  let computed = 0,
    reused = 0;
  const byId = new Map(layer.nodes.map((n) => [n.id, n]));
  let storedGraphError = null;
  if (layer.nodes.some((n) => n.bypassed)) {
    try {
      // A bypass skips computation, never structural cycle/type validation.
      assertAcyclic(layer);
      for (const n of layer.nodes) {
        bypassInput(n, layer);
        for (const [key, q] of parameters(n))
          if (q.source && !socketCompatibleFor(byId.get(q.source), n, key, q.sourcePort))
            throw Error('Incompatible socket types');
      }
    } catch (error) {
      storedGraphError = error;
    }
  }
  // Source identities matter even when two graphs produce identical coordinates.
  const provenances = [new Map(), new Map()];
  function provenance(id, port, currentEra = false) {
    const identities = provenances[currentEra ? 1 : 0];
    const key = outputKey(id, port);
    if (!identities.has(key)) {
      const n = byId.get(id),
        expressions = (key !== id ? [['position', n.params.position]] : evaluatedParameters(n))
          .filter(([, q]) => q.expression && !q.source)
          .map(([input, q]) => [input, expressionReferences(q.expression)]),
        deps = (key !== id ? [['position', n.params.position]] : evaluatedParameters(n)).filter(
          ([, q]) => q.source,
        ),
        reference = key === id && referenceDependency(n);
      if (reference) deps.push(['reference', reference]);
      if (key === id && n.pathAttachment)
        deps.push(['textPath', { source: n.pathAttachment.nodeId }]);
      identities.set(
        key,
        fingerprint(
          JSON.stringify([
            n.id,
            n.type,
            ...(n.bypassed ? ['bypassed'] : []),
            key !== id ? 'position' : null,
            // Reference-free expressions depend only on literals/base values, already
            // represented by evaluated geometry/settings. Preserve their historical
            // identity too, without dropping any real reference from either recipe.
            ...(currentEra || expressions.some(([, refs]) => refs.length) ? [expressions] : []),
            deps.map(([input, q]) => [
              input,
              q.source,
              q.sourcePort,
              provenance(q.source, q.sourcePort, currentEra),
            ]),
          ]),
        ),
      );
    }
    return identities.get(key);
  }
  const offsetComputations = new Set();
  // Separate shares one vector evaluation. Text's value output has its own cache entry
  // and dependencies, so reading Position never requires font/outline generation.
  const outputKey = (id, port) =>
    byId.get(id)?.type === 'Text' && port === 'position' ? `${id}\0position` : id;
  const readingParameters = context.readingParameters;
  const layerReadingParameters = new Set();
  const frameParameterIndexes = new WeakMap();
  function resolveExpressionReference(ref, type = 'Scalar') {
    if (
      ref.qualifier &&
      (type !== 'Color' ||
        documentContext?.schemaVersion < QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
    )
      throw Error(
        `Qualified Color references require a Color parameter and schemaVersion ${QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION}`,
      );
    if (ref.compositionId) {
      if (
        ref.componentComposition &&
        documentContext &&
        documentContext.schemaVersion < COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION
      )
        throw Error(
          `Component Composition references require schemaVersion ${COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION}`,
        );
      const binding = documentContext && projectDocumentContexts.get(documentContext);
      if (!binding) throw Error('Composition property reference requires project context');
      if (ref.componentComposition) {
        if (!binding.layerCompositionIds)
          binding.layerCompositionIds = new Map(
            binding.project.compositions.flatMap((composition) =>
              composition.layers.map((item) => [item.id, composition.id]),
            ),
          );
        const currentCompositionId =
          binding.layerCompositionIds.get(layer.id) || binding.activeCompositionId;
        if (ref.compositionId === currentCompositionId)
          throw Error('Component Composition reference requires a different outer Composition');
      }
      if (documentContext.schemaVersion < COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION)
        throw Error(
          `Composition property references require schemaVersion ${COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION}`,
        );
      const sourceDocument = projectFrameDocument(binding, ref.compositionId, context);
      const sourceLayer = sourceDocument?.layers.find((item) => item.id === ref.layerId);
      if (!sourceLayer)
        throw Object.assign(
          Error(
            `Expression reference Composition or artwork is missing: ${ref.compositionId}/${ref.layerId}`,
          ),
          { code: 'EXPRESSION_REFERENCE_MISSING' },
        );
      if (
        (ref.qualifier ? type !== 'Color' : type !== 'Scalar') ||
        (ref.component !== undefined && !ref.componentComposition)
      )
        throw Error(
          ref.qualifier
            ? 'Qualified Color references require a Color property'
            : 'Composition references require a Scalar property',
        );
      const target = referenceParameter(sourceLayer, ref, type);
      return projectReferenceValue(
        ref,
        frameFor(sourceLayer, sourceDocument).parameterValue(target.id, ref.key),
      );
    }
    if (
      ref.layerId &&
      documentContext &&
      (!Number.isInteger(documentContext.schemaVersion) ||
        documentContext.schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION)
    )
      throw Error(`Artwork references require schemaVersion ${ARTWORK_REFERENCE_SCHEMA_VERSION}`);
    const sourceLayer = ref.layerId
      ? documentContext?.layers.find((item) => item.id === ref.layerId)
      : layer;
    if (!sourceLayer)
      throw Object.assign(Error(`Expression reference artwork is missing: ${ref.layerId}`), {
        code: 'EXPRESSION_REFERENCE_MISSING',
      });
    const target = referenceParameter(sourceLayer, ref, type),
      value =
        sourceLayer === layer
          ? readParameter(target, ref.key)
          : frameFor(sourceLayer).parameterValue(target.id, ref.key);
    return projectReferenceValue(ref, value);
  }
  function indexParameters(n) {
    let indexed = frameParameterIndexes.get(n);
    if (indexed) return indexed;
    indexed = new Map();
    // A frame is an immutable snapshot. Match parameter()'s first entry even
    // for invalid duplicates and discard the index with this evaluation.
    for (const [key, value] of parameters(n)) if (!indexed.has(key)) indexed.set(key, value);
    frameParameterIndexes.set(n, indexed);
    return indexed;
  }
  function readParameter(n, key, indexed) {
    const id = `${layer.id}\0${n.id}\0${key}`;
    if (readingParameters.has(id)) throw Error('Expression reference cycle detected');
    if (layerReadingParameters.size >= 256) throw Error('Expression reference depth exceeds 256');
    if (context.depth >= 512) throw Error('Evaluation depth exceeds the supported limit');
    readingParameters.add(id);
    layerReadingParameters.add(id);
    context.depth++;
    try {
      return readParameterValue(n, key, indexed);
    } finally {
      readingParameters.delete(id);
      layerReadingParameters.delete(id);
      context.depth--;
    }
  }
  // Preserve only exact authored identity, never infer it from a display value.
  function parameterColorSlot(q, seen = new Set()) {
    if (!q || q.type !== 'Color') return null;
    if (q.colorSlotId !== undefined) return q.colorSlotId;
    if (q.source) {
      if (seen.has(q.source)) return null;
      seen.add(q.source);
      const source = byId.get(q.source);
      return source?.type === 'Color' ? parameterColorSlot(source.params.value, seen) : null;
    }
    return null; // Literal and expression-generated colors have no exact slot attribution.
  }
  function readParameterValue(n, key, indexed) {
    if (isTransform(n) && !n.params[key] && key === 'position') return readParameter(n, 'anchor');
    if (isTransform(n) && !n.params[key] && key === 'scaleWidth') return readParameter(n, 'scale');
    let q = (indexed || indexParameters(n)).get(key) || parameterFallback(n, key);
    if (!q) throw Error(`Missing parameter ${key}`);
    let value =
      q.colorSlotId !== undefined
        ? resolveColorSlot(documentContext, q.colorSlotId)
        : q.source
          ? run(q.source, q.sourcePort)
          : q.expression
            ? parameterExpression(
                q,
                resolveExpressionReference,
                parameterBaseValue(layer, n, key, q),
              )
            : n.type === 'Fill' && key === 'color' && q.value === null
              ? { hex: layer.fill, alpha: 1 }
              : clone(q.value);
    if (q.source && !socketCompatibleFor(byId.get(q.source), n, key, q.sourcePort))
      throw Error('Incompatible socket types');
    assertValue(q.type, value);
    if (['Path', 'PaintedPath'].includes(q.type)) assertRepeaterRoute(value, n);
    return value;
  }
  function run(id, port) {
    if (storedGraphError) throw storedGraphError;
    const n = byId.get(id);
    if (!n || !outputType(n, port)) throw Error('Missing output socket');
    const value = runNode(id, port);
    const registered = builtinNodeRegistry.get(n.type);
    if (!registered) return value;
    const output = registered.declaration.outputs.find((item) => nectOutputPort(item) === port);
    const projected = registered.project(output.id, value);
    assertValue(outputType(n, port), projected);
    return projected;
  }
  function runNode(id, port) {
    const key = outputKey(id, port),
      positionOutput = key !== id;
    if (values.has(key)) return values.get(key);
    const activeKey = `${layer.id}\0${key}`;
    if (active.has(activeKey)) throw Error('Cycle detected');
    if (layerActive.size >= 256) throw Error('Graph depth exceeds 256 nodes');
    if (context.depth >= 512) throw Error('Evaluation depth exceeds the supported limit');
    let n = byId.get(id);
    if (!n) throw Error('Missing source node');
    active.add(activeKey);
    layerActive.add(activeKey);
    context.depth++;
    try {
      const inputs = positionOutput ? [['position', n.params.position]] : evaluatedParameters(n);
      const connected = new Set();
      const deps = positionOutput
        ? parameterDependencies(inputs, connected)
        : collectNodeDependencies(n, true, connected);
      if (n.type === 'Reference') referenceTarget(layer, n);
      const external = ['Composition Reference', 'SVG'].includes(n.type)
        ? referencedOutput(n)
        : null;
      const parameterVersions = new Map();
      for (const dep of deps) {
        if (dep.parameterKey && !connected.has(dep))
          parameterVersions.set(
            dep,
            resolveExpressionReference(
              {
                nodeId: dep.source,
                ...(dep.layerId && !connected.has(dep) ? { layerId: dep.layerId } : {}),
                ...(dep.compositionId && !connected.has(dep)
                  ? { compositionId: dep.compositionId }
                  : {}),
                ...(dep.qualifier && !connected.has(dep) ? { qualifier: dep.qualifier } : {}),
                ...(dep.componentComposition && !connected.has(dep)
                  ? { componentComposition: true }
                  : {}),
                key: dep.parameterKey,
                component: dep.component,
              },
              dep.parameterType,
            ),
          );
        else run(dep.source, dep.sourcePort);
      }
      const dependencyVersions = deps.map((q) => [
        q.source,
        q.sourcePort,
        q.parameterKey && !connected.has(q)
          ? [q.parameterKey, q.component, parameterVersions.get(q)]
          : versions.get(outputKey(q.source, q.sourcePort)),
      ]);
      const cacheFont = positionOutput ? null : fonts[n.font];
      const styleFonts = positionOutput
        ? []
        : [
            ...new Set(
              Object.values(n.characterPositions?.styles || {})
                .map((style) => style.font)
                .filter(Boolean),
            ),
          ]
            .sort()
            .map((id) => fonts[id]);
      const imageAsset = n.type === 'Image' ? imageAssets[n.asset] : undefined,
        cacheKey = JSON.stringify(
          positionOutput
            ? [
                inputs,
                dependencyVersions,
                documentContext?.colorSlots,
                isLiveKeyColorRecipe(documentContext || {}) ? documentContext.keyColorRecipe : null,
              ]
            : [
                n.type,
                documentContext?.colorSlots,
                isLiveKeyColorRecipe(documentContext || {}) ? documentContext.keyColorRecipe : null,
                !!n.bypassed,
                n.reference,
                n.compositionReference,
                n.paintMode,
                external?.outputRevision,
                inputs,
                inputs.some(([, q]) => q.type === 'Color' && usesColorOperations(q.expression))
                  ? COLOR_OPERATIONS_VERSION
                  : null,
                n.type === 'Fill' ? layer.fill : null,
                n.type === 'Output' ? [layer.output, layer.allowEmptyOutput] : null,
                n.points,
                n.operation,
                n.edits,
                n.text,
                n.characterPositions,
                n.characterArrangement,
                n.pathAttachment,
                n.font,
                n.variation,
                n.osFont,
                n.writingMode,
                n.alignment,
                n.referenceBounds,
                n.crossAlignment,
                n.positionReference,
                n.kerning,
                n.leading,
                n.textWrap,
                n.strokeCap,
                n.strokeJoin,
                n.strokeDash,
                n.rawPath,
                n.asset,
                n.asset && imageAssets[n.asset]
                  ? [imageAssets[n.asset].width, imageAssets[n.asset].height]
                  : null,
                dependencyVersions,
              ],
        );
      if (!n.bypassed && n.type === 'Closed Path Offset' && readParameter(n, 'amount') !== 0) {
        offsetComputations.add(n.id);
        if (offsetComputations.size > 8)
          throw new OffsetError(
            'OFFSET_LIMIT',
            'At most 8 nonzero Offset nodes per evaluated graph; simplify the graph',
          );
      }
      if (n.type === 'Text' && cacheFont?.nectVariable && !positionOutput) {
        validateVariableText(n, { kind: VARIABLE_KIND });
        cacheFont.readVariable({
          text: n.text,
          axes: canonicalAxes(cacheFont.nectVariable.axes, n.variation.axes).sparse,
        });
      }
      const existing = cache.get(key);
      if (
        existing &&
        existing.key === cacheKey &&
        existing.font === cacheFont &&
        (n.type !== 'Image' || existing.imageAsset === imageAsset) &&
        styleFonts.length === (existing.styleFonts || []).length &&
        styleFonts.every((font, i) => font === existing.styleFonts[i])
      ) {
        reused++;
        const result =
          !n.bypassed && n.type === 'Closed Path Offset' && readParameter(n, 'amount') === 0
            ? readParameter(n, 'path')
            : cloneEvaluated(existing.value);
        values.set(key, result);
        versions.set(key, existing.version);
        warnings.push(...existing.warnings);
        if (!positionOutput && existing.textLayout) textLayouts.set(id, clone(existing.textLayout));
        return result;
      }
      const warningStart = warnings.length;
      computed++;
      const read = (key, indexed) => readParameter(n, key, indexed);
      let result;
      if (n.bypassed) {
        result = cloneEvaluated(read(bypassInput(n)));
        if (result === null) throw Error(`Connect ${bypassInput(n)} to bypass ${n.type}`);
        if (n.type === 'Mask') result = evaluatedPaintedGroup([result], { prefix: n.id });
        assertValue(outputType(n), result);
      } else if (['Composition Reference', 'SVG'].includes(n.type)) {
        const position = read('position');
        result = evaluatedPaintedGroup([external.path], {
          matrix: [1, 0, 0, 1, ...position],
          prefix: n.id,
        });
        setEvaluationFingerprint(result, (currentEra) => [
          n.compositionReference,
          n.id,
          position,
          external.path,
        ]);
      } else if (n.type === 'Reference') {
        result = clone(run(n.reference.nodeId, referencePort(n.reference)));
        assertValue('Vector2', result);
      } else if (positionOutput) result = read('position');
      else if (n.type === 'Vector Transform')
        result = transformVector(transformValues(n, read), read('vector'));
      else if (builtinNodeRegistry.get(n.type)) {
        const registered = builtinNodeRegistry.get(n.type);
        const nodeId = n.id;
        const warn = (message) => warnings.push(`${n.name}: ${message}`);
        const shape = registered.hasShape ? registered.snapshot(n) : undefined;
        const context = shape
          ? Object.freeze({
              nodeId,
              shape,
              warn,
              paintedGroup: (inputs) => {
                const painted = evaluatedPaintedGroup(inputs, { prefix: nodeId });
                setEvaluationFingerprint(painted, () => [nodeId, inputs]);
                return painted;
              },
            })
          : Object.freeze({ nodeId, warn });
        result = registered.evaluate(n.operation, read, context);
        assertValue(registered.interface(n.operation, shape).resultType, result);
      } else if (n.type === 'Polygon' || n.type === 'Star') {
        result = radialPath(n, read);
      } else if (n.type === 'Circle' || n.type === 'Ellipse' || n.type === 'Rectangle') {
        const center = read('center');
        let item;
        if (n.type === 'Circle')
          item = new scope.Path.Circle({
            center,
            radius: Math.max(0, read('radius')),
            insert: false,
          });
        else {
          let size = read('size').map(Math.abs),
            rectangle = new scope.Rectangle(
              center[0] - size[0] / 2,
              center[1] - size[1] / 2,
              ...size,
            );
          item =
            n.type === 'Ellipse'
              ? new scope.Path.Ellipse({ rectangle, insert: false })
              : new scope.Path.Rectangle({ rectangle, insert: false });
        }
        result = fromPaper(item, n.id);
        item.remove();
      } else if (n.type === 'Image') {
        if (!Object.hasOwn(imageAssets, n.asset)) throw Error('Missing image asset');
        const size = read('size');
        if (size.some((v) => v <= 0 || v > 100000))
          throw Error('Image size must be positive and at most 100000');
        result = { kind: 'image', asset: n.asset, position: read('position'), size };
      } else if (n.type === 'Keyer') {
        result = keyedImage(
          read('path'),
          read('keyColor'),
          read('tolerance'),
          read('softness'),
          imageAssets,
        );
      } else if (isImageDither(n.type)) {
        const { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read);
        result = ditheredImage(
          read('path'),
          {
            mode: n.type === 'Ordered Dither Image' ? 'dither' : 'error-diffusion',
            pitch,
            invert,
            origin: { x: gridOrigin[0], y: gridOrigin[1] },
            tonePower,
            ...readDotSettings(n, read),
          },
          imageAssets,
        );
      } else if (n.type === 'Mask') {
        const source = read('path'),
          shape = read('mask');
        if (!source || !shape) throw Error('Connect appearance and closed Path or Image to Mask');
        result = maskAppearance(source, shape, {
          invert: read('invert'),
          feather: read('feather'),
          luminance: read('luminance'),
          prefix: n.id,
          bounds: pathBounds(source),
        });
        copyEvaluationIdentity(source, result.children[0]);
      } else if (n.type === 'Curve') {
        const indexed = indexParameters(n);
        result = {
          contours: [
            {
              id: n.id,
              closed: read('closed'),
              points: n.points.map((pt) => ({
                id: pt.id,
                position: read(`${pt.id}.position`, indexed),
                handleA: read(`${pt.id}.handleA`, indexed),
                handleB: read(`${pt.id}.handleB`, indexed),
              })),
            },
          ],
        };
      } else if (n.type === 'Multi Path') {
        const indexed = indexParameters(n);
        const points = new Map(
          n.points.map((pt) => [
            pt.id,
            {
              id: pt.id,
              position: read(`${pt.id}.position`, indexed),
              handleA: read(`${pt.id}.handleA`, indexed),
              handleB: read(`${pt.id}.handleB`, indexed),
            },
          ]),
        );
        result = {
          contours: n.contours.map((contour) => ({
            ...contour,
            points: contour.points.map((id) => points.get(id)),
          })),
        };
        // Preserve Text's fingerprint contract for existing downstream Remap edits.
        result.unstable = true;
        result.fingerprint = signature(result);
      } else if (n.type === 'Text' && n.osFont) {
        result = osFontTextPath(n, read('position'), read('size'), pathBounds);
        if (n.osFont.outline.wrapOverflow)
          warnings.push(`${n.name}: Wrap overflow; all artwork is retained without clipping`);
        const reason =
          'OS font Text character adjustments and canvas caret editing are not supported; edit Text in Inspector';
        textLayouts.set(n.id, {
          outlineUnits: [{ id: `${n.id}:osu0`, contours: result.contours.map((c) => c.id) }],
          characters: [],
          glyphAdvances: {},
          placements: [],
          lines: [],
          pairKerning: {},
          arrangementEligibility: { eligible: false, reason },
          reason,
        });
        result.characterOccurrences = [
          { source: n.id, route: n.id, matrix: identityMatrix(), reason },
        ];
        result.unstable = true;
        result.fingerprint = signature(result);
      } else if (n.type === 'Text') {
        let font = fonts[n.font];
        if (!font) throw Error('Font is still loading or unavailable');
        const attachment = n.pathAttachment;
        validateTextWrap(n.textWrap, n.writingMode, attachment);
        if (attachment) {
          validateTextPathAttachment(attachment);
          validateTextPathMode(n.text, n.writingMode);
        }
        let layout = layoutText(font, n.text, {
          position: attachment ? [0, 0] : read('position'),
          characterPositions: n.characterPositions,
          characterArrangement: n.characterArrangement,
          pathAttachment: attachment,
          fonts,
          fontId: n.font,
          variation: n.variation,
          fontFeatures: n.fontFeatures,
          size: read('size'),
          writingMode: n.writingMode,
          alignment: attachment ? 'start' : n.alignment,
          referenceBounds: attachment ? 'advance' : n.referenceBounds,
          crossAlignment: attachment ? 'origin' : n.crossAlignment,
          kerning: n.kerning,
          leading: n.leading,
          textWrap: n.textWrap,
          letterSpacing: n.params.letterSpacing ? read('letterSpacing') : 0,
          lineSpacing: n.params.lineSpacing ? read('lineSpacing') : 1.2,
        });
        if (attachment) {
          const sourceNode = byId.get(attachment.nodeId);
          if (!sourceNode || outputType(sourceNode) !== 'Path')
            throw Error('TEXT_PATH_SOURCE: Select a Path output in the same layer');
          const source = run(attachment.nodeId);
          if (source.unstable)
            throw Error(
              'TEXT_PATH_UNSTABLE: Generated contour identities are unsupported; use a Curve or stable primitive',
            );
          if (source.remapSuspended)
            throw Error('TEXT_PATH_REMAP: Rebind suspended Path Remap edits before attaching');
          const contour = source.contours.find((c) => c.id === attachment.contourId);
          if (!contour)
            throw Error(
              'TEXT_PATH_CONTOUR: Selected contour is missing; detach or choose an explicit contour',
            );
          layout = projectTextPath(layout, contour, attachment, n.alignment || 'start');
        }
        warnings.push(...layout.warnings.map((warning) => `${n.name}: ${warning}`));
        const d = layout.path.toPathData(5);
        let item = new scope.CompoundPath({ pathData: d, insert: false });
        result = fromPaper(item, n.id);
        // Glyph command ranges are layout provenance, not character/contour guesses.
        // Keep grouping separate from paint groups to preserve overlapping fills.
        const unitCounts = layout.glyphs.map(
          (glyph) =>
            layout.path.commands
              .slice(glyph.commandStart, glyph.commandEnd)
              .filter((command) => command.type === 'M').length,
        );
        let contourOffset = 0;
        const outlineUnits =
          unitCounts.reduce((sum, count) => sum + count, 0) === result.contours.length
            ? unitCounts.flatMap((count, index) => {
                const contours = result.contours
                  .slice(contourOffset, contourOffset + count)
                  .map((c) => c.id);
                contourOffset += count;
                return count ? [{ id: `${n.id}:u${index}`, contours }] : [];
              })
            : [{ id: `${n.id}:u0`, contours: result.contours.map((c) => c.id) }];
        textLayouts.set(n.id, {
          outlineUnits,
          arrangementStatus: layout.arrangementStatus,
          arrangementEligibility: layout.arrangementEligibility,
          characters: layout.characters,
          glyphAdvances: Object.fromEntries(
            layout.glyphs.map((glyph) => [glyph.sourceIndex, glyph.advance]),
          ),
          attachment: layout.attachment,
          placements: layout.placements,
          lineOffsets: layout.lineOffsets,
          lines: layout.lines,
          pairKerning: layout.pairKerning,
          reason: layout.characterReason,
          suspended: layout.characterSuspended,
        });
        result.characterOccurrences = [
          {
            source: n.id,
            route: n.id,
            matrix: identityMatrix(),
            reason:
              layout.characterReason ||
              (attachment
                ? 'Canvas character dragging is unavailable on Type on Path; use numeric local XY/rotation/scale'
                : null),
            arrangementStatus: layout.arrangementStatus,
            suspended: layout.characterSuspended,
          },
        ];
        result.unstable = true;
        result.fingerprint = signature(result);
        item.remove();
      } else if (n.type === 'Transform' && n.paintMode) {
        const source = read('path');
        if (!source) throw Error('Connect a PaintedPath input');
        result = evaluatedPaintedGroup([source], {
          matrix: transformValues(n, read).matrix,
          prefix: n.id,
        });
        setEvaluationFingerprint(result, (currentEra) => [n.id, source, result.matrix]);
      } else if (n.type === 'Dash Path') {
        const source = read('path');
        result = dashPathGeometry(source, {
          dashLength: read('dashLength'),
          gapLength: read('gapLength'),
          offset: read('offset'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Dash Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Trim Path') {
        const source = read('path'),
          start = read('start'),
          end = read('end');
        result = trimPathGeometry(source, { start, end, prefix: fingerprint(n.id) });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Trim Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Set Path Start') {
        const source = read('path'),
          start = read('start');
        result = setPathStartGeometry(source, { start, prefix: fingerprint(n.id) });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Set Path Start; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Set Closure') {
        const source = read('path'),
          closed = read('closed');
        result = setClosureGeometry(source, { closed, prefix: fingerprint(n.id) });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Set Closure; edit the upstream Text',
        );
        result.unstable = true;
        setEvaluationFingerprint(result, (currentEra) => [
          provenance(n.id, undefined, currentEra),
          closed,
          evaluatedFingerprint(source, currentEra) || signature(source),
          signature(result),
        ]);
      } else if (n.type === 'Subdivide Path') {
        const source = read('path');
        result = subdividePathGeometry(source, {
          segments: read('segments'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Subdivide Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Resample Path') {
        const source = read('path');
        if (!source) throw Error('Connect a Path input');
        if (!operationOptions['Resample Path'].includes(n.operation))
          throw Error('Invalid Resample operation');
        const count = n.operation === 'count' ? read('count') : null,
          spacing = n.operation === 'spacing' ? read('spacing') : null,
          sourcePoints = source.contours.reduce((sum, c) => sum + c.points.length, 0);
        if (n.operation === 'count') {
          if (!Number.isInteger(count) || count < 2 || count > 1024)
            throw Error('Resample count must be an integer from 2 to 1024');
          if (count < 3 && source.contours.some((c) => c.closed))
            throw Error('Closed contours require at least 3 samples');
        } else if (!Number.isFinite(spacing) || spacing < 0.0001 || spacing > 100000)
          throw Error('Resample spacing must be from 0.0001 to 100000 px');
        if (
          source.contours.length > 10000 ||
          sourcePoints > 100000 ||
          (n.operation === 'count' &&
            (source.contours.length * count > 100000 || sourcePoints * count > 2000000))
        )
          throw Error('Resample exceeds geometry budget');
        const item = toPaper(source),
          prefix = fingerprint(n.id);
        try {
          const plans = source.contours.map((c, ci) => {
            const path = item.children[ci],
              length = path.length;
            if (!Number.isFinite(length)) throw Error('Resample exceeds finite path length');
            const samples =
              n.operation === 'count'
                ? count
                : c.points.length === 0
                  ? 0
                  : c.closed
                    ? Math.max(3, Math.ceil(length / spacing))
                    : Math.max(2, Math.ceil(length / spacing) + 1);
            if (
              c.points.length > 0 &&
              (!Number.isInteger(samples) || samples < (c.closed ? 3 : 2) || samples > 1024)
            )
              throw Error('Resample spacing requires more than 1024 samples for a contour');
            return { path, length, samples };
          });
          const outputPoints = plans.reduce((sum, plan) => sum + plan.samples, 0),
            work = plans.reduce(
              (sum, plan, index) => sum + source.contours[index].points.length * plan.samples,
              0,
            );
          if (outputPoints > 100000 || work > 2000000)
            throw Error('Resample exceeds geometry budget');
          result = {
            contours: source.contours.map((c, ci) => {
              const { path, length, samples } = plans[ci];
              return {
                id: `${prefix}:c${ci}`,
                closed: c.closed,
                ...(c.group !== undefined ? { group: c.group } : {}),
                points: c.points.length
                  ? Array.from({ length: samples }, (_, pi) => {
                      const distance = length * (pi / (c.closed ? samples : samples - 1));
                      const sampled = path.getPointAt(distance);
                      const position = sampled
                        ? [sampled.x, sampled.y]
                        : length === 0
                          ? [...c.points[0].position]
                          : null;
                      assertValue('Vector2', position);
                      return {
                        id: `${prefix}:c${ci}:p${pi}`,
                        position,
                        handleA: [0, 0],
                        handleB: [0, 0],
                      };
                    })
                  : [],
              };
            }),
          };
        } finally {
          item.remove();
        }
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Resample Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Geometry Repeater') {
        const source = read('path');
        if (!source) throw Error('Connect a Path input');
        if (n.operation === 'ae-2d') {
          result = repeatGeometry(source, { ...repeaterSettings(read), prefix: fingerprint(n.id) });
        } else {
          const count = read('count');
          if (!Number.isInteger(count) || count < 1 || count > 256)
            throw Error('Repeater count must be an integer from 1 to 256');
          if (
            source.contours.length * count > 10000 ||
            source.contours.reduce((sum, c) => sum + c.points.length, 0) * count > 100000
          )
            throw Error('Repeater exceeds geometry budget');
          const grouped = source.contours.some((c) => c.group !== undefined);
          const groups = grouped
            ? new Map(contourGroups(source).map((group, i) => [group[0].group ?? '', i]))
            : null;
          if (grouped && groups.size * count > 256)
            throw Error('Repeater exceeds 256 independent groups');
          const linear = n.operation === 'linear';
          if (!operationOptions['Geometry Repeater'].includes(n.operation))
            throw Error('Invalid repeater operation');
          const step = linear ? read('step') : [0, 0];
          const center = linear ? [0, 0] : read('center');
          const start = linear ? 0 : read('startAngle');
          const angle = linear ? 0 : read('angle');
          if (n.operation === 'radial-fit' && Math.abs(angle) > 360)
            throw Error('Fit arc must be between -360 and 360 degrees');
          const increment =
            n.operation === 'radial-fit'
              ? angle / (Math.abs(angle) === 360 ? count : Math.max(1, count - 1))
              : angle;
          result = { contours: [] };
          const prefix = fingerprint(n.id);
          for (let copy = 0; copy < count; copy++) {
            const radians = (((start + increment * copy) % 360) * Math.PI) / 180;
            const cos = Math.cos(radians),
              sin = Math.sin(radians);
            function transform(v, handle = false) {
              const x = v[0] - (handle ? 0 : center[0]),
                y = v[1] - (handle ? 0 : center[1]);
              const next = [
                x * cos - y * sin + (handle ? 0 : center[0] + step[0] * copy),
                x * sin + y * cos + (handle ? 0 : center[1] + step[1] * copy),
              ];
              assertValue('Vector2', next);
              return next;
            }
            for (const [ci, c] of source.contours.entries())
              result.contours.push({
                id: `${prefix}:copy${copy}:c${ci}`,
                ...(grouped
                  ? { group: `${prefix}:copy${copy}:g${groups.get(c.group ?? '')}` }
                  : {}),
                closed: c.closed,
                points: c.points.map((pt, pi) => ({
                  id: `${prefix}:copy${copy}:c${ci}:p${pi}`,
                  position: transform(pt.position),
                  handleA: transform(pt.handleA, true),
                  handleB: transform(pt.handleB, true),
                })),
              });
          }
        }
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Geometry Repeater',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Luminance to Points') {
        const source = read('path');
        const { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read);
        const settings = luminanceInstanceSettings(read);
        result = luminanceToPoints(source, {
          ...settings,
          pitch,
          invert,
          tonePower,
          origin: { x: gridOrigin[0], y: gridOrigin[1] },
          prefix: fingerprint(n.id),
          asset: source?.kind === 'image' ? imageAssetFor(source, imageAssets) : undefined,
        });
        result.unstable = true;
        setEvaluationFingerprint(result, (currentEra) => [
          provenance(n.id, undefined, currentEra),
          evaluatedFingerprint(source, currentEra) || null,
          settings,
          pitch,
          invert,
          gridOrigin,
          tonePower,
          result.contours,
        ]);
      } else if (n.type === 'Luminance Instances') {
        if (!n.params.path.source || !n.params.motif.source)
          throw Error('Connect both Source fill and Motif inputs');
        const source = read('path'),
          motif = read('motif');
        if (!source || !motif) throw Error('Connect both Source fill and Motif inputs');
        const motifInfo = validateInstancePath(motif, 'Luminance Instances motif');
        const settings = luminanceInstanceSettings(read),
          { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read);
        const options = {
          pitch,
          invert,
          tonePower,
          origin: { x: gridOrigin[0], y: gridOrigin[1] },
        };
        let field, mask;
        if (source.kind === 'image') {
          field = sampleImageLuminanceGrid(
            imageAssetFor(source, imageAssets),
            source.position,
            source.size,
            options,
          );
        } else {
          if (!source.fill) throw Error('Luminance Instances requires a filled source');
          if (source.contours?.length === 0)
            field = sampleLuminanceGrid(
              source.fill,
              { minX: 0, minY: 0, maxX: 0, maxY: 0 },
              options,
            );
          else {
            mask = prepareLuminanceMask(source);
            field = sampleLuminanceGrid(source.fill, mask.bounds, options);
          }
        }
        const cells = selectLuminanceCenters(
          field.cells,
          mask,
          source.kind === 'image' ? source : null,
        );
        // Validate cardinality and transformed options before expensive exact bounds work.
        const prefix = fingerprint(n.id);
        luminanceInstancesGeometry(motif, cells, { ...settings, pivot: [0, 0], prefix });
        const bounds = motifInfo.pointCount ? pathBounds(motif) : null;
        result = luminanceInstancesGeometry(motif, cells, {
          ...settings,
          pivot: bounds ? [bounds[0][1], bounds[1][1]] : [0, 0],
          prefix,
        });
        delete result.fill;
        delete result.stroke;
        result.characterOccurrences = routeCharacters(
          motif,
          n.id,
          null,
          'Canvas character editing is unavailable through Luminance Instances; edit the source Text',
        );
        result.unstable = true;
        setEvaluationFingerprint(result, (currentEra) => [
          provenance(n.id, undefined, currentEra),
          evaluatedFingerprint(source, currentEra) || null,
          evaluatedFingerprint(motif, currentEra) || null,
          settings,
          pitch,
          invert,
          gridOrigin,
          ...(tonePower !== 1 ? [tonePower] : []),
          cells,
          signature(motif),
          signature(result),
        ]);
      } else if (n.type === 'Instance on Points') {
        if (!n.params.path.source || !n.params.points.source)
          throw Error('Connect both Source and Points Path inputs');
        const source = read('path'),
          targets = read('points'),
          alignTangent = read('alignTangent');
        if (!source || !targets) throw Error('Connect both Source and Points Path inputs');

        // Keep malformed or over-budget geometry away from Paper bounds work.
        // The geometry helper remains the authoritative structural validator.
        const cheapPointCount = (path) => {
          if (!Array.isArray(path?.contours)) return null;
          if (path.contours.length > 10000)
            throw Error('Instance on Points exceeds geometry budget');
          let count = 0;
          for (const contour of path.contours) {
            count += Array.isArray(contour?.points) ? contour.points.length : 0;
            if (count > 100000) throw Error('Instance on Points exceeds geometry budget');
          }
          return count;
        };
        const sourcePoints = cheapPointCount(source),
          targetPoints = cheapPointCount(targets);
        if (sourcePoints === null || targetPoints === null) {
          result = instancePointsGeometry(source, targets, {
            prefix: fingerprint(n.id),
            alignTangent,
          });
        } else if (sourcePoints === 0 || targetPoints === 0) {
          // Empty inputs produce empty geometry without constructing Paper bounds.
          result = instancePointsGeometry(source, targets, {
            prefix: fingerprint(n.id),
            alignTangent,
          });
        } else {
          if (targetPoints > 256) throw Error('Instance on Points exceeds instance budget');
          const sourceGroups = new Set(source.contours.map((contour) => contour?.group ?? ''));
          if (
            source.contours.length * targetPoints > 10000 ||
            sourcePoints * targetPoints > 100000 ||
            sourceGroups.size * targetPoints > 256
          )
            throw Error('Instance on Points exceeds geometry budget');
          const bounds = pathBounds(source),
            pivot = [bounds[0][1], bounds[1][1]],
            prefix = fingerprint(n.id);
          result = instancePointsGeometry(source, targets, { pivot, prefix, alignTangent });
        }
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Instance on Points; edit the source Text',
        );
        result.unstable = true;
        setEvaluationFingerprint(result, (currentEra) => [
          provenance(n.id, undefined, currentEra),
          evaluatedFingerprint(source, currentEra) || null,
          signature(source),
          alignTangent,
          targets.contours.flatMap((contour) => contour.points.map((point) => point.position)),
          signature(result),
        ]);
      } else if (
        ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'].includes(n.type)
      ) {
        const source = read('path');
        if (!source) throw Error(`Connect a PaintedPath input to ${n.type}`);
        if (source.kind !== 'image' && (source.fill === null || source.fill === undefined))
          throw Error(`${n.type} requires a filled source; stroke-only inputs are unsupported`);
        if (source.kind !== 'image') {
          if (!Array.isArray(source.contours) || source.contours.length > 10000)
            throw Error(`${n.type} exceeds the 100000-vertex geometry budget`);
          let inputVertices = 0;
          for (const contour of source.contours) {
            inputVertices += Array.isArray(contour?.points) ? contour.points.length : 0;
            if (inputVertices > 100000)
              throw Error(`${n.type} exceeds the 100000-vertex geometry budget`);
          }
        }
        const { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read),
          origin = { x: gridOrigin[0], y: gridOrigin[1] },
          mode =
            n.type === 'Halftone'
              ? 'halftone'
              : n.type === 'Ordered Dither'
                ? 'dither'
                : n.type === 'Error Diffusion Dither'
                  ? 'error-diffusion'
                  : 'lines',
          orientation = mode === 'lines' ? n.operation : undefined,
          dots = readDotSettings(n, read);
        if (mode === 'lines' && !operationOptions[n.type].includes(orientation))
          throw Error('Line Screen orientation must be horizontal or vertical');
        if (source.kind === 'image') {
          const field = sampleImageLuminanceGrid(
              imageAssetFor(source, imageAssets),
              source.position,
              source.size,
              { pitch, origin, invert, tonePower },
            ),
            [x, y] = source.position,
            [width, height] = source.size,
            mask = prepareLuminanceMask({
              contours: [
                {
                  id: `${n.id}:image-mask`,
                  closed: true,
                  points: [
                    [x, y],
                    [x + width, y],
                    [x + width, y + height],
                    [x, y + height],
                  ].map((position, index) => ({
                    id: `${n.id}:image-mask:${index}`,
                    position,
                    handleA: [0, 0],
                    handleB: [0, 0],
                  })),
                },
              ],
            }),
            generated = generateLuminanceMarks({
              cells: field.cells,
              pitch,
              mode,
              orientation,
              ...dots,
            });
          result = clipLuminanceMarks(
            mask,
            generated.marks.map((mark) => mark.vertices),
            fingerprint(n.id),
          );
          result.fill = { hex: '#000000', alpha: 1 };
        } else if (source.contours.length === 0) {
          // A valid empty painted path (for example, Boolean A−A) remains an
          // empty black fill. Still validate its paint and node settings.
          sampleLuminanceGrid(
            source.fill,
            { minX: 0, minY: 0, maxX: 0, maxY: 0 },
            { pitch, origin, invert, tonePower },
          );
          result = { contours: [], fill: { hex: '#000000', alpha: 1 } };
        } else {
          const mask = prepareLuminanceMask(source),
            field = sampleLuminanceGrid(source.fill, mask.bounds, {
              pitch,
              origin,
              invert,
              tonePower,
            }),
            generated = generateLuminanceMarks({
              cells: field.cells,
              pitch,
              mode,
              orientation,
              ...dots,
            }),
            polygons = generated.marks.map((mark) => mark.vertices);
          result = clipLuminanceMarks(mask, polygons, fingerprint(n.id));
          result.fill = { hex: '#000000', alpha: 1 };
        }
        result.unstable = true;
        setEvaluationFingerprint(result, (currentEra) => [
          provenance(n.id, undefined, currentEra),
          mode,
          ...(mode === 'lines' ? [orientation] : []),
          pitch,
          invert,
          // Preserve schema-14 provenance when the migrated default is unchanged.
          // Non-default origins participate so downstream edits invalidate correctly.
          ...(gridOrigin[0] || gridOrigin[1] ? [gridOrigin] : []),
          ...(tonePower !== 1 ? [tonePower] : []),
          ...dotFingerprint(dots, pitch),
          source.fill,
          evaluatedFingerprint(source, currentEra) || null,
          source.kind === 'image'
            ? JSON.stringify([source.kind, source.asset, source.position, source.size])
            : signature(source),
          signature(result),
        ]);
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          `Canvas character editing is unavailable through ${n.type}; edit upstream Text`,
        );
      } else if (n.type === 'Convert to Path') {
        const source = read('path');
        if (!source) throw Error('Connect a Path or PaintedPath input');
        result = isPaintedGroup(source) ? groupGeometry(source) : cloneEvaluated(source);
        if (
          outputType(byId.get(n.params.path.source), n.params.path.sourcePort) === 'PaintedPath'
        ) {
          delete result.fill;
          delete result.stroke;
          setEvaluationFingerprint(result, (currentEra) => [
            'painted-path-geometry',
            provenance(n.id, undefined, currentEra),
            signature(result),
            result.contours.map((contour) => [contour.id, contour.points.map((point) => point.id)]),
          ]);
        }
      } else if (n.type === 'Closed Path Offset') {
        const source = read('path');
        if (!source) throw Error('Connect a Path input');
        const { amount, miterLimit } = closedOffsetSettings(read);
        if (!operationOptions[n.type].includes(n.operation))
          throw new OffsetError('OFFSET_OPTIONS', 'Invalid Offset join');
        if (amount === 0) result = source;
        else {
          const options = { join: n.operation, miterLimit, prefix: n.id };
          result = source.contours.some((contour) => contour.group !== undefined)
            ? offsetGroupedClosedPathDetailed(source, amount, options, OFFSET_EDITOR_LIMITS).path
            : offsetClosedPath(source, amount, options, OFFSET_EDITOR_LIMITS);
          result.unstable = true;
          setEvaluationFingerprint(result, (currentEra) => [
            provenance(n.id, undefined, currentEra),
            amount,
            n.operation,
            miterLimit,
            evaluatedFingerprint(source, currentEra),
            source.contours,
            signature(result),
          ]);
          result.characterOccurrences = routeCharacters(
            source,
            n.id,
            null,
            'Canvas character editing is unavailable through Closed Path Offset',
          );
        }
      } else if (n.type === 'Imported Path') {
        result = parsePath(n.rawPath, n.id);
        result.unstable = true;
        result.fingerprint = signature(result);
      } else if (n.type === 'Boolean') {
        const keys = booleanInputKeys(n),
          inputs = keys.filter((key) => n.params[key].source).map((key) => [key, read(key)]),
          a = inputs.find(([key]) => key === 'a')?.[1],
          b = inputs.find(([key]) => key === 'b')?.[1];
        if (!a || !b) throw Error('Connect both Boolean paths');
        if (inputs.some(([, path]) => path.contours.some((contour) => !contour.closed)))
          throw Error('Boolean requires closed contours');
        const op = {
          union: 'unite',
          subtract: 'subtract',
          intersect: 'intersect',
          difference: 'exclude',
        }[n.operation];
        if (!op) throw Error('Unknown Boolean operation');
        const liveOperands = new Set();
        try {
          let combined;
          for (const [, path] of inputs) {
            const next = booleanOperand(path);
            liveOperands.add(next);
            if (!combined) {
              combined = next;
              continue;
            }
            const previous = combined;
            combined = previous[op](next, { insert: false });
            liveOperands.add(combined);
            previous.remove();
            liveOperands.delete(previous);
            next.remove();
            liveOperands.delete(next);
          }
          result = fromPaper(combined, n.id);
          result.characterOccurrences = inputs.flatMap(([key, path]) =>
            routeCharacters(
              path,
              n.id + ':' + key,
              null,
              'Canvas character editing is unavailable through Boolean',
            ),
          );
          result.unstable = true;
          result.fingerprint = signature(result);
        } finally {
          for (const item of liveOperands) item.remove();
        }
      } else if (hasEmptyOutput(layer, n)) result = { contours: [] };
      else {
        result = read('path');
        if (!result) throw Error(`Connect a ${n.params.path.type} input`);
        result = cloneEvaluated(result);
        if (isPaintedGroup(result) && n.type !== 'Output')
          throw Error('Edit paint in the source Composition, or use Convert to Path');
        if (n.type === 'Fill') {
          result.fill = read('color');
          setEvaluatedPaintSlot(result, 'fill', parameterColorSlot(n.params.color));
        }
        if (
          ['Linear Gradient Fill', 'Radial Gradient Fill', 'Angular Gradient Fill'].includes(n.type)
        )
          setEvaluatedPaintSlot(result, 'fill', null);
        if (n.type === 'Linear Gradient Fill') {
          result.fill = linearGradientSettings(read);
          setEvaluatedPaintSlot(result, 'fill', {
            startColor: parameterColorSlot(n.params.startColor),
            endColor: parameterColorSlot(n.params.endColor),
          });
        }
        if (n.type === 'Radial Gradient Fill') {
          result.fill = radialGradientSettings(read);
          setEvaluatedPaintSlot(result, 'fill', {
            startColor: parameterColorSlot(n.params.startColor),
            endColor: parameterColorSlot(n.params.endColor),
          });
        }
        if (n.type === 'Angular Gradient Fill') {
          result.fill = angularGradientSettings(read);
          setEvaluatedPaintSlot(result, 'fill', {
            startColor: parameterColorSlot(n.params.startColor),
            color1: parameterColorSlot(n.params.color1),
            color2: parameterColorSlot(n.params.color2),
            endColor: parameterColorSlot(n.params.endColor),
          });
        }
        if (n.type === 'Stroke') {
          const width = read('width');
          if (width < 0 || width > 100000) throw Error('Stroke width must be from 0 to 100000 px');
          const style = validateStrokeStyle(n),
            miterLimit = n.params.miterLimit ? read('miterLimit') : 4,
            dashOffset = n.params.dashOffset ? read('dashOffset') : 0;
          if (miterLimit < 1 || miterLimit > 10000)
            throw Error('Miter limit must be from 1 to 10000');
          if (Math.abs(dashOffset) > 100000)
            throw Error('Dash offset must be from -100000 to 100000 px');
          result.stroke = { ...read('color'), width, ...style, miterLimit, dashOffset };
          setEvaluatedPaintSlot(result, 'stroke', parameterColorSlot(n.params.color));
        }
        if (n.type === 'Path Remap') {
          result.characterOccurrences = routeCharacters(
            result,
            n.id,
            null,
            'Canvas character editing is unavailable through Path Remap',
          );
          let offset = read('offset');
          for (const contour of result.contours)
            for (const pt of contour.points) pt.position = pt.position.map((v, i) => v + offset[i]);
          for (const edit of n.edits) {
            let pt = result.contours.flatMap((c) => c.points).find((p) => p.id === edit.pointId);
            if (!pt || (result.unstable && !remapFingerprintMatches(result, edit.fingerprint))) {
              result.remapSuspended = true;
              warnings.push(
                `${n.name}: vertex edit suspended because its source changed. Rebind explicitly.`,
              );
              continue;
            }
            for (const key of ['position', 'handleA', 'handleB']) {
              const delta = edit.params?.[key] ? read(`remap.${edit.pointId}.${key}`) : edit[key];
              if (delta) pt[key] = pt[key].map((v, i) => v + delta[i]);
            }
          }
        } else if (n.type === 'Transform') {
          let obj = toPaper(result);
          const state = transformValues(n, read);
          obj.transform(new scope.Matrix(...state.matrix));
          let transformed = fromPaper(obj, n.id);
          transformed.contours.forEach((c, ci) => {
            c.id = result.contours[ci].id;
            if (result.contours[ci].group !== undefined) c.group = result.contours[ci].group;
            if (result.contours[ci].repeaterOpacity !== undefined)
              c.repeaterOpacity = result.contours[ci].repeaterOpacity;
            if (result.contours[ci].repeaterFill !== undefined)
              c.repeaterFill = clone(result.contours[ci].repeaterFill);
            if (result.contours[ci].repeaterStroke !== undefined)
              c.repeaterStroke = clone(result.contours[ci].repeaterStroke);
            c.points.forEach((p, pi) => (p.id = result.contours[ci].points[pi].id));
          });
          transformed.characterOccurrences = routeCharacters(result, n.id, state.matrix);
          transformed.unstable = result.unstable;
          transformed.remapSuspended = result.remapSuspended;
          transformed.fingerprint = result.fingerprint;
          copyEvaluationIdentity(result, transformed);
          result = transformed;
          obj.remove();
        }
      }
      if (
        ['Circle', 'Ellipse', 'Rectangle', 'Polygon', 'Star', 'Image'].includes(n.type) &&
        n.positionReference &&
        n.positionReference !== 'origin'
      ) {
        const [horizontal, vertical] = n.positionReference.split('-');
        const indices = { left: 0, center: 1, right: 2, top: 0, middle: 1, bottom: 2 };
        if (
          !['left', 'center', 'right'].includes(horizontal) ||
          !['top', 'middle', 'bottom'].includes(vertical)
        )
          throw Error('Invalid object Position reference');
        const bounds = pathBounds(result),
          position = read(n.type === 'Image' ? 'position' : 'center');
        const delta = position.map(
          (v, axis) => v - bounds[axis][indices[axis ? vertical : horizontal]],
        );
        if (n.type === 'Image') result.position = result.position.map((v, axis) => v + delta[axis]);
        else
          for (const contour of result.contours)
            for (const point of contour.points)
              point.position = point.position.map((v, axis) => v + delta[axis]);
      }
      if (result?.characterOccurrences?.length > 256)
        result.characterOccurrences = [
          ...new Map(
            result.characterOccurrences.map((o) => [
              o.source,
              { ...o, reason: 'Canvas character editing exceeds 256 displayed instances' },
            ]),
          ).values(),
        ];
      values.set(key, result);
      const version = ++revision;
      versions.set(key, version);
      cache.set(key, {
        key: cacheKey,
        font: cacheFont,
        styleFonts,
        imageAsset,
        value: cloneEvaluated(result),
        version,
        warnings: warnings.slice(warningStart),
        textLayout: !positionOutput && textLayouts.has(id) ? clone(textLayouts.get(id)) : undefined,
      });
      return result;
    } finally {
      active.delete(activeKey);
      layerActive.delete(activeKey);
      context.depth--;
    }
  }
  let outputEvaluated = false;
  for (const key of cache.keys()) if (!byId.has(key.split('\0')[0])) cache.delete(key);
  const result = {
    path: null,
    outputRevision: undefined,
    error: null,
    ensureOutput() {
      if (outputEvaluated) return;
      try {
        result.path = run(layer.output);
      } catch (e) {
        result.error = e.message;
        if (e.code) result.errorCode = e.code;
      }
      result.outputRevision = versions.get(outputKey(layer.output));
      outputEvaluated = true;
      result.stats = { computed, reused };
    },
    warnings,
    values,
    run,
    resolveReference: resolveExpressionReference,
    parameterValue(id, key) {
      const n = byId.get(id);
      if (!n) throw Error('Missing node');
      return readParameter(n, key);
    },
    objectResizeState(id) {
      const n = byId.get(id);
      if (!['Rectangle', 'Ellipse', 'Circle', 'Polygon', 'Star', 'Text'].includes(n?.type))
        throw Error('Select a supported generator');
      run(id);
      return {
        ...Object.fromEntries(Object.keys(n.params).map((key) => [key, readParameter(n, key)])),
        positionReference: n.positionReference || 'origin',
        ...(n.type === 'Star'
          ? {
              fallbackInnerRadius: n.params.innerRadius.value,
              fallbackOuterRadius: n.params.outerRadius.value,
            }
          : {}),
      };
    },
    transformValues(id) {
      const n = byId.get(id);
      if (!isTransform(n)) throw Error('Select a Transform');
      return transformValues(n, (key) => readParameter(n, key));
    },
    transformState(id) {
      const n = byId.get(id);
      if (n?.type !== 'Transform') throw Error('Select a Transform');
      if (n.bypassed) throw Error('Restore the Transform before editing its canvas handles');
      run(id);
      return {
        ...transformValues(n, (key) => readParameter(n, key)),
        input: readParameter(n, 'path'),
      };
    },
    textDefaults(id) {
      const n = byId.get(id);
      if (n?.type !== 'Text') throw Error('Select a Text node');
      return {
        font: n.font,
        size: readParameter(n, 'size'),
        width: 1,
        height: 1,
        tracking: readParameter(n, 'letterSpacing'),
        lineSpacing: readParameter(n, 'lineSpacing'),
        baselineShift: 0,
      };
    },
    textLayout(id) {
      run(id);
      return textLayouts.get(id);
    },
    stats: { computed, reused },
  };
  context.frames.set(layer.id, result);
  if (!lazy) result.ensureOutput();
  return result;
}
// Session commits and previews are the common boundary for source edits,
// including gestures and prepared commands. A dangling identity cannot be
// revived by an allocator, and affected cross-layer controls remain valid.
function foreignAffectedVertices(before, candidate, fonts) {
  const nodeKey = (layerId, nodeId) => `${layerId}\0${nodeId}`;
  const entries = (doc) =>
    new Map(doc.layers.flatMap((layer) => layer.nodes.map((n) => [nodeKey(layer.id, n.id), n])));
  const prior = entries(before),
    current = entries(candidate),
    changed = new Set(),
    changedAll = new Set();
  for (const key of new Set([...prior.keys(), ...current.keys()]))
    if (JSON.stringify(prior.get(key)) !== JSON.stringify(current.get(key))) changed.add(key);
  const layerFields = (layer) => {
    const { nodes, ...fields } = layer;
    return JSON.stringify(fields);
  };
  const oldLayers = new Map(before.layers.map((layer) => [layer.id, layer]));
  for (const layer of candidate.layers) {
    const old = oldLayers.get(layer.id);
    if (!old || layerFields(old) !== layerFields(layer))
      for (const n of layer.nodes) {
        const key = nodeKey(layer.id, n.id);
        changed.add(key);
        changedAll.add(key);
      }
  }
  const registryChanged = (name) => {
    const a = before[name] || {},
      b = candidate[name] || {};
    return [...new Set([...Object.keys(a), ...Object.keys(b)])].some((key) => a[key] !== b[key]);
  };
  if (
    ['width', 'height', 'colorSlots', 'keyColorRecipe'].some(
      (key) => JSON.stringify(before[key]) !== JSON.stringify(candidate[key]),
    ) ||
    registryChanged('fontAssets') ||
    registryChanged('imageAssets')
  )
    for (const key of current.keys()) {
      changed.add(key);
      changedAll.add(key);
    }

  // Properties can be read directly even when their node output does not use
  // them (for example Resample spacing in count mode). Build the reverse graph
  // from both snapshots and keep property and output vertices distinct.
  const reverse = new Map(),
    vertices = new Map(),
    byNode = new Map(),
    directForeign = new Set();
  let candidateGraph, priorGraph;
  for (const doc of [before, candidate]) {
    const graph = documentDependencyGraph(doc),
      queue = [],
      seen = new Set();
    if (doc === candidate) candidateGraph = graph;
    else priorGraph = graph;
    for (const layer of doc.layers)
      for (const n of layer.nodes) {
        for (const port of outputPorts(n)) queue.push(graph.output(layer.id, n.id, port.id));
        for (const [key] of parameters(n)) queue.push(graph.property(layer.id, n.id, key));
      }
    for (let i = 0; i < queue.length; i++) {
      const vertex = queue[i],
        key = graph.keyOf(vertex);
      if (seen.has(key)) continue;
      seen.add(key);
      vertices.set(key, vertex);
      const owner = nodeKey(vertex.layerId, vertex.nodeId);
      if (!byNode.has(owner)) byNode.set(owner, new Set());
      byNode.get(owner).add(key);
      for (const dep of graph.edges(vertex)) {
        const source = graph.keyOf(dep),
          foreign = dep.layerId !== vertex.layerId;
        if (!reverse.has(source)) reverse.set(source, []);
        reverse.get(source).push({ consumer: key, foreign });
        if (foreign) directForeign.add(key);
        queue.push(dep);
      }
    }
  }
  // Changed/re-activated destinations must themselves be checked whenever
  // they depend on a foreign property, not only after a source changes.
  const dependsOnForeign = new Set(directForeign),
    dependencies = [...directForeign];
  for (let i = 0; i < dependencies.length; i++)
    for (const edge of reverse.get(dependencies[i]) || [])
      if (!dependsOnForeign.has(edge.consumer)) {
        dependsOnForeign.add(edge.consumer);
        dependencies.push(edge.consumer);
      }
  const queue = [],
    seen = new Set(),
    affected = new Map(),
    activatedVertices = new Set();
  const outputFields = (n) => {
    const { params, points, edits, ...fields } = n;
    return JSON.stringify({
      ...fields,
      points: points.map(({ position, handleA, handleB, ...point }) => point),
      edits: edits.map(({ params: editParams, ...edit }) => edit),
    });
  };
  for (const owner of changed) {
    const oldNode = prior.get(owner),
      newNode = current.get(owner);
    const wholeNode = changedAll.has(owner) || !oldNode || !newNode;
    const outputChanged = wholeNode || outputFields(oldNode) !== outputFields(newNode);
    for (const key of byNode.get(owner) || []) {
      const vertex = vertices.get(key);
      const vertexChanged =
        wholeNode ||
        (vertex.key === undefined
          ? outputChanged
          : JSON.stringify(priorGraph.parameter(oldNode, vertex.key)) !==
            JSON.stringify(candidateGraph.parameter(newNode, vertex.key)));
      if (!vertexChanged) continue;
      const foreign = dependsOnForeign.has(key);
      queue.push({ key, foreign });
      if (foreign) affected.set(key, vertex);
    }
  }
  // A changed Boolean source can switch a downstream node's effective inputs
  // even when that node's authored descriptor and static graph are unchanged.
  // Resolve only Boolean wires which actually change its input set, in short-
  // lived lazy frames, and ask the existing input selector on detached views.
  const frames = new WeakMap();
  const frame = (doc, layer) => {
    let layerFrames = frames.get(doc);
    if (!layerFrames) {
      layerFrames = new Map();
      frames.set(doc, layerFrames);
    }
    if (!layerFrames.has(layer.id))
      layerFrames.set(
        layer.id,
        evaluate(layer, resolveFonts(doc, fonts), doc.imageAssets, doc, null, true),
      );
    return layerFrames.get(layer.id);
  };
  const inputKeys = (n, port) =>
    new Set(
      (n.type === 'Text' && port === 'position'
        ? [['position', n.params.position]]
        : evaluatedParameters(n)
      ).map(([key]) => key),
    );
  const sameKeys = (a, b) => a.size === b.size && [...a].every((key) => b.has(key));
  const literalView = (n, key, value) => ({
    ...n,
    params: {
      ...n.params,
      [key]: { ...n.params[key], value, source: undefined, sourcePort: undefined, expression: '' },
    },
  });
  const effectiveInputs = (doc, graph, vertex) => {
    const n = graph.nodes.get(vertex.layerId)?.get(vertex.nodeId),
      layer = graph.layers.get(vertex.layerId);
    if (!n || !outputType(n, vertex.port)) return new Set();
    let view = n;
    for (const [key, q] of Object.entries(n.params)) {
      if (q.type !== 'Boolean' || !q.source) continue;
      if (
        sameKeys(
          inputKeys(literalView(view, key, true), vertex.port),
          inputKeys(literalView(view, key, false), vertex.port),
        )
      )
        continue;
      try {
        view = literalView(view, key, frame(doc, layer).parameterValue(n.id, key));
      } catch {
        // Keep existing missing or unresolved-input behavior; the ordinary
        // evaluator and command validation own that diagnostic.
        return null;
      }
    }
    return inputKeys(view, vertex.port);
  };
  const reachable = new Set(queue.map((item) => item.key)),
    reachableQueue = [...reachable];
  for (let i = 0; i < reachableQueue.length; i++)
    for (const edge of reverse.get(reachableQueue[i]) || [])
      if (!reachable.has(edge.consumer)) {
        reachable.add(edge.consumer);
        reachableQueue.push(edge.consumer);
      }
  for (const key of reachableQueue) {
    const vertex = vertices.get(key);
    if (vertex.key !== undefined || !dependsOnForeign.has(key)) continue;
    const oldInputs = effectiveInputs(before, priorGraph, vertex),
      newInputs = effectiveInputs(candidate, candidateGraph, vertex);
    if (!oldInputs || !newInputs) continue;
    for (const name of newInputs) {
      if (oldInputs.has(name)) continue;
      const property = candidateGraph.property(vertex.layerId, vertex.nodeId, name),
        propertyKey = candidateGraph.keyOf(property);
      activatedVertices.add(propertyKey);
      if (dependsOnForeign.has(propertyKey)) {
        affected.set(propertyKey, property);
        queue.push({ key: propertyKey, foreign: true });
      }
    }
  }
  for (let i = 0; i < queue.length; i++) {
    const item = queue[i],
      marker = `${item.key}\0${item.foreign ? 1 : 0}`;
    if (seen.has(marker)) continue;
    seen.add(marker);
    for (const edge of reverse.get(item.key) || []) {
      const foreign = item.foreign || edge.foreign;
      if (foreign) affected.set(edge.consumer, vertices.get(edge.consumer));
      queue.push({ key: edge.consumer, foreign });
    }
  }
  return {
    affected: [...affected.values()].filter((v) => current.has(nodeKey(v.layerId, v.nodeId))),
    candidateGraph,
    activatedVertices,
  };
}
function assertArtworkReferenceTransition(
  before,
  candidate,
  fonts = {},
  validateEffective = validate,
) {
  const allRefs = (doc) => {
    const refs = [];
    for (const layer of doc.layers)
      for (const n of layer.nodes)
        for (const [, q] of parameters(n)) {
          const expression = q.expression;
          if (typeof expression !== 'string' || expression.length === 0) continue;
          for (const ref of artworkExpressionReferences(expression)) refs.push(ref);
          for (const ref of compositionPropertyReferences(expression)) refs.push(ref);
          if (doc.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION)
            for (const ref of componentCompositionReferences(expression)) refs.push(ref);
          if (doc.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
            for (const ref of qualifiedColorReferences(expression)) refs.push(ref);
        }
    return refs;
  };
  const previous = allRefs(before),
    next = allRefs(candidate);
  if (!previous.length && !next.length) return;
  const refKey = (ref) =>
    `${ref.qualifier || ''}\0${ref.compositionId || ''}\0${ref.layerId}\0${ref.nodeId}\0${ref.key}\0${ref.component || ''}`;
  const remaining = new Set(next.map(refKey));
  const resolvedIdentity = (doc, ref) => {
    const layer = doc.layers.find((item) => item.id === ref.layerId);
    const owner = layer?.nodes.find((item) => item.id === ref.nodeId);
    if (!owner) return false;
    const point = curvePointReference(ref.key, ref.component);
    return !point || (owner.type === 'Curve' && owner.points.some((item) => item.id === point.id));
  };
  for (const ref of previous)
    if (
      remaining.has(refKey(ref)) &&
      !resolvedIdentity(before, ref) &&
      resolvedIdentity(candidate, ref)
    )
      throw Error('New identity would retarget a missing Artwork reference');

  const { affected, candidateGraph, activatedVertices } = foreignAffectedVertices(
    before,
    candidate,
    fonts,
  );
  if (!affected.length) return;
  const currentFonts = resolveFonts(candidate, fonts),
    oldFonts = resolveFonts(before, fonts),
    effective = cloneDocument(candidate);
  const projectedIndexes = new WeakMap();
  const projectedParameter = (n, key) => {
    let indexed = projectedIndexes.get(n);
    if (!indexed) {
      indexed = new Map();
      for (const [name, q] of parameters(n)) if (!indexed.has(name)) indexed.set(name, q);
      projectedIndexes.set(n, indexed);
    }
    return indexed.get(key) || parameterFallback(n, key);
  };
  for (const layer of candidate.layers) {
    const vertices = affected.filter((vertex) => vertex.layerId === layer.id);
    const affectedNodeIds = new Set(vertices.map((vertex) => vertex.nodeId));
    if (!affectedNodeIds.size) continue;
    const old = before.layers.find((item) => item.id === layer.id);
    const projectedLayer = effective.layers.find((item) => item.id === layer.id);
    const projectedNodes = new Map(projectedLayer.nodes.map((n) => [n.id, n]));
    const evaluation = evaluate(layer, currentFonts, candidate.imageAssets, candidate, null, true);
    let oldEvaluation;
    const check = (now, was) => {
      try {
        now();
      } catch (error) {
        if (
          isMissingReferenceTarget(error) ||
          ['COMPOSITION_TARGET_UNSET', 'COMPOSITION_TARGET_MISSING'].includes(error.code) ||
          /^Connect (a |both )/.test(error.message)
        )
          return;
        if (old && was) {
          oldEvaluation ||= evaluate(old, oldFonts, before.imageAssets, before, null, true);
          try {
            was(oldEvaluation);
          } catch (prior) {
            if (prior.message === error.message) return;
          }
        }
        throw error;
      }
    };
    for (const vertex of vertices) {
      const n = candidateGraph.nodes.get(layer.id)?.get(vertex.nodeId);
      if (!n) continue;
      if (vertex.key === undefined) {
        // A previously renderable output must remain renderable. Unconnected,
        // missing-font and other already incomplete outputs retain their legacy
        // editing behavior; output sockets that do not exist are never run.
        const oldNode = old?.nodes.find((item) => item.id === n.id);
        if (!oldNode || !outputType(oldNode, vertex.port) || !outputType(n, vertex.port)) continue;
        oldEvaluation ||= evaluate(old, oldFonts, before.imageAssets, before, null, true);
        try {
          oldEvaluation.run(n.id, vertex.port);
        } catch {
          continue;
        }
        check(() => evaluation.run(n.id, vertex.port));
        continue;
      }
      const q = candidateGraph.parameter(n, vertex.key);
      if (!q || !['Scalar', 'Vector2', 'Color'].includes(q.type)) continue;
      let value,
        resolved = false;
      check(
        () => {
          value = evaluation.parameterValue(n.id, vertex.key);
          resolved = true;
        },
        // The property may already have been invalid when read directly while
        // inactive. That must not excuse making it an effective output input.
        activatedVertices.has(candidateGraph.keyOf(vertex))
          ? null
          : (e) => e.parameterValue(n.id, vertex.key),
      );
      if (resolved && n.type !== 'Text' && !(n.type === 'Trim Path' && !n.params.path?.source)) {
        const projected = projectedNodes.get(n.id),
          target = projected && projectedParameter(projected, vertex.key);
        if (target) target.value = clone(value);
      }
    }
    for (const n of layer.nodes) {
      if (!affectedNodeIds.has(n.id)) continue;
      if (n.type === 'Geometry Repeater' && n.operation === 'ae-2d' && !n.bypassed)
        check(
          () =>
            assertRepeaterControls(
              (key) => evaluation.parameterValue(n.id, key),
              isMissingReferenceTarget,
            ),
          (e) =>
            assertRepeaterControls((key) => e.parameterValue(n.id, key), isMissingReferenceTarget),
        );
      if (isTransform(n))
        check(
          () => assertTransformControls(evaluation, n),
          (e) => {
            const prior = old.nodes.find((item) => item.id === n.id);
            if (prior) assertTransformControls(e, prior);
          },
        );
      if (n.type === 'Mask' && n.params.feather)
        check(
          () => maskFeather(evaluation.parameterValue(n.id, 'feather')),
          (e) => maskFeather(e.parameterValue(n.id, 'feather')),
        );
    }
    assertClosedOffsetChange(
      layer,
      old,
      currentFonts,
      candidate.imageAssets,
      oldFonts,
      candidate,
      before,
      new Set(vertices.filter((vertex) => vertex.key === undefined).map((vertex) => vertex.nodeId)),
    );
  }
  // Reuse the same hard domain checks as saved values on a detached projection
  // of the affected effective values; authored fallbacks are never changed.
  validateEffective(effective);
}
function projectAsReferenceDocument(project, { normalize = false } = {}) {
  const { compositionSchemaVersion, compositions, ...shared } = project;
  const first = compositions[0];
  const doc = {
    ...shared,
    width: first.width,
    height: first.height,
    ...(first.editor === undefined ? {} : { editor: first.editor }),
    layers: compositions.flatMap((composition) => composition.layers),
  };
  bindProjectDocumentContext(doc, project, first.id);
  if (normalize) {
    const binding = projectDocumentContexts.get(doc);
    doc.layers = compositions.flatMap(
      (composition) => normalizedProjectSource(binding, composition.id).layers,
    );
  }
  return doc;
}

// Check exact identity, graph cycles and changed dependent values against the
// whole project candidate, before either Session or project History commits.
export function assertProjectReferenceTransition(before, candidate, fonts = {}) {
  const previous =
    before.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION
      ? compositionPropertyReferencesInProject(before)
      : [];
  const next =
    candidate.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION
      ? compositionPropertyReferencesInProject(candidate)
      : [];
  if (before.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION)
    previous.push(...componentCompositionReferencesInProject(before));
  if (candidate.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION)
    next.push(...componentCompositionReferencesInProject(candidate));
  const oldColors =
    before.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION
      ? qualifiedColorReferencesInProject(before)
      : [];
  const newColors =
    candidate.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION
      ? qualifiedColorReferencesInProject(candidate)
      : [];
  previous.push(...oldColors.filter((ref) => ref.compositionId));
  next.push(...newColors.filter((ref) => ref.compositionId));
  if (!previous.length && !next.length && !oldColors.length && !newColors.length) return;
  const key = (ref) =>
    `${ref.qualifier || (ref.componentComposition ? 'componentComposition' : '')}\0${ref.compositionId}\0${ref.layerId}\0${ref.nodeId}\0${ref.key}\0${ref.component || ''}`;
  const remaining = new Set(next.map(key));
  const resolves = (project, ref) => {
    const owner = project.compositions
      .find((c) => c.id === ref.compositionId)
      ?.layers.find((layer) => layer.id === ref.layerId)
      ?.nodes.find((n) => n.id === ref.nodeId);
    if (!owner) return false;
    const point = curvePointReference(ref.key, ref.component);
    return !point || (owner.type === 'Curve' && owner.points.some((item) => item.id === point.id));
  };
  for (const ref of previous)
    if (remaining.has(key(ref)) && !resolves(before, ref) && resolves(candidate, ref))
      throw Error('New identity would retarget a missing Composition property reference');
  const oldDoc = projectAsReferenceDocument(before, { normalize: true }),
    nextDoc = projectAsReferenceDocument(candidate, { normalize: true });
  const binding = projectDocumentContexts.get(nextDoc);
  const previousKeys = new Set(previous.map(key));
  for (const ref of next) {
    const layer = normalizedProjectSource(binding, ref.compositionId)?.layers.find(
      (item) => item.id === ref.layerId,
    );
    if (layer?.nodes.some((n) => n.id === ref.nodeId)) {
      try {
        referenceParameter(layer, ref, ref.qualifier ? 'Color' : 'Scalar');
      } catch (error) {
        if (error.code !== 'EXPRESSION_REFERENCE_MISSING' || !previousKeys.has(key(ref)))
          throw error;
      }
    }
  }
  assertDocumentReferenceAcyclic(nextDoc);
  assertArtworkReferenceTransition(oldDoc, nextDoc, fonts, (effective) => {
    for (const composition of candidate.compositions) {
      const ids = new Set(composition.layers.map((layer) => layer.id));
      const doc = {
        ...effective,
        width: composition.width,
        height: composition.height,
        layers: effective.layers.filter((layer) => ids.has(layer.id)),
      };
      if (composition.editor === undefined) delete doc.editor;
      else doc.editor = composition.editor;
      validate(doc);
    }
  });
}

// All attached/wrapped text is checked, even when it is not the displayed Output. A failed
// edit leaves authored state/history unchanged; pre-existing diagnosed errors can be repaired.
export function assertTextPathChange(
  layer,
  previousLayer,
  fonts = {},
  imageAssets = {},
  previousFonts = fonts,
  documentContext = null,
  previousDocumentContext = documentContext,
  affectedNodeIds = null,
) {
  const attached = layer.nodes.filter(
    (n) =>
      n.type === 'Text' &&
      (n.pathAttachment || n.textWrap) &&
      (!affectedNodeIds || affectedNodeIds.has(n.id)),
  );
  if (!attached.length) return;
  assertAcyclic(layer);
  const current = evaluate(layer, fonts, imageAssets, documentContext, null, !!affectedNodeIds);
  let previous;
  for (const n of attached) {
    try {
      current.run(n.id);
    } catch (error) {
      if (isMissingReferenceTarget(error)) continue;
      const old = previousLayer?.nodes.find(
        (x) => x.id === n.id && (x.pathAttachment || x.textWrap),
      );
      if (old) {
        previous ??= evaluate(
          previousLayer,
          previousFonts,
          imageAssets,
          previousDocumentContext,
          null,
          !!affectedNodeIds,
        );
        try {
          previous.run(n.id);
        } catch (before) {
          if (before.message === error.message) continue;
        }
      }
      throw error;
    }
  }
}
export function setTextPathAttachment(
  layer,
  nodeId,
  attachment,
  fonts = {},
  imageAssets = {},
  documentContext = null,
) {
  const n = layer.nodes.find((n) => n.id === nodeId);
  if (n?.type !== 'Text') throw Error('Select a Text node');
  const old = n.pathAttachment;
  try {
    if (attachment === null) delete n.pathAttachment;
    else {
      validateTextPathAttachment(attachment);
      n.pathAttachment = clone(attachment);
    }
    assertAcyclic(layer);
    if (attachment !== null) evaluate(layer, fonts, imageAssets, documentContext).run(nodeId);
  } catch (error) {
    if (old === undefined) delete n.pathAttachment;
    else n.pathAttachment = old;
    throw error;
  }
}
/** Set or remove composition width atomically; source text and character identities stay untouched. */
export function setTextWrap(
  layer,
  nodeId,
  wrap,
  fonts = {},
  imageAssets = {},
  documentContext = null,
) {
  const n = layer.nodes.find((n) => n.id === nodeId);
  if (n?.type !== 'Text') throw Error('Select a Text node');
  if (layer.locked) throw Error('Composition is locked');
  const old = n.textWrap;
  try {
    if (wrap === null) delete n.textWrap;
    else {
      validateTextWrap(wrap, n.writingMode, n.pathAttachment);
      n.textWrap = clone(wrap);
      evaluate(layer, fonts, imageAssets, documentContext).run(nodeId);
    }
  } catch (error) {
    if (old === undefined) delete n.textWrap;
    else n.textWrap = old;
    throw error;
  }
}
export function textPathTargets(
  layer,
  textId,
  fonts = {},
  imageAssets = {},
  documentContext = null,
) {
  const result = evaluate(layer, fonts, imageAssets, documentContext),
    targets = [];
  for (const n of layer.nodes) {
    if (n.id === textId || outputType(n) !== 'Path' || n.type === 'Text') continue;
    try {
      const path = result.run(n.id);
      if (path.unstable || path.remapSuspended) continue;
      for (const contour of path.contours)
        if (contour.points.length >= 2)
          targets.push({
            nodeId: n.id,
            contourId: contour.id,
            name: n.name,
            closed: contour.closed,
          });
    } catch {
      /* Unwired or unsupported sources are not eligible targets. */
    }
  }
  return targets;
}
export function assertClosedOffsetChange(
  layer,
  previousLayer,
  fonts = {},
  imageAssets = {},
  previousFonts = fonts,
  documentContext = null,
  previousDocumentContext = documentContext,
  affectedNodeIds = null,
) {
  assertTextPathChange(
    layer,
    previousLayer,
    fonts,
    imageAssets,
    previousFonts,
    documentContext,
    previousDocumentContext,
    affectedNodeIds,
  );
  // Keep bounded geometry modifiers transactional, including upstream edits.
  const bounded = new Set([
    'Join Geometry',
    'Closed Path Offset',
    'Mirror',
    'Corner Round',
    'Resample Path',
    'Roughen',
    'Smooth Path',
    'Trim Path',
    'Set Path Start',
    'Set Closure',
    'Boolean',
    'Reverse Path',
    'Subdivide Path',
    'Expand Stroke',
    'Simplify Path',
    'Dash Path',
    'Pucker & Bloat',
    'Wave Points',
    'Taper',
    'Bend Points',
    'Twist Points',
    'Polar Transform',
    'Auto Handles',
    'Instance on Points',
    'Luminance Instances',
    'Luminance to Points',
    'Halftone',
    'Ordered Dither',
    'Error Diffusion Dither',
    'Ordered Dither Image',
    'Error Diffusion Dither Image',
    'Line Screen',
  ]);
  const boundedInputs = (n) =>
    n.type === 'Join Geometry'
      ? joinInputKeys(n).map((key) => n.params[key]?.source)
      : n.type === 'Boolean'
        ? booleanInputKeys(n).map((key) => n.params[key]?.source)
        : n.type === 'Luminance Instances'
          ? Object.values(n.params).map((q) => q.source)
          : n.type === 'Instance on Points'
            ? ['path', 'points', 'alignTangent'].map((key) => n.params[key]?.source)
            : [n.params.path?.source];
  if (
    !layer.nodes.some(
      (n) =>
        bounded.has(n.type) &&
        boundedInputs(n).some(Boolean) &&
        (!affectedNodeIds || affectedNodeIds.has(n.id)),
    )
  )
    return;
  const current = evaluate(layer, fonts, imageAssets, documentContext, null, !!affectedNodeIds);
  let previous;
  for (const n of layer.nodes.filter(
    (n) =>
      bounded.has(n.type) &&
      boundedInputs(n).some(Boolean) &&
      (!affectedNodeIds || affectedNodeIds.has(n.id)),
  )) {
    try {
      current.run(n.id);
    } catch (error) {
      // Missing inputs are an ordinary intermediate wiring state.
      if (
        /^Connect (a |both )/.test(error.message) ||
        ['COMPOSITION_TARGET_UNSET', 'COMPOSITION_TARGET_MISSING'].includes(error.code) ||
        isMissingReferenceTarget(error)
      )
        continue;
      const old = previousLayer?.nodes.find(
        (x) => x.id === n.id && bounded.has(x.type) && boundedInputs(x).some(Boolean),
      );
      if (old) {
        previous ??= evaluate(
          previousLayer,
          previousFonts,
          imageAssets,
          previousDocumentContext,
          null,
          !!affectedNodeIds,
        );
        try {
          previous.run(n.id);
        } catch (before) {
          if (before.message === error.message) continue;
        }
      }
      throw error;
    }
  }
}
function normalizePaintRouting(layer) {
  const byId = new Map(layer.nodes.map((n) => [n.id, n]));
  for (let pass = 0; pass < layer.nodes.length; pass++) {
    let changed = false;
    for (const n of layer.nodes)
      if (['Transform', 'Join Geometry'].includes(n.type) && !n.paintMode) {
        const keys = n.type === 'Transform' ? ['path'] : joinInputKeys(n);
        if (
          keys.some(
            (key) =>
              n.params[key].source && outputType(byId.get(n.params[key].source)) === 'PaintedPath',
          )
        ) {
          n.paintMode = true;
          for (const key of keys) n.params[key].type = 'PaintedPath';
          changed = true;
        }
      }
    if (!changed) break;
  }
  for (const n of layer.nodes)
    for (const [key, q] of parameters(n))
      if (q.source && !socketCompatibleFor(byId.get(q.source), n, key, q.sourcePort))
        throw Error('Painted output requires a PaintedPath socket or Convert to Path');
}
export function connect(layer, sourceId, targetId, key, validating = false, sourcePort) {
  let from = layer.nodes.find((n) => n.id === sourceId),
    to = layer.nodes.find((n) => n.id === targetId),
    q = to && parameter(to, key);
  const pendingBooleanInput =
    to?.type === 'Boolean' &&
    key === booleanPendingInputKey &&
    !to.booleanInputOrder &&
    !Object.hasOwn(to.params, key);
  const virtualMaskParameter =
    to?.type === 'Mask' && ['feather', 'luminance'].includes(key) && !Object.hasOwn(to.params, key);
  if (!from || !q) throw Error('Missing socket');
  if (validating && pendingBooleanInput) throw Error('Invalid Boolean input order');
  if (q.locked && !validating) throw Error('Parameter is locked');
  if (
    to.bypassed &&
    to.type !== 'Luminance Instances' &&
    imageLuminanceTargets.has(to.type) &&
    key === 'path' &&
    outputType(from, sourcePort) === 'Image'
  )
    throw Error(
      'Image luminance inputs must be converted to marks; they cannot be connected while bypassed.',
    );
  if (
    to.bypassed &&
    to.type === 'Convert to Path' &&
    key === 'path' &&
    outputType(from, sourcePort) === 'PaintedPath'
  )
    throw Error('Convert to Path cannot accept a PaintedPath input while bypassed.');
  if (!socketCompatibleFor(from, to, key, sourcePort))
    throw Error(`Cannot connect ${outputType(from, sourcePort)} to ${q.type}`);
  if (validating) {
    if (
      outputType(from, sourcePort) === 'PaintedPath' &&
      ['Transform', 'Join Geometry'].includes(to.type) &&
      !to.paintMode
    )
      throw Error('Painted input requires saved paint routing');
    assertAcyclic(layer);
    return;
  }
  const routing = layer.nodes.map((n) => [
    n,
    n.paintMode,
    Object.entries(n.params).map(([key, q]) => [key, q.type]),
  ]);
  if (virtualMaskParameter) to.params[key] = q;
  if (pendingBooleanInput) {
    to.booleanInputOrder = ['a', 'b', key];
    to.params[key] = q;
  }
  const old = q.source,
    oldPort = q.sourcePort;
  q.source = sourceId;
  if (sourcePort !== undefined) q.sourcePort = sourcePort;
  else delete q.sourcePort;
  try {
    assertAcyclic(layer);
    normalizePaintRouting(layer);
  } catch (e) {
    for (const [n, mode, types] of routing) {
      if (mode === undefined) delete n.paintMode;
      else n.paintMode = mode;
      for (const [key, type] of types) n.params[key].type = type;
    }
    if (pendingBooleanInput) {
      delete to.params[key];
      delete to.booleanInputOrder;
    }
    if (virtualMaskParameter) delete to.params[key];
    if (old) q.source = old;
    else delete q.source;
    if (oldPort !== undefined) q.sourcePort = oldPort;
    else delete q.sourcePort;
    throw e;
  }
  normalizeJoinInputs(to);
  normalizeBooleanInputs(to);
}
export function disconnect(n, key) {
  const q = parameter(n, key);
  if (q.locked) throw Error('Parameter is locked');
  delete q.source;
  delete q.sourcePort;
  normalizeJoinInputs(n, true);
  normalizeBooleanInputs(n, true);
}
// Upgrade on load/validation only; storage is never written by migration.
export function migratePaint(doc) {
  if (doc.schemaVersion !== 1) return doc;
  if (!Array.isArray(doc.layers)) throw Error('Invalid layers');
  for (const l of doc.layers) {
    if (!Array.isArray(l.nodes)) throw Error('Invalid composition');
    const outputs = new Map(l.nodes.filter((n) => n.type === 'Output').map((n) => [n.id, n]));
    if (l.nodes.length + outputs.size > 2000)
      throw Error('Invalid composition: migration exceeds 2000 nodes');
    // Legacy Output also exposed geometry. Retain its geometry connections before inserting paint.
    const geometrySource = (source, seen = new Set()) => {
      if (!outputs.has(source)) return source;
      if (seen.has(source)) throw Error('Cycle detected');
      seen.add(source);
      return geometrySource(outputs.get(source).params?.path?.source, seen);
    };
    for (const n of l.nodes) {
      if (!n.params) throw Error('Invalid node');
      for (const [, q] of parameters(n)) {
        if (q.type === 'Path' && outputs.has(q.source) && n.type !== 'Output') {
          const source = geometrySource(q.source);
          if (source) q.source = source;
          else delete q.source;
        }
      }
    }
    for (const out of outputs.values()) {
      if (!out.params?.path || out.params.path.type !== 'Path')
        throw Error('Invalid parameter type');
      if (Object.keys(out.params).some((key) => !['path', 'fill'].includes(key)))
        throw Error('Unexpected parameter');
      const fill = node('Fill', out.position?.[0] ?? 40, (out.position?.[1] ?? 40) + 280);
      fill.params.path = { ...clone(out.params.path), type: 'PaintedPath' };
      fill.params.color = out.params.fill ? clone(out.params.fill) : p('Color', null);
      out.params.path = { ...out.params.path, type: 'PaintedPath', source: fill.id };
      delete out.params.fill;
      l.nodes.splice(l.nodes.indexOf(out), 0, fill);
    }
  }
  doc.schemaVersion = 2;
  return doc;
}
export function validate(doc) {
  let activeKeyers = 0;
  let activeImageDithers = 0;
  const idPattern = nodeIdPattern;
  const ids = new Set();
  function id(value) {
    if (typeof value !== 'string' || !idPattern.test(value) || ids.has(value))
      throw Error('Invalid or duplicate ID');
    ids.add(value);
  }
  function text(value) {
    if (typeof value !== 'string' || value.length > 100000) throw Error('Invalid text field');
  }
  function vec(value) {
    assertValue('Vector2', value);
  }
  function param(q, type, layerFillFallback = false) {
    validateColorSlotBinding(q, colorSlots);
    if (!q || q.type !== type) throw Error('Invalid parameter type');
    if (!(layerFillFallback && q.value === null)) assertValue(type, q.value);
    if (['Path', 'PaintedPath', 'Image'].includes(type) && q.value !== null)
      throw Error('Path fallback must be null');
    text(q.expression);
    if (
      type !== 'Color' &&
      usesBaseValue(q.expression) &&
      (doc.schemaVersion < 26 || !['Scalar', 'Vector2', 'Color'].includes(type))
    )
      throw Error('Authored value expressions require numeric parameters and schemaVersion 26');
    if (type === 'Color' && q.expression) {
      const required = colorExpressionSchemaVersion(q.expression);
      if (doc.schemaVersion < required)
        throw Error(`Color expressions require schemaVersion ${required}`);
      // Validate the typed grammar even when a connection pauses the expression.
      parameterExpressionUses(q);
    }
    if (typeof q.locked !== 'boolean') throw Error('Invalid lock state');
    if (q.source !== undefined && (typeof q.source !== 'string' || !idPattern.test(q.source)))
      throw Error('Invalid source ID');
    if (q.sourcePort !== undefined && (!q.source || !['x', 'y', 'position'].includes(q.sourcePort)))
      throw Error('Invalid source port');
  }
  if (
    !doc ||
    (![
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
      27, 28, 29, 30, 31, 32, 33, 34, 35,
    ].includes(doc.schemaVersion) &&
      doc.schemaVersion !== DITHER_SCHEMA_VERSION &&
      doc.schemaVersion !== IMAGE_DITHER_SCHEMA_VERSION &&
      doc.schemaVersion !== REPEATER_SCHEMA_VERSION &&
      doc.schemaVersion !== REPEATER_ALTERNATE_SCHEMA_VERSION &&
      doc.schemaVersion !== REPEATER_FILL_SCHEMA_VERSION &&
      doc.schemaVersion !== REPEATER_STROKE_SCHEMA_VERSION &&
      doc.schemaVersion !== GRID_ORIGIN_SCHEMA_VERSION &&
      doc.schemaVersion !== GRID_ANGLE_SCHEMA_VERSION &&
      doc.schemaVersion !== OPENTYPE_SCHEMA_VERSION &&
      doc.schemaVersion !== NODE_PRESET_SCHEMA_VERSION &&
      doc.schemaVersion !== BOOLEAN_INPUT_SCHEMA_VERSION &&
      doc.schemaVersion !== COLOR_EXPRESSION_SCHEMA_VERSION &&
      doc.schemaVersion !== COLOR_OPERATIONS_SCHEMA_VERSION &&
      doc.schemaVersion !== NODE_GROUP_SCHEMA_VERSION &&
      doc.schemaVersion !== MASK_FEATHER_SCHEMA_VERSION &&
      doc.schemaVersion !== IMAGE_MASK_SCHEMA_VERSION &&
      doc.schemaVersion !== MASK_LUMINANCE_SCHEMA_VERSION &&
      doc.schemaVersion !== SVG_ASSET_SCHEMA_VERSION &&
      doc.schemaVersion !== COLOR_SLOTS_SCHEMA_VERSION &&
      doc.schemaVersion !== COLOR_DESIGN_SCHEMA_VERSION &&
      doc.schemaVersion !== HARMONY_DESIGN_SCHEMA_VERSION &&
      doc.schemaVersion !== CHROMA_OVERRIDE_SCHEMA_VERSION &&
      doc.schemaVersion !== KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION &&
      doc.schemaVersion !== LIVE_KEY_COLOR_RECIPE_SCHEMA_VERSION &&
      doc.schemaVersion !== CURVE_POINT_REFERENCE_SCHEMA_VERSION &&
      doc.schemaVersion !== CURVE_HANDLE_REFERENCE_SCHEMA_VERSION &&
      doc.schemaVersion !== ARTWORK_REFERENCE_SCHEMA_VERSION &&
      doc.schemaVersion !== COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
      doc.schemaVersion !== QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
      doc.schemaVersion !== COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION)
  )
    throw Error('Unsupported project version');
  const colorSlots = validateColorSlots(doc);
  for (const slot of colorSlots.values()) id(slot.id);
  if (doc.schemaVersion === 1) migratePaint(doc);
  // Apply every migration against the file's incoming version. A feature later in
  // the node list must not advance the version before older nodes are backfilled.
  const sourceSchemaVersion = doc.schemaVersion;
  text(doc.name);
  validateFontAssets(doc.fontAssets);
  if (Object.values(doc.fontAssets || {}).some((a) => a.kind === VARIABLE_KIND))
    variableCheck(doc.schemaVersion >= 25, 'VARIABLE_SCHEMA');
  validateImageAssets(doc.imageAssets);
  for (const asset of Object.values(doc.imageAssets || {})) {
    if (asset.provenance !== undefined) {
      if (doc.schemaVersion < SVG_ASSET_SCHEMA_VERSION)
        throw Error('Asset provenance requires schemaVersion 53');
      validateAssetProvenance(asset.provenance);
    }
  }
  validateGuides(doc.editor, id);
  if (doc.editor?.gridAnchor === 'center' && doc.schemaVersion < 27)
    throw Error('Centered grid requires schemaVersion 27');
  if (
    doc.editor?.gridAngle !== undefined &&
    doc.editor.gridAngle !== 0 &&
    doc.schemaVersion < GRID_ANGLE_SCHEMA_VERSION
  )
    throw Error(`Angled grid requires schemaVersion ${GRID_ANGLE_SCHEMA_VERSION}`);
  if (
    doc.editor?.gridAnchor !== undefined &&
    !['top-left', 'center'].includes(doc.editor.gridAnchor) &&
    doc.schemaVersion < GRID_ORIGIN_SCHEMA_VERSION
  )
    throw Error(`Nine-point grid origin requires schemaVersion ${GRID_ORIGIN_SCHEMA_VERSION}`);
  if (!Array.isArray(doc.layers) || doc.layers.length < 1 || doc.layers.length > 100)
    throw Error('Invalid layers');
  validateLayerFolders(doc, id);
  for (const key of ['width', 'height'])
    if (!Number.isFinite(doc[key]) || doc[key] <= 0 || doc[key] > 100000)
      throw Error('Invalid artboard size');
  for (const l of doc.layers) {
    id(l.id);
    text(l.name);
    if (
      typeof l.visible !== 'boolean' ||
      (l.solo !== undefined && (typeof l.solo !== 'boolean' || sourceSchemaVersion < 33)) ||
      typeof l.locked !== 'boolean' ||
      (l.guide !== undefined && typeof l.guide !== 'boolean') ||
      (l.allowEmptyOutput !== undefined && typeof l.allowEmptyOutput !== 'boolean') ||
      !/^#[0-9a-fA-F]{6}$/.test(l.fill)
    )
      throw Error('Invalid layer state');
    if (!Array.isArray(l.nodes) || l.nodes.length > 2000) throw Error('Invalid composition');
    validateNodeGroups(l, sourceSchemaVersion, id);
    for (const n of l.nodes) {
      id(n.id);
      if (['Keyer', 'Mask'].includes(n.type) && sourceSchemaVersion < 35)
        throw Error('Keyer and Mask require schemaVersion 35');
      if (n.type === 'Keyer' && !n.bypassed && ++activeKeyers > MAX_ACTIVE_KEYERS)
        throw Error('At most 8 active Keyers per document');
      if (isImageDither(n.type) && sourceSchemaVersion < IMAGE_DITHER_SCHEMA_VERSION)
        throw Error('Image Dither requires schemaVersion 41');
      if (isImageDither(n.type) && !n.bypassed && ++activeImageDithers > MAX_ACTIVE_IMAGE_DITHERS)
        throw Error('At most 8 active Image Dithers per document');
      validateReference(n);
      validateSVGAsset(n, sourceSchemaVersion);
      validateCompositionReference(n, sourceSchemaVersion);
      if (
        n.paintMode !== undefined &&
        (n.paintMode !== true ||
          sourceSchemaVersion < 31 ||
          !['Transform', 'Join Geometry'].includes(n.type))
      )
        throw Error('Paint routing requires schemaVersion 31 and Transform or Join Geometry');
      text(n.name);
      if (n.bypassed !== undefined && (typeof n.bypassed !== 'boolean' || sourceSchemaVersion < 10))
        throw Error('Node bypass requires a boolean and schemaVersion 10');
      bypassInput(n, l);
      vec(n.position);
      if (n.width !== undefined && (!Number.isFinite(n.width) || n.width < 220 || n.width > 550))
        throw Error('Invalid node width');
      if (n.collapsed !== undefined && typeof n.collapsed !== 'boolean')
        throw Error('Invalid node collapse state');
      text(n.text);
      text(n.font);
      validateOsFontText(n, sourceSchemaVersion);
      validateTextFeatures(n, sourceSchemaVersion, doc.fontAssets?.[n.font]);
      if (
        n.canvasPositionHandle !== undefined &&
        (n.type !== 'Vector' || typeof n.canvasPositionHandle !== 'boolean')
      )
        throw Error('Invalid Vector canvas position handle setting');
      if (
        n.positionReference !== undefined &&
        (typeof n.positionReference !== 'string' ||
          !['Circle', 'Ellipse', 'Rectangle', 'Polygon', 'Star', 'Image'].includes(n.type) ||
          !/^(origin|(left|center|right)-(top|middle|bottom))$/.test(n.positionReference))
      )
        throw Error('Invalid object Position reference');
      if (n.characterArrangement !== undefined) {
        if (n.type !== 'Text' || sourceSchemaVersion < 24)
          throw Error('Character arrangement requires schemaVersion 24 and a Text node');
        validateCharacterArrangement(n.characterArrangement);
      }
      if (n.characterPositions !== undefined && (n.type !== 'Text' || sourceSchemaVersion < 3))
        throw Error('Character positions require schemaVersion 3 and a Text node');
      if (n.leading !== undefined && (n.type !== 'Text' || sourceSchemaVersion < 6))
        throw Error('Explicit leading requires schemaVersion 6 and a Text node');
      if (n.pathAttachment !== undefined && (n.type !== 'Text' || sourceSchemaVersion < 7))
        throw Error('Type on Path requires schemaVersion 7 and a Text node');
      if (n.textWrap !== undefined && (n.type !== 'Text' || sourceSchemaVersion < 8))
        throw Error('Wrap width requires schemaVersion 8 and a Text node');
      if (n.textWrap?.policy !== undefined && sourceSchemaVersion < 34)
        throw Error('Japanese basic wrapping requires schemaVersion 34');
      validateVariableText(n, doc.fontAssets?.[n.font], doc.schemaVersion);
      if (n.variation) {
        const sparse = canonicalAxes(
          resolveFonts(doc)[n.font].nectVariable.axes,
          n.variation.axes,
        ).sparse;
        if (JSON.stringify(n.variation.axes) !== JSON.stringify(sparse)) n.variation.axes = sparse;
      }
      for (const style of Object.values(n.characterPositions?.styles || {}))
        variableCheck(
          doc.fontAssets?.[style.font]?.kind !== VARIABLE_KIND,
          'VARIABLE_RANGE_UNSUPPORTED',
        );
      if (n.type === 'Text') {
        validateTextWrap(n.textWrap, n.writingMode, n.pathAttachment);
        validateTextPathAttachment(n.pathAttachment);
        if (n.pathAttachment) validateTextPathMode(n.text, n.writingMode);
        validateLeading(n.leading);
        validateCharacterPositions(n.characterPositions);
        if (n.characterPositions?.version >= 2 && sourceSchemaVersion < 4)
          throw Error('Character shape transforms require schemaVersion 4');
        if (n.characterPositions?.version >= 3 && sourceSchemaVersion < 5)
          throw Error('Character typography requires schemaVersion 5');
        if (n.characterPositions?.version >= 4 && sourceSchemaVersion < 6)
          throw Error('Character spacing requires schemaVersion 6');
        if (n.characterPositions?.version >= 5 && sourceSchemaVersion < 9)
          throw Error('Pair kerning requires schemaVersion 9');
        for (const style of Object.values(n.characterPositions?.styles || {}))
          if (
            style.font &&
            style.font !== 'default' &&
            !Object.hasOwn(doc.fontAssets || {}, style.font)
          )
            throw Error('Missing character font asset');
        if (n.referenceBounds !== undefined && !['visible', 'advance'].includes(n.referenceBounds))
          throw Error('Invalid text reference bounds');
        if (
          n.crossAlignment !== undefined &&
          !['origin', 'start', 'center', 'end'].includes(n.crossAlignment)
        )
          throw Error('Invalid cross-axis reference');
        if (n.alignment !== undefined && !['start', 'center', 'end'].includes(n.alignment))
          throw Error('Invalid text alignment');
        if (n.kerning !== undefined && typeof n.kerning !== 'boolean')
          throw Error('Invalid kerning setting');
        if (n.font !== 'default' && !Object.hasOwn(doc.fontAssets || {}, n.font))
          throw Error('Missing font asset');
        if (
          n.writingMode !== undefined &&
          !['horizontal-tb', 'vertical-rl'].includes(n.writingMode)
        )
          throw Error('Invalid writing mode');
      }
      if (n.type === 'Image' && !Object.hasOwn(doc.imageAssets || {}, n.asset))
        throw Error('Missing image asset');
      if (n.rawPath !== undefined) text(n.rawPath);
      if (n.type === 'Imported Path') parsePath(n.rawPath, n.id);
      if (
        !Object.hasOwn(definitions, n.type) ||
        !n.params ||
        !Array.isArray(n.points) ||
        !Array.isArray(n.edits)
      )
        throw Error('Invalid node');
      if (
        ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'].includes(n.type) &&
        sourceSchemaVersion < 15
      ) {
        if (!Object.hasOwn(n.params, 'gridOrigin'))
          n.params.gridOrigin = clone(definitions[n.type].gridOrigin);
      }
      if (Object.hasOwn(definitions[n.type], 'tonePower') && sourceSchemaVersion < 21) {
        if (!Object.hasOwn(n.params, 'tonePower'))
          n.params.tonePower = clone(definitions[n.type].tonePower);
      }
      if (n.type === 'Expand Stroke' && sourceSchemaVersion < 16) {
        for (const key of ['startWidthMultiplier', 'endWidthMultiplier'])
          if (!Object.hasOwn(n.params, key)) n.params[key] = clone(definitions[n.type][key]);
      }
      if (n.type === 'Geometry Repeater') {
        if (n.operation === 'ae-2d' && sourceSchemaVersion < REPEATER_SCHEMA_VERSION)
          throw Error(`2D Repeater requires schemaVersion ${REPEATER_SCHEMA_VERSION}`);
        if (sourceSchemaVersion < REPEATER_SCHEMA_VERSION)
          for (const key of REPEATER_KEYS)
            if (!Object.hasOwn(n.params, key))
              n.params[key] = clone(definitions['Geometry Repeater'][key]);
        if (sourceSchemaVersion < REPEATER_ALTERNATE_SCHEMA_VERSION) {
          if (!Object.hasOwn(n.params, 'alternateScale'))
            n.params.alternateScale = clone(definitions['Geometry Repeater'].alternateScale);
          const q = n.params.alternateScale;
          if (q.value !== 100 || q.expression || q.source)
            throw Error(
              `Repeater Alternate Scale requires schemaVersion ${REPEATER_ALTERNATE_SCHEMA_VERSION}`,
            );
        }
        if (sourceSchemaVersion < REPEATER_FILL_SCHEMA_VERSION) {
          for (const key of ['alternateFill', 'alternateColor'])
            if (!Object.hasOwn(n.params, key))
              n.params[key] = clone(definitions['Geometry Repeater'][key]);
          if (hasRepeaterFillAuthorship(n))
            throw Error(
              `Repeater Alternate Fill requires schemaVersion ${REPEATER_FILL_SCHEMA_VERSION}`,
            );
        }
        if (sourceSchemaVersion < REPEATER_STROKE_SCHEMA_VERSION) {
          for (const key of ['alternateStroke', 'alternateStrokeColor'])
            if (!Object.hasOwn(n.params, key))
              n.params[key] = clone(definitions['Geometry Repeater'][key]);
          if (hasRepeaterStrokeAuthorship(n))
            throw Error(
              `Repeater Alternate Stroke requires schemaVersion ${REPEATER_STROKE_SCHEMA_VERSION}`,
            );
        }
      }
      if (n.type === 'Resample Path' && sourceSchemaVersion < 12) {
        if (n.operation === 'spacing') throw Error('Resample spacing requires schemaVersion 12');
        if (operationOptions.Boolean.includes(n.operation)) n.operation = 'count';
        if (!Object.hasOwn(n.params, 'spacing'))
          n.params.spacing = clone(definitions['Resample Path'].spacing);
      }
      // Instance on Points gains an opt-in local-tangent transform. Old files
      // load with the identity (translation-only) setting, then serialize using
      // the schema that records the new parameter.
      if (n.type === 'Instance on Points' && sourceSchemaVersion < 13 && n.params) {
        if (!Object.hasOwn(n.params, 'alignTangent'))
          n.params.alignTangent = clone(definitions['Instance on Points'].alignTangent);
      }
      migrateDither(n, sourceSchemaVersion);
      if (!(operationOptions[n.type] || operationOptions.Boolean).includes(n.operation))
        throw Error('Invalid operation');
      // Older schema-v1 Text nodes omit spacing; migrate only the documented new fields.
      if (n.type === 'Text')
        for (const key of ['letterSpacing', 'lineSpacing'])
          if (!Object.hasOwn(n.params, key)) n.params[key] = clone(definitions.Text[key]);
      if (n.type === 'Transform' && !Object.hasOwn(n.params, 'anchor'))
        n.params.anchor = clone(definitions.Transform.anchor);
      if (n.type === 'Stroke') {
        validateStrokeStyle(n);
        for (const key of ['miterLimit', 'dashOffset'])
          if (!Object.hasOwn(n.params, key)) n.params[key] = clone(definitions.Stroke[key]);
      }
      if (n.type !== 'Join Geometry' && n.joinInputOrder !== undefined)
        throw Error('Join input order requires Join Geometry');
      if (n.type !== 'Boolean' && n.booleanInputOrder !== undefined)
        throw Error('Boolean input order requires Boolean');
      if (n.type === 'Join Geometry') {
        if (sourceSchemaVersion < 23) {
          if (
            n.joinInputOrder !== undefined ||
            Object.keys(n.params).length !== 2 ||
            !Object.hasOwn(n.params, 'a') ||
            !Object.hasOwn(n.params, 'b')
          )
            throw Error('Expandable Join inputs require schemaVersion 23');
          normalizeJoinInputs(n, false, true);
        }
        const keys = n.joinInputOrder;
        if (!validJoinSlotOrder(keys, n.params)) throw Error('Invalid Join input order');
        for (const key of keys) {
          const q = n.params[key];
          param(q, n.paintMode ? 'PaintedPath' : 'Path');
          if (
            q.expression ||
            Object.keys(q).some(
              (field) =>
                !['type', 'value', 'expression', 'locked', 'source', 'sourcePort'].includes(field),
            )
          )
            throw Error('Invalid Join input parameter');
        }
      }
      if (n.type === 'Boolean') {
        if (sourceSchemaVersion < BOOLEAN_INPUT_SCHEMA_VERSION) {
          if (
            n.booleanInputOrder !== undefined ||
            Object.keys(n.params).length !== 2 ||
            !Object.hasOwn(n.params, 'a') ||
            !Object.hasOwn(n.params, 'b')
          )
            throw Error(
              'Expandable Boolean inputs require schemaVersion ' + BOOLEAN_INPUT_SCHEMA_VERSION,
            );
        } else if (n.booleanInputOrder !== undefined && !Array.isArray(n.booleanInputOrder))
          throw Error('Invalid Boolean input order');
        const keys = booleanInputKeys(n);
        if (
          !Array.isArray(keys) ||
          keys.length < 2 ||
          keys.length > JOIN_SLOT_LIMIT ||
          keys[0] !== 'a' ||
          keys[1] !== 'b' ||
          new Set(keys).size !== keys.length ||
          Object.keys(n.params).length !== keys.length ||
          keys.some(
            (key, index) =>
              typeof key !== 'string' ||
              (index > 1 && !/^input_[A-Za-z0-9_-]{1,100}$/.test(key)) ||
              !Object.hasOwn(n.params, key),
          )
        )
          throw Error('Invalid Boolean input order');
        if (n.booleanInputOrder) {
          const last = n.params[keys.at(-1)];
          if (keys.length < JOIN_SLOT_LIMIT && (last.source || last.locked))
            throw Error('Boolean input order must retain an empty spare socket');
        }
        for (const key of keys) {
          const q = n.params[key];
          param(q, 'Path');
          if (
            q.expression ||
            Object.keys(q).some(
              (field) =>
                !['type', 'value', 'expression', 'locked', 'source', 'sourcePort'].includes(field),
            )
          )
            throw Error('Invalid Boolean input parameter');
        }
      }
      const extraTransform = isTransform(n)
        ? Object.keys(n.params).filter((key) => Object.hasOwn(transformExtensions, key))
        : [];
      const extraMaskFeather = n.type === 'Mask' && Object.hasOwn(n.params, 'feather');
      if (extraMaskFeather) {
        if (sourceSchemaVersion < MASK_FEATHER_SCHEMA_VERSION)
          throw Error('Mask Feather requires schemaVersion 50');
        param(n.params.feather, 'Scalar');
        maskFeather(n.params.feather.value);
      }
      const extraMaskLuminance = n.type === 'Mask' && Object.hasOwn(n.params, 'luminance');
      if (extraMaskLuminance) {
        if (sourceSchemaVersion < MASK_LUMINANCE_SCHEMA_VERSION)
          throw Error('Mask Luminance requires schemaVersion 52');
        param(n.params.luminance, 'Boolean');
      }
      if ((extraTransform.length || n.type === 'Vector Transform') && sourceSchemaVersion < 11)
        throw Error('Extended Transform requires schemaVersion 11');
      if (
        Object.keys(n.params).length !==
        Object.keys(definitions[n.type]).length +
          extraTransform.length +
          (extraMaskFeather ? 1 : 0) +
          (extraMaskLuminance ? 1 : 0) +
          (n.type === 'Join Geometry' ? n.joinInputOrder.length - 2 : 0) +
          (n.type === 'Boolean' && n.booleanInputOrder ? n.booleanInputOrder.length - 2 : 0)
      )
        throw Error('Unexpected parameter');
      for (const key of extraTransform) param(n.params[key], transformExtensions[key].type);
      if (n.params.skew && Math.abs(n.params.skew.value) >= 89.9)
        throw Error('Skew must be between -89.9 and 89.9 degrees');
      for (const [key, def] of Object.entries(definitions[n.type]))
        param(
          n.params[key],
          n.paintMode && def.type === 'Path' ? 'PaintedPath' : def.type,
          n.type === 'Fill' && key === 'color',
        );
      if (n.type === 'Keyer')
        keyerSettings(n.params.keyColor.value, n.params.tolerance.value, n.params.softness.value);
      if (
        n.type === 'Resample Path' &&
        (!Number.isInteger(n.params.count.value) ||
          n.params.count.value < 2 ||
          n.params.count.value > 1024)
      )
        throw Error('Resample count must be an integer from 2 to 1024');
      if (
        n.type === 'Resample Path' &&
        (n.params.spacing.value < 0.0001 || n.params.spacing.value > 100000)
      )
        throw Error('Resample spacing must be from 0.0001 to 100000 px');
      if (
        n.type === 'Subdivide Path' &&
        (!Number.isInteger(n.params.segments.value) ||
          n.params.segments.value < 1 ||
          n.params.segments.value > 32)
      )
        throw Error('Subdivide segments must be an integer from 1 to 32');
      if (
        n.type === 'Roughen' &&
        (n.params.amount.value < 0 ||
          n.params.amount.value > 100000 ||
          !Number.isInteger(n.params.seed.value) ||
          n.params.seed.value < 0 ||
          n.params.seed.value > 2147483647)
      )
        throw Error('Roughen needs amount 0–100000 px and integer seed 0–2147483647');
      if (
        n.type === 'Smooth Path' &&
        (n.params.factor.value < 0 ||
          n.params.factor.value > 1 ||
          !Number.isInteger(n.params.iterations.value) ||
          n.params.iterations.value < 1 ||
          n.params.iterations.value > 50)
      )
        throw Error('Smooth Path needs factor 0–1 and integer iterations 1–50');
      if (
        n.type === 'Simplify Path' &&
        (n.params.tolerance.value < 0 || n.params.tolerance.value > 100000)
      )
        throw Error('Simplify Path tolerance must be from 0 to 100000 px');
      if (n.type === 'Dash Path')
        dashIntervals(0, {
          dashLength: n.params.dashLength.value,
          gapLength: n.params.gapLength.value,
          offset: n.params.offset.value,
        });
      if (
        n.type === 'Pucker & Bloat' &&
        (n.params.amount.value < -100 || n.params.amount.value > 100)
      )
        throw Error('Pucker & Bloat amount must be from -100 to 100');
      if (
        n.type === 'Wave Points' &&
        (Math.abs(n.params.amplitude.value) > 100000 ||
          n.params.wavelength.value < 0.0001 ||
          n.params.wavelength.value > 100000 ||
          Math.abs(n.params.phase.value) > 360)
      )
        throw Error(
          'Wave Points needs amplitude within ±100000 px, wavelength 0.0001–100000 px and phase within ±360 degrees',
        );
      if (n.type === 'Polar Transform')
        polarSettings(
          Object.fromEntries(
            ['origin', 'center', 'width', 'height', 'radius', 'angle', 'inverse'].map((key) => [
              key,
              n.params[key].value,
            ]),
          ),
        );
      if (n.type === 'Linear Gradient Fill') linearGradientSettings((key) => n.params[key].value);
      if (n.type === 'Radial Gradient Fill') radialGradientSettings((key) => n.params[key].value);
      if (n.type === 'Angular Gradient Fill') angularGradientSettings((key) => n.params[key].value);
      if (
        isImageDither(n.type) ||
        ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'].includes(n.type)
      )
        luminanceModifierSettings((key) => n.params[key].value);
      readDotSettings(n, (key) => n.params[key].value);
      if (['Luminance Instances', 'Luminance to Points'].includes(n.type)) {
        luminanceModifierSettings((key) => n.params[key].value);
        luminanceInstanceSettings((key) => n.params[key].value);
      }
      if (n.type === 'Closed Path Offset') closedOffsetSettings((key) => n.params[key].value);
      if (['Polygon', 'Star'].includes(n.type))
        radialSettings(n.type, (key) => n.params[key].value);
      if (
        n.type === 'Geometry Repeater' &&
        n.operation !== 'ae-2d' &&
        (!Number.isInteger(n.params.count.value) ||
          n.params.count.value < 1 ||
          n.params.count.value > 256)
      )
        throw Error('Repeater count must be an integer from 1 to 256');
      if (n.type === 'Geometry Repeater' && n.operation === 'ae-2d')
        repeaterSettings((key) => n.params[key].value);
      if (
        n.type === 'Corner Round' &&
        (n.params.radius.value < 0 || n.params.radius.value > 100000)
      )
        throw Error('Corner Round radius must be from 0 to 100000 px');
      if (n.type === 'Trim Path') {
        const start = n.params.start.value,
          end = n.params.end.value;
        if (start < 0 || start > 1) throw Error('Trim Path start must be from 0 to 1');
        if (end < 0 || end > 1) throw Error('Trim Path end must be from 0 to 1');
        if (
          !n.params.start.source &&
          !n.params.start.expression &&
          !n.params.end.source &&
          !n.params.end.expression &&
          start > end
        )
          throw Error('Trim Path start must not exceed end');
      }
      if (n.type === 'Taper' && (n.params.amount.value < -50 || n.params.amount.value > 50))
        throw Error('Taper amount must be from -50 to 50');
      if (
        n.type === 'Bend Points' &&
        (!Number.isFinite(n.params.angle.value) ||
          n.params.angle.value < -180 ||
          n.params.angle.value > 180)
      )
        throw Error('Bend Points angle must be finite and from -180 to 180 degrees');
      if (
        n.type === 'Twist Points' &&
        (!Number.isFinite(n.params.angle.value) ||
          n.params.angle.value < -360 ||
          n.params.angle.value > 360)
      )
        throw Error('Twist Points angle must be finite and from -360 to 360 degrees');
      if (n.type === 'Auto Handles' && (n.params.strength.value < 0 || n.params.strength.value > 1))
        throw Error('Auto Handles strength must be from 0 to 1');
      if (n.type === 'Set Path Start' && (n.params.start.value < 0 || n.params.start.value > 1))
        throw Error('Set Path Start start must be from 0 to 1');
      if (n.type === 'Stroke' && (n.params.width.value < 0 || n.params.width.value > 100000))
        throw Error('Stroke width must be from 0 to 100000 px');
      if (
        n.type === 'Expand Stroke' &&
        ['startWidthMultiplier', 'endWidthMultiplier'].some(
          (key) => n.params[key].value < 0 || n.params[key].value > 4,
        )
      )
        throw Error('Expand Stroke width multipliers must be from 0 to 4');
      if (
        n.type === 'Stroke' &&
        (n.params.miterLimit.value < 1 ||
          n.params.miterLimit.value > 10000 ||
          Math.abs(n.params.dashOffset.value) > 100000)
      )
        throw Error('Invalid stroke miter limit or dash offset');
      if (n.type === 'Multi Path') {
        if (sourceSchemaVersion < 22) throw Error('Multi Path requires schemaVersion 22');
        if (!Array.isArray(n.contours) || n.contours.length > 10000)
          throw Error('Multi Path exceeds contour budget');
        const pointIds = new Set(n.points.map((pt) => pt.id)),
          used = new Set(),
          contourIds = new Set();
        for (const contour of n.contours) {
          if (
            !contour ||
            typeof contour.id !== 'string' ||
            !idPattern.test(contour.id) ||
            contourIds.has(contour.id) ||
            typeof contour.closed !== 'boolean' ||
            !Array.isArray(contour.points) ||
            (contour.group !== undefined &&
              (typeof contour.group !== 'string' || !idPattern.test(contour.group)))
          )
            throw Error('Invalid Multi Path contour');
          contourIds.add(contour.id);
          for (const pointId of contour.points) {
            if (!pointIds.has(pointId) || used.has(pointId))
              throw Error('Invalid Multi Path point reference');
            used.add(pointId);
          }
        }
        if (used.size !== pointIds.size) throw Error('Unassigned Multi Path point');
        if (!Array.isArray(n.units) || n.units.length > 10000)
          throw Error('Invalid Multi Path units');
        const unitIds = new Set(),
          assignedContours = new Set();
        for (const unit of n.units) {
          if (
            !unit ||
            typeof unit.id !== 'string' ||
            !idPattern.test(unit.id) ||
            unitIds.has(unit.id) ||
            !Array.isArray(unit.contours) ||
            !unit.contours.length
          )
            throw Error('Invalid Multi Path unit');
          unitIds.add(unit.id);
          for (const contourId of unit.contours) {
            if (!contourIds.has(contourId) || assignedContours.has(contourId))
              throw Error('Invalid Multi Path unit contour');
            assignedContours.add(contourId);
          }
        }
        if (assignedContours.size !== contourIds.size) throw Error('Unassigned Multi Path contour');
      }
      if (n.points.length > 10000 || n.edits.length > 10000) throw Error('Too many vertices');
      for (const pt of n.points) {
        id(pt.id);
        for (const k of ['position', 'handleA', 'handleB']) param(pt[k], 'Vector2');
        if (!['corner', 'smooth', 'symmetric'].includes(pt.handleType))
          throw Error('Invalid handle type');
      }
      const editIds = new Set();
      for (const edit of n.edits) {
        if (editIds.has(edit.pointId)) throw Error('Duplicate edit target');
        editIds.add(edit.pointId);
        if (typeof edit.pointId !== 'string' || !idPattern.test(edit.pointId))
          throw Error('Invalid edit target');
        if (edit.fingerprint !== undefined) text(edit.fingerprint);
        if (edit.params) {
          if (Object.keys(edit.params).length !== 3) throw Error('Invalid remap parameters');
          for (const k of ['position', 'handleA', 'handleB']) param(edit.params[k], 'Vector2');
        }
        for (const k of ['position', 'handleA', 'handleB']) if (edit[k] !== undefined) vec(edit[k]);
      }
    }
    if (!l.nodes.some((n) => n.id === l.output && ['Output', 'Image Output'].includes(n.type)))
      throw Error('Missing Output node');
    if (l.allowEmptyOutput === true && l.nodes.find((n) => n.id === l.output).type !== 'Output')
      throw Error('Empty output requires a vector Output');
    for (const n of l.nodes)
      for (const [key, q] of parameters(n))
        if (q.source) connect(l, q.source, n.id, key, true, q.sourcePort);
  }
  // Cross-layer references are forbidden, while genuinely deleted IDs remain
  // loadable as diagnosed dangling targets rather than silently losing the link.
  const owners = new Map(doc.layers.flatMap((l) => l.nodes.map((n) => [n.id, l.id])));
  const layersById = new Map(doc.layers.map((layer) => [layer.id, layer]));
  for (const l of doc.layers) {
    const localNodes = new Map(l.nodes.map((item) => [item.id, item]));
    for (const n of l.nodes)
      if (n.type === 'Reference' && n.reference) {
        const owner = owners.get(n.reference.nodeId);
        if (owner !== undefined && owner !== l.id)
          throw Error('Reference target must be in the same layer');
        if (owner !== undefined) referenceTarget(l, n);
      }
    for (const n of l.nodes)
      if (n.pathAttachment) {
        const target = l.nodes.find((x) => x.id === n.pathAttachment.nodeId);
        if (!target || outputType(target) !== 'Path')
          throw Error('TEXT_PATH_SOURCE: Select a Path output in the same layer');
      }
    for (const n of l.nodes)
      for (const [, q] of parameters(n))
        if (q.expression) {
          for (const ref of expressionReferencesForSchema(q, sourceSchemaVersion)) {
            if (!ref.compositionId && ref.layerId === l.id)
              throw Error('Artwork reference requires a different layer');
            if (
              !ref.compositionId &&
              owners.has(ref.nodeId) &&
              owners.get(ref.nodeId) !== (ref.layerId || l.id)
            )
              throw Error('Expression reference target must be in the same layer');
            const sourceLayer = ref.compositionId
              ? null
              : ref.layerId
                ? layersById.get(ref.layerId)
                : l;
            const owner = ref.layerId
              ? sourceLayer?.nodes.find((item) => item.id === ref.nodeId)
              : localNodes.get(ref.nodeId);
            const pointRef = owner?.type === 'Curve' && curvePointReference(ref.key, ref.component);
            if (pointRef && sourceSchemaVersion < pointRef.schemaVersion)
              throw Error(`Curve point references require schemaVersion ${pointRef.schemaVersion}`);
            if (owner) {
              try {
                referenceParameter(sourceLayer, ref, ref.type);
              } catch (error) {
                if (error.code !== 'EXPRESSION_REFERENCE_MISSING') throw error;
              }
            }
          }
        }
    assertAcyclic(l);
  }
  assertCompositionAcyclic(doc);
  if (
    sourceSchemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION &&
    doc.layers.some(
      (layer) =>
        nodesHaveArtworkReferences(layer.nodes) ||
        (sourceSchemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
          nodesHaveQualifiedColorReferenceSyntax(layer.nodes)),
    )
  )
    assertDocumentReferenceAcyclic(doc);
  const hasNodeType = (type) =>
    doc.layers.some((layer) => layer.nodes.some((n) => n.type === type));
  const hasImageLuminanceConnection = doc.layers.some((layer) => {
    const nodes = new Map(layer.nodes.map((n) => [n.id, n]));
    return layer.nodes.some(
      (target) =>
        imageLuminanceTargets.has(target.type) &&
        target.params.path?.source &&
        outputType(nodes.get(target.params.path.source), target.params.path.sourcePort) === 'Image',
    );
  });
  const hasPaintedPathConversion = doc.layers.some((layer) => {
    const nodes = new Map(layer.nodes.map((n) => [n.id, n]));
    return layer.nodes.some(
      (target) =>
        target.type === 'Convert to Path' &&
        target.params.path?.source &&
        outputType(nodes.get(target.params.path.source), target.params.path.sourcePort) ===
          'PaintedPath',
    );
  });
  if (hasNodeType('Instance on Points') && sourceSchemaVersion < 13)
    doc.schemaVersion = Math.max(doc.schemaVersion, 13);
  if (hasNodeType('Line Screen') && sourceSchemaVersion < 14)
    doc.schemaVersion = Math.max(doc.schemaVersion, 14);
  if (
    ['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'].some((type) =>
      hasNodeType(type),
    ) &&
    sourceSchemaVersion < 15
  )
    doc.schemaVersion = Math.max(doc.schemaVersion, 15);
  if (hasNodeType('Expand Stroke') && sourceSchemaVersion < 16)
    doc.schemaVersion = Math.max(doc.schemaVersion, 16);
  if (hasImageLuminanceConnection && doc.schemaVersion < 17) doc.schemaVersion = 17;
  if (
    doc.schemaVersion < 21 &&
    doc.layers.some((layer) =>
      layer.nodes.some((n) => Object.hasOwn(definitions[n.type], 'tonePower')),
    )
  )
    doc.schemaVersion = Math.max(doc.schemaVersion, 21);
  if (hasNodeType('Luminance Instances') && doc.schemaVersion < 20)
    doc.schemaVersion = Math.max(doc.schemaVersion, 20);
  if (hasNodeType('Error Diffusion Dither') && doc.schemaVersion < 19)
    doc.schemaVersion = Math.max(doc.schemaVersion, 19);
  if (hasPaintedPathConversion && sourceSchemaVersion < 18)
    doc.schemaVersion = Math.max(doc.schemaVersion, 18);
  if (hasNodeType('Join Geometry')) doc.schemaVersion = Math.max(doc.schemaVersion, 23);
  if (
    doc.layers.some((layer) => layer.nodes.some((n) => n.type === 'Boolean' && n.booleanInputOrder))
  )
    doc.schemaVersion = Math.max(doc.schemaVersion, BOOLEAN_INPUT_SCHEMA_VERSION);
  if (
    hasNodeType('Luminance to Points') ||
    doc.layers.some((layer) => layer.nodes.some((n) => hasDotControls(n.type)))
  )
    doc.schemaVersion = Math.max(doc.schemaVersion, DITHER_SCHEMA_VERSION);
  if (doc.nodePresets !== undefined) {
    if (
      sourceSchemaVersion < NODE_PRESET_SCHEMA_VERSION ||
      !Array.isArray(doc.nodePresets) ||
      doc.nodePresets.length > 100
    )
      throw Error('Invalid node presets or preset schema');
    const names = new Set();
    for (const preset of doc.nodePresets) {
      if (!preset || typeof preset !== 'object') throw Error('Invalid node preset');
      id(preset.id);
      if (presetName(preset.name) !== preset.name || names.has(preset.name))
        throw Error('Duplicate or invalid preset name');
      names.add(preset.name);
      validateNodePreset(doc, preset);
    }
  }
  if (sourceSchemaVersion < IMAGE_MASK_SCHEMA_VERSION && doc.layers.some(hasImageMask))
    throw Error('Image alpha Mask requires schemaVersion 51');
  return doc;
}
export function serialize(doc) {
  // This optional historical receipt must never lose non-JSON data in a direct
  // codec call, even when the caller did not construct a Session first.
  if (doc && Object.hasOwn(doc, 'keyColorRecipe')) validateColorSlots(doc);
  const text = JSON.stringify(doc, null, 2);
  if (text.length > MAX_PROJECT_BYTES || utf8ByteLength(text) > MAX_PROJECT_BYTES)
    throw Error('Project exceeds 50 MB');
  return text;
}
export function deserialize(text) {
  if (text.length > MAX_PROJECT_BYTES || utf8ByteLength(text) > MAX_PROJECT_BYTES)
    throw Error('Project exceeds 50 MB');
  return validate(JSON.parse(text));
}
const escape = (s) =>
  String(s).replace(
    /[&<>"']/g,
    (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&apos;' })[c],
  );
// Callers allocate one scene-local ID per top-level layer. The same paint server is
// reused by all independent contour groups, preserving a single artboard-space ramp.
export function renderElement(
  value,
  l,
  assets = {},
  attributes = '',
  gradientId = 'nect-gradient-0',
  precision,
) {
  if (isPaintedGroup(value)) {
    const content = value.children
      .map((child, i) => renderElement(child, l, assets, '', `${gradientId}-${i}`, precision))
      .join('');
    const mask = value.mask;
    const id = `${gradientId}-mask`;
    const definition =
      mask?.shape?.kind === 'image'
        ? imageAlphaMaskSVG(
            mask,
            id,
            renderElement(mask.shape, l, assets, '', `${gradientId}-mask-image`, precision),
            precision,
          )
        : mask?.feather > 0
          ? featherMaskSVG(mask, id, pathData(mask.shape, precision), precision)
          : mask
            ? `<defs><mask id="${id}" maskUnits="userSpaceOnUse" maskContentUnits="userSpaceOnUse" x="${formatSvgNumber(mask.region[0], precision)}" y="${formatSvgNumber(mask.region[1], precision)}" width="${formatSvgExtent(mask.region[2], precision)}" height="${formatSvgExtent(mask.region[3], precision)}" style="mask-type:luminance" color-interpolation="sRGB">${mask.invert ? `<rect x="${formatSvgNumber(mask.region[0], precision)}" y="${formatSvgNumber(mask.region[1], precision)}" width="${formatSvgExtent(mask.region[2], precision)}" height="${formatSvgExtent(mask.region[3], precision)}" fill="white"/>` : ''}<path d="${escape(pathData(mask.shape, precision))}" fill="${mask.invert ? 'black' : 'white'}" fill-rule="nonzero"/></mask></defs>`
            : '';
    // The mask and its source share the group's local coordinates. A later
    // appearance transform moves both, preserving nested masks and source alpha.
    const matrix = value.matrix.map((number) => formatSvgNumber(number, precision));
    if (
      precision !== undefined &&
      value.matrix[0] * value.matrix[3] - value.matrix[1] * value.matrix[2] !== 0 &&
      Number(matrix[0]) * Number(matrix[3]) - Number(matrix[1]) * Number(matrix[2]) === 0
    )
      throw Error(
        `SVG precision ${precision} decimals collapses small geometry; choose a higher precision or Default`,
      );
    return `<g ${attributes} data-layer="${escape(l.name)}" transform="matrix(${matrix.join(' ')})">${mask ? `${definition}<g mask="url(#${id})">${content}</g>` : content}</g>`;
  }
  if (value.kind === 'image') {
    const asset = imageAssetFor(value, assets);
    return `<image ${attributes} data-layer="${escape(l.name)}" x="${formatSvgNumber(value.position[0], precision)}" y="${formatSvgNumber(value.position[1], precision)}" width="${formatSvgExtent(value.size[0], precision)}" height="${formatSvgExtent(value.size[1], precision)}" preserveAspectRatio="none" href="${imageURL(asset)}"/>`;
  }
  const fill = value.fill,
    stroke = value.stroke,
    gradient =
      fill?.kind === 'linear-gradient'
        ? renderLinearGradientFill(fill, gradientId, precision)
        : fill?.kind === 'radial-gradient'
          ? renderRadialGradientFill(fill, gradientId, precision)
          : fill?.kind === 'angular-gradient'
            ? renderAngularGradientFill(fill, gradientId, value, precision)
            : null;
  if (fill && !gradient) assertValue('Color', fill);
  if (stroke) {
    if (stroke.kind !== undefined && stroke.kind !== 'solid')
      throw Error('Only solid strokes are supported; gradient/pattern strokes are unsupported');
    assertValue('Color', { hex: stroke.hex, alpha: stroke.alpha });
    if (!Number.isFinite(stroke.width) || stroke.width < 0 || stroke.width > 100000)
      throw Error('Invalid stroke width');
  }
  if (!value.contours.some((c) => c.points.length)) return '';
  function shape(path, attrs = '', solid = fill, pen = stroke) {
    if (path.contours.some((c) => c.group !== undefined))
      return `<g ${attrs} data-layer="${escape(l.name)}">${contourGroups(path)
        .map((contours) => {
          const opacity = contours[0].repeaterOpacity;
          // Per-copy appearance metadata already shares the opacity group. Only
          // override a final solid fill; unpainted/gradient paths keep their paint.
          const copyFill =
            fill && !gradient && contours[0].repeaterFill ? contours[0].repeaterFill : solid;
          if (copyFill && !gradient) assertValue('Color', copyFill);
          const replacementStroke = contours[0].repeaterStroke;
          if (stroke && replacementStroke) assertValue('Color', replacementStroke);
          const copyStroke =
            stroke && replacementStroke ? { ...stroke, ...replacementStroke } : pen;
          const content = shape(
            { ...path, contours: contours.map(({ group, ...c }) => c) },
            '',
            copyFill,
            copyStroke,
          );
          return opacity === undefined
            ? content
            : `<g opacity="${formatSvgNumber(opacity, precision)}">${content}</g>`;
        })
        .join('')}</g>`;
    return `<path ${attrs} data-layer="${escape(l.name)}" d="${escape(pathData(path, precision))}" fill="${gradient ? gradient.paint : solid ? escape(solid.hex) : 'none'}"${solid && !gradient ? ` fill-opacity="${formatSvgNumber(solid.alpha, precision)}"` : ''} fill-rule="nonzero"${pen ? ` stroke="${escape(pen.hex)}" stroke-opacity="${formatSvgNumber(pen.alpha, precision)}" stroke-width="${formatSvgExtent(pen.width, precision)}" stroke-linecap="${pen.cap}" stroke-linejoin="${pen.join}" stroke-miterlimit="${formatSvgNumber(pen.miterLimit, precision)}" stroke-dasharray="${pen.dash.length ? pen.dash.map((number) => formatSvgNumber(number, precision)).join(' ') : 'none'}" stroke-dashoffset="${formatSvgNumber(pen.dashOffset, precision)}"` : ''}/>`;
  }
  return `${gradient ? `<defs>${gradient.definition}</defs>` : ''}${shape(value, attributes)}`;
}
// Validation includes legacy migrations and asset freezing. Keep both private to
// export, including for mutable callers; cloneDocument would freeze their assets.
export function exportScene(doc, fonts = {}) {
  return exportValidatedScene(validate(inheritProjectDocumentContext(doc, clone(doc))), fonts);
}
function exportValidatedScene(doc, fonts) {
  fonts = resolveFonts(doc, fonts);
  assertVariableDocumentReady(doc, fonts);
  return doc.layers
    .filter((l) => layerIsVisible(doc, l) && !l.guide)
    .map((l) => {
      const result = evaluate(l, fonts, doc.imageAssets, doc);
      if (result.error) throw Error(`${l.name}: ${result.error}`);
      if (result.path?.exportBlockers?.length)
        throw Error(`${l.name}: ${result.path.exportBlockers.join('; ')}`);
      const arrangedSuspension = result.path?.characterOccurrences?.find(
        (o) => o.arrangementStatus?.suspended,
      );
      if (arrangedSuspension)
        throw Error(
          `${l.name}: Maintained ink gaps suspended: ${arrangedSuspension.arrangementStatus.reason}`,
        );
      if (result.path?.characterOccurrences?.some((o) => o.suspended))
        throw Error(
          `${l.name}: Character positions are suspended; restore text/mapping or reset all adjustments before export`,
        );
      const paintProvenance = evaluatedPaintSlots.get(result.path);
      return {
        layer: l,
        value: result.path,
        ...(paintProvenance ? { paintProvenance: clone(paintProvenance) } : {}),
      };
    });
}
export function exportSVG(doc, fonts = {}, options = {}) {
  if (
    !options ||
    typeof options !== 'object' ||
    Array.isArray(options) ||
    Object.keys(options).some((key) => !['unit', 'responsive', 'precision'].includes(key)) ||
    (options.responsive !== undefined && typeof options.responsive !== 'boolean')
  )
    throw Error('Invalid SVG export options');
  const precision = validateSvgPrecision(options.precision);
  doc = validate(inheritProjectDocumentContext(doc, clone(doc)));
  const scene = exportValidatedScene(doc, fonts);
  const rootAttributes = svgRootAttributes(doc, options.unit ?? 'px', {
    responsive: options.responsive ?? false,
  });
  let bytes = 512;
  const elements = [];
  for (const { layer, value } of scene) {
    const element = renderElement(
      value,
      layer,
      doc.imageAssets,
      '',
      `nect-gradient-${doc.layers.indexOf(layer)}`,
      precision,
    );
    bytes += utf8ByteLength(element);
    if (bytes > MAX_PROJECT_BYTES)
      throw Error('SVG export exceeds 50 MB; reduce embedded image size or repetitions');
    elements.push(element);
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" ${rootAttributes}>${elements.join('')}</svg>`;
}
export class History {
  constructor(doc, { retainHistory = true } = {}) {
    this.past = [];
    this.future = [];
    this.doc = doc;
    this.revision = 0;
    this.retainHistory = retainHistory;
  }
  apply(fn) {
    let before = cloneDocument(this.doc);
    try {
      fn(this.doc);
    } catch (e) {
      this.doc = before;
      throw e;
    }
    if (sameDocument(before, this.doc)) return;
    this.record(before);
  }
  record(before) {
    this.revision++;
    if (!this.retainHistory) {
      this.future = [];
      return;
    }
    this.past.push(before);
    this.future = [];
    const retainedFontBytes = () => {
      const data = new Set(
        [this.doc, ...this.past].flatMap((doc) =>
          [...Object.values(doc.fontAssets || {}), ...Object.values(doc.imageAssets || {})].map(
            (asset) => asset.data,
          ),
        ),
      );
      return [...data].reduce((sum, value) => sum + value.length * 0.75, 0);
    };
    while (this.past.length > 100 || (this.past.length > 1 && retainedFontBytes() > 96_000_000))
      this.past.shift();
  }
  undo() {
    if (this.past.length) {
      this.revision++;
      this.future.push(this.doc);
      this.doc = this.past.pop();
    }
  }
  redo() {
    if (this.future.length) {
      this.revision++;
      this.past.push(this.doc);
      this.doc = this.future.pop();
    }
  }
}

// Shared command entry. The UI can read stable frozen History states for its
// selection WeakMap; only typed Session commands can replace authored state.
const frozenDocumentViews = new WeakSet();
function freezeDocumentView(value) {
  if (value && typeof value === 'object' && !frozenDocumentViews.has(value)) {
    frozenDocumentViews.add(value);
    for (const child of Object.values(value)) freezeDocumentView(child);
    Object.freeze(value);
  }
  return value;
}
export function updateTextTypography(target, ids, patch, doc, layer, fonts = {}) {
  if (!Array.isArray(ids) || !ids.length || ids.some((id) => typeof id !== 'string'))
    throw Error('Select character IDs');
  target.characterPositions = setCharacterStyles(
    target.text,
    target.characterPositions,
    Object.fromEntries(ids.map((id) => [id, patch])),
  );
  doc.schemaVersion = Math.max(doc.schemaVersion, target.characterPositions?.version >= 4 ? 6 : 5);
  validate(doc);
  const check = evaluate(
    { ...layer, output: target.id },
    resolveFonts(doc, fonts),
    doc.imageAssets,
    doc,
  );
  if (check.error) throw Error(check.error);
  const layout = check.textLayout(target.id);
  if (layout.reason) throw Error(layout.reason);
}

export function createVariableDocumentPreparer(options = {}) {
  return createVariableBridge({
    ...options,
    resolveFonts,
    validate,
    cloneDocument: (document) => inheritProjectDocumentContext(document, clone(document)),
  });
}

// Shape every current and captured variable Text request without first applying
// the captured graph to the authored document. The temporary nodes contain
// only the fields that determine a variable-font run.
function layerSnapshotPreparationDocument(doc, captured) {
  const requests = new Map();
  for (const layer of [...doc.layers, captured.snapshot])
    for (const item of layer.nodes)
      if (item.type === 'Text' && doc.fontAssets?.[item.font]?.kind === VARIABLE_KIND) {
        const key = JSON.stringify([item.font, item.text, item.variation?.axes]);
        if (!requests.has(key)) requests.set(key, item);
      }
  const projection = blankDocument();
  projection.schemaVersion = Math.max(doc.schemaVersion, captured.schemaVersion);
  projection.fontAssets = Object.fromEntries(
    [...new Set([...requests.values()].map((item) => item.font))].map((id) => [
      id,
      doc.fontAssets[id],
    ]),
  );
  projection.layers = [];
  let layer;
  for (const [index, item] of [...requests.values()].entries()) {
    if (!layer || layer.nodes.length >= 2000) {
      layer = blankLayer('Variable text preparation');
      layer.id = `variable-snapshot-layer-${projection.layers.length}`;
      layer.nodes[0].id = `variable-snapshot-output-${projection.layers.length}`;
      layer.output = layer.nodes[0].id;
      projection.layers.push(layer);
    }
    const text = node('Text');
    text.id = `variable-snapshot-text-${index}`;
    text.text = item.text;
    text.font = item.font;
    text.variation = clone(item.variation);
    if (item.writingMode !== undefined) text.writingMode = item.writingMode;
    if (item.kerning !== undefined) text.kerning = item.kerning;
    layer.nodes.push(text);
  }
  if (!projection.layers.length) projection.layers.push(blankLayer('Variable text preparation'));
  return projection;
}

export class CommandSession {
  #history;
  #fonts;
  #busy;
  #onCommitted;
  #textDrafts = new Map();
  #gesture = null;
  #prepared = null;
  #prepareSequence = 0;
  #nodeClipboard = null;
  #nodeClipboardOwner;
  #beforeCandidateCommit;
  #captureArtworkMoveAuthority;
  #resolveLayerClipboardSnapshot;
  #retainColorClipboardForProjectContext;
  #projectContextOwner = { epoch: 0, members: new WeakSet() };
  constructor(
    doc,
    {
      fonts = () => ({}),
      busy = () => false,
      onCommitted = () => {},
      retainHistory = true,
      beforeCandidateCommit = null,
      captureArtworkMoveAuthority = () => () => {},
      resolveLayerClipboardSnapshot = () => {
        throw Error('Layer clipboard is unavailable');
      },
      retainColorClipboardForProjectContext = null,
      nodeClipboardOwner = null,
      nodeClipboardFrom = null,
    } = {},
  ) {
    if (
      (nodeClipboardOwner !== null && typeof nodeClipboardOwner !== 'object') ||
      (nodeClipboardFrom !== null &&
        (!nodeClipboardOwner ||
          !(nodeClipboardFrom instanceof CommandSession) ||
          nodeClipboardFrom.#nodeClipboardOwner !== nodeClipboardOwner))
    )
      throw Error('Invalid node clipboard owner');
    this.#nodeClipboardOwner = nodeClipboardOwner;
    if (beforeCandidateCommit !== null && typeof beforeCandidateCommit !== 'function')
      throw Error('Invalid candidate preflight');
    if (typeof captureArtworkMoveAuthority !== 'function') throw Error('Invalid artwork authority');
    if (typeof resolveLayerClipboardSnapshot !== 'function')
      throw Error('Invalid Layer clipboard authority');
    if (
      retainColorClipboardForProjectContext !== null &&
      typeof retainColorClipboardForProjectContext !== 'function'
    )
      throw Error('Invalid Color clipboard authority');
    validate(doc);
    if (Object.values(doc.fontAssets || {}).some((asset) => asset.kind === VARIABLE_KIND))
      assertVariableDocumentReady(doc, resolveFonts(doc, fonts()));
    this.#history = new History(cloneDocument(doc), { retainHistory });
    this.#trackProjectContextRoots();
    this.#fonts = fonts;
    this.#busy = busy;
    this.#onCommitted = onCommitted;
    this.#beforeCandidateCommit = beforeCandidateCommit;
    this.#captureArtworkMoveAuthority = captureArtworkMoveAuthority;
    this.#resolveLayerClipboardSnapshot = resolveLayerClipboardSnapshot;
    this.#retainColorClipboardForProjectContext = retainColorClipboardForProjectContext;
    this.id = uid('session');
    if (nodeClipboardFrom) this.#inheritNodeClipboard(nodeClipboardFrom);
  }
  #inheritNodeClipboard(previous) {
    const source = previous.#nodeClipboard;
    if (!source) return;
    // Only the metadata is mutable. Keep the private captured graph/assets,
    // but never let a retired Session advance this Session's offset or expiry.
    const clipboard = { ...source },
      before = previous.#history.doc,
      after = this.#history.doc,
      beforeBinding = projectDocumentContexts.get(before),
      afterBinding = projectDocumentContexts.get(after);
    if (
      (beforeBinding?.activeCompositionId ?? null) !==
        (afterBinding?.activeCompositionId ?? null) &&
      (clipboard.externalNodeIds.length ||
        clipboard.artworkReferences.length ||
        clipboard.nodes.some((node) => node.compositionReference))
    )
      return;
    if (clipboard.colorArtworkReferences) {
      // A trusted same-Composition transition can renew a valid context, never revive an
      // externally rebound context or a reference which already expired.
      if (
        !beforeBinding ||
        !afterBinding ||
        beforeBinding.activeCompositionId !== afterBinding.activeCompositionId ||
        source.colorContextIdentity !== beforeBinding.clipboardIdentity ||
        source.colorContextEpoch !== previous.#projectContextOwner.epoch
      )
        clipboard.artworkReferencesExpired = true;
      clipboard.colorContextIdentity = afterBinding?.clipboardIdentity ?? null;
      clipboard.colorContextEpoch = this.#projectContextOwner.epoch;
    }
    if (
      clipboard.artworkReferences.length &&
      !clipboard.artworkReferencesExpired &&
      (!artworkClipboardTargetsIntact(before, clipboard) ||
        !artworkClipboardTargetsIntact(after, clipboard))
    )
      clipboard.artworkReferencesExpired = true;
    this.#nodeClipboard = clipboard;
  }
  captureNodeClipboardFreshness() {
    const document = this.#history.doc,
      revision = this.#history.revision,
      binding = projectDocumentContexts.get(document),
      contextEpoch = this.#projectContextOwner.epoch,
      clipboard = this.#nodeClipboard,
      pasteCount = clipboard?.pasteCount,
      expired = clipboard?.artworkReferencesExpired,
      colorIdentity = clipboard?.colorContextIdentity,
      colorEpoch = clipboard?.colorContextEpoch;
    return () => {
      if (
        this.#history.doc !== document ||
        this.#history.revision !== revision ||
        projectDocumentContexts.get(document) !== binding ||
        this.#projectContextOwner.epoch !== contextEpoch ||
        this.#nodeClipboard !== clipboard ||
        clipboard?.pasteCount !== pasteCount ||
        clipboard?.artworkReferencesExpired !== expired ||
        clipboard?.colorContextIdentity !== colorIdentity ||
        clipboard?.colorContextEpoch !== colorEpoch
      )
        throw Error('STALE_REVISION: node clipboard changed');
    };
  }
  #trackProjectContextRoots() {
    const roots = [this.#history.doc, ...this.#history.past, ...this.#history.future];
    this.#projectContextOwner.members = new WeakSet(roots);
    for (const root of roots) projectContextOwners.set(root, this.#projectContextOwner);
  }
  setProjectContext(project, compositionId) {
    const doc = this.#history.doc,
      revision = this.#history.revision,
      binding = projectDocumentContexts.get(doc),
      contextEpoch = this.#projectContextOwner.epoch,
      clipboard = this.#nodeClipboard;
    const assertCurrent = () => {
      if (
        this.#history.doc !== doc ||
        this.#history.revision !== revision ||
        projectDocumentContexts.get(doc) !== binding ||
        this.#projectContextOwner.epoch !== contextEpoch ||
        this.#nodeClipboard !== clipboard
      )
        throw Error('STALE_REVISION: document changed');
    };
    const next = prepareProjectDocumentContext(doc, project, compositionId);
    assertCurrent();
    let retain = false;
    if (
      clipboard?.colorArtworkReferences &&
      !clipboard.artworkReferencesExpired &&
      clipboard.colorContextEpoch === contextEpoch &&
      clipboard.colorContextIdentity === (binding?.clipboardIdentity ?? null)
    ) {
      retain = this.#retainColorClipboardForProjectContext?.(project, compositionId) === true;
      assertCurrent();
    }
    projectDocumentContexts.set(doc, next);
    if (clipboard?.colorArtworkReferences) {
      if (retain) clipboard.colorContextIdentity = next.clipboardIdentity;
      else clipboard.artworkReferencesExpired = true;
    }
  }
  get historyView() {
    const session = this;
    return Object.freeze({
      get doc() {
        return freezeDocumentView(session.#history.doc);
      },
      get past() {
        return Object.freeze(session.#history.past.map(freezeDocumentView));
      },
      get future() {
        return Object.freeze(session.#history.future.map(freezeDocumentView));
      },
      get revision() {
        return session.#history.revision;
      },
    });
  }
  get state() {
    return { sessionId: this.id, expectedRevision: this.#history.revision };
  }
  get hasTextComposition() {
    return [...this.#textDrafts.values()].some(
      (draft) => draft.composition && draft.revision === this.#history.revision,
    );
  }
  get hasTextDrafts() {
    return this.#textDrafts.size > 0;
  }
  get hasChangedTextDrafts() {
    return [...this.#textDrafts.values()].some(
      (draft) => draft.revision === this.#history.revision && draft.state.changed,
    );
  }
  get hasGesture() {
    return this.#gesture !== null;
  }
  get gestureState() {
    const draft = this.#gesture;
    return draft
      ? {
          gestureId: draft.id,
          kind: draft.target.kind,
          expectedPreviewRevision: draft.version,
          ...(draft.penLayer
            ? {
                layerId: draft.penLayer.id,
                nodeId: draft.penNodeId ?? draft.penLayer.nodes[0].id,
                pointId: draft.penPointId ?? draft.penLayer.nodes[0].points.at(-1)?.id,
                ...(draft.resuming
                  ? {
                      resuming: true,
                      side: draft.penSide,
                      closingPointId: draft.closingPointId,
                      ...(draft.penContourId ? { contourId: draft.penContourId } : {}),
                    }
                  : {}),
                pointPressed: draft.pressed,
              }
            : {}),
        }
      : null;
  }
  previewSnapshot(gestureId) {
    if (!this.#gesture || gestureId !== this.#gesture.id)
      throw Error('STALE_GESTURE: unknown draft');
    return {
      ...this.gestureState,
      baseRevision: this.#gesture.revision,
      document: cloneDocument(this.#gesture.candidate),
    };
  }
  textDraftSnapshot(draftId) {
    const draft = this.#textDrafts.get(draftId);
    if (!draft) throw Error('STALE_TEXT_DRAFT: unknown draft');
    const candidate = cloneDocument(this.#history.doc);
    const layer = candidate.layers.find((layer) => layer.id === draft.layerId);
    const node = layer?.nodes.find((node) => node.id === draft.nodeId);
    draft.assertCurrent(node, this.#history.revision);
    if (layer.locked) throw Error('Composition is locked');
    node.text = draft.raw;
    if (draft.record) node.characterPositions = clone(draft.record);
    else delete node.characterPositions;
    validate(candidate);
    return { ...this.state, ...draft.state, document: candidate };
  }
  #textGestureValues(base, target) {
    const layer = base.layers.find((layer) => layer.id === target.layerId),
      node = layer?.nodes.find((node) => node.id === target.nodeId && node.type === 'Text');
    if (!node) throw Error('Select a Text node');
    if (layer.locked) throw Error('Composition is locked');
    const result = evaluate(
        { ...layer, output: node.id },
        resolveFonts(base, this.#fonts()),
        base.imageAssets,
        base,
      ),
      layout = result.textLayout(node.id);
    if (result.error || layout.reason) throw Error(result.error || layout.reason);
    if (node.locked) throw Error('Text node is locked');
    if (target.textKind === 'arrangement-gap') {
      if (!layout.arrangementStatus?.active)
        throw Error(layout.arrangementStatus?.reason || 'Apply maintained ink gaps first');
      return [node.characterArrangement.gap];
    }
    if (
      layout.arrangementStatus?.active &&
      ((target.textKind === 'offset' &&
        target.axis === (node.characterArrangement.axis === 'x' ? 0 : 1)) ||
        target.textKind === 'kerning' ||
        (target.textKind === 'style' && target.key === 'tracking'))
    )
      throw Error('Maintained ink gaps control spacing; disable to edit');
    if (target.textKind === 'leading') {
      if (node.leading?.mode !== 'manual') throw Error('Manual leading required');
      return [node.leading.value];
    }
    if (target.textKind === 'wrap') {
      if (!node.textWrap) throw Error('Text wrap required');
      return [node.textWrap.width ?? node.textWrap.height];
    }
    const ids = characterIdsInRange(node.text, node.characterPositions, 0, node.text.length);
    if (
      !Array.isArray(target.ids) ||
      !target.ids.length ||
      new Set(target.ids).size !== target.ids.length ||
      target.ids.some((id) => !ids.includes(id))
    )
      throw Error('Select stable character IDs');
    if (target.textKind === 'kerning') {
      const key = characterPairFromIds(node.text, node.characterPositions, target.ids);
      if (target.pairKey !== key || !layout.pairKerning?.[key])
        throw Error('Select an ordered character pair');
      return [layout.pairKerning[key].value];
    }
    if (target.textKind === 'style') {
      if (!['size', 'width', 'height', 'tracking', 'baselineShift'].includes(target.key))
        throw Error('Invalid character style');
      const defaults = result.textDefaults(node.id);
      return target.ids.map(
        (id) => node.characterPositions?.styles?.[id]?.[target.key] ?? defaults[target.key],
      );
    }
    if (target.textKind === 'offset' && ![0, 1].includes(target.axis))
      throw Error('Invalid character axis');
    if (target.textKind === 'transform' && !['rotation', 'scale'].includes(target.key))
      throw Error('Invalid character transform');
    if (!['offset', 'transform'].includes(target.textKind)) throw Error('Invalid Text gesture');
    return target.ids.map((id) => {
      const character = layout.characters.find((character) => character.id === id);
      if (!character) throw Error('Character mapping unavailable');
      return target.textKind === 'offset' ? character.offset[target.axis] : character[target.key];
    });
  }
  #gestureCandidate(base, target, update) {
    if (!target || typeof target !== 'object') throw Error('Invalid gesture target');
    if (target.kind === 'color-slots') {
      const slots = new Set((base.colorSlots || []).map((slot) => slot.id));
      if (
        Object.getPrototypeOf(target) !== Object.prototype ||
        Object.keys(target).length !== 2 ||
        Object.keys(target).some((key) => !['kind', 'slotIds'].includes(key)) ||
        !Array.isArray(target.slotIds) ||
        !target.slotIds.length ||
        target.slotIds.length > 256 ||
        new Set(target.slotIds).size !== target.slotIds.length ||
        target.slotIds.some((id) => typeof id !== 'string' || !slots.has(id))
      )
        throw Error('Select distinct existing shared Color slots');
      if (
        !update ||
        Object.getPrototypeOf(update) !== Object.prototype ||
        Object.keys(update).some((key) => !['updates', 'design'].includes(key)) ||
        !Array.isArray(update.updates) ||
        update.updates.length !== target.slotIds.length ||
        update.updates.some(
          (row) =>
            !row ||
            Object.getPrototypeOf(row) !== Object.prototype ||
            Object.keys(row).length !== 2 ||
            Object.keys(row).some((key) => !['slotId', 'value'].includes(key)) ||
            !target.slotIds.includes(row.slotId),
        )
      )
        throw Error('Color gesture updates must match the frozen slot targets');
      return this.#commandCandidate('set-color-slots', update, base).candidate;
    }
    if (target.kind === 'transform-anchor')
      return this.#commandCandidate(
        'reposition-transform-anchor',
        { ...target, delta: update.delta },
        base,
      ).candidate;
    if (target.kind === 'text-font') {
      const layer = base.layers.find((layer) => layer.id === target.layerId);
      const node = layer?.nodes.find((node) => node.id === target.nodeId && node.type === 'Text');
      if (!node) throw Error('Select a Text node');
      if (layer.locked) throw Error('Composition is locked');
      if (target.characterIds !== undefined) {
        const ids = characterIdsInRange(node.text, node.characterPositions, 0, node.text.length);
        if (
          !Array.isArray(target.characterIds) ||
          !target.characterIds.length ||
          new Set(target.characterIds).size !== target.characterIds.length ||
          target.characterIds.some((id) => !ids.includes(id))
        )
          throw Error('Select stable character IDs');
      }
      // Opening the picker must also work when the current face lacks a glyph;
      // browsing a supported face is how the user repairs that state.
      if (update.initial === true) return cloneDocument(base);
      const payload = { layerId: target.layerId, nodeId: target.nodeId };
      if (target.characterIds !== undefined) payload.characterIds = target.characterIds;
      if (update.bytes !== undefined)
        return this.#commandCandidate(
          'adopt-font',
          {
            ...payload,
            bytes: update.bytes,
            fileName: update.fileName,
          },
          base,
        ).candidate;
      if (typeof update.font !== 'string') throw Error('Select a font');
      const candidate = this.#commandCandidate(
        target.characterIds === undefined ? 'set-text-settings' : 'set-character-style',
        {
          ...payload,
          ...(target.characterIds === undefined
            ? { settings: { font: update.font } }
            : { patch: { font: update.font || null } }),
        },
        base,
      ).candidate;
      const layerPreview = candidate.layers.find((layer) => layer.id === target.layerId);
      const checked = evaluate(
        { ...layerPreview, output: target.nodeId },
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      if (checked.error) throw Error(checked.error);
      return candidate;
    }
    if (target.kind === 'point-handles')
      return this.#commandCandidate(
        'create-point-handles',
        {
          layerId: target.layerId,
          nodeId: target.nodeId,
          pointId: target.pointId,
          delta: update.delta,
        },
        base,
      ).candidate;
    if (target.kind === 'path-units-translate')
      return this.#commandCandidate(
        'translate-path-units',
        {
          layerId: target.layerId,
          nodeId: target.nodeId,
          unitIds: target.unitIds,
          delta: update.delta,
        },
        base,
      ).candidate;
    if (target.kind === 'artwork-translate')
      return this.#commandCandidate(
        'translate-artworks',
        {
          layerIds: target.layerIds,
          delta: update.delta,
        },
        base,
      ).candidate;
    if (target.kind === 'guide-position')
      return this.#commandCandidate(
        'set-guide',
        {
          guideId: target.guideId,
          key: 'position',
          value: update.value,
        },
        base,
      ).candidate;
    if (target.kind === 'character-translate') {
      const draggedNode = base.layers
        .find((l) => l.id === target.layerId)
        ?.nodes.find((n) => n.id === target.nodeId);
      this.#textGestureValues(base, {
        ...target,
        textKind: 'offset',
        axis: draggedNode?.characterArrangement?.axis === 'x' ? 1 : 0,
      });
      assertValue('Vector2', update.delta);
      if (update.delta.every((value) => value === 0)) return cloneDocument(base);
      const node = base.layers
        .find((l) => l.id === target.layerId)
        .nodes.find((n) => n.id === target.nodeId);
      return this.#commandCandidate(
        'set-character-offsets',
        {
          layerId: target.layerId,
          nodeId: target.nodeId,
          changes: Object.fromEntries(
            target.ids.map((id) => [
              id,
              (node.characterPositions?.offsets?.[id] ?? [0, 0]).map((v, i) => v + update.delta[i]),
            ]),
          ),
        },
        base,
      ).candidate;
    }
    if (['nodes-translate', 'curve-point', 'remap-point'].includes(target.kind)) {
      const perPoint = update.pointMoves !== undefined,
        absoluteRemap = update.value !== undefined;
      if (absoluteRemap) {
        if (target.kind !== 'remap-point' || perPoint || update.delta !== undefined)
          throw Error('Invalid absolute Remap value');
        assertValue('Vector2', update.value);
      }
      if (perPoint) {
        if (
          !['curve-point', 'remap-point'].includes(target.kind) ||
          target.key !== 'position' ||
          update.delta !== undefined ||
          !Array.isArray(update.pointMoves)
        )
          throw Error('Invalid per-point movement');
      } else if (!absoluteRemap) assertValue('Vector2', update.delta);
      const pointMoves = new Map();
      if (perPoint)
        for (const move of update.pointMoves) {
          if (!move || typeof move.pointId !== 'string' || pointMoves.has(move.pointId))
            throw Error('Select distinct point movements');
          assertValue('Vector2', move.delta);
          pointMoves.set(move.pointId, move.delta);
        }
      const candidate = cloneDocument(base),
        layer = candidate.layers.find((l) => l.id === target.layerId);
      if (!layer) throw Error('Missing composition');
      if (layer.locked) throw Error('Composition is locked');
      const write = (q, original = q?.value, delta = update.delta) => {
        if (!q || q.type !== 'Vector2') throw Error('Missing vector parameter');
        if (q.locked || q.source || q.expression) throw Error('Parameter is locked or driven');
        q.value = original.map((v, i) => v + delta[i]);
      };
      let absoluteUnchanged = false;
      if (target.kind === 'nodes-translate') {
        if (
          !Array.isArray(target.nodeIds) ||
          !target.nodeIds.length ||
          new Set(target.nodeIds).size !== target.nodeIds.length
        )
          throw Error('Select distinct generator nodes');
        for (const id of target.nodeIds) {
          const node = layer.nodes.find((n) => n.id === id);
          if (!node || node.bypassed || node.pathAttachment) throw Error('Unsupported node move');
          if (['Curve', 'Multi Path'].includes(node.type)) {
            for (const point of node.points) write(point.position);
          } else {
            if (
              !(target.nodeIds.length === 1 && isTransform(node)) &&
              ![
                'Circle',
                'Ellipse',
                'Rectangle',
                'Polygon',
                'Star',
                'Text',
                'Image',
                'Composition Reference',
                'SVG',
                'Vector',
                'Null',
              ].includes(node.type)
            )
              throw Error('Unsupported node move');
            write(
              node.params[
                isTransform(node)
                  ? node.params.position
                    ? 'position'
                    : 'offset'
                  : node.type === 'Vector'
                    ? 'value'
                    : ['Text', 'Image', 'Null', 'Composition Reference', 'SVG'].includes(node.type)
                      ? 'position'
                      : 'center'
              ],
            );
          }
        }
      } else {
        if (!['position', 'handleA', 'handleB'].includes(target.key))
          throw Error('Invalid point component');
        const node = layer.nodes.find(
          (n) =>
            n.id === target.nodeId &&
            (target.kind === 'curve-point'
              ? ['Curve', 'Multi Path'].includes(n.type)
              : n.type === 'Path Remap'),
        );
        if (!node || node.bypassed || node.locked === true)
          throw Error('Missing or locked editable point node');
        const pointIds = target.pointIds ?? [target.pointId];
        if (
          !Array.isArray(pointIds) ||
          !pointIds.length ||
          new Set(pointIds).size !== pointIds.length ||
          !pointIds.includes(target.pointId) ||
          (pointIds.length > 1 && target.key !== 'position')
        )
          throw Error('Select distinct point anchors');
        if (absoluteRemap && pointIds.length !== 1)
          throw Error('Absolute Remap value requires one point');
        if (
          update.independent !== undefined &&
          (typeof update.independent !== 'boolean' || target.key === 'position')
        )
          throw Error('Independent handles require a handle drag');
        if (
          perPoint &&
          (pointMoves.size !== pointIds.length || pointIds.some((id) => !pointMoves.has(id)))
        )
          throw Error('Point movements must match the selected anchors');
        if (target.kind === 'curve-point') {
          const editablePoints = new Map(node.points.map((p) => [p.id, p]));
          for (const pointId of pointIds) {
            const point = editablePoints.get(pointId);
            if (!point) throw Error('Missing point');
            write(point[target.key], undefined, perPoint ? pointMoves.get(pointId) : update.delta);
            if (update.independent) point.handleType = 'corner';
            if (target.key !== 'position' && point.handleType !== 'corner') {
              const other = point[target.key === 'handleA' ? 'handleB' : 'handleA'];
              if (!other.locked && !other.source && !other.expression) {
                const value = point[target.key].value,
                  length =
                    point.handleType === 'symmetric'
                      ? Math.hypot(...value)
                      : Math.hypot(...other.value),
                  magnitude = Math.hypot(...value) || 1;
                other.value = value.map((v) => (-v / magnitude) * length);
              }
            }
          }
        } else {
          const input = evaluate(
            layer,
            resolveFonts(candidate, this.#fonts()),
            candidate.imageAssets,
            candidate,
          ).run(node.params.path.source);
          for (const pointId of pointIds) {
            const existing = node.edits.find((e) => e.pointId === pointId);
            const original = existing && perPoint ? clone(existing) : null;
            if (existing && input.unstable && !remapFingerprintMatches(input, existing.fingerprint))
              throw Error('Retained edit is suspended; rebind before dragging');
            const edit = enableRemapPoint(node, input, pointId);
            const delta = perPoint ? pointMoves.get(pointId) : update.delta;
            if (absoluteRemap) {
              const q = edit.params[target.key];
              if (!q || q.type !== 'Vector2') throw Error('Missing vector parameter');
              if (q.locked || q.source || q.expression)
                throw Error('Parameter is locked or driven');
              absoluteUnchanged = update.value.every((value, index) => value === q.value[index]);
              q.value = [...update.value];
            } else write(edit.params[target.key], undefined, delta);
            if (perPoint && delta.every((value) => value === 0)) {
              const index = node.edits.indexOf(edit);
              if (original) node.edits[index] = original;
              else node.edits.splice(index, 1);
            }
          }
        }
      }
      // Returning to the origin must retain sparse Remap edits and the redo branch.
      if (
        absoluteRemap
          ? absoluteUnchanged
          : (perPoint ? [...pointMoves.values()] : [update.delta]).every((delta) =>
              delta.every((value) => value === 0),
            )
      )
        return cloneDocument(base);
      validate(candidate);
      assertClosedOffsetChange(
        layer,
        base.layers.find((l) => l.id === layer.id),
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        resolveFonts(base, this.#fonts()),
        candidate,
        base,
      );
      return candidate;
    }
    if (target.kind === 'text-values') {
      const initial = this.#textGestureValues(base, target),
        values = update.values;
      if (
        !Array.isArray(values) ||
        values.length !== initial.length ||
        !values.every(Number.isFinite)
      )
        throw Error('Invalid Text gesture values');
      if (values.every((value, index) => value === initial[index])) return cloneDocument(base);
      const payload = { layerId: target.layerId, nodeId: target.nodeId };
      if (target.textKind === 'arrangement-gap')
        return this.#commandCandidate(
          'set-character-arrangement',
          { ...payload, updateGap: true, gap: values[0] },
          base,
        ).candidate;
      if (target.textKind === 'leading' || target.textKind === 'wrap')
        return this.#commandCandidate(
          'set-text-settings',
          {
            ...payload,
            settings:
              target.textKind === 'leading'
                ? { leading: { mode: 'manual', value: values[0] } }
                : {
                    textWrap: withWrapExtent(
                      base.layers
                        .find((l) => l.id === target.layerId)
                        .nodes.find((n) => n.id === target.nodeId).textWrap,
                      values[0],
                    ),
                  },
          },
          base,
        ).candidate;
      if (target.textKind === 'kerning')
        return this.#commandCandidate(
          'set-character-kerning',
          { ...payload, changes: { [target.pairKey]: values[0] } },
          base,
        ).candidate;
      const candidate = cloneDocument(base),
        layer = candidate.layers.find((layer) => layer.id === target.layerId),
        node = layer.nodes.find((node) => node.id === target.nodeId);
      let next;
      if (target.textKind === 'offset')
        next = setCharacterOffsets(
          node.text,
          node.characterPositions,
          Object.fromEntries(
            target.ids.map((id, i) => [
              id,
              (node.characterPositions?.offsets?.[id] ?? [0, 0]).map((value, axis) =>
                axis === target.axis ? values[i] : value,
              ),
            ]),
          ),
        );
      else if (target.textKind === 'transform')
        next = setCharacterTransforms(
          node.text,
          node.characterPositions,
          Object.fromEntries(
            target.ids.map((id, i) => [
              id,
              {
                rotation: 0,
                scale: 1,
                ...node.characterPositions?.transforms?.[id],
                [target.key]: values[i],
              },
            ]),
          ),
        );
      else
        next = setCharacterStyles(
          node.text,
          node.characterPositions,
          Object.fromEntries(target.ids.map((id, i) => [id, { [target.key]: values[i] }])),
        );
      if (next) {
        node.characterPositions = next;
        candidate.schemaVersion = Math.max(
          candidate.schemaVersion,
          next.version >= 5
            ? 9
            : next.version >= 4
              ? 6
              : next.version >= 3
                ? 5
                : next.version >= 2
                  ? 4
                  : 3,
        );
      }
      validate(candidate);
      const result = evaluate(
        { ...layer, output: node.id },
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      if (result.error || result.textLayout(node.id).reason)
        throw Error(result.error || result.textLayout(node.id).reason);
      assertClosedOffsetChange(
        layer,
        base.layers.find((layer) => layer.id === target.layerId),
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        resolveFonts(base, this.#fonts()),
        candidate,
        base,
      );
      return candidate;
    }
    if (target.kind === 'numeric-command') {
      if (
        ![
          'set-document-settings',
          'set-guide-setting',
          'set-guide',
          'edit-remap',
          'set-text-settings',
        ].includes(target.command)
      )
        throw Error('Unsupported numeric control');
      assertValue('Scalar', update.value);
      const payload = clone(target.payload);
      if (target.command === 'set-text-settings') {
        const n = base.layers
          .find((l) => l.id === payload.layerId)
          ?.nodes.find((n) => n.id === payload.nodeId);
        if (!n?.pathAttachment) throw Error('Select a path contour first');
        payload.settings = { pathAttachment: { ...clone(n.pathAttachment), start: update.value } };
      } else payload.value = update.value;
      return this.#commandCandidate(target.command, payload, base).candidate;
    }
    if (target.kind === 'parameter') {
      const node = base.layers
          .find((layer) => layer.id === target.layerId)
          ?.nodes.find((node) => node.id === target.nodeId),
        q = node && parameter(node, target.key);
      if (!q) throw Error('Missing parameter');
      const value = target.alpha ? { ...q.value, alpha: update.value } : update.value;
      if (target.alpha && q.type !== 'Color') throw Error('Alpha requires a Color');
      return this.#commandCandidate(
        target.authoredBase
          ? 'set-parameter-base'
          : target.transformControl && isTransform(node)
            ? 'set-transform-control'
            : 'set-parameter',
        {
          layerId: target.layerId,
          nodeId: target.nodeId,
          key: target.key,
          ...(target.axis !== undefined ? { axis: target.axis } : {}),
          value,
        },
        base,
      ).candidate;
    }
    const candidate = cloneDocument(base),
      layer = candidate.layers.find((layer) => layer.id === target.layerId);
    if (!layer) throw Error('Missing composition');
    if (layer.locked) throw Error('Composition is locked');
    if (target.kind === 'graph-translate') {
      if (
        !Array.isArray(target.nodeIds) ||
        !target.nodeIds.length ||
        new Set(target.nodeIds).size !== target.nodeIds.length
      )
        throw Error('Select distinct graph nodes');
      const nodes = target.nodeIds.map((id) => {
        const item = layer.nodes.find((node) => node.id === id);
        if (!item) throw Error('Missing node');
        return item;
      });
      assertValue('Vector2', update.delta);
      const delta = update.delta.map((value, axis) =>
        Math.max(value, -Math.min(...nodes.map((node) => node.position[axis]))),
      );
      for (const node of nodes)
        node.position = node.position.map((value, axis) => value + delta[axis]);
    } else if (target.kind === 'graph-width') {
      const node = layer.nodes.find((node) => node.id === target.nodeId);
      if (!node) throw Error('Missing node');
      if (!Number.isFinite(update.width) || update.width < 220 || update.width > 550)
        throw Error('Invalid node width');
      if (update.width !== (node.width ?? 264)) node.width = update.width;
    } else throw Error('Unsupported gesture kind');
    validate(candidate);
    return candidate;
  }
  #penCommand(type, payload, draft) {
    if (draft.target.kind !== 'pen') throw Error('Select a Pen draft');
    const nodeIn = (layer) =>
      layer.nodes.find((n) => n.id === (draft.penNodeId ?? layer.nodes[0].id));
    if (type === 'commit-pen') {
      if (draft.pressed) throw Error('Finish or cancel the pressed point');
      if (typeof payload.closed !== 'boolean') throw Error('Choose open or closed path');
      const layer = clone(draft.penLayer),
        curve = nodeIn(layer),
        ids = penContourIds(curve, draft.penContourId);
      const created = { layerId: layer.id, nodeId: curve.id };
      if (ids.length < 2) {
        this.#gesture = null;
        return { ...this.state, ...created, discarded: true, changed: false };
      }
      if (curve.type === 'Curve') curve.params.closed.value = payload.closed;
      else curve.contours.find((c) => c.id === draft.penContourId).closed = payload.closed;
      const candidate = cloneDocument(draft.base);
      if (draft.resuming) {
        const original = draft.base.layers.find((l) => l.id === layer.id);
        resolvePenEndpoint(original, curve.id, draft.target.endpointId, draft.penContourId);
        if (insertionRemapDependents(original, curve.id).length)
          throw Error(
            'Continuation is unavailable while this source feeds retained Path Remap edits',
          );
        candidate.layers[candidate.layers.findIndex((l) => l.id === layer.id)] = layer;
        validate(candidate);
        assertClosedOffsetChange(
          layer,
          original,
          resolveFonts(candidate, this.#fonts()),
          candidate.imageAssets,
          resolveFonts(draft.base, this.#fonts()),
          candidate,
          draft.base,
        );
      } else appendPenArtwork(candidate, layer);
      assertNoAccidentalCurvePointRebind(draft.base, candidate, this.#nodeClipboard);
      assertArtworkReferenceTransition(draft.base, candidate, this.#fonts());
      const encoded = serialize(candidate);
      if (encoded === serialize(draft.base)) {
        this.#gesture = null;
        return { ...this.state, ...created, discarded: true, changed: false };
      }
      this.#beforeCandidateCommit?.(this.#history.doc, candidate);
      this.#history.apply(() => {
        this.#history.doc = candidate;
      });
      this.#gesture = null;
      return { ...this.state, ...created, changed: true };
    }
    let layer = clone(draft.penLayer),
      prior = draft.pressedBefore,
      pressed = draft.pressed,
      curve = nodeIn(layer),
      pointId = draft.penPointId ?? curve.points.at(-1)?.id;
    if (type === 'append-pen-point') {
      if (pressed) throw Error('Finish or cancel the pressed point');
      assertValue('Vector2', payload.position);
      if (curve.points.length >= 10000) throw Error('Pen paths are limited to 10000 points');
      prior = clone(draft.penLayer);
      const created = point(...payload.position);
      insertContinuationPoint(curve, draft.penContourId, draft.penSide || 'tail', created);
      pointId = created.id;
      pressed = true;
    } else {
      if (!pressed) throw Error('No pressed Pen point');
      if (type === 'update-pen-point') {
        assertValue('Vector2', payload.delta);
        const pt = curve.points.find((p) => p.id === pointId),
          head = draft.penSide === 'head';
        pt[head ? 'handleA' : 'handleB'].value = clone(payload.delta);
        pt[head ? 'handleB' : 'handleA'].value = payload.delta.map((v) => -v);
        pt.handleType = 'symmetric';
      } else if (type === 'accept-pen-point') {
        pressed = false;
        prior = null;
      } else if (type === 'cancel-pen-point') {
        if (!prior) {
          this.#gesture = null;
          return { ...this.state, discarded: true };
        }
        layer = clone(prior);
        curve = nodeIn(layer);
        const ids = penContourIds(curve, draft.penContourId);
        pointId = draft.penSide === 'head' ? ids[0] : ids.at(-1);
        prior = null;
        pressed = false;
      } else throw Error('Unsupported Pen command');
    }
    const candidate = cloneDocument(draft.base);
    if (draft.resuming)
      candidate.layers[candidate.layers.findIndex((l) => l.id === layer.id)] = layer;
    else candidate.layers.push(layer);
    assertNoAccidentalCurvePointRebind(draft.base, candidate, this.#nodeClipboard);
    assertArtworkReferenceTransition(draft.base, candidate, this.#fonts());
    if (candidate.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION)
      this.#beforeCandidateCommit?.(this.#history.doc, candidate);
    validate(candidate);
    if (draft.resuming)
      assertClosedOffsetChange(
        layer,
        draft.base.layers.find((l) => l.id === layer.id),
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        resolveFonts(draft.base, this.#fonts()),
        candidate,
        draft.base,
      );
    draft.penLayer = layer;
    draft.penPointId = pointId;
    draft.pressedBefore = prior;
    draft.pressed = pressed;
    draft.candidate = candidate;
    draft.version++;
    return { ...this.state, ...this.gestureState };
  }
  snapshot() {
    return cloneDocument(this.#history.doc);
  }
  async prepareCommand(envelope, bridge, options = {}) {
    const { sessionId, expectedRevision, type, payload = {} } = envelope;
    if (sessionId !== this.id || expectedRevision !== this.#history.revision)
      throw Error('STALE_REVISION: document changed');
    if (this.#busy() || this.#gesture || this.hasTextComposition || this.#textDrafts.size)
      throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
    if (
      ![
        'set-text-content',
        'set-text-settings',
        'set-variable-font',
        'paste-layer-snapshot',
        'undo',
        'redo',
      ].includes(type)
    )
      throw Error('VARIABLE_COMMAND_UNSUPPORTED');
    const target = this.#history.doc.layers
      .find((l) => l.id === payload.layerId)
      ?.nodes.find((n) => n.id === payload.nodeId);
    if (target?.locked) throw Error('Text node is locked');
    const sequence = ++this.#prepareSequence;
    this.#prepared = null;
    const snapshotPaste = type === 'paste-layer-snapshot';
    const entry = snapshotPaste
      ? {
          document: this.#history.doc,
          revision: this.#history.revision,
          binding: projectDocumentContexts.get(this.#history.doc),
          contextEpoch: this.#projectContextOwner.epoch,
          clipboard: this.#nodeClipboard,
        }
      : null;
    const captured = snapshotPaste ? this.#resolveLayerClipboardSnapshot(payload.token) : null;
    const assertLocal = () => {
      if (
        sequence !== this.#prepareSequence ||
        sessionId !== this.id ||
        expectedRevision !== this.#history.revision ||
        (entry &&
          (this.#history.doc !== entry.document ||
            this.#history.revision !== entry.revision ||
            projectDocumentContexts.get(this.#history.doc) !== entry.binding ||
            this.#projectContextOwner.epoch !== entry.contextEpoch ||
            this.#nodeClipboard !== entry.clipboard))
      )
        throw Error('STALE_REVISION: document changed');
      if (captured && this.#resolveLayerClipboardSnapshot(payload.token) !== captured)
        throw Error('STALE_SESSION: copied Layer changed');
    };
    assertLocal();
    const authority = snapshotPaste ? this.#captureArtworkMoveAuthority() : null;
    const assertCurrent = () => {
      assertLocal();
      authority?.();
      assertLocal();
      const busy = this.#busy();
      assertLocal();
      if (busy || this.#gesture || this.hasTextComposition || this.#textDrafts.size)
        throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
      assertLocal();
      if (this.#gesture || this.hasTextComposition || this.#textDrafts.size)
        throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
    };
    if (snapshotPaste) assertCurrent();
    let candidate, result;
    if (type === 'undo' || type === 'redo')
      candidate = cloneDocument(
        (type === 'undo' ? this.#history.past : this.#history.future).at(-1) || this.#history.doc,
      );
    else if (!snapshotPaste)
      candidate = this.#commandCandidate(type, payload, this.#history.doc, true).candidate;
    const input = snapshotPaste
      ? layerSnapshotPreparationDocument(entry.document, captured)
      : candidate;
    if (snapshotPaste) assertCurrent();
    const ticket = await bridge.prepareDocument(input, options);
    try {
      assertLocal();
      if (snapshotPaste) assertCurrent();
      const fonts = { ...this.#fonts(), ...bridge.preparedFonts(ticket) };
      if (snapshotPaste) {
        assertCurrent();
        ({ candidate, result } = this.#commandCandidate(
          type,
          payload,
          entry.document,
          true,
          fonts,
        ));
        assertCurrent();
      }
      assertVariableDocumentReady(candidate, resolveFonts(candidate, fonts));
      // Validate downstream bounded geometry against the prepared candidate, before changing history.
      for (const layer of candidate.layers)
        assertClosedOffsetChange(
          layer,
          this.#history.doc.layers.find((l) => l.id === layer.id),
          resolveFonts(candidate, fonts),
          candidate.imageAssets,
          resolveFonts(this.#history.doc, snapshotPaste ? fonts : this.#fonts()),
          candidate,
          this.#history.doc,
        );
      if (snapshotPaste) assertCurrent();
      const token = Object.freeze({});
      this.#prepared = {
        token,
        ticket,
        bridge,
        candidate,
        expectedRevision,
        type,
        sequence,
        result,
        assertCurrent: snapshotPaste ? assertCurrent : null,
      };
      return token;
    } catch (error) {
      // Do not cancel a newer request that replaced this ticket.
      try {
        bridge.assertFresh(ticket);
        bridge.cancel();
      } catch {}
      throw error;
    }
  }
  preparedSnapshot(token) {
    const p = this.#prepared;
    if (
      !p ||
      p.token !== token ||
      p.expectedRevision !== this.#history.revision ||
      p.sequence !== this.#prepareSequence
    )
      throw Error('STALE_REVISION: document changed');
    p.bridge.assertFresh(p.ticket);
    p.assertCurrent?.();
    return {
      document: cloneDocument(p.candidate),
      fonts: { ...this.#fonts(), ...p.bridge.preparedFonts(p.ticket) },
    };
  }
  #observeCommit(action, { skipFailed = false, suppressNotification = () => false } = {}) {
    const before = this.#history.revision;
    let completed = false;
    try {
      const result = action();
      completed = true;
      return result;
    } finally {
      if (this.#history.revision !== before && (!skipFailed || completed)) {
        this.#trackProjectContextRoots();
        const clipboard = this.#nodeClipboard;
        if (
          clipboard?.colorArtworkReferences &&
          (clipboard.colorContextEpoch !== this.#projectContextOwner.epoch ||
            clipboard.colorContextIdentity !==
              (projectDocumentContexts.get(this.#history.doc)?.clipboardIdentity ?? null))
        )
          clipboard.artworkReferencesExpired = true;
        if (
          clipboard?.artworkReferences.length &&
          !clipboard.artworkReferencesExpired &&
          !artworkClipboardTargetsIntact(this.#history.doc, clipboard)
        )
          clipboard.artworkReferencesExpired = true;
        // An adapter cannot roll back an already committed edit.
        if (!suppressNotification())
          try {
            this.#onCommitted(Object.freeze(this.state));
          } catch {}
      }
    }
  }
  commitPrepared(token) {
    return this.#observeCommit(() => this.#commitPrepared(token), {
      skipFailed: this.#prepared?.type === 'paste-layer-snapshot',
    });
  }
  #commitPrepared(token) {
    const p = this.#prepared;
    if (
      !p ||
      p.token !== token ||
      p.expectedRevision !== this.#history.revision ||
      p.sequence !== this.#prepareSequence
    )
      throw Error('STALE_REVISION: document changed');
    if (this.#busy() || this.#gesture || this.hasTextComposition || this.#textDrafts.size)
      throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
    p.assertCurrent?.();
    p.bridge.assertFresh(p.ticket);
    assertVariableDocumentReady(
      p.candidate,
      resolveFonts(p.candidate, p.bridge.preparedFonts(p.ticket)),
    );
    p.assertCurrent?.();
    const revision = this.#history.revision;
    if (p.type === 'undo' || p.type === 'redo') this.#history[p.type]();
    else if (!sameDocument(this.#history.doc, p.candidate)) {
      assertNoAccidentalCurvePointRebind(this.#history.doc, p.candidate, this.#nodeClipboard);
      assertArtworkReferenceTransition(this.#history.doc, p.candidate, {
        ...this.#fonts(),
        ...p.bridge.preparedFonts(p.ticket),
      });
      p.assertCurrent?.();
      this.#beforeCandidateCommit?.(
        this.#history.doc,
        p.candidate,
        p.bridge.preparedFonts(p.ticket),
      );
      p.assertCurrent?.();
      this.#history.apply(() => {
        this.#history.doc = p.candidate;
      });
    }
    this.#trackProjectContextRoots();
    p.bridge.activate(p.ticket);
    if (p.type === 'import-svg') this.#nodeClipboard = null;
    this.#prepared = null;
    return { ...this.state, ...p.result, changed: revision !== this.#history.revision };
  }
  cancelPrepared(bridge, token = null) {
    if (token && this.#prepared?.token !== token) return;
    this.#prepared = null;
    this.#prepareSequence++;
    bridge.cancel();
  }
  dispatch(command) {
    const entry = {
      document: this.#history.doc,
      revision: this.#history.revision,
      binding: projectDocumentContexts.get(this.#history.doc),
      contextEpoch: this.#projectContextOwner.epoch,
      clipboard: this.#nodeClipboard,
    };
    const commandType = command?.type,
      copyAttempt = { isCopy: commandType === 'copy-nodes' };
    const artworkMove =
      commandType === 'translate-artworks' ||
      (commandType === 'begin-gesture' && command.payload?.target?.kind === 'artwork-translate') ||
      (['update-gesture', 'commit-gesture'].includes(commandType) &&
        this.#gesture?.target.kind === 'artwork-translate');
    const guardedEdit = [
      'paste-nodes',
      'paste-layer-snapshot',
      'move-layers-to-folder',
      'remove-layers-from-folder',
    ].includes(commandType);
    return this.#observeCommit(() => this.#dispatch(command, entry, copyAttempt), {
      skipFailed: artworkMove || guardedEdit,
      suppressNotification: () => copyAttempt.isCopy,
    });
  }
  #dispatch({ sessionId, expectedRevision, type, payload = {} }, entry, copyAttempt) {
    copyAttempt.isCopy = type === 'copy-nodes';
    if (sessionId !== this.id) throw Error('STALE_SESSION: document changed');
    // Cleanup can release the caller's exact token even after a legacy edit made
    // its baseline stale. It never writes the committed document.
    if (type === 'cancel-gesture') {
      if (!this.#gesture || payload.gestureId !== this.#gesture.id)
        throw Error('STALE_GESTURE: unknown draft');
      this.#gesture = null;
      return this.state;
    }
    if (expectedRevision !== this.#history.revision)
      throw Error('STALE_REVISION: document changed');
    if (!payload || typeof payload !== 'object' || Array.isArray(payload))
      throw Error('Invalid command payload');
    const guardedEdit = [
      'paste-nodes',
      'paste-layer-snapshot',
      'move-layers-to-folder',
      'remove-layers-from-folder',
    ].includes(type);
    const layerClipboardSnapshot =
      type === 'paste-layer-snapshot' ? this.#resolveLayerClipboardSnapshot(payload.token) : null;
    const assertGuardedLocal = () => {
      if (
        guardedEdit &&
        (this.#history.doc !== entry.document ||
          this.#history.revision !== entry.revision ||
          projectDocumentContexts.get(this.#history.doc) !== entry.binding ||
          this.#projectContextOwner.epoch !== entry.contextEpoch ||
          this.#nodeClipboard !== entry.clipboard)
      )
        throw Error('STALE_REVISION: document changed');
      if (
        layerClipboardSnapshot &&
        this.#resolveLayerClipboardSnapshot(payload.token) !== layerClipboardSnapshot
      )
        throw Error('STALE_SESSION: copied Layer changed');
    };
    assertGuardedLocal();
    // These edits invoke external font and precommit hooks before History.
    const guardedAuthority = guardedEdit ? this.#captureArtworkMoveAuthority() : null;
    const assertGuardedCurrent = () => {
      assertGuardedLocal();
      guardedAuthority?.();
      assertGuardedLocal();
    };
    const assertGuardedDraftsClear = () => {
      if (
        this.#gesture ||
        this.hasTextComposition ||
        (type !== 'paste-nodes' && this.#textDrafts.size > 0) ||
        [...this.#textDrafts.values()].some(
          (draft) => draft.revision === this.#history.revision && draft.state.changed,
        )
      )
        throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
    };
    const assertGuardedReady = () => {
      if (!guardedEdit) return;
      assertGuardedCurrent();
      const busy = this.#busy();
      assertGuardedCurrent();
      if (busy) throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
      assertGuardedDraftsClear();
      assertGuardedCurrent();
      // Authority is a callback too: it can start a draft without a revision.
      // This final check reads only Session-private state before History writes.
      assertGuardedDraftsClear();
    };
    assertGuardedCurrent();
    const artworkMove =
      type === 'translate-artworks' ||
      (type === 'begin-gesture' && payload.target?.kind === 'artwork-translate') ||
      (['update-gesture', 'commit-gesture'].includes(type) &&
        this.#gesture?.target.kind === 'artwork-translate');
    const moveDocument = artworkMove ? this.#history.doc : null;
    const moveRevision = artworkMove ? this.#history.revision : null;
    const moveBinding = artworkMove ? projectDocumentContexts.get(moveDocument) : null;
    const moveDraft = artworkMove ? this.#gesture : null;
    const moveDraftVersion = moveDraft?.version;
    const ownerAuthority = artworkMove ? this.#captureArtworkMoveAuthority() : null;
    const assertArtworkMoveCurrent = () => {
      if (!artworkMove) return;
      const assertLocal = () => {
        if (
          this.#history.doc !== moveDocument ||
          this.#history.revision !== moveRevision ||
          projectDocumentContexts.get(this.#history.doc) !== moveBinding
        )
          throw Error('STALE_REVISION: document changed');
        if (this.#gesture !== moveDraft || (moveDraft && moveDraft.version !== moveDraftVersion))
          throw Error('STALE_GESTURE: draft changed');
        if (moveDraft && moveDraft.moveBinding !== moveBinding)
          throw Error('STALE_GESTURE: project context changed');
      };
      assertLocal();
      ownerAuthority?.();
      assertLocal();
    };
    if (type === 'import-composition') {
      const document = this.#history.doc;
      const revision = this.#history.revision;
      const assertCurrent = () => {
        const checkRevision = () => {
          if (this.#history.revision !== revision || this.#history.doc !== document)
            throw Error('STALE_REVISION: document changed');
        };
        checkRevision();
        if (
          this.#busy() ||
          this.#gesture ||
          this.hasTextComposition ||
          [...this.#textDrafts.values()].some(
            (draft) => draft.revision === this.#history.revision && draft.state.changed,
          )
        )
          throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
        checkRevision();
      };
      assertCurrent();
      if (typeof payload.sourceText !== 'string' || typeof payload.compositionId !== 'string')
        throw Error('Invalid composition import payload');
      const imported = prepareCompositionImport(
        document,
        payload.sourceText,
        payload.compositionId,
        payload,
      );
      assertCurrent();
      assertNoAccidentalCurvePointRebind(document, imported, this.#nodeClipboard);
      assertArtworkReferenceTransition(document, imported, this.#fonts());
      assertCurrent();
      this.#beforeCandidateCommit?.(document, imported);
      assertCurrent();
      this.#history.apply(() => {
        this.#history.doc = imported;
      });
      return this.state;
    }
    if (type === 'begin-gesture') {
      if (
        this.#gesture ||
        this.#busy() ||
        this.hasTextComposition ||
        [...this.#textDrafts.values()].some(
          (draft) => draft.revision === this.#history.revision && draft.state.changed,
        )
      )
        throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
      const target = clone(payload.target),
        base = cloneDocument(this.#history.doc);
      if (
        target?.kind === 'pen' &&
        (target.nodeId !== undefined ||
          target.endpointId !== undefined ||
          target.layerId !== undefined)
      ) {
        if (payload.position !== undefined)
          throw Error('Choose an endpoint before adding a new point');
        const layer = base.layers.find((l) => l.id === target.layerId),
          source = resolvePenEndpoint(layer, target.nodeId, target.endpointId, target.contourId);
        if (insertionRemapDependents(layer, source.node.id).length)
          throw Error(
            'Continuation is unavailable while this source feeds retained Path Remap edits',
          );
        this.#gesture = {
          id: uid('gesture'),
          target,
          base,
          candidate: cloneDocument(base),
          revision: this.#history.revision,
          version: 0,
          penLayer: clone(layer),
          penNodeId: source.node.id,
          penContourId: source.contourId,
          penSide: source.side,
          penPointId: source.pointId,
          closingPointId: source.closingPointId,
          resuming: true,
          pressed: false,
          pressedBefore: null,
        };
        return { ...this.state, ...this.gestureState };
      }
      if (target?.kind === 'pen') {
        const layer = createPenArtworkDraft(base, payload.position),
          candidate = cloneDocument(base);
        candidate.layers.push(layer);
        this.#gesture = {
          id: uid('gesture'),
          target,
          base,
          candidate,
          revision: this.#history.revision,
          version: 0,
          penLayer: layer,
          pressed: true,
          pressedBefore: null,
        };
        return { ...this.state, ...this.gestureState };
      }
      const layer = base.layers.find((layer) => layer.id === target?.layerId),
        node = layer?.nodes.find((node) => node.id === target.nodeId);
      const q = node && target.kind === 'parameter' ? parameter(node, target.key) : null;
      let initial =
        target?.kind === 'text-font'
          ? { initial: true }
          : target?.kind === 'text-values'
            ? { values: this.#textGestureValues(base, target) }
            : [
                  'transform-anchor',
                  'graph-translate',
                  'artwork-translate',
                  'nodes-translate',
                  'character-translate',
                  'path-units-translate',
                  'curve-point',
                  'remap-point',
                ].includes(target?.kind)
              ? { delta: [0, 0] }
              : target?.kind === 'guide-position'
                ? { value: base.editor?.guides?.find((g) => g.id === target.guideId)?.position }
                : target?.kind === 'numeric-command'
                  ? { value: target.initial }
                  : target?.kind === 'parameter'
                    ? {
                        value: target.alpha
                          ? q?.value?.alpha
                          : target.axis !== undefined
                            ? q?.value?.[target.axis]
                            : q?.value,
                      }
                    : { width: node?.width ?? 264 };
      if (target?.kind === 'color-slots')
        initial = {
          updates: (Array.isArray(target.slotIds) ? target.slotIds : []).map((slotId) => ({
            slotId,
            value: base.colorSlots?.find((slot) => slot.id === slotId)?.value,
          })),
        };
      this.#gestureCandidate(base, target, initial);
      assertArtworkMoveCurrent();
      this.#gesture = {
        id: uid('gesture'),
        target,
        base,
        candidate: base,
        revision: this.#history.revision,
        version: 0,
        ...(artworkMove ? { moveBinding } : {}),
      };
      return { ...this.state, ...this.gestureState };
    }
    if (
      [
        'update-gesture',
        'commit-gesture',
        'append-pen-point',
        'update-pen-point',
        'accept-pen-point',
        'cancel-pen-point',
        'commit-pen',
      ].includes(type)
    ) {
      const draft = this.#gesture;
      if (
        !draft ||
        payload.gestureId !== draft.id ||
        payload.expectedPreviewRevision !== draft.version
      )
        throw Error('STALE_GESTURE: draft changed');
      if (draft.revision !== this.#history.revision || !sameDocument(draft.base, this.#history.doc))
        throw Error('STALE_GESTURE: document changed');
      if (type.endsWith('-pen-point') || type === 'commit-pen')
        return this.#penCommand(type, payload, draft);
      if (draft.target.kind === 'pen') throw Error('Use Pen point and path commands');
      if (type === 'update-gesture') {
        const candidate = this.#gestureCandidate(draft.base, draft.target, payload.update);
        assertArtworkReferenceTransition(draft.base, candidate, this.#fonts());
        if (candidate.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION)
          this.#beforeCandidateCommit?.(this.#history.doc, candidate);
        const update = clone(payload.update);
        assertArtworkMoveCurrent();
        draft.candidate = candidate;
        draft.update = update;
        draft.version++;
        return { ...this.state, ...this.gestureState };
      }
      // Font adoption allocates private asset IDs during preview. Commit that
      // validated candidate verbatim instead of allocating different IDs again.
      const candidate =
        draft.update && draft.target.kind !== 'text-font'
          ? this.#gestureCandidate(draft.base, draft.target, draft.update)
          : draft.candidate;
      validate(candidate);
      assertNoAccidentalCurvePointRebind(draft.base, candidate, this.#nodeClipboard);
      assertArtworkReferenceTransition(draft.base, candidate, this.#fonts());
      assertVariableDocumentReady(candidate, resolveFonts(candidate, this.#fonts()));
      assertArtworkMoveCurrent();
      const revision = this.#history.revision;
      const changed = !sameDocument(this.#history.doc, candidate);
      assertArtworkMoveCurrent();
      if (changed) {
        this.#beforeCandidateCommit?.(this.#history.doc, candidate);
        assertArtworkMoveCurrent();
        this.#history.apply(() => {
          this.#history.doc = candidate;
        });
      }
      this.#gesture = null;
      return { ...this.state, changed: revision !== this.#history.revision };
    }
    if (this.#gesture && type !== 'cancel-text-draft')
      throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
    if (type === 'cancel-text-draft') {
      this.#textDrafts.delete(payload.draftId);
      return this.state;
    }
    if (type === 'begin-text-draft' || type === 'stage-text-draft') {
      let draft,
        draftId = payload.draftId;
      if (type === 'begin-text-draft') {
        const layer = this.#history.doc.layers.find((layer) => layer.id === payload.layerId),
          node = layer?.nodes.find((node) => node.id === payload.nodeId && node.type === 'Text');
        if (!node) throw Error('Select a Text node');
        if (layer.locked) throw Error('Composition is locked');
        for (const [id, pending] of this.#textDrafts) {
          if (pending.layerId === layer.id && pending.nodeId === node.id) {
            if (pending.composition && pending.revision === this.#history.revision)
              throw Error('Finish or cancel text composition first');
            this.#textDrafts.delete(id);
          }
        }
        draftId = uid('text-draft');
        draft = new TextDraft({
          layerId: layer.id,
          nodeId: node.id,
          revision: this.#history.revision,
          text: node.text,
          record: node.characterPositions,
        });
        this.#textDrafts.set(draftId, draft);
      } else {
        draft = this.#textDrafts.get(draftId);
        if (!draft) throw Error('STALE_TEXT_DRAFT: unknown draft');
        const layer = this.#history.doc.layers.find((layer) => layer.id === draft.layerId),
          node = layer?.nodes.find((node) => node.id === draft.nodeId);
        if (layer?.locked) throw Error('Composition is locked');
        draft.assertCurrent(node, this.#history.revision);
        draft.stage(payload);
      }
      return { ...this.state, draftId, ...draft.state };
    }
    if (this.#busy() || this.hasTextComposition)
      throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
    if (
      [
        'copy-nodes',
        'paste-nodes',
        'save-node-preset',
        'update-node-preset',
        'apply-node-preset',
      ].includes(type) &&
      [...this.#textDrafts.values()].some(
        (d) => d.revision === this.#history.revision && d.state.changed,
      )
    )
      throw Error('Finish or cancel the Text draft before copying/pasting nodes');
    if (type === 'copy-nodes') {
      const doc = this.#history.doc,
        revision = this.#history.revision,
        binding = projectDocumentContexts.get(doc),
        contextEpoch = this.#projectContextOwner.epoch,
        previousClipboard = this.#nodeClipboard,
        layer = doc.layers.find((l) => l.id === payload.layerId);
      const assertCopyCurrent = () => {
        if (
          this.#history.doc !== doc ||
          this.#history.revision !== revision ||
          projectDocumentContexts.get(doc) !== binding ||
          this.#projectContextOwner.epoch !== contextEpoch ||
          this.#nodeClipboard !== previousClipboard
        )
          throw Error('STALE_REVISION: document changed');
      };
      if (
        !layer ||
        !Array.isArray(payload.nodeIds) ||
        !payload.nodeIds.length ||
        new Set(payload.nodeIds).size !== payload.nodeIds.length ||
        payload.nodeIds.some((id) => !layer.nodes.some((n) => n.id === id))
      )
        throw Error('Select distinct nodes in this composition');
      const selected = layer.nodes.filter((n) => payload.nodeIds.includes(n.id)),
        nodes = selected.filter((n) => !['Output', 'Image Output'].includes(n.type));
      if (!nodes.length)
        throw Error('Copy source/effect nodes; each Composition keeps its own Output');
      if (
        doc.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
        nodesHaveComponentCompositionReferenceSyntax(nodes)
      )
        throw Error('Copying nodes with Component Composition references is not supported');
      if (
        doc.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
        nodesHaveCompositionPropertyReferenceSyntax(nodes)
      )
        throw Error('Copying nodes with Composition property references is not supported');
      const colorArtworkReferences = colorArtworkClipboardReferences(nodes, doc.schemaVersion);
      const ids = new Set(nodes.map((n) => n.id)),
        externalNodeIds = [
          ...new Set(
            [
              ...nodes.flatMap((n) => [
                // A connected descriptor can retain arbitrary metadata named
                // layerId; its socket source is still local to this Artwork.
                ...parameters(n)
                  .filter(([, q]) => q.source)
                  .map(([, q]) => q.source),
                ...nodeDependencies(n)
                  .filter((d) => !d.layerId)
                  .map((d) => d.source),
              ]),
              ...curvePointReferenceOwners(nodes).filter((id) =>
                layer.nodes.some((node) => node.id === id && node.type === 'Curve'),
              ),
            ].filter((id) => !ids.has(id)),
          ),
        ];
      const fontIds = new Set(
          nodes
            .filter((n) => n.type === 'Text')
            .flatMap((n) => [
              n.font,
              ...Object.values(n.characterPositions?.styles || {}).map((s) => s.font),
            ])
            .filter((id) => id && id !== 'default'),
        ),
        imageIds = new Set(nodes.filter((n) => n.type === 'Image').map((n) => n.asset));
      const artworkLayers = new Map(doc.layers.map((item) => [item.id, item]));
      const artworkReferences = [
        ...new Map(
          [...artworkReferencesInLayers([{ nodes }]), ...colorArtworkReferences].map((ref) => [
            JSON.stringify([ref.qualifier || '', ref.layerId, ref.nodeId, ref.key, ref.component]),
            {
              ...ref,
              ownerType: artworkLayers
                .get(ref.layerId)
                ?.nodes.find((item) => item.id === ref.nodeId)?.type,
            },
          ]),
        ).values(),
      ];
      if (doc.schemaVersion < ARTWORK_REFERENCE_SCHEMA_VERSION && artworkReferences.length)
        throw Error('Legacy Artwork reference text cannot be copied into a newer schema');
      const artworkInputCount = new Set(
        artworkReferences.map((ref) => `${ref.layerId}\0${ref.nodeId}`),
      ).size;
      const clipboard = {
        nodes: clone(nodes),
        sourceLayerId: layer.id,
        schemaVersion: doc.schemaVersion,
        referenceSchemaVersion: clipboardReferenceSchemaVersion(nodes, layer.nodes),
        pasteCount: 0,
        externalNodeIds,
        artworkReferences,
        colorArtworkReferences: colorArtworkReferences.length > 0,
        colorContextIdentity: binding?.clipboardIdentity ?? null,
        colorContextEpoch: contextEpoch,
        artworkInputCount,
        artworkReferencesExpired: false,
        fonts: Object.fromEntries([...fontIds].map((id) => [id, doc.fontAssets?.[id]])),
        images: Object.fromEntries([...imageIds].map((id) => [id, doc.imageAssets?.[id]])),
      };
      if (artworkReferences.length && !artworkClipboardTargetsIntact(doc, clipboard))
        throw Error('Cannot copy: an Artwork source is missing or changed; restore it first');
      if (colorArtworkReferences.length) {
        assertCopyCurrent();
        if (
          doc !== entry.document ||
          revision !== entry.revision ||
          binding !== entry.binding ||
          contextEpoch !== entry.contextEpoch ||
          previousClipboard !== entry.clipboard
        )
          throw Error('STALE_REVISION: document changed');
      }
      this.#nodeClipboard = clipboard;
      return {
        ...this.state,
        changed: false,
        copiedCount: nodes.length,
        externalInputCount: externalNodeIds.length + artworkInputCount,
        skippedOutputs: selected.length - nodes.length,
      };
    }
    if (type === 'undo' || type === 'redo') {
      const next = (type === 'undo' ? this.#history.past : this.#history.future).at(-1);
      if (next) assertVariableDocumentReady(next, resolveFonts(next, this.#fonts()));
      this.#history[type]();
      return this.state;
    }
    let commandBase = this.#history.doc;
    const arrangementDrafts = ['set-character-arrangement', 'clear-character-arrangement'].includes(
      type,
    )
      ? [...this.#textDrafts.entries()].filter(
          ([, draft]) =>
            draft.nodeId === payload.nodeId && draft.revision === this.#history.revision,
        )
      : [];
    if (arrangementDrafts.length) {
      commandBase = cloneDocument(commandBase);
      for (const [, draft] of arrangementDrafts) {
        const node = commandBase.layers
          .find((l) => l.id === draft.layerId)
          ?.nodes.find((n) => n.id === draft.nodeId);
        draft.assertCurrent(node, this.#history.revision);
        node.text = draft.state.changed ? draft.raw : draft.baseText;
        if (draft.record) node.characterPositions = clone(draft.record);
        else delete node.characterPositions;
      }
    }
    const { candidate, result, committingDraft } = this.#commandCandidate(
      type,
      payload,
      commandBase,
    );
    assertGuardedReady();
    assertVariableDocumentReady(candidate, resolveFonts(candidate, this.#fonts()));
    assertGuardedReady();
    assertNoAccidentalCurvePointRebind(this.#history.doc, candidate, this.#nodeClipboard);
    assertArtworkReferenceTransition(this.#history.doc, candidate, this.#fonts());
    assertGuardedReady();
    assertArtworkMoveCurrent();
    const revision = this.#history.revision;
    if (!sameDocument(this.#history.doc, candidate)) {
      this.#beforeCandidateCommit?.(this.#history.doc, candidate);
      assertGuardedReady();
      assertArtworkMoveCurrent();
      if (this.#history.retainHistory) {
        this.#history.apply(() => {
          this.#history.doc = candidate;
        });
      } else {
        // The project owner retains History. This already-validated candidate
        // replaces the whole inner snapshot; no callback mutates the old one,
        // so a detached rollback copy would be immediately discarded.
        const before = this.#history.doc;
        this.#history.doc = candidate;
        if (!sameDocument(before, candidate)) this.#history.record(before);
      }
    }
    if (type === 'paste-nodes' && revision !== this.#history.revision)
      this.#nodeClipboard.pasteCount++;
    if (type === 'import-svg') this.#nodeClipboard = null;
    for (const [id] of arrangementDrafts) this.#textDrafts.delete(id);
    if (committingDraft) this.#textDrafts.delete(payload.draftId);
    return { ...this.state, ...result, changed: revision !== this.#history.revision };
  }
  #commandCandidate(type, payload, before, deferVariable = false, preparedFonts = null) {
    if (
      [
        'create-layer-folder',
        'rename-layer-folder',
        'add-layers-to-folder',
        'move-layers-to-folder',
        'remove-layers-from-folder',
        'dissolve-layer-folder',
      ].includes(type)
    ) {
      const candidate = cloneDocument(before);
      const result =
        type === 'create-layer-folder'
          ? createLayerFolder(candidate, {
              ...payload,
              folderId: payload.folderId ?? uid('folder'),
            })
          : type === 'rename-layer-folder'
            ? renameLayerFolder(candidate, payload)
            : type === 'add-layers-to-folder'
              ? addLayersToFolder(candidate, payload)
              : type === 'move-layers-to-folder'
                ? moveLayersToFolder(candidate, payload)
                : type === 'remove-layers-from-folder'
                  ? removeLayersFromFolder(candidate, payload)
                  : dissolveLayerFolder(candidate, payload);
      validate(candidate);
      return { candidate, result };
    }
    if (colorSlotCommands.has(type)) {
      const candidate = cloneDocument(before);
      const result = applyColorSlotCommand(candidate, type, payload, () => uid('color'));
      validate(candidate);
      return { candidate, result };
    }
    if (nodeGroupCommands.has(type)) {
      const candidate = cloneDocument(before),
        layer = candidate.layers.find((l) => l.id === payload.layerId);
      if (!layer) throw Error('Missing composition');
      if (layer.locked) throw Error('Composition is locked');
      const result = applyNodeGroupCommand(candidate, layer, type, payload, () =>
        uid('node-group'),
      );
      validate(candidate);
      return { candidate, result };
    }
    if (type === 'create-point-handles') {
      const candidate = cloneDocument(before),
        layer = candidate.layers.find((l) => l.id === payload.layerId);
      const item = layer?.nodes.find(
        (n) => n.id === payload.nodeId && ['Curve', 'Multi Path', 'Path Remap'].includes(n.type),
      );
      if (!item) throw Error('Select an editable path node');
      if (item.locked === true) throw Error('Path node is locked');
      if (layer.locked) throw Error('Composition is locked');
      if (item.bypassed) throw Error('Enable the path before editing handles');
      const evaluated = evaluate(
        { ...layer, output: item.id },
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      if (evaluated.error) throw Error(evaluated.error);
      if (evaluated.path.remapSuspended)
        throw Error('Path edits are suspended; rebind explicitly first');
      const contour = evaluated.path.contours.find((c) =>
        c.points.some((p) => p.id === payload.pointId),
      );
      const index = contour?.points.findIndex((p) => p.id === payload.pointId),
        current = contour?.points[index];
      if (!current) throw Error('Select a path anchor');
      const input = item.type === 'Path Remap' ? evaluated.run(item.params.path.source) : null;
      const pt = input
        ? enableRemapPoint(item, input, payload.pointId).params
        : item.points.find((p) => p.id === payload.pointId);
      if (
        ['position', 'handleA', 'handleB'].some(
          (key) => pt[key].locked || pt[key].source || pt[key].expression,
        )
      )
        throw Error('Anchor or handles are locked or driven');
      let delta = payload.delta;
      if (delta === undefined) {
        if (Math.hypot(...current.handleA) || Math.hypot(...current.handleB))
          return { candidate: cloneDocument(before), result: {} };
        const previous = (
            contour.points[index - 1] ?? (contour.closed ? contour.points.at(-1) : current)
          ).position,
          next = (contour.points[index + 1] ?? (contour.closed ? contour.points[0] : current))
            .position;
        delta = next.map((v, axis) => (v - previous[axis]) / 6);
        if (!delta.some((v) => v !== 0)) delta = [20, 0];
      }
      assertValue('Vector2', delta);
      const original = input?.contours
        .flatMap((c) => c.points)
        .find((p) => p.id === payload.pointId);
      pt.handleA.value = delta.map((v, axis) => -v - (original?.handleA[axis] ?? 0));
      pt.handleB.value = delta.map((v, axis) => v - (original?.handleB[axis] ?? 0));
      if (!input) pt.handleType = 'symmetric';
      validate(candidate);
      return { candidate, result: {} };
    }
    if (type === 'translate-path-units') {
      assertValue('Vector2', payload.delta);
      const candidate = cloneDocument(before),
        layer = candidate.layers.find((l) => l.id === payload.layerId);
      const item = layer?.nodes.find((n) => n.id === payload.nodeId && n.type === 'Multi Path');
      if (!item) throw Error('Select a Multi Path node');
      if (item.locked === true) throw Error('Path node is locked');
      if (layer.locked) throw Error('Composition is locked');
      if (
        !Array.isArray(payload.unitIds) ||
        !payload.unitIds.length ||
        new Set(payload.unitIds).size !== payload.unitIds.length ||
        payload.unitIds.some((id) => !item.units.some((u) => u.id === id))
      )
        throw Error('Select distinct path units');
      const contours = new Set(
        item.units.filter((u) => payload.unitIds.includes(u.id)).flatMap((u) => u.contours),
      );
      const points = new Set(
        item.contours.filter((c) => contours.has(c.id)).flatMap((c) => c.points),
      );
      for (const pt of item.points.filter((pt) => points.has(pt.id))) {
        if (pt.position.locked || pt.position.source || pt.position.expression)
          throw Error('Path unit position is locked or driven');
        pt.position.value = pt.position.value.map((v, axis) => v + payload.delta[axis]);
      }
      validate(candidate);
      return { candidate, result: {} };
    }
    if (['align-points', 'distribute-points', 'space-points'].includes(type)) {
      const planner =
        type === 'align-points'
          ? planPointAlignment
          : type === 'space-points'
            ? planPointSpacing
            : planPointDistribution;
      const pointMoves = planner(
        before,
        payload.layerId,
        payload.nodeId,
        payload.pointIds,
        payload.action,
        payload.sourceKey,
        { fonts: resolveFonts(before, this.#fonts()), spacing: payload.spacing },
      );
      return this.#commandCandidate('translate-points', { ...payload, pointMoves }, before);
    }
    if (type === 'translate-points') {
      const item = before.layers
        .find((l) => l.id === payload.layerId)
        ?.nodes.find((n) => n.id === payload.nodeId);
      if (!['Curve', 'Multi Path', 'Path Remap'].includes(item?.type) || item.locked)
        throw Error('Select an editable source path');
      if (
        !Array.isArray(payload.pointIds) ||
        !payload.pointIds.length ||
        payload.pointIds.length > 10000 ||
        new Set(payload.pointIds).size !== payload.pointIds.length
      )
        throw Error('Select distinct point anchors');
      return {
        candidate: this.#gestureCandidate(
          before,
          {
            kind: item.type === 'Path Remap' ? 'remap-point' : 'curve-point',
            layerId: payload.layerId,
            nodeId: item.id,
            pointId: payload.pointIds[0],
            pointIds: payload.pointIds,
            key: 'position',
          },
          payload.pointMoves === undefined
            ? { delta: payload.delta }
            : {
                pointMoves: payload.pointMoves,
                ...(payload.delta === undefined ? {} : { delta: payload.delta }),
              },
        ),
        result: {},
      };
    }
    if (['set-os-font-text', 'clear-os-font-text', 'replace-os-text-font'].includes(type)) {
      const candidate = cloneDocument(before),
        layer = candidate.layers.find((l) => l.id === payload.layerId),
        source = layer?.nodes.find((n) => n.id === payload.nodeId && n.type === 'Text');
      if (!source || source.locked || layer.locked || source.bypassed)
        throw Error('Select an unlocked enabled Text');
      if ([...this.#textDrafts.values()].some((d) => d.nodeId === source.id))
        throw Error('Finish or cancel the Text draft first');
      let replacement;
      if (type === 'replace-os-text-font') {
        if (!source.osFont) throw Error('Select OS font Text');
        if (payload.characterIds !== undefined)
          throw Error('OS font replacement requires whole Text');
        const fromBytes = Object.hasOwn(payload, 'bytes');
        if (fromBytes === Object.hasOwn(payload, 'fontId'))
          throw Error('Choose exactly one replacement font');
        if (fromBytes) {
          if (!(payload.bytes instanceof ArrayBuffer) || payload.bytes.byteLength > MAX_FONT_BYTES)
            throw Error('Font bytes require an ArrayBuffer up to 20 MB');
          replacement = makeFontAsset(
            payload.bytes.slice(0),
            payload.fileName,
            candidate.fontAssets,
          );
          candidate.fontAssets = { ...candidate.fontAssets, [replacement.id]: replacement.asset };
        } else {
          const asset = candidate.fontAssets?.[payload.fontId];
          if (payload.fontId !== 'default' && !asset) throw Error('Missing replacement font');
          if (asset?.kind) throw Error('OS font replacement requires a static font');
          replacement = { id: payload.fontId, asset };
        }
        // The static legacy shaper defaults to ligatures-off. Keep a prepared
        // Windows ligatures-on intent explicitly when the static layout/font
        // can represent it; unsupported combinations fail below, never strip.
        if (source.osFont.outline.ligatures === true && !source.fontFeatures) {
          source.fontFeatures = { liga: true };
          candidate.schemaVersion = Math.max(candidate.schemaVersion, OPENTYPE_SCHEMA_VERSION);
        }
        delete source.osFont;
        source.font = replacement.id;
      } else if (type === 'clear-os-font-text') {
        if (!source.osFont) throw Error('Select OS font Text');
        delete source.osFont;
        source.font = 'default';
      } else {
        const modern = [
          'directwrite-v1',
          'directwrite-v2',
          'directwrite-v3',
          'directwrite-v4',
        ].includes(payload.outline?.engine);
        // Source removal is an explicit installed-font choice. An edit adapter
        // that forgot a file capability must not relabel its retained geometry
        // as a same-named system font.
        if (source.osFont?.source !== undefined && payload.source === undefined)
          throw Error('Private font source is missing; reattach or choose an installed font');
        if (!modern && payload.source !== undefined)
          throw Error('Private font sources require DirectWrite Text');
        const referenceSource = payload.source === null ? undefined : payload.source;
        const settings = payload.settings || {};
        if (
          settings.axes !== undefined &&
          (!settings.axes || Object.getPrototypeOf(settings.axes) !== Object.prototype)
        )
          throw Error('Invalid font-axis settings');
        if (
          Object.keys(settings).some(
            (k) =>
              ![
                'size',
                'tracking',
                'lineSpacing',
                'kerning',
                'ligatures',
                'alignment',
                'axes',
                'writingMode',
                'textWrap',
              ].includes(k),
          ) ||
          (!modern && Object.keys(settings).length) ||
          (settings.ligatures !== undefined && settings.ligatures !== payload.outline.ligatures)
        )
          throw Error('Invalid or mismatched prepared Text settings');
        const next = {
          ...source,
          text: payload.text ?? source.text,
          font: 'default',
          params: clone(source.params),
          osFont: modern
            ? desktopOsReference(payload.outline, referenceSource)
            : {
                version: 1,
                family: payload.outline?.family,
                style: payload.outline?.style,
                outline: clone(payload.outline),
              },
        };
        // Do not discard variable/rich/path semantics as a side effect of choosing
        // an OS font. The same source restrictions apply before and after adoption.
        if (modern) {
          for (const [key, param] of [
            ['size', 'size'],
            ['tracking', 'letterSpacing'],
            ['lineSpacing', 'lineSpacing'],
          ]) {
            if (settings[key] !== undefined) {
              if (
                source.params[param].locked ||
                source.params[param].source ||
                source.params[param].expression
              )
                throw Error('Text setting is locked or driven');
              next.params[param].value = settings[key];
            }
          }
          if (Object.hasOwn(settings, 'textWrap'))
            next.textWrap = settings.textWrap === null ? undefined : clone(settings.textWrap);
          if (settings.kerning !== undefined) next.kerning = settings.kerning;
          if (settings.alignment !== undefined) next.alignment = settings.alignment;
          if (settings.writingMode !== undefined) next.writingMode = settings.writingMode;
          if (
            settings.axes !== undefined &&
            (![3, 4, 5].includes(next.osFont.version) ||
              Object.keys(settings.axes).length !== next.osFont.axes.length ||
              next.osFont.axes.some(
                (a) => Math.fround(settings.axes[a.tag]) !== Math.fround(a.value),
              ))
          )
            throw Error('Mismatched prepared font axes');
          desktopTextIntent(next, next.osFont.face, {}, next.osFont.source);
        } else
          osOutlineGeometry(payload.outline, { ...source, text: next.text }, uid, {
            allowEmpty: true,
          });
        if (source.characterPositions)
          next.characterPositions = reconcileCharacterEdit(source.characterPositions, {
            previousRaw: source.text,
            nextRaw: next.text,
          }).record;
        Object.assign(source, next);
        if (Object.hasOwn(settings, 'textWrap') && settings.textWrap === null)
          delete source.textWrap;
        candidate.schemaVersion = Math.max(
          candidate.schemaVersion,
          source.osFont.source ? PRIVATE_FONT_SCHEMA_VERSION : 0,
          source.osFont.version === 5
            ? 34
            : source.osFont.version === 4
              ? 32
              : source.osFont.version === 3
                ? 30
                : modern
                  ? 29
                  : 28,
        );
        validateOsFontText(source, candidate.schemaVersion);
      }
      validate(candidate);
      if (['clear-os-font-text', 'replace-os-text-font'].includes(type)) {
        // Replacing a saved OS appearance must be all-or-nothing. Validation
        // alone checks IDs/types, not coverage or layout in the replacement face.
        const evaluated = evaluate(
          { ...layer, output: source.id },
          resolveFonts(candidate, this.#fonts()),
          candidate.imageAssets,
          candidate,
        );
        if (evaluated.error)
          throw Error(
            `OS font retained: replacement font cannot render this Text. ${evaluated.error}`,
          );
      }
      serialize(candidate);
      return {
        candidate,
        result: {
          nodeId: source.id,
          ...(replacement ? { fontId: replacement.id, fontName: replacement.asset?.name } : {}),
        },
      };
    }
    if (['bake-text-outline', 'bake-os-text-outline'].includes(type)) {
      const layer = before.layers.find((l) => l.id === payload.layerId);
      const source = layer?.nodes.find((n) => n.id === payload.nodeId);
      if (source?.type !== 'Text') throw Error('Select a Text node to bake');
      if (source.locked === true) throw Error('Text node is locked');
      if (
        [...this.#textDrafts.values()].some(
          (draft) => draft.nodeId === source.id && draft.revision === this.#history.revision,
        )
      )
        throw Error('Finish or cancel the Text draft before baking');
      if (layer.locked) throw Error('Composition is locked');
      if (source.bypassed) throw Error('Enable Text before baking');
      if (parameters(source).some(([, q]) => q.locked))
        throw Error('Text has locked parameters; unlock before baking');
      if (
        before.layers.some(
          (otherLayer) =>
            otherLayer.id !== layer.id &&
            otherLayer.nodes.some((n) =>
              parameters(n).some(
                ([, q]) =>
                  q.expression &&
                  artworkExpressionReferences(q.expression).some(
                    (ref) => ref.layerId === layer.id && ref.nodeId === source.id,
                  ),
              ),
            ),
        )
      )
        throw Error('Text parameter references are in use; disconnect them before baking');
      for (const other of layer.nodes) {
        if (other.id === source.id) continue;
        const connected = new Set();
        if (
          collectNodeDependencies(other, false, connected).some(
            (dep) =>
              dep.source === source.id &&
              ((dep.parameterKey !== undefined && !connected.has(dep)) ||
                dep.sourcePort !== undefined ||
                other.type === 'Reference'),
          )
        )
          throw Error(
            'Text Position or parameter references are in use; disconnect them before baking',
          );
      }
      let path, outlineUnits;
      if (type === 'bake-os-text-outline') {
        if (
          ['directwrite-v1', 'directwrite-v2', 'directwrite-v3', 'directwrite-v4'].includes(
            payload.outline?.engine,
          )
        ) {
          if (parameters(source).some(([, q]) => q.source || q.expression))
            throw Error('Disconnect Text parameters before direct OS conversion');
          const prepared = {
            ...source,
            font: 'default',
            osFont: desktopOsReference(payload.outline),
          };
          path = desktopFontTextPath(
            prepared,
            source.params.position.value,
            source.params.size.value,
            pathBounds,
          );
        } else {
          path = osOutlineGeometry(payload.outline, source, uid);
          const bounds = pathBounds(path),
            x = bounds[0][source.alignment === 'center' ? 1 : source.alignment === 'end' ? 2 : 0],
            y =
              source.crossAlignment && source.crossAlignment !== 'origin'
                ? bounds[1][
                    source.crossAlignment === 'center' ? 1 : source.crossAlignment === 'end' ? 2 : 0
                  ]
                : payload.outline.ascent;
          const delta = [source.params.position.value[0] - x, source.params.position.value[1] - y];
          for (const contour of path.contours)
            for (const pt of contour.points) pt.position = pt.position.map((v, k) => v + delta[k]);
        }
        outlineUnits = [{ id: uid('unit'), contours: path.contours.map((c) => c.id) }];
      } else {
        const evaluated = evaluate(
          { ...layer, output: source.id },
          resolveFonts(before, this.#fonts()),
          before.imageAssets,
          before,
        );
        if (evaluated.error)
          throw Object.assign(
            Error(evaluated.error),
            evaluated.errorCode ? { code: evaluated.errorCode } : {},
          );
        path = evaluated.path;
        const arrangement = evaluated.textLayout(source.id).arrangementStatus;
        if (arrangement?.suspended)
          throw Error(`Maintained ink gaps suspended: ${arrangement.reason}`);
        if (path.characterOccurrences?.some((o) => o.suspended))
          throw Error('Character adjustments are suspended; repair them before baking');
        outlineUnits = clone(evaluated.textLayout(source.id).outlineUnits);
      }
      if (!path.contours.some((c) => c.points.length)) throw Error('Text has no outlines to bake');
      if (
        path.contours.length > 10000 ||
        path.contours.reduce((sum, c) => sum + c.points.length, 0) > 10000
      )
        throw Error('Text outline exceeds the 10000 vertex/contour budget');
      const baked = {
        ...node('Multi Path', ...source.position),
        id: source.id,
        name: source.name,
        width: source.width,
        collapsed: source.collapsed,
        text: '',
        font: '',
        contours: path.contours.map(({ id, closed, group, points }) => ({
          id,
          closed,
          ...(group !== undefined ? { group } : {}),
          points: points.map((pt) => pt.id),
        })),
        units: outlineUnits,
        points: path.contours.flatMap((c) =>
          c.points.map((pt) => ({
            id: pt.id,
            handleType: 'corner',
            position: p('Vector2', clone(pt.position)),
            handleA: p('Vector2', clone(pt.handleA)),
            handleB: p('Vector2', clone(pt.handleB)),
          })),
        ),
      };
      const candidate = cloneDocument(before);
      candidate.layers.find((l) => l.id === layer.id).nodes[layer.nodes.indexOf(source)] = baked;
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 22);
      serialize(candidate); // Validate every reference and project-size limit before committing.
      return { candidate, result: { nodeId: baked.id } };
    }

    if (type === 'reposition-transform-anchor') {
      const candidate = cloneDocument(before),
        layer = candidate.layers.find((l) => l.id === payload.layerId),
        n = layer?.nodes.find((n) => n.id === payload.nodeId);
      if (n?.type !== 'Transform' || n.bypassed) throw Error('Select an active Transform');
      if (layer.locked) throw Error('Composition is locked');
      for (const key of ['anchor', 'position']) {
        const q = n.params[key];
        if (q && (q.locked || q.source || q.expression))
          throw Error('Anchor and Position must be unlocked and undriven');
      }
      const state = evaluate(
        layer,
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      ).transformState(n.id);
      if (!state.input?.contours.some((c) => c.points.length))
        throw Error('Connect nonempty input geometry');
      let anchor = payload.anchor;
      if (payload.preset !== undefined) {
        if (!Number.isInteger(payload.preset) || payload.preset < 0 || payload.preset > 8)
          throw Error('Unknown anchor preset');
        const bounds = pathBounds(state.input);
        anchor = [bounds[0][payload.preset % 3], bounds[1][Math.floor(payload.preset / 3)]];
      } else if (payload.delta !== undefined) anchor = anchorFromOutputDelta(state, payload.delta);
      const next = repositionTransformAnchor(state, anchor);
      if (next.anchor.every((v, i) => v === state.anchor[i])) return { candidate, result: {} };
      n.params.anchor.value = next.anchor;
      n.params.position = {
        ...(n.params.position || clone(transformExtensions.position)),
        value: next.position,
      };
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 11);
      validate(candidate);
      return { candidate, result: {} };
    }

    if (type === 'set-transform-control') {
      const base = cloneDocument(before),
        layer = base.layers.find((l) => l.id === payload.layerId),
        n = layer?.nodes.find((n) => n.id === payload.nodeId);
      if (
        !isTransform(n) ||
        ![
          'anchor',
          'position',
          'offset',
          'scale',
          'scaleWidth',
          'uniformScale',
          'skew',
          'skewAxis',
          'rotation',
        ].includes(payload.key)
      )
        throw Error('Select a Transform control');
      const q = parameter(n, payload.key);
      if (!n.params[payload.key] && ['position', 'scaleWidth'].includes(payload.key))
        q.value = evaluate(
          layer,
          resolveFonts(base, this.#fonts()),
          base.imageAssets,
          base,
        ).parameterValue(n.id, payload.key);
      const unchanged =
        !q.source &&
        !q.expression &&
        JSON.stringify(payload.axis === undefined ? q.value : q.value[payload.axis]) ===
          JSON.stringify(payload.value);
      if (unchanged) return { candidate: base, result: {} };
      if (payload.key === 'anchor' && !n.params.position) {
        const anchor = evaluate(
          layer,
          resolveFonts(base, this.#fonts()),
          base.imageAssets,
          base,
        ).parameterValue(n.id, 'anchor');
        n.params.position = { ...clone(transformExtensions.position), value: clone(anchor) };
        base.schemaVersion = Math.max(base.schemaVersion, 11);
      }
      return this.#commandCandidate('set-parameter', payload, base);
    }

    if (type === 'align-node-outputs') {
      const moves = planNodeOutputAlignment(
        before,
        payload.layerId,
        payload.nodeIds,
        payload.action,
        { fonts: resolveFonts(before, this.#fonts()), reference: payload.reference },
      );
      let candidate = cloneDocument(before);
      for (const move of moves)
        if (move.delta.some((value) => value !== 0))
          candidate = this.#gestureCandidate(candidate, move.target, { delta: move.delta });
      return { candidate, result: {} };
    }
    if (type === 'insert-svg-asset') {
      const candidate = cloneDocument(before);
      const result = insertSVGAsset(candidate, payload, resolveFonts(candidate, this.#fonts()));
      return { candidate, result };
    }
    if (type === 'add-svg-part') {
      const candidate = cloneDocument(before);
      const result = appendSVGPart(candidate, payload, resolveFonts(candidate, this.#fonts()));
      return { candidate, result };
    }
    if (type === 'import-svg') {
      const candidate = importSVGTree(payload.source);
      // Replace the active Composition's artwork, not the project-wide data
      // carried by its Document projection. Other Compositions may still use
      // these assets, presets, and newer-schema nodes.
      candidate.name = before.name;
      candidate.schemaVersion = Math.max(before.schemaVersion, candidate.schemaVersion);
      for (const key of [
        'imageAssets',
        'fontAssets',
        'nodePresets',
        'colorSlots',
        'colorDesign',
        'keyColorRecipe',
      ]) {
        if (Object.hasOwn(before, key))
          candidate[key] = ['nodePresets', 'colorSlots', 'colorDesign', 'keyColorRecipe'].includes(
            key,
          )
            ? clone(before[key])
            : { ...before[key] };
      }
      validate(candidate);
      serialize(candidate);
      return {
        candidate,
        result: { layerId: candidate.layers[0].id, nodeId: candidate.layers[0].nodes[0]?.id },
      };
    }
    if (
      [
        'create-artwork',
        'create-blank-layer',
        'create-demo-layer',
        'delete-layer',
        'delete-layers',
        'order-layer',
        'reorder-layer',
        'duplicate-artworks',
        'duplicate-layer',
        'paste-layer-snapshot',
        'adopt-image',
        'translate-artworks',
        'align-artworks',
        'distribute-artworks',
        'distribute-artwork-gaps',
      ].includes(type)
    ) {
      const candidate = cloneDocument(before);
      let result;
      if (type === 'create-artwork')
        result = appendArtwork(candidate, payload.artworkType, {
          at: payload.at,
          name: payload.name,
          writingMode: payload.writingMode,
        });
      else if (type === 'create-blank-layer')
        result = appendBlankComposition(candidate, { name: payload.name });
      else if (type === 'create-demo-layer') {
        const created = layer(payload.name ?? `Composition ${candidate.layers.length + 1}`);
        candidate.layers.push(created);
        result = { layerId: created.id, nodeId: created.nodes[0].id };
      } else if (type === 'delete-layer') result = removeArtworkLayer(candidate, payload.layerId);
      else if (type === 'delete-layers') result = removeArtworkLayers(candidate, payload.layerIds);
      else if (type === 'order-layer')
        result = moveArtworkLayer(candidate, payload.layerId, payload.direction);
      else if (type === 'reorder-layer')
        result = moveArtworkLayerBefore(candidate, payload.layerId, payload.beforeLayerId);
      else if (type === 'duplicate-artworks') {
        if (
          !Array.isArray(payload.layerIds) ||
          !payload.layerIds.length ||
          new Set(payload.layerIds).size !== payload.layerIds.length
        )
          throw Error('Select distinct artwork layers');
        const options = { fonts: resolveFonts(candidate, this.#fonts()) };
        result = {
          copies:
            payload.layerIds.length === 1
              ? [
                  {
                    sourceLayerId: payload.layerIds[0],
                    ...duplicateArtworkLayer(candidate, payload.layerIds[0], options),
                  },
                ]
              : duplicateArtworkLayers(candidate, payload.layerIds, options),
        };
      } else if (type === 'duplicate-layer') {
        const fonts = resolveFonts(candidate, this.#fonts());
        result = duplicateLayerGeneric(candidate, payload.layerId, { fonts });
      } else if (type === 'paste-layer-snapshot') {
        const captured = this.#resolveLayerClipboardSnapshot(payload.token),
          fonts = resolveFonts(candidate, { ...this.#fonts(), ...preparedFonts });
        result = duplicateLayerGeneric(candidate, captured.layerId, {
          fonts,
          snapshot: captured.snapshot,
          snapshotSchemaVersion: captured.schemaVersion,
        });
      } else if (
        [
          'translate-artworks',
          'align-artworks',
          'distribute-artworks',
          'distribute-artwork-gaps',
        ].includes(type)
      ) {
        if (!Array.isArray(payload.layerIds)) throw Error('Select artwork layer IDs');
        const options = { fonts: resolveFonts(candidate, this.#fonts()) };
        if (type === 'align-artworks') {
          if (
            payload.reference !== undefined &&
            !['selection', 'artboard'].includes(payload.reference)
          )
            throw Error('Invalid alignment reference');
          options.reference = payload.reference;
        }
        if (type === 'distribute-artwork-gaps' && payload.spacing !== undefined)
          options.spacing = payload.spacing;
        if (type === 'translate-artworks') {
          const resolved = resolveArtworkMoveSelection(candidate, payload.layerIds, options);
          if (!resolved.supported) throw Error(resolved.reason);
          translateArtworkTargets(candidate, resolved.target, payload.delta);
          const targets =
            resolved.target.scope === 'artwork-selection'
              ? resolved.target.targets
              : [resolved.target];
          for (const target of targets) {
            if (!target.canvasOnly) continue;
            const check = resolveArtworkMoveSelection(candidate, [target.layerId], options);
            if (!check.supported) throw Error(check.reason);
          }
        } else {
          const edit =
            type === 'align-artworks'
              ? alignArtworkSelection
              : type === 'distribute-artwork-gaps'
                ? distributeArtworkGaps
                : distributeArtworkSelection;
          edit(candidate, payload.layerIds, payload.action, options);
        }
        result = {};
      } else {
        const route = payload.route ?? 'layer';
        if (!['layer', 'graph'].includes(route)) throw Error('Invalid image placement route');
        for (const key of ['at', 'nodePosition']) {
          const value = payload[key];
          if (
            value !== undefined &&
            (!Array.isArray(value) ||
              value.length !== 2 ||
              !value.every((v) => Number.isFinite(v) && Math.abs(v) <= 100000))
          )
            throw Error(`Invalid image ${key}`);
        }
        if (payload.anchor !== undefined && !['origin', 'center'].includes(payload.anchor))
          throw Error('Invalid image placement anchor');
        if (payload.joinBeforeOut) throw Error('Image graph placement is disconnected');
        let target;
        if (route === 'graph') {
          target = candidate.layers.find((l) => l.id === payload.layerId);
          if (!target || target.locked || target.guide || !target.visible)
            throw Error('Choose an unlocked visible layer');
          if (target.nodes.length >= 2000)
            throw Error('Image insertion exceeds the 2000-node budget');
        }
        const size = pngSize(decodeImage(payload.png)),
          assetId = uid('image');
        if (payload.provenance !== undefined) {
          validateAssetProvenance(payload.provenance);
          candidate.schemaVersion = Math.max(candidate.schemaVersion, SVG_ASSET_SCHEMA_VERSION);
        }
        if (Object.hasOwn(candidate.imageAssets || {}, assetId))
          throw Error('Image identity collision');
        candidate.imageAssets = {
          ...candidate.imageAssets,
          [assetId]: {
            name: payload.name,
            mime: 'image/png',
            ...size,
            data: payload.png,
            ...(payload.provenance !== undefined ? { provenance: clone(payload.provenance) } : {}),
          },
        };
        validateImageAssets(candidate.imageAssets);
        const artwork = target || layer(payload.name),
          generator = node(
            'Image',
            payload.nodePosition?.[0] ?? 40,
            payload.nodePosition?.[1] ?? 40,
          ),
          scale = Math.min(1, candidate.width / size.width, candidate.height / size.height);
        generator.asset = assetId;
        generator.params.size.value = [size.width * scale, size.height * scale];
        generator.params.position.value = payload.at
          ? payload.at.map(
              (value, axis) =>
                value - (payload.anchor === 'center' ? generator.params.size.value[axis] / 2 : 0),
            )
          : [
              (candidate.width - generator.params.size.value[0]) / 2,
              (candidate.height - generator.params.size.value[1]) / 2,
            ];
        if (target) target.nodes.push(generator);
        else {
          const output = node('Image Output', 380, 40);
          output.params.path.source = generator.id;
          artwork.nodes = [generator, output];
          artwork.output = output.id;
          candidate.layers.push(artwork);
        }
        serialize(candidate);
        result = { layerId: artwork.id, nodeId: generator.id, assetId };
      }
      validate(candidate);
      return { candidate, result };
    }
    if (
      [
        'set-layer-state',
        'clear-layer-solo',
        'set-document-settings',
        'set-guide-setting',
        'create-guide',
        'set-guide',
        'delete-guide',
        'set-layout-grid',
      ].includes(type)
    ) {
      const candidate = cloneDocument(before);
      let result = {};
      if (type === 'clear-layer-solo') {
        for (const layer of candidate.layers) if (layer.solo === true) layer.solo = false;
      } else if (type === 'set-layer-state') {
        const layer = candidate.layers.find((layer) => layer.id === payload.layerId);
        if (!layer) throw Error('Missing composition');
        if (
          !['visible', 'locked', 'guide', 'solo'].includes(payload.key) ||
          typeof payload.value !== 'boolean'
        )
          throw Error('Invalid layer state');
        if (layer.locked && payload.key === 'guide') throw Error('Composition is locked');
        layer[payload.key] = payload.value;
        if (payload.key === 'solo') candidate.schemaVersion = Math.max(candidate.schemaVersion, 33);
      } else if (type === 'set-document-settings') {
        if (!['width', 'height'].includes(payload.key)) throw Error('Invalid document setting');
        candidate[payload.key] = payload.value;
      } else {
        const aids = (candidate.editor ||= guideDefaults());
        if (type === 'set-guide-setting') {
          if (!Object.hasOwn(guideDefaults(), payload.key) || payload.key === 'guides')
            throw Error('Invalid guide setting');
          aids[payload.key] = clone(payload.value);
          if (payload.key === 'gridAnchor' && payload.value === 'center')
            candidate.schemaVersion = Math.max(candidate.schemaVersion, 27);
          else if (payload.key === 'gridAnchor' && payload.value !== 'top-left')
            candidate.schemaVersion = Math.max(candidate.schemaVersion, GRID_ORIGIN_SCHEMA_VERSION);
          if (payload.key === 'gridAngle' && payload.value !== 0)
            candidate.schemaVersion = Math.max(candidate.schemaVersion, GRID_ANGLE_SCHEMA_VERSION);
        } else if (type === 'set-layout-grid')
          setLayoutGrid(candidate, clone(payload.config), () => uid('guide'));
        else {
          if (aids.locked) throw Error('Guides are locked');
          if (type === 'create-guide') {
            const guide = {
              id: uid('guide'),
              axis: payload.axis,
              position: candidate[payload.axis === 'x' ? 'width' : 'height'] / 2,
              locked: false,
            };
            aids.guides.push(guide);
            result = { guideId: guide.id };
          } else {
            const guide = aids.guides.find((g) => g.id === payload.guideId);
            if (!guide) throw Error('Missing guide');
            if (type === 'delete-guide') {
              if (guide.locked) throw Error('Guide is locked');
              aids.guides = aids.guides.filter((g) => g !== guide);
            } else {
              if (!['locked', 'position'].includes(payload.key)) throw Error('Invalid guide field');
              if (payload.key === 'position' && guide.locked) throw Error('Guide is locked');
              guide[payload.key] = clone(payload.value);
            }
          }
        }
      }
      validate(candidate);
      return { candidate, result };
    }
    if (
      ![
        'connect',
        'disconnect',
        'set-operation',
        'set-parameter',
        'set-parameter-base',
        'set-parameter-expression',
        'set-parameter-lock',
        'set-text-content',
        'commit-text-draft',
        'set-text-settings',
        'set-variable-font',
        'set-character-arrangement',
        'clear-character-arrangement',
        'set-character-style',
        'set-character-offsets',
        'set-character-transforms',
        'set-character-kerning',
        'reset-character-adjustments',
        'adopt-font',
        'rename',
        'create-node',
        'duplicate-nodes',
        'paste-nodes',
        'save-node-preset',
        'update-node-preset',
        'apply-node-preset',
        'delete-node',
        'delete-nodes',
        'preview-node',
        'set-node-bypassed',
        'set-reference',
        'set-composition-reference',
        'set-node-settings',
        'insert-path-point',
        'create-point',
        'delete-point',
        'set-point-mode',
        'edit-remap',
      ].includes(type)
    )
      throw Error('Unsupported command');
    let result = {};
    const committingDraft =
      type === 'commit-text-draft' ? this.#textDrafts.get(payload.draftId) : null;
    if (type === 'commit-text-draft' && !committingDraft)
      throw Error('STALE_TEXT_DRAFT: unknown draft');
    const candidate = cloneDocument(before);
    const target = candidate.layers.find(
      (layer) => layer.id === (committingDraft?.layerId ?? payload.layerId),
    );
    if (!target) throw Error('Missing composition');
    if (target.locked) throw Error('Composition is locked');
    const transformNode = target.nodes.find((n) => n.id === payload.nodeId);
    if (
      transformNode?.type === 'Mask' &&
      ['feather', 'luminance'].includes(payload.key) &&
      !Object.hasOwn(transformNode.params, payload.key)
    ) {
      const virtual = payload.key === 'feather' ? p('Scalar', 0) : p('Boolean', false);
      if (
        (['set-parameter', 'set-parameter-base'].includes(type) &&
          payload.value === virtual.value) ||
        (type === 'set-parameter-lock' && payload.locked === false) ||
        (type === 'set-parameter-expression' && payload.expression === '') ||
        type === 'disconnect'
      )
        return { candidate, result: {} };
      transformNode.params[payload.key] = virtual;
    }
    if (transformNode?.characterArrangement) {
      const check = evaluate(
        { ...target, output: transformNode.id },
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      if (!check.error && check.textLayout(transformNode.id)?.arrangementStatus?.active) {
        const axis = transformNode.characterArrangement.axis === 'x' ? 0 : 1;
        const blocked =
          (type === 'set-character-offsets' &&
            Object.entries(payload.changes || {}).some(
              ([id, v]) =>
                v?.[axis] !== (transformNode.characterPositions?.offsets?.[id]?.[axis] || 0),
            )) ||
          (type === 'reset-character-adjustments' && payload.mode === 'offsets') ||
          type === 'set-character-kerning' ||
          (type === 'set-character-style' &&
            Object.hasOwn(payload.patch || {}, 'tracking') &&
            payload.patch.tracking !== null) ||
          (type === 'set-text-settings' && Object.hasOwn(payload.settings || {}, 'kerning')) ||
          (['set-parameter', 'set-parameter-base'].includes(type) &&
            payload.key === 'letterSpacing');
        if (blocked) throw Error('Maintained ink gaps control spacing; disable to edit');
      }
    }

    if (
      isTransform(transformNode) &&
      Object.hasOwn(transformExtensions, payload.key) &&
      !transformNode.params[payload.key]
    ) {
      const virtual = transformParameter(transformNode, payload.key);
      if (['position', 'scaleWidth'].includes(payload.key))
        virtual.value = clone(
          evaluate(
            target,
            resolveFonts(candidate, this.#fonts()),
            candidate.imageAssets,
            candidate,
          ).parameterValue(transformNode.id, payload.key),
        );
      if (
        (type === 'set-parameter-lock' && payload.locked === virtual.locked) ||
        (type === 'set-parameter-expression' && payload.expression === virtual.expression) ||
        (type === 'disconnect' && !virtual.source)
      )
        return { candidate, result: {} };
      transformNode.params[payload.key] = clone(virtual);
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 11);
    }
    if (type === 'save-node-preset' || type === 'update-node-preset') {
      const presets = candidate.nodePresets ?? [];
      const index = presets.findIndex((p) => p.id === payload.presetId);
      if (type === 'save-node-preset' && index !== -1) throw Error('Preset ID already exists');
      if (type === 'update-node-preset' && index === -1) throw Error('Missing preset');
      const id =
        type === 'update-node-preset' ? payload.presetId : (payload.presetId ?? uid('preset'));
      const saved = captureNodePreset(
        candidate,
        target,
        payload.nodeIds,
        payload.name ?? (type === 'update-node-preset' ? presets[index].name : undefined),
        id,
      );
      if (presets.some((p, i) => i !== index && p.name === saved.name))
        throw Error('Preset name already exists');
      if (index === -1) presets.push(saved);
      else presets[index] = saved;
      candidate.nodePresets = presets;
      candidate.schemaVersion = Math.max(candidate.schemaVersion, NODE_PRESET_SCHEMA_VERSION);
      result = { presetId: id };
    } else if (type === 'apply-node-preset') {
      const preset = candidate.nodePresets?.find((p) => p.id === payload.presetId);
      if (!preset) throw Error('Missing preset');
      validateNodePreset(candidate, preset);
      assertNoExternalPointReferenceRebind(preset.nodes, target.nodes);
      const offset = payload.offset ?? [30, 30];
      if (!Array.isArray(offset) || offset.length !== 2 || !offset.every(Number.isFinite))
        throw Error('Invalid preset offset');
      if (target.nodes.length + preset.nodes.length > 2000) throw Error('Too many nodes');
      const copies = duplicateNodes(
        { nodes: preset.nodes },
        preset.nodes.map((n) => n.id),
        { append: false },
      );
      copies.forEach((n, i) => {
        n.position = preset.nodes[i].position.map((v, axis) => v + offset[axis]);
      });
      target.nodes.push(...copies);
      result = { presetId: preset.id, nodeIds: copies.map((n) => n.id) };
    } else if (type === 'rename') {
      const item =
        payload.nodeId === undefined
          ? target
          : target.nodes.find((node) => node.id === payload.nodeId);
      if (!item) throw Error('Missing name target');
      if (payload.expectedName !== undefined && item.name !== payload.expectedName)
        throw Error('Name target changed');
      if (typeof payload.name !== 'string' || !payload.name.trim())
        throw Error('Name cannot be empty');
      item.name = payload.name.trim();
    } else if (type === 'create-node') {
      const created = node(payload.nodeType, ...(payload.position ?? [40, 40]));
      if (isImageDither(created.type))
        candidate.schemaVersion = Math.max(candidate.schemaVersion, IMAGE_DITHER_SCHEMA_VERSION);
      if (['Keyer', 'Mask'].includes(created.type))
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 35);
      if (created.type === 'Composition Reference')
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 31);
      if (created.type === 'Join Geometry')
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 23);
      if (created.type === 'Multi Path')
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 22);
      target.nodes.push(created);
      if (created.type === 'Vector Transform')
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 11);
      result = { nodeId: created.id };
    } else if (type === 'paste-nodes') {
      const clipboard = this.#nodeClipboard;
      if (!clipboard)
        throw Error(
          'Node clipboard is empty. Copy in this document’s graph; OS/other-document paste is not supported',
        );
      if (
        clipboard.colorArtworkReferences &&
        (clipboard.colorContextEpoch !== this.#projectContextOwner.epoch ||
          clipboard.colorContextIdentity !==
            (projectDocumentContexts.get(before)?.clipboardIdentity ?? null))
      )
        clipboard.artworkReferencesExpired = true;
      if (
        (candidate.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION ||
          clipboard.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION) &&
        nodesHaveComponentCompositionReferenceSyntax(clipboard.nodes)
      )
        throw Error('Pasting nodes with Component Composition references is not supported');
      if (
        (candidate.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION ||
          clipboard.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) &&
        nodesHaveQualifiedColorReferenceSyntax(clipboard.nodes) &&
        !clipboard.colorArtworkReferences
      )
        throw Error('Pasting nodes with qualified Color references is not supported');
      if (
        clipboard.colorArtworkReferences &&
        candidate.schemaVersion < QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION
      )
        throw Error(
          'Pasting Color Artwork references requires schemaVersion 64; copy again in this Document',
        );
      if (
        (candidate.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION ||
          clipboard.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION) &&
        nodesHaveCompositionPropertyReferenceSyntax(clipboard.nodes)
      )
        throw Error('Pasting nodes with Composition property references is not supported');
      if (clipboard.artworkReferences.length) {
        if (clipboard.sourceLayerId !== target.id)
          throw Error('Paste nodes with Artwork references in their original Artwork');
        if (
          clipboard.artworkReferencesExpired ||
          !artworkClipboardTargetsIntact(candidate, clipboard)
        )
          throw Error('Copied Artwork source is missing or changed; restore it and copy again');
      }
      if (target.nodes.length + clipboard.nodes.length > 2000) throw Error('Too many nodes');
      if (
        clipboard.externalNodeIds.length &&
        (clipboard.sourceLayerId !== target.id ||
          clipboard.externalNodeIds.some((id) => !target.nodes.some((n) => n.id === id)))
      )
        throw Error(
          `Paste needs ${clipboard.externalNodeIds.length} unselected input/reference node(s) in the original Composition. Copy those sources too`,
        );
      assertNoExternalPointReferenceRebind(
        clipboard.nodes,
        target.nodes,
        clipboard.sourceLayerId === target.id ? new Set(clipboard.externalNodeIds) : new Set(),
      );
      for (const [id, asset] of Object.entries(clipboard.fonts))
        if (!asset || candidate.fontAssets?.[id]?.data !== asset.data)
          throw Error('Copied font is no longer present unchanged in this Document; copy again');
      for (const [id, asset] of Object.entries(clipboard.images))
        if (
          !asset ||
          candidate.imageAssets?.[id]?.data !== asset.data ||
          candidate.imageAssets[id].width !== asset.width ||
          candidate.imageAssets[id].height !== asset.height
        )
          throw Error('Copied image is no longer present unchanged in this Document; copy again');
      for (const n of clipboard.nodes)
        if (
          n.compositionReference &&
          !candidate.layers.some((l) => l.id === n.compositionReference.compositionId)
        )
          throw Error('Copied Composition reference is missing; restore its source or copy again');
      // Use exactly the existing duplication identity contracts. Clipboard nodes
      // are a frozen-in-time authored snapshot, not IDs read again at Paste.
      const source = { nodes: clone(clipboard.nodes) },
        copies = duplicateNodes(
          source,
          source.nodes.map((n) => n.id),
          { append: false },
        ),
        offset = payload.offset ?? [
          30 * (clipboard.pasteCount + 1),
          30 * (clipboard.pasteCount + 1),
        ];
      if (!Array.isArray(offset) || offset.length !== 2 || !offset.every(Number.isFinite))
        throw Error('Invalid paste offset');
      copies.forEach((copy, i) => {
        copy.position = clipboard.nodes[i].position.map((v, axis) => v + offset[axis]);
      });
      target.nodes.push(...copies);
      // For schema 64/65 source data, selected references determine whether a
      // destination already at schema 60 needs 61 or 62. Older clipboard sources
      // and pre-60 destinations keep their prior conservative floor.
      let transferredSchemaVersion = clipboard.schemaVersion;
      if (clipboard.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION) {
        transferredSchemaVersion =
          before.schemaVersion >= CURVE_POINT_REFERENCE_SCHEMA_VERSION
            ? clipboard.referenceSchemaVersion
            : ARTWORK_REFERENCE_SCHEMA_VERSION;
        const binding = projectDocumentContexts.get(before);
        if (transferredSchemaVersion >= CURVE_HANDLE_REFERENCE_SCHEMA_VERSION) {
          assertNoLegacyHandleDocumentPromotion(before);
          if (binding) assertNoLegacyHandleReferencePromotion(binding.project);
        }
        if (transferredSchemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION) {
          assertNoLegacyArtworkDocumentPromotion(before);
          if (binding) assertNoLegacyArtworkReferencePromotion(binding.project);
        }
      }
      if (transferredSchemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION)
        assertNoLegacyQualifiedColorDocumentPromotion(candidate);
      candidate.schemaVersion = Math.max(candidate.schemaVersion, transferredSchemaVersion);
      result = {
        nodeIds: copies.map((n) => n.id),
        externalInputCount: clipboard.externalNodeIds.length + clipboard.artworkInputCount,
      };
    } else if (type === 'duplicate-nodes') {
      if (
        !Array.isArray(payload.nodeIds) ||
        !payload.nodeIds.length ||
        new Set(payload.nodeIds).size !== payload.nodeIds.length ||
        payload.nodeIds.some((id) => !target.nodes.some((node) => node.id === id))
      )
        throw Error('Select distinct nodes in this composition');
      if (payload.offset !== undefined) {
        if (payload.position !== undefined) throw Error('Choose position or offset');
        if (
          !Array.isArray(payload.offset) ||
          payload.offset.length !== 2 ||
          !payload.offset.every(Number.isFinite)
        )
          throw Error('Invalid duplicate offset');
      }
      const originals = target.nodes.filter((n) => payload.nodeIds.includes(n.id));
      if (
        candidate.schemaVersion >= COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION &&
        nodesHaveComponentCompositionReferenceSyntax(originals)
      )
        throw Error('Duplicating nodes with Component Composition references is not supported');
      if (
        candidate.schemaVersion >= QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION &&
        nodesHaveQualifiedColorReferenceSyntax(originals)
      )
        throw Error('Duplicating nodes with qualified Color references is not supported');
      if (
        candidate.schemaVersion >= COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION &&
        nodesHaveCompositionPropertyReferenceSyntax(originals)
      )
        throw Error('Duplicating nodes with Composition property references is not supported');
      const copies = duplicateNodes(target, payload.nodeIds);
      if (payload.offset !== undefined)
        copies.forEach((copy, index) => {
          copy.position = originals[index].position.map((v, axis) => v + payload.offset[axis]);
        });
      if (payload.position !== undefined) {
        if (copies.length !== 1) throw Error('Explicit position requires one node');
        copies[0].position = clone(payload.position);
      }
      result = { nodeIds: copies.map((node) => node.id) };
    } else if (type === 'delete-node' || type === 'delete-nodes') {
      const ids = type === 'delete-node' ? [payload.nodeId] : payload.nodeIds;
      if (!Array.isArray(ids) || !ids.length || new Set(ids).size !== ids.length)
        throw Error('Select unique nodes to delete');
      const selected = new Set(ids);
      if (ids.some((id) => !target.nodes.some((node) => node.id === id)))
        throw Error('Missing node');
      const protectedIds = target.nodes
        .filter(
          (n) => n.locked || n.id === target.output || ['Output', 'Image Output'].includes(n.type),
        )
        .map((n) => n.id);
      if (type === 'delete-node' && protectedIds.some((id) => selected.has(id)))
        throw Error('Cannot delete a locked or Output node');
      result = {
        deletedNodeIds: ids.filter((id) => !protectedIds.includes(id)),
        retainedNodeIds: ids.filter((id) => protectedIds.includes(id)),
      };
      for (const id of protectedIds) selected.delete(id);
      target.nodes = target.nodes.filter((node) => !selected.has(node.id));
      removeDeletedGroupMembers(target, selected);
      for (const node of target.nodes)
        for (const [, q] of parameters(node))
          if (selected.has(q.source)) {
            delete q.source;
            delete q.sourcePort;
          }
      for (const node of target.nodes) {
        normalizeJoinInputs(node, true);
        normalizeBooleanInputs(node, true);
      }
    } else if (type === 'preview-node') {
      const source = target.nodes.find((node) => node.id === payload.nodeId);
      if (!source) throw Error('Missing node');
      const output = target.nodes.find((node) => node.id === target.output);
      if (output?.locked) throw Error('Output node is locked');
      if (source.type === 'Mask' && output?.type === 'Image Output') {
        if (output.params.path.locked) throw Error('Output input is locked');
        if (output.params.path.expression) throw Error('Output input is driven by an expression');
        output.type = 'Output';
        if (output.name === 'Image Output') output.name = 'Output';
        output.params.path.type = 'PaintedPath';
      }
      if (outputType(source) === 'Path') {
        const paint = node('Fill', ...(payload.position ?? [40, 40]));
        target.nodes.push(paint);
        connect(target, source.id, paint.id, 'path');
        connect(target, paint.id, target.output, 'path');
      } else connect(target, source.id, target.output, 'path');
    } else if (type === 'insert-path-point') {
      const item = target.nodes.find((n) => n.id === payload.nodeId);
      if (!item || !['Curve', 'Multi Path'].includes(item.type))
        throw Error('Generated path: select the original Curve or Multi Path to insert a point');
      if (item.locked || item.bypassed) throw Error('Path is locked or bypassed');
      if (target.visible === false) throw Error('Show the composition before inserting a point');
      if (item.points.length >= 10000) throw Error('Too many vertices (10000 per node)');
      // Retained Remap deltas can change meaning or suspend after topology changes.
      // This initial slice refuses affected retained edits; unrelated branches
      // remain editable. Never silently update a fingerprint or rebind an edit.
      if (insertionRemapDependents(target, item.id).length)
        throw Error(
          'Point insertion is unavailable because this source feeds retained Path Remap edits: topology changes can suspend or alter them. No edits were rebound.',
        );
      const closedParameter = item.type === 'Curve' ? item.params.closed : null;
      const blocked = (q) => q.locked || q.source || q.expression;
      if (closedParameter && blocked(closedParameter))
        throw Error('Path closure is locked or driven');
      const contour =
        item.type === 'Curve'
          ? { id: item.id, points: item.points.map((p) => p.id), closed: closedParameter.value }
          : item.contours.find((c) => c.id === payload.contourId);
      if (!contour || contour.id !== payload.contourId) throw Error('Missing owned contour');
      const index = contour.points.indexOf(payload.startPointId),
        next = (index + 1) % contour.points.length;
      if (
        index < 0 ||
        (!contour.closed && next === 0) ||
        contour.points[next] !== payload.endPointId ||
        contour.points.length < 2
      )
        throw Error('Select adjacent points in contour order');
      const a = item.points.find((p) => p.id === payload.startPointId),
        b = item.points.find((p) => p.id === payload.endPointId);
      if ([a.position, a.handleB, b.position, b.handleA].some(blocked))
        throw Error('Segment anchors or handles are locked or driven');
      const evaluated = evaluate(
        { ...target, output: item.id },
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      if (evaluated.error) throw Error(evaluated.error);
      const split = splitPathSegment(
        { position: a.position.value, handleB: a.handleB.value },
        { position: b.position.value, handleA: b.handleA.value },
        payload.t,
      );
      const created = point(...split.position);
      created.handleA.value = split.handleA;
      created.handleB.value = split.handleB;
      created.handleType =
        Math.hypot(...split.handleA) && Math.hypot(...split.handleB) ? 'smooth' : 'corner';
      a.handleB.value = split.startHandle;
      b.handleA.value = split.endHandle;
      for (const pt of [a, b])
        if (
          pt.handleType === 'symmetric' &&
          pt.handleA.value.some((v, axis) => v !== -pt.handleB.value[axis])
        )
          pt.handleType = 'smooth';
      if (item.type === 'Curve') item.points.splice(index + 1, 0, created);
      else {
        item.points.push(created);
        contour.points.splice(index + 1, 0, created.id);
      }
      result = { pointId: created.id, contourId: contour.id };
    } else if (['create-point', 'delete-point', 'set-point-mode'].includes(type)) {
      const item = target.nodes.find(
        (node) => node.id === payload.nodeId && ['Curve', 'Multi Path'].includes(node.type),
      );
      if (!item) throw Error('Select an editable path node');
      if (item.type === 'Multi Path' && type === 'create-point')
        throw Error('Adding points to a Multi Path is not supported; use Subdivide Path');
      if (type === 'create-point') {
        const created = point(...(payload.position ?? [400, 300]));
        item.points.push(created);
        result = { pointId: created.id };
      } else {
        if (type === 'delete-point') {
          // Resolve the entire selection before mutating the candidate. The row
          // action and Canvas shortcut share one atomic, undoable command.
          const ids = [...new Set(payload.pointIds ?? [payload.pointId])];
          if (!ids.length || ids.some((id) => !item.points.some((pt) => pt.id === id)))
            throw Error('Missing point');
          const points = item.points.filter((pt) => ids.includes(pt.id));
          if (points.some((pt) => ['position', 'handleA', 'handleB'].some((key) => pt[key].locked)))
            throw Error('Point is locked; unlock before deleting');
          item.points = item.points.filter((pt) => !ids.includes(pt.id));
          if (item.type === 'Multi Path')
            for (const contour of item.contours)
              contour.points = contour.points.filter((id) => !ids.includes(id));
        } else {
          const pt = item.points.find((pt) => pt.id === payload.pointId);
          if (!pt) throw Error('Missing point');
          if (!['corner', 'smooth', 'symmetric'].includes(payload.mode))
            throw Error('Invalid handle type');
          if (
            payload.mode !== 'corner' &&
            (pt.handleA.locked || pt.handleA.source || pt.handleA.expression)
          )
            throw Error('Handle A is locked or driven; unlock before changing coupled handle mode');
          pt.handleType = payload.mode;
          if (payload.mode === 'symmetric') pt.handleA.value = pt.handleB.value.map((v) => -v);
          else if (payload.mode === 'smooth') {
            const length = Math.hypot(...pt.handleA.value),
              mag = Math.hypot(...pt.handleB.value);
            if (mag) pt.handleA.value = pt.handleB.value.map((v) => (-v / mag) * length);
          }
        }
      }
    } else if (type === 'edit-remap') {
      const item = target.nodes.find(
        (node) => node.id === payload.nodeId && node.type === 'Path Remap',
      );
      if (!item) throw Error('Select a Path Remap node');
      if (payload.action === 'clear') item.edits = [];
      else {
        const input = evaluate(
          target,
          resolveFonts(candidate, this.#fonts()),
          candidate.imageAssets,
          candidate,
        ).run(item.params.path.source);
        if (payload.action === 'translate')
          translateRemapPoints(item, input, payload.pointIds, payload.delta);
        else if (payload.action === 'enable') enableRemapPoint(item, input, payload.pointId);
        else if (payload.action === 'rebind') rebindRemapPoint(item, input, payload.pointId);
        else if (payload.action === 'set') {
          if (
            !['position', 'handleA', 'handleB'].includes(payload.component) ||
            ![0, 1].includes(payload.axis)
          )
            throw Error('Invalid remap field');
          assertValue('Scalar', payload.value);
          if (!input.contours.some((c) => c.points.some((p) => p.id === payload.pointId)))
            throw Error('Missing remap target');
          let edit = item.edits.find((e) => e.pointId === payload.pointId);
          if (edit?.params) throw Error('Use the parameter command for enabled remap values');
          if (!edit) {
            edit = {
              pointId: payload.pointId,
              ...(input.fingerprint ? { fingerprint: input.fingerprint } : {}),
            };
            item.edits.push(edit);
          }
          edit[payload.component] ||= [0, 0];
          edit[payload.component][payload.axis] = payload.value;
        } else throw Error('Invalid remap action');
      }
    } else if (type === 'set-node-bypassed') {
      setNodeBypassed(target, payload.nodeId, payload.bypassed);
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 10);
    } else if (type === 'set-composition-reference') {
      const item = target.nodes.find((n) => n.id === payload.nodeId);
      if (item?.type !== 'Composition Reference') throw Error('Select a Composition Reference');
      if (
        payload.target !== null &&
        !candidate.layers.some((l) => l.id === payload.target?.compositionId)
      )
        throw Error('Missing source Composition');
      item.compositionReference = clone(payload.target);
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 31);
      validateCompositionReference(item, candidate.schemaVersion);
      assertCompositionAcyclic(candidate);
    } else if (type === 'set-reference')
      setReference(target, payload.nodeId, clone(payload.target));
    else if (type === 'set-node-settings') {
      const item = target.nodes.find((node) => node.id === payload.nodeId),
        settings = payload.settings;
      if (!item) throw Error('Missing node');
      if (
        !settings ||
        typeof settings !== 'object' ||
        Array.isArray(settings) ||
        Object.keys(settings).some(
          (key) =>
            ![
              'collapsed',
              'canvasPositionHandle',
              'positionReference',
              'strokeCap',
              'strokeJoin',
              'strokeDash',
            ].includes(key),
        )
      )
        throw Error('Invalid node settings');
      if (Object.keys(settings).some((key) => key.startsWith('stroke')) && item.type !== 'Stroke')
        throw Error('Stroke settings require a Stroke node');
      for (const [key, value] of Object.entries(settings)) item[key] = clone(value);
    } else if (type === 'connect')
      connect(target, payload.sourceId, payload.nodeId, payload.key, false, payload.sourcePort);
    else if (type === 'disconnect') {
      const node = target.nodes.find((node) => node.id === payload.nodeId);
      if (!node) throw Error('Missing node');
      disconnect(node, payload.key);
    } else if (type === 'set-operation') {
      setOperation(target, payload.nodeId, payload.operation);
      if (
        target.nodes.find((node) => node.id === payload.nodeId)?.type === 'Geometry Repeater' &&
        payload.operation === 'ae-2d'
      )
        candidate.schemaVersion = Math.max(candidate.schemaVersion, REPEATER_SCHEMA_VERSION);
      if (
        target.nodes.find((node) => node.id === payload.nodeId)?.type === 'Resample Path' &&
        payload.operation === 'spacing'
      )
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 12);
    } else if (['set-character-arrangement', 'clear-character-arrangement'].includes(type)) {
      const node = target.nodes.find((n) => n.id === payload.nodeId && n.type === 'Text');
      if (!node) throw Error('Select a Text node');
      if (node.locked) throw Error('Text node is locked');
      if (type === 'clear-character-arrangement') delete node.characterArrangement;
      else {
        if (
          Object.keys(payload).some(
            (key) => !['layerId', 'nodeId', 'mode', 'gap', 'updateGap'].includes(key),
          ) ||
          (payload.updateGap !== undefined && payload.updateGap !== true) ||
          (payload.updateGap === true && payload.mode !== undefined)
        )
          throw Error('Arrangement commands derive membership and anchor from current text');
        if (!Number.isFinite(payload.gap) || payload.gap < 0 || payload.gap > 100000)
          throw Error('Ink gap must be between 0 and 100000 px');
        const evaluateCurrent = () => {
          const result = evaluate(
            { ...target, output: node.id },
            resolveFonts(candidate, this.#fonts()),
            candidate.imageAssets,
            candidate,
          );
          if (result.error) throw Error(result.error);
          return { result, layout: result.textLayout(node.id) };
        };
        if (payload.updateGap === true) {
          if (!node.characterArrangement) throw Error('Apply maintained ink gaps first');
          const { layout } = evaluateCurrent();
          if (!layout.arrangementStatus?.active)
            throw Error(layout.arrangementStatus?.reason || 'Maintained ink gaps unavailable');
          node.characterArrangement.gap = payload.gap;
        } else {
          if (payload.mode !== 'equal-ink-gap')
            throw Error('Unsupported character arrangement mode');
          // Capture the currently displayed edge, including a previous valid arrangement.
          const { result, layout } = evaluateCurrent();
          if (!layout.arrangementEligibility?.eligible)
            throw Error(layout.arrangementEligibility?.reason || 'Character mapping unavailable');
          if (!node.characterPositions)
            node.characterPositions = createCharacterPositions(node.text);
          const axis = node.writingMode === 'vertical-rl' ? 'y' : 'x',
            index = axis === 'x' ? 0 : 1;
          node.characterArrangement = {
            version: 1,
            mode: 'equal-ink-gap',
            axis,
            ids: node.characterPositions.units.map((u) => u.id),
            gap: payload.gap,
            anchorOffset:
              layout.characters[0].bounds[index] -
              result.parameterValue(node.id, 'position')[index],
          };
        }
        validateCharacterArrangement(node.characterArrangement);
        const { layout } = evaluateCurrent();
        if (!layout.arrangementStatus?.active)
          throw Error(layout.arrangementStatus?.reason || 'Maintained ink gaps unavailable');
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 24);
      }
    } else if (type === 'set-variable-font') {
      const node = target.nodes.find((n) => n.id === payload.nodeId && n.type === 'Text');
      if (!node) throw Error('Select a Text node');
      if (node.locked) throw Error('Text node is locked');
      if (payload.asset) {
        const { id, asset } = payload.asset;
        if (candidate.fontAssets?.[id] && candidate.fontAssets[id].data !== asset.data)
          throw Error('VARIABLE_ASSET_CONFLICT');
        candidate.fontAssets = { ...candidate.fontAssets, [id]: asset };
      }
      node.font = payload.fontId;
      if (candidate.fontAssets?.[node.font]?.kind === VARIABLE_KIND) {
        candidate.schemaVersion = Math.max(candidate.schemaVersion, 25);
        const axes = resolveFonts(candidate)[node.font].nectVariable.axes;
        node.variation = {
          version: 1,
          fontId: node.font,
          axes: canonicalAxes(axes, payload.axes).sparse,
        };
      } else {
        if (payload.axes !== null) throw Error('VARIABLE_FONT_MISMATCH');
        delete node.variation;
      }
    } else if (type === 'set-text-content' || type === 'commit-text-draft') {
      const node = target.nodes.find(
        (node) => node.id === (committingDraft?.nodeId ?? payload.nodeId) && node.type === 'Text',
      );
      if (!node) throw Error('Select a Text node');
      if (node.variation && node.locked) throw Error('Text node is locked');
      let text, record;
      if (committingDraft) {
        if (payload.expectedDraftRevision !== committingDraft.version)
          throw Error('STALE_TEXT_DRAFT: input changed');
        committingDraft.assertCurrent(node, this.#history.revision, payload.expectedDraftRevision);
        text = committingDraft.state.changed ? committingDraft.raw : committingDraft.baseText;
        record = clone(committingDraft.record);
      } else {
        text = payload.text;
        normalizeCharacterText(text);
        record = reconcileCharacterEdit(node.characterPositions, {
          previousRaw: node.text,
          nextRaw: text,
          selectionStart: payload.selectionStart,
          selectionEnd: payload.selectionEnd,
          inputType: payload.inputType,
          forceReplacement: payload.forceReplacement,
        }).record;
      }
      node.text = text;
      if (record) node.characterPositions = record;
      else delete node.characterPositions;
    } else if (
      [
        'set-character-offsets',
        'set-character-transforms',
        'set-character-kerning',
        'reset-character-adjustments',
      ].includes(type)
    ) {
      const node = target.nodes.find((node) => node.id === payload.nodeId && node.type === 'Text');
      if (!node) throw Error('Select a Text node');
      let next;
      if (type === 'set-character-offsets')
        next = setCharacterOffsets(node.text, node.characterPositions, payload.changes);
      else if (type === 'set-character-kerning')
        next = setCharacterKerningPairs(node.text, node.characterPositions, payload.changes);
      else if (type === 'set-character-transforms') {
        if (
          !Array.isArray(payload.characterIds) ||
          !payload.characterIds.length ||
          new Set(payload.characterIds).size !== payload.characterIds.length ||
          !payload.patch ||
          typeof payload.patch !== 'object' ||
          Array.isArray(payload.patch) ||
          Object.keys(payload.patch).some((key) => !['rotation', 'scale'].includes(key))
        )
          throw Error('Invalid character transform selection or patch');
        next = setCharacterTransforms(
          node.text,
          node.characterPositions,
          Object.fromEntries(
            payload.characterIds.map((id) => [
              id,
              {
                rotation: 0,
                scale: 1,
                ...node.characterPositions?.transforms?.[id],
                ...payload.patch,
              },
            ]),
          ),
        );
      } else {
        if (!['all', 'offsets', 'transforms'].includes(payload.mode))
          throw Error('Invalid character reset mode');
        if (payload.mode === 'all') delete node.characterArrangement;
        next =
          payload.mode === 'all'
            ? resetCharacterAdjustments(node.text, node.characterPositions)
            : payload.mode === 'offsets'
              ? resetCharacterOffsets(node.text, node.characterPositions, payload.characterIds)
              : resetCharacterTransforms(node.text, node.characterPositions, payload.characterIds);
      }
      if (next) {
        node.characterPositions = next;
        candidate.schemaVersion = Math.max(
          candidate.schemaVersion,
          next.version >= 5
            ? 9
            : next.version >= 4
              ? 6
              : next.version >= 3
                ? 5
                : next.version >= 2
                  ? 4
                  : 3,
        );
      }
      if (type === 'set-character-kerning') {
        const check = evaluate(
          { ...target, output: node.id },
          resolveFonts(candidate, this.#fonts()),
          candidate.imageAssets,
          candidate,
        );
        if (check.error) throw Error(check.error);
        if (check.textLayout(node.id).reason) throw Error(check.textLayout(node.id).reason);
      }
    } else if (['set-text-settings', 'set-character-style', 'adopt-font'].includes(type)) {
      const node = target.nodes.find((node) => node.id === payload.nodeId && node.type === 'Text');
      if (!node) throw Error('Select a Text node');
      if (node.variation && node.locked) throw Error('Text node is locked');
      const baseFonts = this.#fonts();
      if (type === 'set-text-settings') {
        const settings = payload.settings;
        if (
          !settings ||
          typeof settings !== 'object' ||
          Array.isArray(settings) ||
          Object.keys(settings).some(
            (key) =>
              ![
                'font',
                'leading',
                'fontFeatures',
                'pathAttachment',
                'textWrap',
                'referenceBounds',
                'crossAlignment',
                'alignment',
                'kerning',
                'writingMode',
              ].includes(key),
          )
        )
          throw Error('Invalid Text settings');
        for (const [key, value] of Object.entries(settings)) {
          if (
            ['leading', 'pathAttachment', 'textWrap', 'fontFeatures'].includes(key) &&
            value === null
          )
            delete node[key];
          else node[key] = clone(value);
        }
        if (Object.hasOwn(settings, 'leading'))
          candidate.schemaVersion = Math.max(candidate.schemaVersion, 6);
        if (node.fontFeatures)
          candidate.schemaVersion = Math.max(candidate.schemaVersion, OPENTYPE_SCHEMA_VERSION);
        if (Object.hasOwn(settings, 'fontFeatures') || node.fontFeatures) {
          const preflight = evaluate(
            { ...target, output: node.id },
            resolveFonts(candidate, baseFonts),
            candidate.imageAssets,
            candidate,
          );
          if (preflight.error) throw Error(preflight.error);
        }
        if (node.pathAttachment) candidate.schemaVersion = Math.max(candidate.schemaVersion, 7);
        if (node.textWrap)
          candidate.schemaVersion = Math.max(
            candidate.schemaVersion,
            node.textWrap.policy ? 34 : 8,
          );
        if (Object.hasOwn(settings, 'pathAttachment'))
          setTextPathAttachment(
            target,
            node.id,
            settings.pathAttachment,
            resolveFonts(candidate, baseFonts),
            candidate.imageAssets,
            candidate,
          );
        if (Object.hasOwn(settings, 'textWrap'))
          setTextWrap(
            target,
            node.id,
            settings.textWrap,
            resolveFonts(candidate, baseFonts),
            candidate.imageAssets,
            candidate,
          );
      } else if (type === 'set-character-style') {
        updateTextTypography(
          node,
          payload.characterIds,
          payload.patch,
          candidate,
          target,
          baseFonts,
        );
      } else {
        if (!(payload.bytes instanceof ArrayBuffer) || payload.bytes.byteLength > MAX_FONT_BYTES)
          throw Error('Font bytes require an ArrayBuffer up to 20 MB');
        const prepared = makeFontAsset(
          payload.bytes.slice(0),
          payload.fileName,
          candidate.fontAssets,
        );
        candidate.fontAssets = { ...candidate.fontAssets, [prepared.id]: prepared.asset };
        if (payload.characterIds !== undefined)
          updateTextTypography(
            node,
            payload.characterIds,
            { font: prepared.id },
            candidate,
            target,
            baseFonts,
          );
        else node.font = prepared.id;
        const preflight = evaluate(
          { ...target, output: node.id },
          resolveFonts(candidate, baseFonts),
          candidate.imageAssets,
          candidate,
        );
        if (preflight.error) throw Error(preflight.error);
        serialize(candidate);
        result = { fontId: prepared.id, fontName: prepared.asset.name };
      }
    } else {
      const node = target.nodes.find((node) => node.id === payload.nodeId),
        q = node && parameter(node, payload.key);
      if (!q) throw Error('Missing parameter');
      if (type === 'set-parameter-lock') {
        if (typeof payload.locked !== 'boolean') throw Error('Invalid lock state');
        if (
          payload.locked &&
          node.type === 'Boolean' &&
          payload.key === booleanPendingInputKey &&
          !node.booleanInputOrder
        ) {
          node.booleanInputOrder = ['a', 'b', payload.key];
          node.params[payload.key] = q;
        }
        q.locked = payload.locked;
        normalizeJoinInputs(node, true);
        normalizeBooleanInputs(node, true);
      } else {
        if (q.locked) throw Error('Parameter is locked');
        if (type === 'set-parameter-expression') {
          if (
            !['Scalar', 'Vector2', 'Color'].includes(q.type) ||
            typeof payload.expression !== 'string'
          )
            throw Error('Expected a Scalar, Vector2 or Color expression');
          q.expression = payload.expression;
          if (q.expression)
            for (const ref of parameterExpressionUses(q)) {
              if (ref.componentComposition) {
                assertSafeComponentCompositionPromotion(before);
                candidate.schemaVersion = Math.max(
                  candidate.schemaVersion,
                  COMPONENT_COMPOSITION_REFERENCE_SCHEMA_VERSION,
                );
              } else if (ref.qualifier) {
                assertNoLegacyQualifiedColorDocumentPromotion(before);
                candidate.schemaVersion = Math.max(
                  candidate.schemaVersion,
                  QUALIFIED_COLOR_REFERENCE_SCHEMA_VERSION,
                );
              } else if (ref.compositionId)
                candidate.schemaVersion = Math.max(
                  candidate.schemaVersion,
                  COMPOSITION_PROPERTY_REFERENCE_SCHEMA_VERSION,
                );
              else if (ref.layerId)
                candidate.schemaVersion = Math.max(
                  candidate.schemaVersion,
                  ARTWORK_REFERENCE_SCHEMA_VERSION,
                );
              const sourceLayer = ref.layerId
                ? candidate.layers.find((item) => item.id === ref.layerId)
                : target;
              const owner = sourceLayer?.nodes.find((item) => item.id === ref.nodeId),
                pointRef = owner?.type === 'Curve' && curvePointReference(ref.key, ref.component);
              if (pointRef)
                candidate.schemaVersion = Math.max(candidate.schemaVersion, pointRef.schemaVersion);
            }
          if (q.type === 'Color' && q.expression)
            candidate.schemaVersion = Math.max(
              candidate.schemaVersion,
              colorExpressionSchemaVersion(q.expression),
            );
          if (usesBaseValue(q.expression))
            candidate.schemaVersion = Math.max(candidate.schemaVersion, 26);
          if (payload.expression) {
            assertAcyclic(target);
            if (candidate.schemaVersion >= ARTWORK_REFERENCE_SCHEMA_VERSION)
              assertDocumentReferenceAcyclic(candidate);
            const evaluation = evaluate(
              target,
              resolveFonts(candidate, this.#fonts()),
              candidate.imageAssets,
              candidate,
            );
            const resolve = (ref, type = 'Scalar') => evaluation.resolveReference(ref, type);
            const value = parameterExpression(
              q,
              resolve,
              parameterBaseValue(target, node, payload.key, q),
            );
            assertValue(q.type, value);
          }
        } else {
          if (type === 'set-parameter-base' && !['Scalar', 'Vector2', 'Color'].includes(q.type))
            throw Error('Authored base editing requires a Scalar, Vector2 or Color parameter');
          if (q.colorSlotId !== undefined)
            throw Error('Unbind the shared Color before editing its fallback');
          if (type !== 'set-parameter-base' && (q.source || q.expression))
            throw Error(
              'Parameter is driven; disconnect input or clear expression before editing its fallback',
            );
          if (
            isTransform(node) &&
            payload.key === 'uniformScale' &&
            payload.value === false &&
            !node.params.scaleWidth
          ) {
            const width = evaluate(
              target,
              resolveFonts(candidate, this.#fonts()),
              candidate.imageAssets,
              candidate,
            ).parameterValue(node.id, 'scaleWidth');
            node.params.scaleWidth = { ...clone(transformExtensions.scaleWidth), value: width };
          }
          if (payload.axis !== undefined) {
            if (q.type !== 'Vector2' || ![0, 1].includes(payload.axis))
              throw Error('Invalid parameter axis');
            assertValue('Scalar', payload.value);
            q.value[payload.axis] = payload.value;
          } else {
            assertValue(q.type, payload.value);
            q.value = clone(payload.value);
          }
        }
      }
    }
    if (
      candidate.layers.some((l) =>
        l.nodes.some((n) => n.paintMode || ['Composition Reference', 'SVG'].includes(n.type)),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 31);
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => n.type === 'Boolean' && n.booleanInputOrder),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, BOOLEAN_INPUT_SCHEMA_VERSION);
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => {
          const q = n.type === 'Geometry Repeater' && n.params.alternateScale;
          return q && (q.value !== 100 || q.expression || q.source);
        }),
      )
    )
      candidate.schemaVersion = Math.max(
        candidate.schemaVersion,
        REPEATER_ALTERNATE_SCHEMA_VERSION,
      );
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => n.type === 'Geometry Repeater' && hasRepeaterFillAuthorship(n)),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, REPEATER_FILL_SCHEMA_VERSION);
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => n.type === 'Geometry Repeater' && hasRepeaterStrokeAuthorship(n)),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, REPEATER_STROKE_SCHEMA_VERSION);
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => n.type === 'Mask' && Object.hasOwn(n.params, 'feather')),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, MASK_FEATHER_SCHEMA_VERSION);
    if (candidate.layers.some(hasImageMask))
      candidate.schemaVersion = Math.max(candidate.schemaVersion, IMAGE_MASK_SCHEMA_VERSION);
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => n.type === 'Mask' && Object.hasOwn(n.params, 'luminance')),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, MASK_LUMINANCE_SCHEMA_VERSION);
    validate(candidate);
    const maskSources = new Map(target.nodes.map((n) => [n.id, n]));
    const checkedMasks = target.nodes.filter(
      (n) =>
        n.type === 'Mask' &&
        (Object.hasOwn(n.params, 'feather') ||
          Object.hasOwn(n.params, 'luminance') ||
          isImageMask(n, maskSources)),
    );
    if (checkedMasks.length) {
      const evaluation = evaluate(
        target,
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      for (const mask of checkedMasks) {
        try {
          const feather = maskFeather(evaluation.parameterValue(mask.id, 'feather'));
          evaluation.parameterValue(mask.id, 'luminance');
          if (
            !mask.bypassed &&
            mask.params.path.source &&
            mask.params.mask.source &&
            (feather > 0 || evaluation.parameterValue(mask.id, 'mask')?.kind === 'image')
          )
            evaluation.run(mask.id);
        } catch (error) {
          if (!isMissingReferenceTarget(error)) throw error;
        }
      }
    }
    if (['save-node-preset', 'update-node-preset', 'apply-node-preset'].includes(type))
      serialize(candidate);
    if (type === 'set-parameter-base')
      evaluate(
        target,
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      ).parameterValue(payload.nodeId, payload.key);
    // Validate resolved numeric controls as well as their saved fallbacks. This
    // works for disconnected nodes and rejects fractional wired/expression edits
    // before History admits the candidate.
    const repeaters = target.nodes.filter(
      (n) => n.type === 'Geometry Repeater' && n.operation === 'ae-2d' && !n.bypassed,
    );
    if (repeaters.length) {
      const evaluation = evaluate(
        target,
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      );
      for (const repeater of repeaters) {
        assertRepeaterControls(
          (key) => evaluation.parameterValue(repeater.id, key),
          isMissingReferenceTarget,
        );
      }
    }
    if (deferVariable) return { candidate, result, committingDraft };
    if (isTransform(transformNode))
      assertTransformControls(
        evaluate(target, resolveFonts(candidate, this.#fonts()), candidate.imageAssets, candidate),
        transformNode,
      );
    assertClosedOffsetChange(
      target,
      before.layers.find((layer) => layer.id === target.id),
      resolveFonts(candidate, this.#fonts()),
      candidate.imageAssets,
      resolveFonts(before, this.#fonts()),
      candidate,
      before,
    );
    return { candidate, result, committingDraft };
  }
}
