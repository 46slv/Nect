// Only accepts SVG emitted by colorCoveragePasses, never an arbitrary external URL.
// Browser-only adapter; import is inert so DOM-free callers can inject a decoder.
export async function rasterizeCoverageAlpha(svg, { width, height, channel = 'alpha' }) {
  if (
    typeof svg !== 'string' ||
    svg.length > 4 * 1024 * 1024 ||
    !['alpha', 'premultiplied-red'].includes(channel) ||
    ![width, height].every((v) => Number.isInteger(v) && v > 0 && v <= 256)
  )
    throw Error('Invalid coverage raster request');
  const image = new Image();
  const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
  let timer;
  try {
    await new Promise((resolve, reject) => {
      image.onload = resolve;
      image.onerror = () => reject(Error('Coverage SVG could not be decoded'));
      timer = setTimeout(() => reject(Error('Coverage SVG decode timed out')), 10000);
      image.src = url;
    });
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const basis = channel === 'premultiplied-red';
    const context = canvas.getContext('2d', {
      willReadFrequently: true,
      ...(basis ? { colorType: 'float16', colorSpace: 'srgb' } : {}),
    });
    if (!context) throw Error('Coverage canvas is unavailable');
    let attributes;
    if (basis) {
      attributes = context.getContextAttributes?.();
      if (attributes?.colorType !== 'float16' || attributes.colorSpace !== 'srgb')
        throw Error('Angular coverage requires an actual float16 sRGB canvas');
    }
    context.drawImage(image, 0, 0, width, height);
    const readback = context.getImageData(
      0,
      0,
      width,
      height,
      ...(basis ? [{ colorSpace: 'srgb', pixelFormat: 'rgba-float16' }] : []),
    );
    const pixels = readback.data;
    if (
      basis &&
      (readback.pixelFormat !== 'rgba-float16' ||
        readback.colorSpace !== 'srgb' ||
        typeof globalThis.Float16Array !== 'function' ||
        !(pixels instanceof globalThis.Float16Array))
    )
      throw Error('Angular coverage requires actual rgba-float16 readback');
    const alpha = new Float32Array(width * height);
    // Canvas readback is unpremultiplied RGBA. Basis-color contributions need
    // R*A exactly once: all wedge overlap and shape AA are already composited.
    for (let i = 0; i < alpha.length; i++) {
      const opacity = basis ? pixels[i * 4 + 3] : pixels[i * 4 + 3] / 255;
      const red = basis ? pixels[i * 4] : 0;
      if (
        basis &&
        ![opacity, red].every((value) => Number.isFinite(value) && value >= 0 && value <= 1)
      )
        throw Error('Invalid float16 coverage readback');
      alpha[i] = basis ? red * opacity : opacity;
    }
    if (basis)
      Object.defineProperty(alpha, 'coveragePrecision', {
        value: Object.freeze({
          contextColorType: attributes.colorType,
          contextColorSpace: attributes.colorSpace,
          pixelFormat: readback.pixelFormat,
          readbackColorSpace: readback.colorSpace,
          arrayType: pixels.constructor.name,
          bytesPerElement: pixels.BYTES_PER_ELEMENT,
        }),
      });
    return alpha;
  } finally {
    clearTimeout(timer);
    try {
      image.onload = image.onerror = null;
      image.src = '';
    } finally {
      URL.revokeObjectURL(url);
    }
  }
}
