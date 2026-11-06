// Internal Sample-domain image luminance evaluation. This is not a saved node,
// an implicit Space conversion, or a replacement for legacy center sampling.
const LABEL = 'Image Sample Luminance v1';
const MAX_TOKEN_LENGTH = 256;
const MAX_IMAGE_PIXELS = 4_000_000;
const MAX_SAMPLES = 65_536;
const MAX_PIXEL_VISITS = 16_000_000;

const typedArrayPrototype = Object.getPrototypeOf(Uint8Array.prototype);
const typedArrayBuffer = Object.getOwnPropertyDescriptor(typedArrayPrototype, 'buffer').get;
const typedArrayByteLength = Object.getOwnPropertyDescriptor(typedArrayPrototype, 'byteLength').get;
const arrayBufferResizable = Object.getOwnPropertyDescriptor(
  ArrayBuffer.prototype,
  'resizable',
)?.get;

function captureData(value, keys) {
  if (
    value === null ||
    typeof value !== 'object' ||
    Array.isArray(value) ||
    Object.getPrototypeOf(value) !== Object.prototype
  )
    return null;
  const present = Reflect.ownKeys(value);
  if (present.length !== keys.length || present.some((key) => !keys.includes(key))) return null;
  const result = Object.create(null);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor || !Object.hasOwn(descriptor, 'value')) return null;
    result[key] = descriptor.value;
  }
  return result;
}

function appendData(array, value) {
  Object.defineProperty(array, array.length, {
    value,
    writable: true,
    enumerable: true,
    configurable: true,
  });
}

function captureArray(value, expectedLength) {
  if (!Array.isArray(value) || Object.getPrototypeOf(value) !== Array.prototype) return null;
  const length = Object.getOwnPropertyDescriptor(value, 'length');
  if (!length || !Object.hasOwn(length, 'value') || length.value !== expectedLength) return null;
  if (Reflect.ownKeys(value).length !== expectedLength + 1) return null;
  const result = [];
  for (let i = 0; i < expectedLength; i++) {
    const descriptor = Object.getOwnPropertyDescriptor(value, String(i));
    if (!descriptor || !Object.hasOwn(descriptor, 'value')) return null;
    appendData(result, descriptor.value);
  }
  return result;
}

function finiteArray(value, length) {
  const result = captureArray(value, length);
  if (!result) return null;
  for (let i = 0; i < length; i++) if (!Number.isFinite(result[i])) return null;
  return result;
}

function token(value) {
  return typeof value === 'string' && value.length > 0 && value.length <= MAX_TOKEN_LENGTH;
}

function imageBytes(value, expectedLength) {
  if (!ArrayBuffer.isView(value) || Object.getPrototypeOf(value) !== Uint8Array.prototype)
    return null;
  const buffer = typedArrayBuffer.call(value);
  if (Object.getPrototypeOf(buffer) !== ArrayBuffer.prototype) return null;
  if (arrayBufferResizable && arrayBufferResizable.call(buffer)) return null;
  return typedArrayByteLength.call(value) === expectedLength ? value : null;
}

function rectangle(centerInput, basisInput, matrix, width, height) {
  const center = finiteArray(centerInput, 2),
    basis = captureArray(basisInput, 2),
    u = basis && finiteArray(basis[0], 2),
    v = basis && finiteArray(basis[1], 2);
  if (!center || !u || !v) throw Error(`${LABEL} requires finite centers and full basis vectors`);
  const a = matrix[0],
    b = matrix[1],
    c = matrix[2],
    d = matrix[3],
    e = matrix[4],
    f = matrix[5],
    x = a * center[0] + c * center[1] + e,
    y = b * center[0] + d * center[1] + f,
    ux = a * u[0] + c * u[1],
    uy = b * u[0] + d * u[1],
    vx = a * v[0] + c * v[1],
    vy = b * v[0] + d * v[1];
  if (![x, y, ux, uy, vx, vy].every(Number.isFinite))
    throw Error(`${LABEL} transform exceeds finite coordinates`);
  if (
    !(
      (ux !== 0 && uy === 0 && vx === 0 && vy !== 0) ||
      (ux === 0 && uy !== 0 && vx !== 0 && vy === 0)
    )
  )
    throw Error(`${LABEL} supports only nondegenerate axis-aligned image footprints`);
  const halfWidth = (Math.abs(ux) + Math.abs(vx)) / 2,
    halfHeight = (Math.abs(uy) + Math.abs(vy)) / 2,
    minX = x - halfWidth,
    maxX = x + halfWidth,
    minY = y - halfHeight,
    maxY = y + halfHeight,
    spanX = maxX - minX,
    spanY = maxY - minY;
  if (![minX, maxX, minY, maxY, spanX, spanY].every(Number.isFinite) || spanX <= 0 || spanY <= 0)
    throw Error(`${LABEL} footprint exceeds finite representable coordinates`);
  const x0 = Math.floor(Math.max(0, Math.min(width, minX))),
    x1 = Math.ceil(Math.max(0, Math.min(width, maxX))),
    y0 = Math.floor(Math.max(0, Math.min(height, minY))),
    y1 = Math.ceil(Math.max(0, Math.min(height, maxY)));
  return { minX, maxX, minY, maxY, spanX, spanY, x0, x1, y0, y1 };
}

function linearize(byte) {
  const channel = byte / 255;
  return channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4;
}

