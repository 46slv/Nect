import { ditheredImage } from './image-keyer.js';
import { readDotSettings } from './dither-controls.js';
import { luminanceModifierSettings } from './luminance-modifier-settings.js';

function evaluateImageDither(type, mode, operation, read, context) {
  const { pitch, invert, gridOrigin, tonePower } = luminanceModifierSettings(read);
  return ditheredImage(
    read('path'),
    {
      mode,
      pitch,
      invert,
      origin: { x: gridOrigin[0], y: gridOrigin[1] },
      tonePower,
      ...readDotSettings({ type, operation }, read),
    },
    context.imageAssets,
  );
}

export const evaluateOrderedImageDither = (operation, read, context) =>
  evaluateImageDither('Ordered Dither Image', 'dither', operation, read, context);
export const evaluateErrorDiffusionImageDither = (operation, read, context) =>
  evaluateImageDither('Error Diffusion Dither Image', 'error-diffusion', operation, read, context);
