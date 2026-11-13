import { fingerprint } from './fingerprint.js';
import { signature } from './path-signature.js';
import { routeCharacters } from './character-geometry.js';
import { imageAssetFor } from './image-keyer.js';
import { sampleLuminanceGrid } from './luminance-field.js';
import { sampleImageLuminanceGrid } from './image-luminance-field.js';
import { prepareLuminanceMask } from './luminance-mask.js';
import { validateInstancePath } from './instance-points-geometry.js';
import { luminanceModifierSettings } from './luminance-modifier-settings.js';
import {
  luminanceInstanceSettings,
  luminanceInstancesGeometry,
  selectLuminanceCenters,
} from './luminance-instances.js';

export function evaluateLuminanceInstances(_operation, read, context) {
  if (!context.hasConnectedInput('path') || !context.hasConnectedInput('motif'))
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
      imageAssetFor(source, context.imageAssets),
      source.position,
      source.size,
      options,
    );
  } else {
    if (!source.fill) throw Error('Luminance Instances requires a filled source');
    if (source.contours?.length === 0)
      field = sampleLuminanceGrid(source.fill, { minX: 0, minY: 0, maxX: 0, maxY: 0 }, options);
    else {
      mask = prepareLuminanceMask(source);
      field = sampleLuminanceGrid(source.fill, mask.bounds, options);
    }
  }
  const cells = selectLuminanceCenters(field.cells, mask, source.kind === 'image' ? source : null);
  // Validate cardinality and transformed options before expensive exact bounds work.
  const prefix = fingerprint(context.nodeId);
  luminanceInstancesGeometry(motif, cells, { ...settings, pivot: [0, 0], prefix });
  const bounds = motifInfo.pointCount ? context.pathBounds(motif) : null,
    result = luminanceInstancesGeometry(motif, cells, {
      ...settings,
      pivot: bounds ? [bounds[0][1], bounds[1][1]] : [0, 0],
      prefix,
    });
  delete result.fill;
  delete result.stroke;
  result.characterOccurrences = routeCharacters(
    motif,
    context.nodeId,
    null,
    'Canvas character editing is unavailable through Luminance Instances; edit the source Text',
  );
  result.unstable = true;
  context.identity.setFingerprint(result, (currentEra) => [
    context.identity.provenance(currentEra),
    context.identity.fingerprint(source, currentEra) || null,
    context.identity.fingerprint(motif, currentEra) || null,
    settings,
    pitch,
    invert,
    gridOrigin,
    ...(tonePower !== 1 ? [tonePower] : []),
    cells,
    signature(motif),
    signature(result),
  ]);
  return result;
}
