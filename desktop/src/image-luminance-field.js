import { validateTonePower, toneDarkness } from './luminance-tone.js';
import { Inflate } from 'pako/browser/inflate';
import { decodeImage, MAX_IMAGE_BYTES, pngSize } from './images.js';

// Pako 3.0.2's browser/inflate ESM entry is 31,943 published bytes; it is pure
// JavaScript and works in both the browser and the DOM-free Node CLI.
// Pako is a browser/Node-safe JavaScript zlib implementation. Use its streaming
// Inflate API, not inflate(), so an untrusted IDAT stream cannot accumulate an
// unbounded result. `chunkSize` also bounds Pako's one reusable output buffer.
const MAX_IMAGE_EFFECT_PIXELS = 4_000_000;
const MAX_GRID_CELLS = 4096;
const INFLATE_CHUNK_BYTES = 8192;
const MAX_CACHED_RGBA_BYTES = MAX_IMAGE_EFFECT_PIXELS * 4;
const GRID_DEFAULT_PITCH = 16;

// One-entry identity cache: at most one decoded source survives between calls,
// and it can contain no more than 16 MB of RGBA data.
let cachedAsset = null;
let cachedRgba = null;

function failUnsupportedEncoding() {
  throw Error(
    'Unsupported PNG encoding for image luminance; expected noninterlaced 8-bit RGB or RGBA',
  );
}

function parsePng(asset) {
  if (
    !asset ||
    typeof asset !== 'object' ||
    asset.mime !== 'image/png' ||
    !Number.isInteger(asset.width) ||
    !Number.isInteger(asset.height) ||
    typeof asset.data !== 'string'
  )
    throw Error('Image luminance requires a canonical PNG image asset');

  const encoded = decodeImage(asset.data);
  if (encoded.byteLength > MAX_IMAGE_BYTES) throw Error('Image exceeds 12 MB');

  // pngSize verifies the PNG signature, every chunk CRC, dimensions, IDAT and
  // IEND before any decompression is attempted.
  const checkedSize = pngSize(encoded);
  if (asset.width !== checkedSize.width || asset.height !== checkedSize.height)
    throw Error('Image dimensions do not match its PNG');
  if (checkedSize.width * checkedSize.height > MAX_IMAGE_EFFECT_PIXELS)
    throw Error('Image luminance is limited to 4 megapixels per image');

  const view = new DataView(encoded.buffer, encoded.byteOffset, encoded.byteLength);
  let at = 8,
    sawHeader = false,
    sawData = false,
    endedData = false,
    sawSrgb = false,
    sawPhys = false,
    channels = 0,
    idatBytes = 0;
  // At most the already-validated encoded PNG length is retained for IDAT.
  // Avoid a JS array of chunk references, which would be costly for an image
  // split across many tiny valid chunks.
  const compressed = new Uint8Array(encoded.length);

  while (at + 12 <= encoded.length) {
    const length = view.getUint32(at),
      tag = String.fromCharCode(...encoded.subarray(at + 4, at + 8)),
      dataStart = at + 8,
      next = dataStart + length + 4;
    if (next > encoded.length) throw Error('Truncated PNG');

    if (!sawHeader) {
      if (tag !== 'IHDR' || length !== 13) throw Error('Invalid PNG header');
      sawHeader = true;
      const bitDepth = encoded[dataStart + 8],
        colorType = encoded[dataStart + 9],
        compression = encoded[dataStart + 10],
        filter = encoded[dataStart + 11],
        interlace = encoded[dataStart + 12];
      if (
        bitDepth !== 8 ||
        ![2, 6].includes(colorType) ||
        compression !== 0 ||
        filter !== 0 ||
        interlace !== 0
      )
        failUnsupportedEncoding();
      channels = colorType === 2 ? 3 : 4;
    } else if (tag === 'IHDR') {
      throw Error('Duplicate PNG header');
    } else if (tag === 'IDAT') {
      if (endedData) throw Error('PNG IDAT chunks must be contiguous');
      sawData = true;
      if (idatBytes + length > MAX_IMAGE_BYTES) throw Error('PNG compressed data exceeds 12 MB');
      compressed.set(encoded.subarray(dataStart, dataStart + length), idatBytes);
      idatBytes += length;
    } else if (tag === 'IEND') {
      if (!sawData || length !== 0 || next !== encoded.length) throw Error('Incomplete PNG');
      break;
    } else {
      if (sawData) endedData = true;
      // These two ancillary chunks are metadata only; sRGB agrees with the
      // sampling model, and pHYs does not change pixel-channel interpretation.
      // Reject color transforms, palette/transparency chunks and unknown
      // metadata rather than silently changing their semantics.
      if (tag === 'sRGB') {
        if (sawData || sawSrgb || length !== 1 || encoded[dataStart] > 3)
          throw Error('Invalid or misplaced PNG sRGB chunk');
        sawSrgb = true;
      } else if (tag === 'pHYs') {
        if (sawData || sawPhys || length !== 9 || encoded[dataStart + 8] > 1)
          throw Error('Invalid or misplaced PNG pHYs chunk');
        sawPhys = true;
      } else {
        failUnsupportedEncoding();
      }
    }
    at = next;
  }

  if (!sawHeader || !sawData || idatBytes === 0) throw Error('Incomplete PNG');
  return {
    encodedWidth: checkedSize.width,
    encodedHeight: checkedSize.height,
    channels,
    compressed: compressed.subarray(0, idatBytes),
  };
}

