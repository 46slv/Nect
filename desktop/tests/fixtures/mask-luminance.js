import { imageAlphaFixture } from './mask-image-alpha.js';
import { parameter } from '../../src/core.js';
import { encodeImage } from '../../src/images.js';
import { encodeRgbaPng } from '../../src/image-keyer.js';
export const maskColors = [
  [0, 0, 0, 255],
  [255, 255, 255, 255],
  [128, 128, 128, 255],
  [255, 0, 0, 255],
  [0, 255, 0, 255],
  [0, 0, 255, 255],
  [255, 255, 255, 128],
  [128, 128, 128, 128],
  [255, 255, 255, 0],
];
export const expectedLuminance = maskColors.map(([r, g, b, a]) =>
  Math.round(((0.2126 * r + 0.7152 * g + 0.0722 * b) * a) / 255),
);
export function luminanceFixture({
  luminance = false,
  feather = 0,
  invert = false,
  keyed = false,
  postTransform,
} = {}) {
  const f = imageAlphaFixture({ feather, invert, keyed, postTransform }),
    rgba = new Uint8Array(18 * 4 * 4);
  for (let y = 0; y < 4; y++)
    for (let x = 0; x < 18; x++) rgba.set(maskColors[Math.floor(x / 2)], (y * 18 + x) * 4);
  f.doc.imageAssets[f.material.asset] = {
    name: 'Mask luminance swatches.png',
    mime: 'image/png',
    width: 18,
    height: 4,
    data: encodeImage(encodeRgbaPng(18, 4, rgba)),
  };
  f.doc.width = 180;
  f.doc.height = 80;
  f.image.params.size.value = [180, 80];
  f.material.params.size.value = [180, 80];
  f.doc.name = 'Mask mode comparison';
  if (luminance) {
    f.mask.params.luminance = { ...parameter(f.mask, 'luminance'), value: true };
    f.doc.schemaVersion = 52;
  }
  const positions = {
    'example-image': [40, 40],
    'example-keyer': [350, 40],
    'example-mask-shape': [660, 400],
    'alpha-material': [40, 400],
    'alpha-keyer': [350, 400],
    'example-mask': [660, 40],
    'example-output': [970, 40],
    'feather-appearance-transform': [970, 400],
  };
  for (const n of f.layer.nodes) n.position = positions[n.id] || n.position;
  return { ...f, rgba };
}

export const equalLuminanceRGB = [
  [94, 148, 30],
  [162, 108, 226],
];
export function equalLuminanceFixture() {
  const f = luminanceFixture({ luminance: true }),
    rgba = f.rgba.slice();
  for (let y = 0; y < 4; y++)
    for (let x = 0; x < 8; x++)
      rgba.set([...equalLuminanceRGB[Math.floor(x / 2) % 2], x < 4 ? 255 : 128], (y * 18 + x) * 4);
  f.doc.imageAssets[f.material.asset] = {
    name: 'Equal luminance colors.png',
    mime: 'image/png',
    width: 18,
    height: 4,
    data: encodeImage(encodeRgbaPng(18, 4, rgba)),
  };
  f.doc.name = 'Equal luminance RGB oracle';
  return { ...f, rgba };
}
