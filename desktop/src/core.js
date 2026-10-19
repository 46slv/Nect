import { resolvePenEndpoint, penContourIds, insertContinuationPoint } from './pen-continuation.js';
import { appendSVGPart } from './svg-part.js';
import { svgRootAttributes } from './artboard-units.js';
import { OPENTYPE_SCHEMA_VERSION, validateTextFeatures } from './opentype-features.js';
import { imageAssetFor, keyedImage, keyerSettings, MAX_ACTIVE_KEYERS } from './image-keyer.js';
import { maskAppearance } from './appearance-mask.js';
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
import {
  createVariableDocumentPreparer as createVariableBridge,
  assertVariableDocumentReady,
  validateVariableText,
  VARIABLE_KIND,
} from './variable-font/document-bridge.js';
import { canonicalAxes, requireThat as variableCheck } from './variable-font/contract.js';
import { validateCharacterArrangement } from './character-arrangement.js';
import { validateTonePower } from './luminance-tone.js';
import {
  DITHER_SCHEMA_VERSION,
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
import { autoHandlesGeometry } from './auto-handles-geometry.js';
import { taperGeometry } from './taper-geometry.js';
import { bendGeometry } from './bend-geometry.js';
import { twistGeometry } from './twist-geometry.js';
import { waveGeometry } from './wave-geometry.js';
import { puckerBloatGeometry } from './pucker-bloat-geometry.js';
import { dashIntervals } from './dash-intervals.js';
import { simplifyGeometry } from './simplify-geometry.js';
import { expandStrokeGeometry } from './expand-stroke-geometry.js';
import { smoothGeometry } from './smooth-geometry.js';
import { roughenGeometry } from './roughen-geometry.js';
import { roundCorners } from './corner-round-geometry.js';
import { mirrorGeometry } from './mirror-geometry.js';
import {
  repeatGeometry,
  repeaterSettings,
  REPEATER_KEYS,
  REPEATER_SCHEMA_VERSION,
  assertRepeaterRoute,
} from './repeater.js';
import { reversePathGeometry } from './reverse-path-geometry.js';
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
import { distributeArtworkSelection } from './artwork-distribution.js';
import { distributeArtworkGaps } from './artwork-gap-distribution.js';
import { importSVGTree } from './svg-import.js';
import { prepareCompositionImport } from './project-composition-import.js';
import { validateGuides, guideDefaults, setLayoutGrid } from './guides.js';
import { createLinearGradientFill, renderLinearGradientFill } from './linear-gradient.js';
import { createRadialGradientFill, renderRadialGradientFill } from './radial-gradient.js';
export {
  guideDefaults,
  guideSettings,
  resolveGridOrigin,
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
  return copy;
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
export const definitions = {
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
  'Join Geometry': { a: p('Path', null), b: p('Path', null) },
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
  'Smooth Path': {
    path: p('Path', null),
    factor: p('Scalar', 0.5),
    iterations: p('Scalar', 3),
    keepEndpoints: p('Boolean', true),
  },
  'Dash Path': {
    path: p('Path', null),
    dashLength: p('Scalar', 12),
    gapLength: p('Scalar', 6),
    offset: p('Scalar', 0),
  },
  'Simplify Path': { path: p('Path', null), tolerance: p('Scalar', 1) },
  'Expand Stroke': {
    path: p('PaintedPath', null),
    startWidthMultiplier: p('Scalar', 1),
    endWidthMultiplier: p('Scalar', 1),
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
  Taper: { path: p('Path', null), amount: p('Scalar', 0) },
  'Bend Points': { path: p('Path', null), angle: p('Scalar', 0) },
  'Twist Points': { path: p('Path', null), angle: p('Scalar', 0) },
  'Auto Handles': { path: p('Path', null), strength: p('Scalar', 1) },
  'Reverse Path': { path: p('Path', null) },
  'Wave Points': {
    path: p('Path', null),
    amplitude: p('Scalar', 20),
    wavelength: p('Scalar', 150),
    phase: p('Scalar', 0),
    keepEndpoints: p('Boolean', false),
  },
  'Pucker & Bloat': {
    path: p('Path', null),
    amount: p('Scalar', 0),
    autoCenter: p('Boolean', true),
    center: p('Vector2', [0, 0]),
  },
  'Subdivide Path': { path: p('Path', null), segments: p('Scalar', 2) },
  Roughen: {
    path: p('Path', null),
    amount: p('Scalar', 3),
    seed: p('Scalar', 1),
    keepEndpoints: p('Boolean', true),
  },
  'Resample Path': {
    path: p('Path', null),
    count: p('Scalar', 32),
    spacing: p('Scalar', 24),
  },
  'Corner Round': { path: p('Path', null), radius: p('Scalar', 12) },
  Mirror: {
    path: p('Path', null),
    center: p('Vector2', [480, 270]),
    angle: p('Scalar', 0),
    keepOriginal: p('Boolean', false),
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
  Value: { value: p('Scalar', 100) },
  Math: { a: p('Scalar', 0), b: p('Scalar', 0), min: p('Scalar', 0), max: p('Scalar', 1) },
  'Vector Math': { a: p('Vector2', [0, 0]), b: p('Vector2', [0, 0]), scale: p('Scalar', 1) },
  'Separate Vector': { vector: p('Vector2', [0, 0]) },
  'Combine Vector': { x: p('Scalar', 0), y: p('Scalar', 0) },
  Toggle: { value: p('Boolean', true) },
  Color: { value: p('Color', { hex: '#36434e', alpha: 1 }) },
  Vector: { value: p('Vector2', [300, 250]) },
  Null: { position: p('Vector2', [300, 250]) },
  Reference: {},
  'Composition Reference': { position: p('Vector2', [0, 0]) },
  Expression: { value: p('Scalar', 100) },
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
    name: type === 'Join Geometry' ? 'Join Paths' : type,
    position: [x, y],
    width: 264,
    collapsed: false,
    params: clone(definitions[type]),
    ...(type === 'Join Geometry' ? { joinInputOrder: ['a', 'b'] } : {}),
    points: type === 'Curve' ? [point(250, 150), point(450, 150), point(350, 350)] : [],
    operation: isDither(type)
      ? 'square'
      : type === 'Resample Path'
        ? 'count'
        : type === 'Taper' || type === 'Bend Points'
          ? 'horizontal'
          : type === 'Line Screen'
            ? 'horizontal'
            : type === 'Wave Points'
              ? 'vertical'
              : type === 'Mirror'
                ? 'left-right'
                : type === 'Closed Path Offset'
                  ? 'miter'
                  : type === 'Geometry Repeater'
                    ? 'linear'
                    : ['Math', 'Vector Math'].includes(type)
                      ? 'add'
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
    ...(type === 'Composition Reference' ? { compositionReference: null } : {}),
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
  return n.type === 'Separate Vector'
    ? [
        { id: 'x', label: 'X', type: 'Scalar' },
        { id: 'y', label: 'Y', type: 'Scalar' },
      ]
    : n.type === 'Text'
      ? [
          { id: undefined, label: 'Path', type: 'Path' },
          { id: 'position', label: 'Position', type: 'Vector2' },
        ]
      : [{ id: undefined, label: n.type === 'Null' ? 'Position' : '', type: outputType(n) }];
}
export function outputType(n, port) {
  if (n.type === 'Composition Reference' || n.paintMode)
    return port === undefined ? 'PaintedPath' : undefined;
  if (n.type === 'Separate Vector') return ['x', 'y'].includes(port) ? 'Scalar' : undefined;
  if (n.type === 'Text' && port === 'position') return 'Vector2';
  if (port !== undefined) return undefined;
  if (n.type === 'Vector Math')
    return ['length', 'distance', 'dot'].includes(n.operation) ? 'Scalar' : 'Vector2';
  if (
    [
      'Fill',
      'Linear Gradient Fill',
      'Radial Gradient Fill',
      'Halftone',
      'Ordered Dither',
      'Error Diffusion Dither',
      'Line Screen',
      'Stroke',
      'Output',
    ].includes(n.type)
  )
    return 'PaintedPath';
  if (n.type === 'Color') return 'Color';
  if (n.type === 'Image' || n.type === 'Image Output' || n.type === 'Keyer') return 'Image';
  if (n.type === 'Mask') return 'PaintedPath';
  return ['Value', 'Expression', 'Math'].includes(n.type)
    ? 'Scalar'
    : ['Vector', 'Combine Vector', 'Null', 'Reference', 'Vector Transform'].includes(n.type)
      ? 'Vector2'
      : n.type === 'Toggle'
        ? 'Boolean'
        : 'Path';
}
// Deliberate identity contracts. Never guess the first socket of a new node type.
const bypassInputs = Object.freeze({
  Keyer: 'path',
  Mask: 'path',
  Boolean: 'a',
  'Join Geometry': 'a',
  Math: 'a',
  'Vector Math': 'a',
  Transform: 'path',
  Mirror: 'path',
  'Corner Round': 'path',
  'Resample Path': 'path',
  Roughen: 'path',
  'Smooth Path': 'path',
  'Reverse Path': 'path',
  'Trim Path': 'path',
  'Set Path Start': 'path',
  'Set Closure': 'path',
  'Subdivide Path': 'path',
  'Pucker & Bloat': 'path',
  Taper: 'path',
  'Bend Points': 'path',
  'Twist Points': 'path',
  'Auto Handles': 'path',
  'Wave Points': 'path',
  'Simplify Path': 'path',
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
function parameterDependencies(entries) {
  return entries.flatMap(([, q]) =>
    q.source
      ? [q]
      : q.expression
        ? expressionReferences(q.expression).map((ref) => ({
            source: ref.nodeId,
            parameterKey: ref.key,
            component: ref.component,
          }))
        : [],
  );
}
export function nodeDependencies(n, activeOnly = false) {
  const deps = parameterDependencies(activeOnly ? evaluatedParameters(n) : parameters(n));
  const reference = referenceDependency(n);
  if (reference) deps.push(reference);
  if (n.type === 'Text' && n.pathAttachment) deps.push({ source: n.pathAttachment.nodeId });
  return deps;
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
export function referenceTarget(layer, n) {
  if (n?.type !== 'Reference') throw Error('Select a Reference node');
  if (!n.reference) throw Error('Reference target is not selected');
  const target = layer.nodes.find((x) => x.id === n.reference.nodeId);
  if (!target) throw Error(`Reference target is missing: ${n.reference.nodeId}`);
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
    for (const dep of nodeDependencies(n)) {
      if (!byId.has(dep.source) && n.type !== 'Reference' && !dep.parameterKey)
        throw Error('Missing source node');
      visit(dep.source);
    }
    active.delete(id);
    seen.add(id);
  }
  for (const n of layer.nodes) visit(n.id);
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
  const remap = new Map(originals.map((n) => [n.id, uid('node')]));
  const copies = originals.map((original) => {
    const copy = clone(original);
    copy.id = remap.get(original.id);
    copy.name += ' copy';
    copy.position = copy.position.map((v) => v + 30);
    const pointIds = new Map(copy.points.map((pt) => [pt.id, uid('point')]));
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
      if (q.expression)
        q.expression = q.expression.replace(/\bref\s*\(\s*"([a-zA-Z0-9:_-]+)"/g, (match, id) =>
          remap.has(id) ? match.replace(`"${id}"`, `"${remap.get(id)}"`) : match,
        );
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
  const ids = new Set(preset.nodes.map((n) => n.id));
  for (const n of preset.nodes) {
    if (['Output', 'Image Output', 'Composition Reference'].includes(n.type))
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
    schemaVersion: preset.nodes.some(
      (n) => n.type === 'Boolean' && n.booleanInputOrder !== undefined,
    )
      ? BOOLEAN_INPUT_SCHEMA_VERSION
      : NODE_PRESET_SCHEMA_VERSION,
    name: 'Preset validation',
    width: doc.width,
    height: doc.height,
    layers: [layer],
    fontAssets: preset.fontAssets,
    imageAssets: preset.imageAssets,
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
        q.type === 'PaintedPath'))
  );
}
export const JOIN_SLOT_LIMIT = 1024;
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
  const entries =
    n.type === 'Join Geometry'
      ? joinInputKeys(n).map((key) => [key, n.params[key]])
      : n.type === 'Boolean'
        ? booleanInputKeys(n).map((key) => [key, n.params[key]])
        : Object.entries(n.params);
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
  Taper: ['horizontal', 'vertical'],
  'Bend Points': ['horizontal', 'vertical'],
  'Wave Points': ['vertical', 'horizontal'],
  Mirror: ['left-right', 'up-down', 'angle'],
  'Closed Path Offset': ['miter', 'bevel', 'round'],
  'Geometry Repeater': ['linear', 'radial-fit', 'radial-step', 'ae-2d'],
  'Resample Path': ['count', 'spacing'],
  'Line Screen': ['horizontal', 'vertical'],
  Boolean: ['union', 'subtract', 'intersect', 'difference'],
  Math: ['add', 'subtract', 'multiply', 'divide', 'min', 'max', 'abs', 'clamp'],
  'Vector Math': [
    'add',
    'subtract',
    'multiply',
    'divide',
    'scale',
    'normalize',
    'length',
    'distance',
    'dot',
  ],
};
export function activeParameters(n) {
  const entries = parameters(n);
  if (n.type === 'Pucker & Bloat' && n.params.autoCenter.value && !n.params.autoCenter.source)
    return entries.filter(([key]) => key !== 'center');
  if (n.type === 'Mirror')
    return entries.filter(([key]) => key !== 'angle' || n.operation === 'angle');
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
  if (n.type === 'Vector Math') {
    const keys =
      n.operation === 'scale'
        ? ['a', 'scale']
        : ['normalize', 'length'].includes(n.operation)
          ? ['a']
          : ['a', 'b'];
    return entries.filter(([key]) => keys.includes(key));
  }
  if (
    isTransform(n) &&
    (!n.params.uniformScale ||
      (n.params.uniformScale.value === true &&
        !n.params.uniformScale.source &&
        !n.params.uniformScale.expression))
  )
    return entries.filter(([key]) => key !== 'scaleWidth');
  if (n.type !== 'Math') return entries;
  const keys =
    n.operation === 'abs' ? ['a'] : n.operation === 'clamp' ? ['a', 'min', 'max'] : ['a', 'b'];
  return entries.filter(([key]) => keys.includes(key));
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
function mathValue(operation, read) {
  if (!operationOptions.Math.includes(operation)) throw Error('Invalid Math operation');
  const a = read('a');
  if (operation === 'abs') return Math.abs(a);
  if (operation === 'clamp') {
    const min = read('min'),
      max = read('max');
    if (min > max) throw Error('Clamp minimum exceeds maximum');
    return Math.min(max, Math.max(min, a));
  }
  const b = read('b');
  return {
    add: () => a + b,
    subtract: () => a - b,
    multiply: () => a * b,
    divide: () => (b === 0 ? 0 : a / b),
    min: () => Math.min(a, b),
    max: () => Math.max(a, b),
  }[operation]();
}
function vectorMathValue(operation, read) {
  if (!operationOptions['Vector Math'].includes(operation))
    throw Error('Invalid Vector Math operation');
  const a = read('a');
  if (operation === 'length') return Math.hypot(...a);
  if (operation === 'normalize') {
    // Scale before measuring so both huge and subnormal finite inputs normalize safely.
    const largest = Math.max(...a.map(Math.abs));
    if (largest === 0) return [0, 0];
    const scaled = a.map((v) => v / largest),
      length = Math.hypot(...scaled);
    return scaled.map((v) => v / length);
  }
  if (operation === 'scale') {
    const scale = read('scale');
    return a.map((v) => v * scale);
  }
  const b = read('b');
  if (operation === 'distance') return Math.hypot(a[0] - b[0], a[1] - b[1]);
  if (operation === 'dot') return a[0] * b[0] + a[1] * b[1];
  return a.map((v, i) =>
    ({
      add: () => v + b[i],
      subtract: () => v - b[i],
      multiply: () => v * b[i],
      divide: () => (b[i] === 0 ? 0 : v / b[i]),
    })[operation](),
  );
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
    if (existing && input.unstable && existing.fingerprint !== input.fingerprint)
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
  if (
    n.type === 'Boolean' &&
    key === booleanPendingInputKey &&
    !n.booleanInputOrder &&
    (n.params.b?.source || n.params.b?.locked)
  )
    return p('Path', null);
  return transformParameter(n, key);
}
export function expression(source, resolveReference, referencesOnly = false, baseValue) {
  if (source.length > 4096) throw Error('Expression exceeds 4096 characters');
  const tokens =
    source.match(
      /"[a-zA-Z0-9:_.-]+"|(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?|[a-zA-Z]+|[()+\-*/%,]/g,
    ) || [];
  if (tokens.join('') !== source.replace(/\s/g, '')) throw Error('Unsupported expression syntax');
  let i = 0;
  const funcs = {
    sin: Math.sin,
    cos: Math.cos,
    tan: Math.tan,
    abs: Math.abs,
    sqrt: Math.sqrt,
    min: Math.min,
    max: Math.max,
    round: Math.round,
  };
  function atom() {
    let t = tokens[i++];
    if (t === '-') return -atom();
    if (t === '+') return atom();
    if (t === '(') {
      let v = parse(0);
      if (tokens[i++] !== ')') throw Error('Missing closing parenthesis');
      return v;
    }
    if (t === 'pi') return Math.PI;
    if (t === 'e') return Math.E;
    if (t === 'value') {
      if (referencesOnly) return 0;
      assertValue('Scalar', baseValue);
      return baseValue;
    }
    if (t === 'ref') {
      if (tokens[i++] !== '(') throw Error('Reference requires parentheses');
      const readString = () => {
        const token = tokens[i++];
        if (!token || !/^"[a-zA-Z0-9:_.-]+"$/.test(token))
          throw Error('Reference requires quoted IDs and parameter keys');
        return token.slice(1, -1);
      };
      const nodeId = readString();
      if (!nodeIdPattern.test(nodeId) || tokens[i++] !== ',') throw Error('Invalid reference');
      const key = readString();
      let component;
      if (tokens[i] === ',') {
        i++;
        component = readString();
      }
      if (component !== undefined && !['x', 'y'].includes(component))
        throw Error('Reference component must be x or y');
      if (tokens[i++] !== ')') throw Error('Invalid reference arguments');
      if (!resolveReference) throw Error('Reference requires a composition');
      return resolveReference({ nodeId, key, component });
    }
    if (Object.hasOwn(funcs, t)) {
      if (tokens[i++] !== '(') throw Error('Function requires parentheses');
      let args = [parse(0)];
      while (tokens[i] === ',') {
        i++;
        args.push(parse(0));
      }
      if (tokens[i++] !== ')') throw Error('Missing closing parenthesis');
      return funcs[t](...args);
    }
    if (t && /^\d|^\./.test(t)) return Number(t);
    throw Error('Expected a number');
  }
  function parse(min) {
    let v = atom();
    while (i < tokens.length) {
      let op = tokens[i],
        prec = { '+': 1, '-': 1, '*': 2, '/': 2, '%': 2 }[op];
      if (!prec || prec < min) break;
      i++;
      let r = parse(prec + 1);
      v = op === '+' ? v + r : op === '-' ? v - r : op === '*' ? v * r : op === '/' ? v / r : v % r;
    }
    return v;
  }
  let result = parse(0);
  if (i !== tokens.length || (!referencesOnly && !Number.isFinite(result)))
    throw Error('Expression must produce a finite number');
  return result;
}
// Only bare value is new syntax; a quoted parameter key named "value" is not.
function usesBaseValue(source) {
  return /\bvalue\b/.test(source.replace(/"[^"]*"/g, ''));
}
function parameterExpression(q, resolveReference) {
  return q.type === 'Vector2'
    ? q.expression
        .split(';')
        .map((part, axis) => expression(part, resolveReference, false, q.value[axis]))
    : expression(q.expression, resolveReference, false, q.value);
}
// IDs are deliberately restricted: no JavaScript, object access, or string evaluation.
export function parameterReferenceExpression(nodeId, key, component) {
  const text = `ref(${JSON.stringify(nodeId)}, ${JSON.stringify(key)}${component ? `, ${JSON.stringify(component)}` : ''})`;
  expression(text, () => 0, true);
  return text;
}
export function expressionReferences(source) {
  // Existing dormant literal expressions keep their prior import semantics.
  if (!/\bref\b/.test(source)) return [];
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
function referenceParameter(layer, ref) {
  const target = layer.nodes.find((n) => n.id === ref.nodeId);
  if (!target) throw Error(`Expression reference target is missing: ${ref.nodeId}`);
  const q = Object.hasOwn(target.params, ref.key)
    ? target.params[ref.key]
    : transformParameter(target, ref.key);
  if (!q || (ref.component === undefined ? q.type !== 'Scalar' : q.type !== 'Vector2'))
    throw Error('Expression reference requires a Scalar parameter or a Vector2 component');
  return target;
}
export function expressionReferenceTargets(layer, nodeId) {
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
      Object.entries(n.params).flatMap(([key, q]) =>
        (q.type === 'Scalar' ? [undefined] : q.type === 'Vector2' ? ['x', 'y'] : []).map(
          (component) => ({
            nodeId: n.id,
            name: n.name,
            key,
            component,
            label: `${n.name} · ${key}${component ? `.${component}` : ''}`,
            expression: parameterReferenceExpression(n.id, key, component),
          }),
        ),
      ),
    );
}
function assertValue(type, value) {
  if (
    type === 'Color' &&
    (!value ||
      typeof value !== 'object' ||
      Array.isArray(value) ||
      Object.keys(value).length !== 2 ||
      typeof value.hex !== 'string' ||
      !/^#[0-9a-fA-F]{6}$/.test(value.hex) ||
      !Number.isFinite(value.alpha) ||
      value.alpha < 0 ||
      value.alpha > 1)
  )
    throw Error('Expected Color with hex #RRGGBB and alpha from 0 to 1');
  if (type === 'Scalar' && !Number.isFinite(value)) throw Error('Expected finite Scalar');
  if (
    type === 'Vector2' &&
    (!Array.isArray(value) || value.length !== 2 || !value.every(Number.isFinite))
  )
    throw Error('Expected Vector2');
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
function contourGroups(path) {
  const groups = new Map();
  for (const c of path.contours) {
    const key = c.group ?? '';
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(c);
  }
  return [...groups.values()];
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
export function signature(path) {
  return JSON.stringify(
    path.contours.map((c) => [
      c.closed,
      c.points.map((p) => [p.position, p.handleA, p.handleB]),
      ...(c.group === undefined ? [] : [c.group]),
      ...(c.repeaterOpacity === undefined ? [] : [c.repeaterOpacity]),
    ]),
  );
}
export function pathData(path) {
  const obj = toPaper(path);
  const result = obj.pathData;
  obj.remove();
  return result;
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
) {
  const context = compositionContext || { active: new Set(), results: new Map() };
  function referencedOutput(n) {
    const targetId = n.compositionReference?.compositionId;
    if (!targetId)
      throw Object.assign(Error('Choose a source Composition'), {
        code: 'COMPOSITION_TARGET_UNSET',
      });
    if (!documentContext) throw Error('Composition reference requires its document');
    if (targetId === layer.id || context.active.has(targetId))
      throw Error('Composition reference cycle detected');
    const target = documentContext.layers.find((l) => l.id === targetId);
    if (!target)
      throw Object.assign(Error(`Missing Composition: ${targetId}`), {
        code: 'COMPOSITION_TARGET_MISSING',
      });
    if (!context.results.has(targetId)) {
      context.active.add(layer.id);
      context.active.add(targetId);
      try {
        context.results.set(
          targetId,
          evaluate(target, fonts, imageAssets, documentContext, context),
        );
      } finally {
        context.active.delete(targetId);
        context.active.delete(layer.id);
      }
    }
    const result = context.results.get(targetId);
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
    active = new Set(),
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
  const provenances = new Map();
  function provenance(id, port) {
    const key = outputKey(id, port);
    if (!provenances.has(key)) {
      const n = byId.get(id),
        deps = (key !== id ? [['position', n.params.position]] : evaluatedParameters(n)).filter(
          ([, q]) => q.source,
        ),
        reference = key === id && referenceDependency(n);
      if (reference) deps.push(['reference', reference]);
      if (key === id && n.pathAttachment)
        deps.push(['textPath', { source: n.pathAttachment.nodeId }]);
      provenances.set(
        key,
        fingerprint(
          JSON.stringify([
            n.id,
            n.type,
            ...(n.bypassed ? ['bypassed'] : []),
            key !== id ? 'position' : null,
            (key !== id ? [['position', n.params.position]] : evaluatedParameters(n))
              .filter(([, q]) => q.expression && !q.source)
              .map(([input, q]) => [input, expressionReferences(q.expression)]),
            deps.map(([input, q]) => [
              input,
              q.source,
              q.sourcePort,
              provenance(q.source, q.sourcePort),
            ]),
          ]),
        ),
      );
    }
    return provenances.get(key);
  }
  const offsetComputations = new Set();
  // Separate shares one vector evaluation. Text's value output has its own cache entry
  // and dependencies, so reading Position never requires font/outline generation.
  const outputKey = (id, port) =>
    byId.get(id)?.type === 'Text' && port === 'position' ? `${id}\0position` : id;
  const readingParameters = new Set();
  function resolveExpressionReference(ref) {
    const target = referenceParameter(layer, ref),
      value = readParameter(target, ref.key);
    return ref.component === undefined ? value : value[ref.component === 'x' ? 0 : 1];
  }
  function readParameter(n, key) {
    const id = `${n.id}\0${key}`;
    if (readingParameters.has(id)) throw Error('Expression reference cycle detected');
    if (readingParameters.size >= 256) throw Error('Expression reference depth exceeds 256');
    readingParameters.add(id);
    try {
      return readParameterValue(n, key);
    } finally {
      readingParameters.delete(id);
    }
  }
  function readParameterValue(n, key) {
    if (isTransform(n) && !n.params[key] && key === 'position') return readParameter(n, 'anchor');
    if (isTransform(n) && !n.params[key] && key === 'scaleWidth') return readParameter(n, 'scale');
    let q = parameter(n, key);
    if (!q) throw Error(`Missing parameter ${key}`);
    if (q.type === 'Color' && q.expression) throw Error('Color expressions are not supported');
    let value = q.source
      ? run(q.source, q.sourcePort)
      : q.expression
        ? parameterExpression(q, resolveExpressionReference)
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
    return n.type === 'Separate Vector' ? value[port === 'x' ? 0 : 1] : value;
  }
  function runNode(id, port) {
    const key = outputKey(id, port),
      positionOutput = key !== id;
    if (values.has(key)) return values.get(key);
    if (active.has(id)) throw Error('Cycle detected');
    if (active.size >= 256) throw Error('Graph depth exceeds 256 nodes');
    let n = byId.get(id);
    if (!n) throw Error('Missing source node');
    active.add(id);
    try {
      const inputs = positionOutput ? [['position', n.params.position]] : evaluatedParameters(n);
      const deps = positionOutput ? parameterDependencies(inputs) : nodeDependencies(n, true);
      if (n.type === 'Reference') referenceTarget(layer, n);
      const external = n.type === 'Composition Reference' ? referencedOutput(n) : null;
      const parameterVersions = new Map();
      for (const dep of deps) {
        if (dep.parameterKey)
          parameterVersions.set(
            dep,
            resolveExpressionReference({
              nodeId: dep.source,
              key: dep.parameterKey,
              component: dep.component,
            }),
          );
        else run(dep.source, dep.sourcePort);
      }
      const dependencyVersions = deps.map((q) => [
        q.source,
        q.sourcePort,
        q.parameterKey
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
            ? [inputs, dependencyVersions]
            : [
                n.type,
                !!n.bypassed,
                n.reference,
                n.compositionReference,
                n.paintMode,
                external?.outputRevision,
                inputs,
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
            : clone(existing.value);
        values.set(key, result);
        versions.set(key, existing.version);
        active.delete(id);
        warnings.push(...existing.warnings);
        if (!positionOutput && existing.textLayout) textLayouts.set(id, clone(existing.textLayout));
        return result;
      }
      const warningStart = warnings.length;
      computed++;
      const read = (key) => readParameter(n, key);
      let result;
      if (n.bypassed) {
        result = clone(read(bypassInput(n)));
        if (result === null) throw Error(`Connect ${bypassInput(n)} to bypass ${n.type}`);
        if (n.type === 'Mask') result = paintedGroup([result], { prefix: n.id });
        assertValue(outputType(n), result);
      } else if (n.type === 'Composition Reference') {
        const position = read('position');
        result = paintedGroup([external.path], { matrix: [1, 0, 0, 1, ...position], prefix: n.id });
        result.fingerprint = fingerprint(
          JSON.stringify([n.compositionReference, n.id, position, external.path]),
        );
      } else if (n.type === 'Reference') {
        result = clone(run(n.reference.nodeId, referencePort(n.reference)));
        assertValue('Vector2', result);
      } else if (positionOutput || n.type === 'Null') result = read('position');
      else if (['Value', 'Vector', 'Expression', 'Toggle', 'Color'].includes(n.type))
        result = read('value');
      else if (n.type === 'Vector Transform')
        result = transformVector(transformValues(n, read), read('vector'));
      else if (n.type === 'Vector Math') {
        result = vectorMathValue(n.operation, read);
        assertValue(outputType(n), result);
      } else if (n.type === 'Separate Vector') result = read('vector');
      else if (n.type === 'Math') {
        result = mathValue(n.operation, read);
        assertValue('Scalar', result);
      } else if (n.type === 'Combine Vector') {
        result = [read('x'), read('y')];
        assertValue('Vector2', result);
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
      } else if (n.type === 'Mask') {
        const source = read('path'),
          shape = read('mask');
        if (!source || !shape) throw Error('Connect appearance and closed shape to Mask');
        result = maskAppearance(source, shape, {
          invert: read('invert'),
          prefix: n.id,
          bounds: pathBounds(source),
        });
      } else if (n.type === 'Curve')
        result = {
          contours: [
            {
              id: n.id,
              closed: read('closed'),
              points: n.points.map((pt) => ({
                id: pt.id,
                position: read(`${pt.id}.position`),
                handleA: read(`${pt.id}.handleA`),
                handleB: read(`${pt.id}.handleB`),
              })),
            },
          ],
        };
      else if (n.type === 'Multi Path') {
        const points = new Map(
          n.points.map((pt) => [
            pt.id,
            {
              id: pt.id,
              position: read(`${pt.id}.position`),
              handleA: read(`${pt.id}.handleA`),
              handleB: read(`${pt.id}.handleB`),
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
      } else if (n.type === 'Join Geometry' && n.paintMode) {
        const inputs = joinInputKeys(n)
          .filter((key) => n.params[key].source)
          .map((key) => read(key));
        if (!inputs.length) throw Error('Connect a path to Join Geometry');
        result = paintedGroup(inputs, { prefix: n.id });
        result.fingerprint = fingerprint(JSON.stringify([n.id, inputs]));
      } else if (n.type === 'Transform' && n.paintMode) {
        const source = read('path');
        if (!source) throw Error('Connect a PaintedPath input');
        result = paintedGroup([source], { matrix: transformValues(n, read).matrix, prefix: n.id });
        result.fingerprint = fingerprint(JSON.stringify([n.id, source, result.matrix]));
      } else if (n.type === 'Join Geometry') {
        const inputs = joinInputKeys(n)
          .filter((key) => n.params[key].source)
          .map((key) => [key, read(key)]);
        if (!inputs.length) throw Error('Connect a path to Join Geometry');
        let contourCount = 0,
          pointCount = 0;
        const groups = [];
        for (const [, path] of inputs) {
          contourCount += path.contours.length;
          for (const contour of path.contours) pointCount += contour.points.length;
          if (contourCount > 10000 || pointCount > 100000)
            throw Error('Join exceeds geometry budget');
          const inputGroups = contourGroups(path);
          if (groups.length + inputGroups.length > 256)
            throw Error('Join exceeds 256 independent groups');
          groups.push(...inputGroups);
        }
        const prefix = fingerprint(n.id);
        result = {
          contours: groups.flatMap((group, gi) =>
            group.map((c, ci) => ({
              ...clone(c),
              id: `${prefix}:g${gi}:c${ci}`,
              group: `${prefix}:g${gi}`,
              points: c.points.map((pt, pi) => ({
                ...clone(pt),
                id: `${prefix}:g${gi}:c${ci}:p${pi}`,
              })),
            })),
          ),
        };
        result.characterOccurrences = inputs.flatMap(([key, path]) =>
          routeCharacters(path, `${n.id}:${key}`),
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
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
      } else if (n.type === 'Simplify Path') {
        const source = read('path');
        result = simplifyGeometry(source, {
          tolerance: read('tolerance'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Simplify Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Expand Stroke') {
        const source = read('path');
        result = expandStrokeGeometry(source, {
          prefix: fingerprint(n.id),
          startWidthMultiplier: read('startWidthMultiplier'),
          endWidthMultiplier: read('endWidthMultiplier'),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Expand Stroke; edit the source Text',
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
        result.fingerprint = fingerprint(
          JSON.stringify([
            provenance(n.id),
            closed,
            source.fingerprint || signature(source),
            signature(result),
          ]),
        );
      } else if (n.type === 'Reverse Path') {
        const source = read('path');
        result = reversePathGeometry(source, { prefix: fingerprint(n.id) });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Reverse Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Taper') {
        const source = read('path');
        result = taperGeometry(source, {
          amount: read('amount'),
          axis: n.operation,
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Taper; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Bend Points') {
        const source = read('path');
        result = bendGeometry(source, {
          angle: read('angle'),
          axis: n.operation,
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Bend Points; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Auto Handles') {
        const source = read('path');
        result = autoHandlesGeometry(source, {
          strength: read('strength'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Auto Handles; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Twist Points') {
        const source = read('path');
        result = twistGeometry(source, {
          angle: read('angle'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Twist Points; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Wave Points') {
        const source = read('path');
        result = waveGeometry(source, {
          amplitude: read('amplitude'),
          wavelength: read('wavelength'),
          phase: read('phase'),
          direction: n.operation,
          keepEndpoints: read('keepEndpoints'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Wave Points; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Pucker & Bloat') {
        const source = read('path'),
          autoCenter = read('autoCenter');
        result = puckerBloatGeometry(source, {
          amount: read('amount'),
          autoCenter,
          ...(autoCenter ? {} : { center: read('center') }),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Pucker & Bloat; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
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
      } else if (n.type === 'Smooth Path') {
        const source = read('path');
        result = smoothGeometry(source, {
          factor: read('factor'),
          iterations: read('iterations'),
          keepEndpoints: read('keepEndpoints'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Smooth Path; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Roughen') {
        const source = read('path');
        result = roughenGeometry(source, {
          amount: read('amount'),
          seed: read('seed'),
          keepEndpoints: read('keepEndpoints'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Roughen; edit the source Text',
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
      } else if (n.type === 'Corner Round') {
        const source = read('path');
        const radius = read('radius');
        if (radius < 0 || radius > 100000)
          throw Error('Corner Round radius must be from 0 to 100000 px');
        result = roundCorners(source, { radius, prefix: fingerprint(n.id) });
        for (const [reason, count] of Object.entries(result.cornerDiagnostics?.skipped || {}))
          warnings.push(
            `${n.name}: ${count} corner(s) left unchanged (${reason}). Curved corners use conservative local G1 blends.`,
          );
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Corner Round; edit the source Text',
        );
        result.unstable = true;
        result.fingerprint = fingerprint(signature(result));
      } else if (n.type === 'Mirror') {
        if (!operationOptions.Mirror.includes(n.operation)) throw Error('Invalid Mirror operation');
        const source = read('path');
        result = mirrorGeometry(source, {
          center: read('center'),
          angle: n.operation === 'left-right' ? 90 : n.operation === 'up-down' ? 0 : read('angle'),
          keepOriginal: read('keepOriginal'),
          prefix: fingerprint(n.id),
        });
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          'Canvas character editing is unavailable through Mirror; edit the source Text',
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
        result.fingerprint = fingerprint(
          JSON.stringify([
            provenance(n.id),
            source.fingerprint || null,
            settings,
            pitch,
            invert,
            gridOrigin,
            tonePower,
            result.contours,
          ]),
        );
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
        result.fingerprint = fingerprint(
          JSON.stringify([
            provenance(n.id),
            source.fingerprint || null,
            motif.fingerprint || null,
            settings,
            pitch,
            invert,
            gridOrigin,
            ...(tonePower !== 1 ? [tonePower] : []),
            cells,
            signature(motif),
            signature(result),
          ]),
        );
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
        result.fingerprint = fingerprint(
          JSON.stringify([
            provenance(n.id),
            source.fingerprint || null,
            signature(source),
            alignTangent,
            targets.contours.flatMap((contour) => contour.points.map((point) => point.position)),
            signature(result),
          ]),
        );
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
        result.fingerprint = fingerprint(
          JSON.stringify([
            provenance(n.id),
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
            source.fingerprint || null,
            source.kind === 'image'
              ? JSON.stringify([source.kind, source.asset, source.position, source.size])
              : signature(source),
            signature(result),
          ]),
        );
        result.characterOccurrences = routeCharacters(
          source,
          n.id,
          null,
          `Canvas character editing is unavailable through ${n.type}; edit upstream Text`,
        );
      } else if (n.type === 'Convert to Path') {
        const source = read('path');
        if (!source) throw Error('Connect a Path or PaintedPath input');
        result = isPaintedGroup(source) ? groupGeometry(source) : clone(source);
        if (
          outputType(byId.get(n.params.path.source), n.params.path.sourcePort) === 'PaintedPath'
        ) {
          delete result.fill;
          delete result.stroke;
          result.fingerprint = fingerprint(
            JSON.stringify([
              'painted-path-geometry',
              provenance(n.id),
              signature(result),
              result.contours.map((contour) => [
                contour.id,
                contour.points.map((point) => point.id),
              ]),
            ]),
          );
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
          result.fingerprint = fingerprint(
            JSON.stringify([
              provenance(n.id),
              amount,
              n.operation,
              miterLimit,
              source.fingerprint,
              source.contours,
              signature(result),
            ]),
          );
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
        result = clone(result);
        if (isPaintedGroup(result) && n.type !== 'Output')
          throw Error('Edit paint in the source Composition, or use Convert to Path');
        if (n.type === 'Fill') result.fill = read('color');
        if (n.type === 'Linear Gradient Fill') result.fill = linearGradientSettings(read);
        if (n.type === 'Radial Gradient Fill') result.fill = radialGradientSettings(read);
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
            if (!pt || (result.unstable && edit.fingerprint !== result.fingerprint)) {
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
            c.points.forEach((p, pi) => (p.id = result.contours[ci].points[pi].id));
          });
          transformed.characterOccurrences = routeCharacters(result, n.id, state.matrix);
          transformed.unstable = result.unstable;
          transformed.remapSuspended = result.remapSuspended;
          transformed.fingerprint = result.fingerprint;
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
      active.delete(id);
      values.set(key, result);
      const version = ++revision;
      versions.set(key, version);
      cache.set(key, {
        key: cacheKey,
        font: cacheFont,
        styleFonts,
        imageAsset,
        value: clone(result),
        version,
        warnings: warnings.slice(warningStart),
        textLayout: !positionOutput && textLayouts.has(id) ? clone(textLayouts.get(id)) : undefined,
      });
      return result;
    } finally {
      active.delete(id);
    }
  }
  let path = null,
    error = null,
    errorCode;
  try {
    path = run(layer.output);
  } catch (e) {
    error = e.message;
    errorCode = e.code;
  }
  for (const key of cache.keys()) if (!byId.has(key.split('\0')[0])) cache.delete(key);
  return {
    path,
    outputRevision: versions.get(outputKey(layer.output)),
    error,
    ...(errorCode ? { errorCode } : {}),
    warnings,
    values,
    run,
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
) {
  const attached = layer.nodes.filter((n) => n.type === 'Text' && (n.pathAttachment || n.textWrap));
  if (!attached.length) return;
  assertAcyclic(layer);
  const current = evaluate(layer, fonts, imageAssets, documentContext);
  let previous;
  for (const n of attached) {
    try {
      current.run(n.id);
    } catch (error) {
      const old = previousLayer?.nodes.find(
        (x) => x.id === n.id && (x.pathAttachment || x.textWrap),
      );
      if (old) {
        previous ??= evaluate(previousLayer, previousFonts, imageAssets, previousDocumentContext);
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
export function setTextPathAttachment(layer, nodeId, attachment, fonts = {}, imageAssets = {}) {
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
    if (attachment !== null) evaluate(layer, fonts, imageAssets).run(nodeId);
  } catch (error) {
    if (old === undefined) delete n.pathAttachment;
    else n.pathAttachment = old;
    throw error;
  }
}
/** Set or remove composition width atomically; source text and character identities stay untouched. */
export function setTextWrap(layer, nodeId, wrap, fonts = {}, imageAssets = {}) {
  const n = layer.nodes.find((n) => n.id === nodeId);
  if (n?.type !== 'Text') throw Error('Select a Text node');
  if (layer.locked) throw Error('Composition is locked');
  const old = n.textWrap;
  try {
    if (wrap === null) delete n.textWrap;
    else {
      validateTextWrap(wrap, n.writingMode, n.pathAttachment);
      n.textWrap = clone(wrap);
      evaluate(layer, fonts, imageAssets).run(nodeId);
    }
  } catch (error) {
    if (old === undefined) delete n.textWrap;
    else n.textWrap = old;
    throw error;
  }
}
export function textPathTargets(layer, textId, fonts = {}, imageAssets = {}) {
  const result = evaluate(layer, fonts, imageAssets),
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
) {
  assertTextPathChange(
    layer,
    previousLayer,
    fonts,
    imageAssets,
    previousFonts,
    documentContext,
    previousDocumentContext,
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
    'Auto Handles',
    'Instance on Points',
    'Luminance Instances',
    'Luminance to Points',
    'Halftone',
    'Ordered Dither',
    'Error Diffusion Dither',
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
  if (!layer.nodes.some((n) => bounded.has(n.type) && boundedInputs(n).some(Boolean))) return;
  const current = evaluate(layer, fonts, imageAssets, documentContext);
  let previous;
  for (const n of layer.nodes.filter(
    (n) => bounded.has(n.type) && boundedInputs(n).some(Boolean),
  )) {
    try {
      current.run(n.id);
    } catch (error) {
      // Missing inputs are an ordinary intermediate wiring state.
      if (
        /^Connect (a |both )/.test(error.message) ||
        ['COMPOSITION_TARGET_UNSET', 'COMPOSITION_TARGET_MISSING'].includes(error.code)
      )
        continue;
      const old = previousLayer?.nodes.find(
        (x) => x.id === n.id && bounded.has(x.type) && boundedInputs(x).some(Boolean),
      );
      if (old) {
        previous ??= evaluate(previousLayer, previousFonts, imageAssets, previousDocumentContext);
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
    if (!q || q.type !== type) throw Error('Invalid parameter type');
    if (!(layerFillFallback && q.value === null)) assertValue(type, q.value);
    if (['Path', 'PaintedPath', 'Image'].includes(type) && q.value !== null)
      throw Error('Path fallback must be null');
    text(q.expression);
    if (
      usesBaseValue(q.expression) &&
      (doc.schemaVersion < 26 || !['Scalar', 'Vector2'].includes(type))
    )
      throw Error('Authored value expressions require numeric parameters and schemaVersion 26');
    if (type === 'Color' && q.expression) throw Error('Color expressions are not supported');
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
      doc.schemaVersion !== REPEATER_SCHEMA_VERSION &&
      doc.schemaVersion !== OPENTYPE_SCHEMA_VERSION &&
      doc.schemaVersion !== NODE_PRESET_SCHEMA_VERSION &&
      doc.schemaVersion !== BOOLEAN_INPUT_SCHEMA_VERSION)
  )
    throw Error('Unsupported project version');
  if (doc.schemaVersion === 1) migratePaint(doc);
  // Apply every migration against the file's incoming version. A feature later in
  // the node list must not advance the version before older nodes are backfilled.
  const sourceSchemaVersion = doc.schemaVersion;
  text(doc.name);
  validateFontAssets(doc.fontAssets);
  if (Object.values(doc.fontAssets || {}).some((a) => a.kind === VARIABLE_KIND))
    variableCheck(doc.schemaVersion >= 25, 'VARIABLE_SCHEMA');
  validateImageAssets(doc.imageAssets);
  validateGuides(doc.editor, id);
  if (doc.editor?.gridAnchor === 'center' && doc.schemaVersion < 27)
    throw Error('Centered grid requires schemaVersion 27');
  if (!Array.isArray(doc.layers) || doc.layers.length < 1 || doc.layers.length > 100)
    throw Error('Invalid layers');
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
    for (const n of l.nodes) {
      id(n.id);
      if (['Keyer', 'Mask'].includes(n.type) && sourceSchemaVersion < 35)
        throw Error('Keyer and Mask require schemaVersion 35');
      if (n.type === 'Keyer' && !n.bypassed && ++activeKeyers > MAX_ACTIVE_KEYERS)
        throw Error('At most 8 active Keyers per document');
      validateReference(n);
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
          throw Error('Invalid Join input order');
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
      if ((extraTransform.length || n.type === 'Vector Transform') && sourceSchemaVersion < 11)
        throw Error('Extended Transform requires schemaVersion 11');
      if (
        Object.keys(n.params).length !==
        Object.keys(definitions[n.type]).length +
          extraTransform.length +
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
      if (n.type === 'Linear Gradient Fill') linearGradientSettings((key) => n.params[key].value);
      if (n.type === 'Radial Gradient Fill') radialGradientSettings((key) => n.params[key].value);
      if (['Halftone', 'Ordered Dither', 'Line Screen', 'Error Diffusion Dither'].includes(n.type))
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
  for (const l of doc.layers) {
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
          for (const ref of expressionReferences(q.expression)) {
            if (owners.has(ref.nodeId) && owners.get(ref.nodeId) !== l.id)
              throw Error('Expression reference target must be in the same layer');
            if (owners.has(ref.nodeId)) referenceParameter(l, ref);
          }
        }
    assertAcyclic(l);
  }
  assertCompositionAcyclic(doc);
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
  return doc;
}
export function serialize(doc) {
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
) {
  if (isPaintedGroup(value)) {
    const content = value.children
      .map((child, i) => renderElement(child, l, assets, '', `${gradientId}-${i}`))
      .join('');
    const mask = value.mask;
    const id = `${gradientId}-mask`;
    const definition = mask
      ? `<defs><mask id="${id}" maskUnits="userSpaceOnUse" maskContentUnits="userSpaceOnUse" x="${mask.region[0]}" y="${mask.region[1]}" width="${mask.region[2]}" height="${mask.region[3]}" style="mask-type:luminance" color-interpolation="sRGB">${mask.invert ? `<rect x="${mask.region[0]}" y="${mask.region[1]}" width="${mask.region[2]}" height="${mask.region[3]}" fill="white"/>` : ''}<path d="${escape(pathData(mask.shape))}" fill="${mask.invert ? 'black' : 'white'}" fill-rule="nonzero"/></mask></defs>`
      : '';
    // The mask and its source share the group's local coordinates. A later
    // appearance transform moves both, preserving nested masks and source alpha.
    return `<g ${attributes} data-layer="${escape(l.name)}" transform="matrix(${value.matrix.join(' ')})">${mask ? `${definition}<g mask="url(#${id})">${content}</g>` : content}</g>`;
  }
  if (value.kind === 'image') {
    const asset = imageAssetFor(value, assets);
    return `<image ${attributes} data-layer="${escape(l.name)}" x="${value.position[0]}" y="${value.position[1]}" width="${value.size[0]}" height="${value.size[1]}" preserveAspectRatio="none" href="${imageURL(asset)}"/>`;
  }
  const fill = value.fill,
    stroke = value.stroke,
    gradient =
      fill?.kind === 'linear-gradient'
        ? renderLinearGradientFill(fill, gradientId)
        : fill?.kind === 'radial-gradient'
          ? renderRadialGradientFill(fill, gradientId)
          : null;
  if (fill && !gradient) assertValue('Color', fill);
  if (stroke) {
    assertValue('Color', { hex: stroke.hex, alpha: stroke.alpha });
    if (!Number.isFinite(stroke.width) || stroke.width < 0 || stroke.width > 100000)
      throw Error('Invalid stroke width');
  }
  if (!value.contours.some((c) => c.points.length)) return '';
  function shape(path, attrs = '') {
    if (path.contours.some((c) => c.group !== undefined))
      return `<g ${attrs} data-layer="${escape(l.name)}">${contourGroups(path)
        .map((contours) => {
          const opacity = contours[0].repeaterOpacity;
          const content = shape({ ...path, contours: contours.map(({ group, ...c }) => c) });
          return opacity === undefined ? content : `<g opacity="${opacity}">${content}</g>`;
        })
        .join('')}</g>`;
    return `<path ${attrs} data-layer="${escape(l.name)}" d="${escape(pathData(path))}" fill="${gradient ? gradient.paint : fill ? escape(fill.hex) : 'none'}"${fill && !gradient ? ` fill-opacity="${fill.alpha}"` : ''} fill-rule="nonzero"${stroke ? ` stroke="${escape(stroke.hex)}" stroke-opacity="${stroke.alpha}" stroke-width="${stroke.width}" stroke-linecap="${stroke.cap}" stroke-linejoin="${stroke.join}" stroke-miterlimit="${stroke.miterLimit}" stroke-dasharray="${stroke.dash.length ? stroke.dash.join(' ') : 'none'}" stroke-dashoffset="${stroke.dashOffset}"` : ''}/>`;
  }
  return `${gradient ? `<defs>${gradient.definition}</defs>` : ''}${shape(value, attributes)}`;
}
// Validation includes legacy migrations and asset freezing. Keep both private to
// export, including for mutable callers; cloneDocument would freeze their assets.
export function exportScene(doc, fonts = {}) {
  return exportValidatedScene(validate(clone(doc)), fonts);
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
      return { layer: l, value: result.path };
    });
}
export function exportSVG(doc, fonts = {}, options = {}) {
  if (
    !options ||
    typeof options !== 'object' ||
    Array.isArray(options) ||
    Object.keys(options).some((key) => !['unit', 'responsive'].includes(key)) ||
    (options.responsive !== undefined && typeof options.responsive !== 'boolean')
  )
    throw Error('Invalid SVG export options');
  doc = validate(clone(doc));
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
  return createVariableBridge({ ...options, resolveFonts, validate });
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
  constructor(
    doc,
    { fonts = () => ({}), busy = () => false, onCommitted = () => {}, retainHistory = true } = {},
  ) {
    validate(doc);
    if (Object.values(doc.fontAssets || {}).some((asset) => asset.kind === VARIABLE_KIND))
      assertVariableDocumentReady(doc, resolveFonts(doc, fonts()));
    this.#history = new History(cloneDocument(doc), { retainHistory });
    this.#fonts = fonts;
    this.#busy = busy;
    this.#onCommitted = onCommitted;
    this.id = uid('session');
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
      assertValue('Vector2', update.delta);
      const candidate = cloneDocument(base),
        layer = candidate.layers.find((l) => l.id === target.layerId);
      if (!layer) throw Error('Missing composition');
      if (layer.locked) throw Error('Composition is locked');
      const write = (q, original = q?.value) => {
        if (!q || q.type !== 'Vector2') throw Error('Missing vector parameter');
        if (q.locked || q.source || q.expression) throw Error('Parameter is locked or driven');
        q.value = original.map((v, i) => v + update.delta[i]);
      };
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
                    : ['Text', 'Image', 'Null', 'Composition Reference'].includes(node.type)
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
        if (
          update.independent !== undefined &&
          (typeof update.independent !== 'boolean' || target.key === 'position')
        )
          throw Error('Independent handles require a handle drag');
        if (target.kind === 'curve-point') {
          const editablePoints = new Map(node.points.map((p) => [p.id, p]));
          for (const pointId of pointIds) {
            const point = editablePoints.get(pointId);
            if (!point) throw Error('Missing point');
            write(point[target.key]);
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
            if (existing && input.unstable && existing.fingerprint !== input.fingerprint)
              throw Error('Retained edit is suspended; rebind before dragging');
            const edit = enableRemapPoint(node, input, pointId);
            write(edit.params[target.key]);
          }
        }
      }
      // Returning to the origin must retain sparse Remap edits and the redo branch.
      if (update.delta.every((value) => value === 0)) return cloneDocument(base);
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
      const encoded = serialize(candidate);
      if (encoded === serialize(draft.base)) {
        this.#gesture = null;
        return { ...this.state, ...created, discarded: true, changed: false };
      }
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
      !['set-text-content', 'set-text-settings', 'set-variable-font', 'undo', 'redo'].includes(type)
    )
      throw Error('VARIABLE_COMMAND_UNSUPPORTED');
    const target = this.#history.doc.layers
      .find((l) => l.id === payload.layerId)
      ?.nodes.find((n) => n.id === payload.nodeId);
    if (target?.locked) throw Error('Text node is locked');
    const sequence = ++this.#prepareSequence;
    this.#prepared = null;
    let candidate;
    if (type === 'undo' || type === 'redo')
      candidate = cloneDocument(
        (type === 'undo' ? this.#history.past : this.#history.future).at(-1) || this.#history.doc,
      );
    else candidate = this.#commandCandidate(type, payload, this.#history.doc, true).candidate;
    const ticket = await bridge.prepareDocument(candidate, options);
    if (
      sequence !== this.#prepareSequence ||
      sessionId !== this.id ||
      expectedRevision !== this.#history.revision
    )
      throw Error('STALE_REVISION: document changed');
    const fonts = { ...this.#fonts(), ...bridge.preparedFonts(ticket) };
    assertVariableDocumentReady(candidate, resolveFonts(candidate, fonts));
    // Validate downstream bounded geometry against the prepared candidate, before changing history.
    for (const layer of candidate.layers)
      assertClosedOffsetChange(
        layer,
        this.#history.doc.layers.find((l) => l.id === layer.id),
        resolveFonts(candidate, fonts),
        candidate.imageAssets,
        resolveFonts(this.#history.doc, this.#fonts()),
        candidate,
        this.#history.doc,
      );
    const token = Object.freeze({});
    this.#prepared = { token, ticket, bridge, candidate, expectedRevision, type, sequence };
    return token;
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
    return {
      document: cloneDocument(p.candidate),
      fonts: { ...this.#fonts(), ...p.bridge.preparedFonts(p.ticket) },
    };
  }
  #observeCommit(action) {
    const before = this.#history.revision;
    try {
      return action();
    } finally {
      if (this.#history.revision !== before) {
        // An adapter cannot roll back an already committed edit.
        try {
          this.#onCommitted(Object.freeze(this.state));
        } catch {}
      }
    }
  }
  commitPrepared(token) {
    return this.#observeCommit(() => this.#commitPrepared(token));
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
    p.bridge.assertFresh(p.ticket);
    assertVariableDocumentReady(
      p.candidate,
      resolveFonts(p.candidate, p.bridge.preparedFonts(p.ticket)),
    );
    const revision = this.#history.revision;
    if (p.type === 'undo' || p.type === 'redo') this.#history[p.type]();
    else if (!sameDocument(this.#history.doc, p.candidate))
      this.#history.apply(() => {
        this.#history.doc = p.candidate;
      });
    p.bridge.activate(p.ticket);
    if (p.type === 'import-svg') this.#nodeClipboard = null;
    this.#prepared = null;
    return { ...this.state, changed: revision !== this.#history.revision };
  }
  cancelPrepared(bridge) {
    this.#prepared = null;
    this.#prepareSequence++;
    bridge.cancel();
  }
  dispatch(command) {
    return this.#observeCommit(() => this.#dispatch(command));
  }
  #dispatch({ sessionId, expectedRevision, type, payload = {} }) {
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
    if (type === 'import-composition') {
      if (
        this.#busy() ||
        this.#gesture ||
        this.hasTextComposition ||
        [...this.#textDrafts.values()].some(
          (draft) => draft.revision === this.#history.revision && draft.state.changed,
        )
      )
        throw Error('GESTURE_ACTIVE: finish or cancel the active edit');
      if (typeof payload.sourceText !== 'string' || typeof payload.compositionId !== 'string')
        throw Error('Invalid composition import payload');
      const imported = prepareCompositionImport(
        this.#history.doc,
        payload.sourceText,
        payload.compositionId,
      );
      this.#history.apply((candidate) => {
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
      const initial =
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
      this.#gestureCandidate(base, target, initial);
      this.#gesture = {
        id: uid('gesture'),
        target,
        base,
        candidate: base,
        revision: this.#history.revision,
        version: 0,
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
        draft.candidate = candidate;
        draft.update = clone(payload.update);
        draft.version++;
        return { ...this.state, ...this.gestureState };
      }
      // Font adoption allocates private asset IDs during preview. Commit that
      // validated candidate verbatim instead of allocating different IDs again.
      if (draft.update && draft.target.kind !== 'text-font')
        draft.candidate = this.#gestureCandidate(draft.base, draft.target, draft.update);
      validate(draft.candidate);
      assertVariableDocumentReady(draft.candidate, resolveFonts(draft.candidate, this.#fonts()));
      const revision = this.#history.revision;
      if (!sameDocument(this.#history.doc, draft.candidate))
        this.#history.apply(() => {
          this.#history.doc = draft.candidate;
        });
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
        layer = doc.layers.find((l) => l.id === payload.layerId);
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
      const ids = new Set(nodes.map((n) => n.id)),
        externalNodeIds = [
          ...new Set(
            nodes
              .flatMap((n) => nodeDependencies(n).map((d) => d.source))
              .filter((id) => !ids.has(id)),
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
      this.#nodeClipboard = {
        nodes: clone(nodes),
        sourceLayerId: layer.id,
        schemaVersion: doc.schemaVersion,
        pasteCount: 0,
        externalNodeIds,
        fonts: Object.fromEntries([...fontIds].map((id) => [id, doc.fontAssets?.[id]])),
        images: Object.fromEntries([...imageIds].map((id) => [id, doc.imageAssets?.[id]])),
      };
      return {
        ...this.state,
        changed: false,
        copiedCount: nodes.length,
        externalInputCount: externalNodeIds.length,
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
    assertVariableDocumentReady(candidate, resolveFonts(candidate, this.#fonts()));
    const revision = this.#history.revision;
    if (!sameDocument(this.#history.doc, candidate))
      this.#history.apply(() => {
        this.#history.doc = candidate;
      });
    if (type === 'paste-nodes' && revision !== this.#history.revision)
      this.#nodeClipboard.pasteCount++;
    if (type === 'import-svg') this.#nodeClipboard = null;
    for (const [id] of arrangementDrafts) this.#textDrafts.delete(id);
    if (committingDraft) this.#textDrafts.delete(payload.draftId);
    return { ...this.state, ...result, changed: revision !== this.#history.revision };
  }
  #commandCandidate(type, payload, before, deferVariable = false) {
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
          { delta: payload.delta },
        ),
        result: {},
      };
    }
    if (['set-os-font-text', 'clear-os-font-text'].includes(type)) {
      const candidate = cloneDocument(before),
        layer = candidate.layers.find((l) => l.id === payload.layerId),
        source = layer?.nodes.find((n) => n.id === payload.nodeId && n.type === 'Text');
      if (!source || source.locked || layer.locked || source.bypassed)
        throw Error('Select an unlocked enabled Text');
      if ([...this.#textDrafts.values()].some((d) => d.nodeId === source.id))
        throw Error('Finish or cancel the Text draft first');
      if (type === 'clear-os-font-text') {
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
            ? desktopOsReference(payload.outline)
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
          desktopTextIntent(next, next.osFont.face);
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
        candidate.schemaVersion = Math.max(
          candidate.schemaVersion,
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
      if (type === 'clear-os-font-text') {
        // Replacing a saved OS appearance must be all-or-nothing. Validation
        // alone checks IDs/types, not coverage or layout in the replacement face.
        const replacement = evaluate(
          { ...layer, output: source.id },
          resolveFonts(candidate, this.#fonts()),
          candidate.imageAssets,
          candidate,
        );
        if (replacement.error)
          throw Error(
            `OS font retained: replacement font cannot render this Text. ${replacement.error}`,
          );
      }
      serialize(candidate);
      return { candidate, result: { nodeId: source.id } };
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
      for (const other of layer.nodes) {
        if (other.id === source.id) continue;
        if (
          nodeDependencies(other).some(
            (dep) =>
              dep.source === source.id &&
              (dep.parameterKey !== undefined ||
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
        ).transformValues(n.id)[payload.key];
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
        ).transformValues(n.id).anchor;
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
    if (type === 'add-svg-part') {
      const candidate = cloneDocument(before);
      const result = appendSVGPart(candidate, payload, resolveFonts(candidate, this.#fonts()));
      return { candidate, result };
    }
    if (type === 'import-svg') {
      const candidate = importSVGTree(payload.source);
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
        result = duplicateLayerGeneric(candidate, payload.layerId);
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
        const size = pngSize(decodeImage(payload.png)),
          assetId = uid('image');
        if (Object.hasOwn(candidate.imageAssets || {}, assetId))
          throw Error('Image identity collision');
        candidate.imageAssets = {
          ...candidate.imageAssets,
          [assetId]: { name: payload.name, mime: 'image/png', ...size, data: payload.png },
        };
        validateImageAssets(candidate.imageAssets);
        const artwork = layer(payload.name),
          generator = node('Image'),
          output = node('Image Output', 380, 40),
          scale = Math.min(1, candidate.width / size.width, candidate.height / size.height);
        generator.asset = assetId;
        generator.params.size.value = [size.width * scale, size.height * scale];
        generator.params.position.value = [
          (candidate.width - generator.params.size.value[0]) / 2,
          (candidate.height - generator.params.size.value[1]) / 2,
        ];
        output.params.path.source = generator.id;
        artwork.nodes = [generator, output];
        artwork.output = output.id;
        candidate.layers.push(artwork);
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
          ).transformValues(transformNode.id)[payload.key],
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
      if (target.nodes.length + clipboard.nodes.length > 2000) throw Error('Too many nodes');
      if (
        clipboard.externalNodeIds.length &&
        (clipboard.sourceLayerId !== target.id ||
          clipboard.externalNodeIds.some((id) => !target.nodes.some((n) => n.id === id)))
      )
        throw Error(
          `Paste needs ${clipboard.externalNodeIds.length} unselected input/reference node(s) in the original Composition. Copy those sources too`,
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
      candidate.schemaVersion = Math.max(candidate.schemaVersion, clipboard.schemaVersion);
      result = {
        nodeIds: copies.map((n) => n.id),
        externalInputCount: clipboard.externalNodeIds.length,
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
          );
        if (Object.hasOwn(settings, 'textWrap'))
          setTextWrap(
            target,
            node.id,
            settings.textWrap,
            resolveFonts(candidate, baseFonts),
            candidate.imageAssets,
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
          if (!['Scalar', 'Vector2'].includes(q.type) || typeof payload.expression !== 'string')
            throw Error('Expected a numeric expression');
          q.expression = payload.expression;
          if (usesBaseValue(q.expression))
            candidate.schemaVersion = Math.max(candidate.schemaVersion, 26);
          if (payload.expression) {
            assertAcyclic(target);
            const evaluation = evaluate(
              target,
              resolveFonts(candidate, this.#fonts()),
              candidate.imageAssets,
              candidate,
            );
            const resolve = (ref) => {
              const referenced = referenceParameter(target, ref);
              const value = evaluation.parameterValue(referenced.id, ref.key);
              return ref.component === undefined ? value : value[ref.component === 'x' ? 0 : 1];
            };
            const value = parameterExpression(q, resolve);
            assertValue(q.type, value);
          }
        } else {
          if (type === 'set-parameter-base' && !['Scalar', 'Vector2'].includes(q.type))
            throw Error('Authored base editing requires a numeric parameter');
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
            ).transformValues(node.id).scaleWidth;
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
        l.nodes.some((n) => n.paintMode || n.type === 'Composition Reference'),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, 31);
    if (
      candidate.layers.some((layer) =>
        layer.nodes.some((n) => n.type === 'Boolean' && n.booleanInputOrder),
      )
    )
      candidate.schemaVersion = Math.max(candidate.schemaVersion, BOOLEAN_INPUT_SCHEMA_VERSION);
    validate(candidate);
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
      for (const repeater of repeaters)
        repeaterSettings((key) => evaluation.parameterValue(repeater.id, key));
    }
    if (deferVariable) return { candidate, result, committingDraft };
    if (isTransform(transformNode))
      evaluate(
        target,
        resolveFonts(candidate, this.#fonts()),
        candidate.imageAssets,
        candidate,
      ).transformValues(transformNode.id);
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