function paethPredictor(left, up, upLeft) {
  const estimate = left + up - upLeft,
    leftDistance = Math.abs(estimate - left),
    upDistance = Math.abs(estimate - up),
    upLeftDistance = Math.abs(estimate - upLeft);
  if (leftDistance <= upDistance && leftDistance <= upLeftDistance) return left;
  if (upDistance <= upLeftDistance) return up;
  return upLeft;
}

function decodeRgba(asset) {
  const { encodedWidth: width, encodedHeight: height, channels, compressed } = parsePng(asset),
    pixelCount = width * height,
    rgbaBytes = pixelCount * 4,
    rowBytes = width * channels,
    expectedInflatedBytes = (rowBytes + 1) * height;
  if (
    !Number.isSafeInteger(expectedInflatedBytes) ||
    expectedInflatedBytes > MAX_IMAGE_EFFECT_PIXELS * 5
  )
    throw Error('PNG decompression exceeds the 4 megapixel image-luminance limit');

  const rgba = new Uint8Array(rgbaBytes);
  let row = new Uint8Array(rowBytes),
    previous = new Uint8Array(rowBytes);
  let inflatedBytes = 0,
    rowPosition = 0,
    rowDataPosition = 0,
    filterType = -1,
    rowIndex = 0;

  const consumeInflated = (chunk) => {
    if (inflatedBytes + chunk.length > expectedInflatedBytes)
      throw Error('PNG decompression exceeds its declared image dimensions');
    inflatedBytes += chunk.length;
    let inputAt = 0;
    while (inputAt < chunk.length) {
      if (rowPosition === 0) {
        filterType = chunk[inputAt++];
        rowPosition = 1;
        if (filterType > 4) throw Error('Invalid PNG scanline filter');
        if (inputAt === chunk.length) break;
      }

      const take = Math.min(rowBytes - rowDataPosition, chunk.length - inputAt);
      row.set(chunk.subarray(inputAt, inputAt + take), rowDataPosition);
      rowDataPosition += take;
      inputAt += take;
      if (rowDataPosition !== rowBytes) continue;

      for (let x = 0; x < rowBytes; x++) {
        const encoded = row[x],
          left = x >= channels ? row[x - channels] : 0,
          up = previous[x],
          upLeft = x >= channels ? previous[x - channels] : 0;
        if (filterType === 1) row[x] = (encoded + left) & 255;
        else if (filterType === 2) row[x] = (encoded + up) & 255;
        else if (filterType === 3) row[x] = (encoded + Math.floor((left + up) / 2)) & 255;
        else if (filterType === 4) row[x] = (encoded + paethPredictor(left, up, upLeft)) & 255;
      }

      let sourceAt = 0,
        targetAt = rowIndex * width * 4;
      for (let x = 0; x < width; x++) {
        rgba[targetAt++] = row[sourceAt++];
        rgba[targetAt++] = row[sourceAt++];
        rgba[targetAt++] = row[sourceAt++];
        rgba[targetAt++] = channels === 4 ? row[sourceAt++] : 255;
      }
      const recycled = previous;
      previous = row;
      row = recycled;
      rowDataPosition = 0;
      rowPosition = 0;
      filterType = -1;
      rowIndex++;
    }
  };

  const inflater = new Inflate({ windowBits: 15, chunkSize: INFLATE_CHUNK_BYTES });
  inflater.onData = consumeInflated;
  try {
    const success = inflater.push(compressed, true);
    if (!success || inflater.err !== 0) throw Error('Invalid PNG compressed image data');
    // Pako's Inflate exposes `ended`; `strm` is private in declarations but is
    // present in its runtime ESM. Check remaining input so a PNG cannot smuggle
    // trailing bytes beyond the single expected zlib stream.
    if (!inflater.ended || inflater.strm.avail_in !== 0)
      throw Error('Invalid PNG compressed image data');
  } catch (error) {
    if (error instanceof Error && /PNG decompression exceeds|scanline filter/.test(error.message))
      throw error;
    throw Error('Invalid PNG compressed image data');
  }

  if (
    inflatedBytes !== expectedInflatedBytes ||
    rowIndex !== height ||
    rowPosition !== 0 ||
    rowDataPosition !== 0
  )
    throw Error('PNG decompressed dimensions do not match its header');
  if (rgba.byteLength > MAX_CACHED_RGBA_BYTES)
    throw Error('Decoded image exceeds the 16 MB luminance cache limit');
  return rgba;
}

