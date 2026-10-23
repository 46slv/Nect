import {
  checkImageSize,
  sourceImageSize,
  pngSize,
  encodeImage,
  imageURL,
  MAX_IMAGE_BYTES,
  crc32,
} from './images.js';
import { CSS_PIXELS_PER_INCH } from './artboard-units.js';
const PNG_SIGNATURE = Uint8Array.of(137, 80, 78, 71, 13, 10, 26, 10);
const MAX_PNG_DENSITY = 0xffffffff;
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
export function pngPixelsPerMetre(ppi) {
  if (!Number.isFinite(ppi) || ppi <= 0) throw Error('PNG PPI must be a positive finite number');
  const density = Math.round((ppi * 1000) / 25.4);
  if (!Number.isSafeInteger(density) || density < 1 || density > MAX_PNG_DENSITY)
    throw Error('PNG PPI cannot be represented in density metadata');
  return density;
}
function pngDensityChunk(pixelsPerMetre) {
  const chunk = new Uint8Array(21),
    view = new DataView(chunk.buffer);
  view.setUint32(0, 9);
  chunk.set([112, 72, 89, 115], 4); // pHYs
  view.setUint32(8, pixelsPerMetre);
  view.setUint32(12, pixelsPerMetre);
  chunk[16] = 1; // Pixels per metre.
  view.setUint32(17, crc32(chunk, 4, 17));
  return chunk;
}
/** Replace canvas density metadata without changing encoded image samples. */
export function withPngPpiMetadata(bytes, ppi) {
  const pixelsPerMetre = pngPixelsPerMetre(ppi),
    source = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (
    source.length < PNG_SIGNATURE.length ||
    !PNG_SIGNATURE.every((value, i) => source[i] === value)
  )
    throw Error('Invalid PNG for density metadata');

  const view = new DataView(source.buffer, source.byteOffset, source.byteLength),
    chunks = [];
  let offset = PNG_SIGNATURE.length,
    hasHeader = false,
    hasImageData = false,
    hasEnd = false,
    hasDensity = false,
    inserted = false;
  while (offset < source.length) {
    if (offset + 12 > source.length) throw Error('Truncated PNG');
    const length = view.getUint32(offset),
      end = offset + length + 12;
    if (end > source.length) throw Error('Truncated PNG');
    const type = String.fromCharCode(...source.subarray(offset + 4, offset + 8));
    if (crc32(source, offset + 4, offset + 8 + length) !== view.getUint32(offset + 8 + length))
      throw Error('PNG checksum mismatch; image is corrupt');

    if (!hasHeader) {
      if (type !== 'IHDR' || length !== 13) throw Error('Invalid PNG header');
      hasHeader = true;
    } else if (type === 'IHDR') throw Error('Duplicate PNG header');

    if (type === 'pHYs') {
      if (hasDensity || hasImageData || length !== 9 || source[offset + 16] > 1)
        throw Error('Invalid or misplaced PNG pHYs chunk');
      hasDensity = true;
    } else {
      if (type === 'IDAT') {
        if (!inserted) {
          chunks.push(pngDensityChunk(pixelsPerMetre));
          inserted = true;
        }
        hasImageData = true;
      }
      if (type === 'IEND') {
        if (length !== 0 || !hasImageData || end !== source.length)
          throw Error('Invalid PNG end chunk');
        hasEnd = true;
      }
      chunks.push(source.subarray(offset, end));
    }
    offset = end;
    if (hasEnd) break;
  }
  if (!hasHeader || !hasImageData || !hasEnd || offset !== source.length || !inserted)
    throw Error('Incomplete PNG');

  const outputLength =
      PNG_SIGNATURE.length + chunks.reduce((size, chunk) => size + chunk.length, 0),
    output = new Uint8Array(outputLength);
  output.set(PNG_SIGNATURE);
  let at = PNG_SIGNATURE.length;
  for (const chunk of chunks) {
    output.set(chunk, at);
    at += chunk.length;
  }
  return output;
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
      pngPixelsPerMetre(sizing.ppi);
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
export async function rasterizeSVG(svg, size, ppi = null) {
  checkImageSize(size.width, size.height);
  if (ppi !== null) pngPixelsPerMetre(ppi);
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
    const png = await pngBlob(canvas);
    if (ppi === null) return png;
    return new Blob([withPngPpiMetadata(await png.arrayBuffer(), ppi)], { type: 'image/png' });
  } finally {
    URL.revokeObjectURL(url);
  }
}
