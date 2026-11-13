import { fingerprint } from './fingerprint.js';
import { imageAssetFor } from './image-keyer.js';
import { luminanceInstanceSettings } from './luminance-instances.js';
import { luminanceModifierSettings } from './luminance-modifier-settings.js';
import { luminanceToPoints } from './luminance-points.js';

export function evaluateLuminancePoints(_operation, read, context) {
  const source = read('path'),
    { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read),
    settings = luminanceInstanceSettings(read),
    result = luminanceToPoints(source, {
      ...settings,
      pitch,
      invert,
      tonePower,
      origin: { x: gridOrigin[0], y: gridOrigin[1] },
      prefix: fingerprint(context.nodeId),
      asset: source?.kind === 'image' ? imageAssetFor(source, context.imageAssets) : undefined,
    });
  result.unstable = true;
  context.identity.setFingerprint(result, (currentEra) => [
    context.identity.provenance(currentEra),
    context.identity.fingerprint(source, currentEra) || null,
    settings,
    pitch,
    invert,
    gridOrigin,
    tonePower,
    result.contours,
  ]);
  return result;
}
