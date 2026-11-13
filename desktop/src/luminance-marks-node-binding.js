import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';
import { imageAssetFor } from './image-keyer.js';
import { sampleLuminanceGrid } from './luminance-field.js';
import { sampleImageLuminanceGrid } from './image-luminance-field.js';
import { generateLuminanceMarks } from './luminance-marks.js';
import { prepareLuminanceMask, clipLuminanceMarks } from './luminance-mask.js';
import { readDotSettings, dotFingerprint } from './dither-controls.js';
import { luminanceModifierSettings } from './luminance-modifier-settings.js';

function evaluateMarks(type, mode, operation, read, context) {
  const source = read('path');
  if (!source) throw Error(`Connect a PaintedPath input to ${type}`);
  if (source.kind !== 'image' && (source.fill === null || source.fill === undefined))
    throw Error(`${type} requires a filled source; stroke-only inputs are unsupported`);
  if (source.kind !== 'image') {
    if (!Array.isArray(source.contours) || source.contours.length > 10000)
      throw Error(`${type} exceeds the 100000-vertex geometry budget`);
    let inputVertices = 0;
    for (const contour of source.contours) {
      inputVertices += Array.isArray(contour?.points) ? contour.points.length : 0;
      if (inputVertices > 100000) throw Error(`${type} exceeds the 100000-vertex geometry budget`);
    }
  }
  const { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read),
    origin = { x: gridOrigin[0], y: gridOrigin[1] },
    orientation = mode === 'lines' ? operation : undefined,
    dots = readDotSettings({ type, operation }, read);
  if (mode === 'lines' && !['horizontal', 'vertical'].includes(orientation))
    throw Error('Line Screen orientation must be horizontal or vertical');
  let result;
  if (source.kind === 'image') {
    const field = sampleImageLuminanceGrid(
        imageAssetFor(source, context.imageAssets),
        source.position,
        source.size,
        { pitch, origin, invert, tonePower },
      ),
      [x, y] = source.position,
      [width, height] = source.size,
      mask = prepareLuminanceMask({
        contours: [
          {
            id: `${context.nodeId}:image-mask`,
            closed: true,
            points: [
              [x, y],
              [x + width, y],
              [x + width, y + height],
              [x, y + height],
            ].map((position, index) => ({
              id: `${context.nodeId}:image-mask:${index}`,
              position,
              handleA: [0, 0],
              handleB: [0, 0],
            })),
          },
        ],
      }),
      generated = generateLuminanceMarks({ cells: field.cells, pitch, mode, orientation, ...dots });
    result = clipLuminanceMarks(
      mask,
      generated.marks.map((mark) => mark.vertices),
      fingerprint(context.nodeId),
    );
    result.fill = { hex: '#000000', alpha: 1 };
  } else if (source.contours.length === 0) {
    // Keep a valid empty painted input empty while validating paint and controls.
    sampleLuminanceGrid(
      source.fill,
      { minX: 0, minY: 0, maxX: 0, maxY: 0 },
      { pitch, origin, invert, tonePower },
    );
    result = { contours: [], fill: { hex: '#000000', alpha: 1 } };
  } else {
    const mask = prepareLuminanceMask(source),
      field = sampleLuminanceGrid(source.fill, mask.bounds, { pitch, origin, invert, tonePower }),
      generated = generateLuminanceMarks({ cells: field.cells, pitch, mode, orientation, ...dots }),
      polygons = generated.marks.map((mark) => mark.vertices);
    result = clipLuminanceMarks(mask, polygons, fingerprint(context.nodeId));
    result.fill = { hex: '#000000', alpha: 1 };
  }
  result.unstable = true;
  context.identity.setFingerprint(result, (currentEra) => [
    context.identity.provenance(currentEra),
    mode,
    ...(mode === 'lines' ? [orientation] : []),
    pitch,
    invert,
    // Keep the existing identity for unchanged origin and tone defaults.
    ...(gridOrigin[0] || gridOrigin[1] ? [gridOrigin] : []),
    ...(tonePower !== 1 ? [tonePower] : []),
    ...dotFingerprint(dots, pitch),
    source.fill,
    context.identity.fingerprint(source, currentEra) || null,
    source.kind === 'image'
      ? JSON.stringify([source.kind, source.asset, source.position, source.size])
      : signature(source),
    signature(result),
  ]);
  result.characterOccurrences = routeCharacters(
    source,
    context.nodeId,
    null,
    `Canvas character editing is unavailable through ${type}; edit upstream Text`,
  );
  return result;
}

export const evaluateHalftone = (operation, read, context) =>
  evaluateMarks('Halftone', 'halftone', operation, read, context);
export const evaluateOrderedDither = (operation, read, context) =>
  evaluateMarks('Ordered Dither', 'dither', operation, read, context);
export const evaluateErrorDiffusion = (operation, read, context) =>
  evaluateMarks('Error Diffusion Dither', 'error-diffusion', operation, read, context);
export const evaluateLineScreen = (operation, read, context) =>
  evaluateMarks('Line Screen', 'lines', operation, read, context);
