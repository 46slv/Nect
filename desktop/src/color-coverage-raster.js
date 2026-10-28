// Only accepts SVG emitted by colorCoveragePasses, never an arbitrary external URL.
// Browser-only adapter; import is inert so DOM-free callers can inject a decoder.
export async function rasterizeCoverageAlpha(svg, { width, height }) {
  if (
    typeof svg !== 'string' ||
    svg.length > 4 * 1024 * 1024 ||
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
    const context = canvas.getContext('2d', { willReadFrequently: true });
    if (!context) throw Error('Coverage canvas is unavailable');
    context.drawImage(image, 0, 0, width, height);
    const pixels = context.getImageData(0, 0, width, height).data;
    const alpha = new Float32Array(width * height);
    for (let i = 0; i < alpha.length; i++) alpha[i] = pixels[i * 4 + 3] / 255;
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