function rgbaFor(asset) {
  if (asset === cachedAsset && cachedRgba) return cachedRgba;
  const decoded = decodeRgba(asset);
  cachedAsset = asset;
  cachedRgba = decoded;
  return decoded;
}

function hasOnlyKeys(value, keys) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !Array.isArray(value) &&
    Reflect.ownKeys(value).every((key) => keys.includes(key))
  );
}

function copyOrigin(value) {
  if (
    value === null ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Reflect.ownKeys(value).length !== 2 ||
    !Object.hasOwn(value, 'x') ||
    !Object.hasOwn(value, 'y') ||
    !Number.isFinite(value.x) ||
    !Number.isFinite(value.y)
  )
    throw Error('Expected finite grid origin with x and y');
  return { x: value.x, y: value.y };
}

function gridIndexRange(min, max, origin, pitch) {
  const from = (min - origin) / pitch,
    to = (max - origin) / pitch;
  if (!Number.isFinite(from) || !Number.isFinite(to))
    throw Error('Grid bounds produce indices outside the supported numeric range');
  const firstValue = Math.floor(from),
    lastValue = Math.ceil(to) - 1,
    first = firstValue === 0 ? 0 : firstValue,
    last = lastValue === 0 ? 0 : lastValue;
  if (first > last) return { first: 0, last: -1, count: 0 };
  if (!Number.isSafeInteger(first) || !Number.isSafeInteger(last))
    throw Error('Grid bounds produce indices outside the supported numeric range');
  const count = last - first + 1;
  if (!Number.isSafeInteger(count)) throw Error('Grid bounds produce too many cells');
  return { first, last, count };
}

function linearizeSrgb(channel) {
  const encoded = channel / 255;
  return encoded <= 0.04045 ? encoded / 12.92 : ((encoded + 0.055) / 1.055) ** 2.4;
}

