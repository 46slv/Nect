import {
  checkImageSize,
  sourceImageSize,
  pngSize,
  encodeImage,
  imageURL,
  MAX_IMAGE_BYTES,
} from './images.js';
import { CSS_PIXELS_PER_INCH } from './artboard-units.js';
/** Browser adapter: core export scene remains DOM-free and renderer-independent. */
export async function loadImage(url) {
  const image = new Image();
  image.src = url;
  try {
    await image.decode();
  } catch {
    throw Error('Image could not be decoded');
  }
  checkImageSize(image.naturalWidth, image.naturalHeight);
  return image;
}
function pngBlob(canvas) {
  return new Promise((resolve, reject) =>
    canvas.toBlob((blob) => {
      if (!blob) reject(Error('PNG encoding failed'));
      else resolve(blob);
    }, 'image/png'),
  );
}
export async function prepareImage(file) {
  if (file.size > MAX_IMAGE_BYTES) throw Error('Image exceeds 12 MB');
  const bytes = await file.arrayBuffer();
  const info = sourceImageSize(bytes); // Bound dimensions before the decoder allocates pixels.
  const url = URL.createObjectURL(new Blob([bytes], { type: info.mime }));
  try {
    const image = await loadImage(url);
    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    canvas.getContext('2d').drawImage(image, 0, 0);
    const blob = await pngBlob(canvas),
      data = await blob.arrayBuffer();
    const size = pngSize(data);
    return { name: file.name.slice(0, 500), mime: 'image/png', ...size, data: encodeImage(data) };
  } finally {
    URL.revokeObjectURL(url);
  }
}
export async function preflightImages(doc) {
  for (const asset of Object.values(doc.imageAssets || {})) {
    const image = await loadImage(imageURL(asset));
    if (image.naturalWidth !== asset.width || image.naturalHeight !== asset.height)
      throw Error('Decoded image dimensions do not match');
  }
}
export function pngDimensions(doc, sizing = 1) {
  if (
    !doc ||
    typeof doc !== 'object' ||
    !Number.isFinite(doc.width) ||
    !Number.isFinite(doc.height) ||
    doc.width <= 0 ||
    doc.height <= 0
  )
    throw Error('PNG source dimensions must be positive and finite');

  let mode, scale;
  if (typeof sizing === 'number') {
    mode = 'scale';
    scale = sizing;
  } else {
    if (!sizing || typeof sizing !== 'object' || Array.isArray(sizing))
      throw Error('Invalid PNG sizing options');
    if (sizing.mode === 'scale') {
      mode = 'scale';
      if (Object.keys(sizing).some((key) => !['mode', 'scale'].includes(key)))
        throw Error('Invalid PNG scale options');
      scale = sizing.scale;
    } else if (sizing.mode === 'ppi') {
      mode = 'ppi';
      if (Object.keys(sizing).some((key) => !['mode', 'ppi'].includes(key)))
        throw Error('Invalid PNG PPI options');
      if (!Number.isFinite(sizing.ppi) || sizing.ppi <= 0)
        throw Error('PNG PPI must be a positive finite number');
      scale = sizing.ppi / CSS_PIXELS_PER_INCH;
      if (!Number.isFinite(scale) || scale <= 0)
        throw Error('PNG PPI is outside the supported range');
    } else throw Error('PNG sizing mode must be scale or ppi');
  }
  if (mode === 'scale' && (!Number.isFinite(scale) || scale < 0.1 || scale > 8))
    throw Error('PNG scale must be between 0.1× and 8×');
  const width = doc.width * scale,
    height = doc.height * scale;
  if (!Number.isFinite(width) || !Number.isFinite(height))
    throw Error('PNG dimensions exceed the supported range');
  return checkImageSize(Math.max(1, Math.round(width)), Math.max(1, Math.round(height)));
}
export async function rasterizeSVG(svg, size) {
  checkImageSize(size.width, size.height);
  const scaled = svg.replace(
    /width="[^"]+" height="[^"]+"/,
    `width="${size.width}" height="${size.height}"`,
  );
  const url = URL.createObjectURL(new Blob([scaled], { type: 'image/svg+xml' }));
  try {
    const image = await loadImage(url),
      canvas = document.createElement('canvas');
    canvas.width = size.width;
    canvas.height = size.height;
    canvas.getContext('2d').drawImage(image, 0, 0, size.width, size.height);
    return await pngBlob(canvas);
  } finally {
    URL.revokeObjectURL(url);
  }
}
