import { keyedImage } from './image-keyer.js';

export function evaluateKeyer(_operation, read, context) {
  return keyedImage(
    read('path'),
    read('keyColor'),
    read('tolerance'),
    read('softness'),
    context.imageAssets,
  );
}
