import { node, connect } from '../../src/core.js';
import { encodeImage } from '../../src/images.js';
import { encodeRgbaPng } from '../../src/image-keyer.js';
import { featherFixture } from './mask-feather.js';

export function imageAlphaFixture({
  keyed = false,
  feather = 0,
  invert = false,
  connectMask = true,
  rgbVariant = false,
  postTransform,
} = {}) {
  const f = featherFixture(feather, { invert, postTransform }),
    material = node('Image'),
    keyer = node('Keyer'),
    rgba = new Uint8Array(8 * 4 * 4);
  for (let y = 0; y < 4; y++)
    for (let x = 0; x < 8; x++) {
      const color =
        x < 2
          ? [30, 200, 70, 0]
          : x < 4
            ? [0, 255, 0, 128]
            : x < 6
              ? [255, 0, 0, 128]
              : [0, 0, 0, 255];
      if (rgbVariant) color.splice(0, 3, 255 - color[0], 255 - color[1], 255 - color[2]);
      rgba.set(color, (y * 8 + x) * 4);
    }
  material.id = 'alpha-material';
  material.name = 'Alpha material';
  material.asset = 'image-alpha-material';
  material.params.position.value = [0, 0];
  material.params.size.value = [80, 80];
  keyer.id = 'alpha-keyer';
  keyer.name = 'Remove material green';
  keyer.params.keyColor.value = { hex: '#00ff00', alpha: 1 };
  keyer.params.tolerance.value = 0;
  keyer.params.softness.value = 0;
  f.doc.imageAssets[material.asset] = {
    name: 'synthetic-alpha.png',
    mime: 'image/png',
    width: 8,
    height: 4,
    data: encodeImage(encodeRgbaPng(8, 4, rgba)),
  };
  f.layer.nodes.splice(f.layer.nodes.indexOf(f.mask), 0, material, keyer);
  connect(f.layer, material.id, keyer.id, 'path');
  if (connectMask) {
    connect(f.layer, keyed ? keyer.id : material.id, f.mask.id, 'mask');
    f.doc.schemaVersion = 51;
  }
  f.doc.name = 'Image alpha Mask';
  return { ...f, material, alphaKeyer: keyer, rgba };
}
