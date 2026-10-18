import { Deflate } from 'pako/browser/deflate';
import { crc32, encodeImage, MAX_IMAGE_BYTES, checkImageSize } from './images.js';
import { copyImageRgba } from './image-luminance-field.js';

export const MAX_KEYER_PIXELS = 4_000_000;
export const MAX_ACTIVE_KEYERS = 8;
export function keyerSettings(keyColor, tolerance, softness) {
  if (
    !keyColor ||
    typeof keyColor.hex !== 'string' ||
    !/^#[0-9a-f]{6}$/i.test(keyColor.hex) ||
    ![tolerance, softness].every((v) => Number.isFinite(v) && v >= 0 && v <= 1)
  )
    throw Error('Keyer needs an RGB key and range/softness from 0 to 1');
  return {
    type: 'keyer',
    keyColor: { hex: keyColor.hex.toLowerCase(), alpha: 1 },
    tolerance,
    softness,
  };
}
/** Straight RGB is never premultiplied. Zero-alpha RGB is sanitized for resampling. */
export function keyRgba(source, settings) {
  const s = keyerSettings(settings.keyColor, settings.tolerance, settings.softness);
  if (!(source instanceof Uint8Array) || source.length % 4 || source.length > MAX_KEYER_PIXELS * 4)
    throw Error('Keyer is limited to 4 megapixels');
  const rgba = source.slice(),
    key = [1, 3, 5].map((i) => parseInt(s.keyColor.hex.slice(i, i + 2), 16)),
    divisor = 255 * Math.sqrt(3);
  for (let i = 0; i < rgba.length; i += 4) {
    const distance =
        Math.hypot(rgba[i] - key[0], rgba[i + 1] - key[1], rgba[i + 2] - key[2]) / divisor,
      keep =
        s.softness === 0
          ? distance <= s.tolerance
            ? 0
            : 1
          : Math.max(0, Math.min(1, (distance - s.tolerance) / s.softness));
    rgba[i + 3] = Math.round(rgba[i + 3] * keep);
    if (rgba[i + 3] === 0) rgba[i] = rgba[i + 1] = rgba[i + 2] = 0;
  }
  return rgba;
}
export function encodeRgbaPng(width, height, rgba) {
  checkImageSize(width, height);
  if (
    width * height > MAX_KEYER_PIXELS ||
    !(rgba instanceof Uint8Array) ||
    rgba.length !== width * height * 4
  )
    throw Error('Image processing requires RGBA with at most 4 megapixels');
  const compressed = [],
    deflater = new Deflate({ level: 1, chunkSize: 8192 });
  let length = 0;
  deflater.onData = (chunk) => {
    length += chunk.length;
    if (length + 57 > MAX_IMAGE_BYTES) throw Error('Processed PNG exceeds 12 MB');
    compressed.push(chunk.slice());
  };
  const row = new Uint8Array(width * 4 + 1);
  for (let y = 0; y < height; y++) {
    row.set(rgba.subarray(y * width * 4, (y + 1) * width * 4), 1);
    if (!deflater.push(row, y === height - 1) || deflater.err) throw Error('PNG encoding failed');
  }
  const png = new Uint8Array(length + 57),
    view = new DataView(png.buffer);
  png.set([137, 80, 78, 71, 13, 10, 26, 10]);
  let at = 8;
  const chunk = (tag, data) => {
    view.setUint32(at, data.length);
    for (let i = 0; i < 4; i++) png[at + 4 + i] = tag.charCodeAt(i);
    png.set(data, at + 8);
    view.setUint32(at + 8 + data.length, crc32(png, at + 4, at + 8 + data.length));
    at += data.length + 12;
  };
  const header = new Uint8Array(13),
    h = new DataView(header.buffer);
  h.setUint32(0, width);
  h.setUint32(4, height);
  header[8] = 8;
  header[9] = 6;
  chunk('IHDR', header);
  const idat = new Uint8Array(length);
  let offset = 0;
  for (const part of compressed) {
    idat.set(part, offset);
    offset += part.length;
  }
  chunk('IDAT', idat);
  chunk('IEND', new Uint8Array());
  return png;
}
// Evaluated Images retain only recipes. One compressed derived asset survives;
// graph caches never clone a PNG for every downstream node or revision.
let cached = null;
export function imageAssetFor(value, assets = {}) {
  const original = assets[value?.asset];
  if (!original) throw Error('Missing image asset');
  const effects = value.imageEffects;
  if (effects === undefined) return original;
  if (!Array.isArray(effects) || effects.length > MAX_ACTIVE_KEYERS)
    throw Error('Image effect chain exceeds 8 Keyers');
  const normalized = effects.map((e) => {
      if (
        !e ||
        e.type !== 'keyer' ||
        Object.keys(e).some((k) => !['type', 'keyColor', 'tolerance', 'softness'].includes(k))
      )
        throw Error('Invalid image effect recipe');
      return keyerSettings(e.keyColor, e.tolerance, e.softness);
    }),
    keys = normalized.map((e) => JSON.stringify(e));
  let start = 0,
    output = original;
  if (
    cached?.source === original &&
    cached.data === original.data &&
    cached.keys.length <= keys.length &&
    cached.keys.every((k, i) => keys[i] === k)
  ) {
    start = cached.keys.length;
    output = cached.output;
  }
  for (let i = start; i < normalized.length; i++) {
    const pixels = copyImageRgba(output),
      rgba = keyRgba(pixels, normalized[i]),
      png = encodeRgbaPng(original.width, original.height, rgba);
    output = Object.freeze({
      name: original.name,
      mime: 'image/png',
      width: original.width,
      height: original.height,
      data: encodeImage(png),
    });
  }
  cached = { source: original, data: original.data, keys, output };
  return output;
}
export function keyedImage(source, keyColor, tolerance, softness, assets) {
  if (source?.kind !== 'image') throw Error('Connect an Image to Keyer');
  const result = {
    ...source,
    imageEffects: [...(source.imageEffects || []), keyerSettings(keyColor, tolerance, softness)],
  };
  imageAssetFor(result, assets);
  return result;
}