function clampUnit(value) {
  return Math.max(0, Math.min(1, value));
}

function sampleRectangle(rect, bytes, width) {
  let red = 0,
    green = 0,
    blue = 0,
    alpha = 0;
  for (let y = rect.y0; y < rect.y1; y++) {
    const wy = (Math.min(y + 1, rect.maxY) - Math.max(y, rect.minY)) / rect.spanY;
    for (let x = rect.x0; x < rect.x1; x++) {
      const wx = (Math.min(x + 1, rect.maxX) - Math.max(x, rect.minX)) / rect.spanX,
        offset = (y * width + x) * 4,
        weightedAlpha = wx * wy * (bytes[offset + 3] / 255);
      alpha += weightedAlpha;
      red += weightedAlpha * linearize(bytes[offset]);
      green += weightedAlpha * linearize(bytes[offset + 1]);
      blue += weightedAlpha * linearize(bytes[offset + 2]);
    }
  }
  const background = 1 - clampUnit(alpha);
  return (
    0.2126 * clampUnit(red + background) +
    0.7152 * clampUnit(green + background) +
    0.0722 * clampUnit(blue + background)
  );
}

/** Evaluate explicit image cell averages against a caller-owned Sample population. */
export function evaluateImageSampleLuminanceV1(fieldInput, contextInput) {
  const field = captureData(fieldInput, [
      'kind',
      'version',
      'valueType',
      'role',
      'unit',
      'sourceId',
      'sourceRevision',
      'sampling',
      'edge',
      'alpha',
      'transform',
    ]),
    context = captureData(contextInput, [
      'domain',
      'count',
      'maxSamples',
      'maxPixelVisits',
      'image',
      'footprint',
    ]);
  if (
    !field ||
    field.kind !== 'ImageLuminance' ||
    field.version !== 1 ||
    field.valueType !== 'Scalar' ||
    !['luminance', 'darkness'].includes(field.role) ||
    field.unit !== null ||
    !token(field.sourceId) ||
    !token(field.sourceRevision) ||
    field.sampling !== 'cell-average' ||
    field.edge !== 'transparent' ||
    field.alpha !== 'composite-over-white'
  )
    throw Error(`${LABEL} requires a supported explicit descriptor`);
  if (
    !context ||
    context.domain !== 'Sample' ||
    !Number.isSafeInteger(context.count) ||
    context.count < 0 ||
    context.count > MAX_SAMPLES ||
    !Number.isSafeInteger(context.maxSamples) ||
    context.maxSamples < context.count ||
    !Number.isSafeInteger(context.maxPixelVisits) ||
    context.maxPixelVisits < 0 ||
    context.maxPixelVisits > MAX_PIXEL_VISITS
  )
    throw Error(`${LABEL} exceeds the Sample or work budget`);
  const transform = captureData(field.transform, ['fromFrame', 'to', 'matrix']),
    footprint = captureData(context.footprint, ['frameToken', 'centers', 'bases']),
    image = captureData(context.image, ['sourceId', 'sourceRevision', 'width', 'height', 'rgba']);
  if (
    !transform ||
    !footprint ||
    !token(transform.fromFrame) ||
    transform.fromFrame !== footprint.frameToken ||
    transform.to !== 'image-pixel-v1'
  )
    throw Error(`${LABEL} requires matching explicit frame and Image Pixel transform`);
  const matrix = finiteArray(transform.matrix, 6);
  if (!matrix) throw Error(`${LABEL} requires a finite affine transform`);
  if (
    !image ||
    image.sourceId !== field.sourceId ||
    image.sourceRevision !== field.sourceRevision ||
    !Number.isSafeInteger(image.width) ||
    !Number.isSafeInteger(image.height) ||
    image.width <= 0 ||
    image.height <= 0 ||
    image.width > Math.floor(MAX_IMAGE_PIXELS / image.height)
  )
    throw Error(`${LABEL} requires a matching bounded image source`);
  const rgba = imageBytes(image.rgba, image.width * image.height * 4);
  if (!rgba) throw Error(`${LABEL} requires fixed, non-shared RGBA8 data`);
  const centers = captureArray(footprint.centers, context.count),
    bases = captureArray(footprint.bases, context.count);
  if (!centers || !bases) throw Error(`${LABEL} requires a full footprint for each Sample`);
  const rectangles = [];
  let visits = 0;
  for (let i = 0; i < context.count; i++) {
    const rect = rectangle(centers[i], bases[i], matrix, image.width, image.height),
      columns = rect.x1 - rect.x0,
      rows = rect.y1 - rect.y0;
    if (columns && rows > Math.floor((context.maxPixelVisits - visits) / columns))
      throw Error(`${LABEL} exceeds the pixel-visit budget`);
    visits += columns * rows;
    appendData(rectangles, rect);
  }
  // No source buffer is exposed, and no pixel work occurs before preflight.
  const bytes = new Uint8Array(image.width * image.height * 4);
  bytes.set(rgba);
  const values = [];
  for (let i = 0; i < rectangles.length; i++) {
    const luminance = sampleRectangle(rectangles[i], bytes, image.width);
    appendData(values, field.role === 'darkness' ? 1 - clampUnit(luminance) : luminance);
  }
  return {
    kind: 'Varying',
    version: 1,
    valueType: 'Scalar',
    role: field.role,
    unit: null,
    count: context.count,
    values,
  };
}