function pixelLuminance(rgba, width, x, y) {
  const offset = (y * width + x) * 4,
    alpha = rgba[offset + 3] / 255,
    red = alpha * linearizeSrgb(rgba[offset]) + (1 - alpha),
    green = alpha * linearizeSrgb(rgba[offset + 1]) + (1 - alpha),
    blue = alpha * linearizeSrgb(rgba[offset + 2]) + (1 - alpha);
  return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

/**
 * Sample an embedded PNG at existing world-grid cell centers. The decoded
 * runtime cache is keyed only by immutable asset-object identity and stores a
 * single RGBA image (max 16 MB); pixel data is never returned or serialized.
 */
export function sampleImageLuminanceGrid(asset, position, size, options = {}) {
  if (!hasOnlyKeys(options, ['pitch', 'origin', 'invert', 'tonePower']))
    throw Error('Grid options may contain only pitch, origin, invert and tonePower');
  if (
    !Array.isArray(position) ||
    position.length !== 2 ||
    !position.every(Number.isFinite) ||
    !Array.isArray(size) ||
    size.length !== 2 ||
    !size.every(Number.isFinite) ||
    size.some((value) => value <= 0 || value > 100000)
  )
    throw Error('Image position and size must be finite points with positive bounded size');

  const [minX, minY] = position,
    [widthOnCanvas, heightOnCanvas] = size,
    maxX = minX + widthOnCanvas,
    maxY = minY + heightOnCanvas;
  if (!Number.isFinite(maxX) || !Number.isFinite(maxY) || maxX <= minX || maxY <= minY)
    throw Error('Image placement must have finite positive area');

  const pitch = options.pitch === undefined ? GRID_DEFAULT_PITCH : options.pitch,
    origin = options.origin === undefined ? { x: 0, y: 0 } : copyOrigin(options.origin),
    invert = options.invert === undefined ? false : options.invert,
    tonePower = validateTonePower(options.tonePower);
  if (!Number.isFinite(pitch) || pitch < 1 || pitch > 1024)
    throw Error('Grid pitch must be finite and from 1 to 1024');
  if (typeof invert !== 'boolean') throw Error('Grid invert option must be boolean');

  const columns = gridIndexRange(minX, maxX, origin.x, pitch),
    rows = gridIndexRange(minY, maxY, origin.y, pitch);
  if (
    columns.count > MAX_GRID_CELLS ||
    rows.count > MAX_GRID_CELLS ||
    columns.count * rows.count > MAX_GRID_CELLS
  )
    throw Error(`Luminance grid exceeds the ${MAX_GRID_CELLS}-cell limit`);

  // Validate and decode only after all world-grid bounds have passed their
  // small work budget; unrelated asset objects are never touched.
  const rgba = rgbaFor(asset),
    imageWidth = asset.width,
    imageHeight = asset.height,
    cells = [];
  for (let j = rows.first; j <= rows.last; j++) {
    const y = origin.y + (j + 0.5) * pitch;
    for (let i = columns.first; i <= columns.last; i++) {
      const x = origin.x + (i + 0.5) * pitch;
      if (!Number.isFinite(x) || !Number.isFinite(y))
        throw Error('Grid bounds produce coordinates outside the supported numeric range');

      // Any cell included here has positive-area intersection with the image.
      // Extend the nearest edge texel to a center outside the placed rectangle;
      // mark geometry is clipped to that rectangle by the caller.
      const texelX =
          x <= minX
            ? 0
            : x >= maxX
              ? imageWidth - 1
              : Math.max(
                  0,
                  Math.min(imageWidth - 1, Math.floor(((x - minX) / widthOnCanvas) * imageWidth)),
                ),
        texelY =
          y <= minY
            ? 0
            : y >= maxY
              ? imageHeight - 1
              : Math.max(
                  0,
                  Math.min(
                    imageHeight - 1,
                    Math.floor(((y - minY) / heightOnCanvas) * imageHeight),
                  ),
                ),
        luminance = pixelLuminance(rgba, imageWidth, texelX, texelY);
      cells.push({ i, j, x, y, luminance, darkness: toneDarkness(luminance, invert, tonePower) });
    }
  }
  return { pitch, origin, bounds: { minX, minY, maxX, maxY }, cells };
}
