import { document, node, connect } from '../../src/core.js';
import { encodeRgbaPng } from '../../src/image-keyer.js';
import { encodeImage } from '../../src/images.js';
export function keyerPickFixture() {
  const doc = document(),
    layer = doc.layers[0],
    image = node('Image'),
    first = node('Keyer'),
    keyer = node('Keyer'),
    out = node('Image Output');
  doc.schemaVersion = 49;
  doc.width = 320;
  doc.height = 180;
  doc.name = 'NECT_KEYER_INPUT_PICK';
  const rgba = new Uint8Array([
    0, 255, 0, 255, 12, 34, 56, 128, 200, 50, 40, 0, 255, 0, 0, 255, 255, 255, 255, 255, 0, 0, 255,
    255, 0, 0, 0, 255, 80, 80, 80, 255,
  ]);
  doc.imageAssets = {
    'image-pick': {
      name: 'Synthetic input.png',
      mime: 'image/png',
      width: 4,
      height: 2,
      data: encodeImage(encodeRgbaPng(4, 2, rgba)),
    },
  };
  image.asset = 'image-pick';
  image.params.position.value = [20, 30];
  image.params.size.value = [284, 118];
  first.params.tolerance.value = 0;
  first.params.softness.value = 0;
  keyer.params.keyColor.value = { hex: '#ff00ff', alpha: 1 };
  keyer.params.tolerance.value = 0;
  keyer.params.softness.value = 0;
  layer.nodes = [image, first, keyer, out];
  layer.output = out.id;
  layer.nodes.forEach((n, i) => {
    n.position = [30 + 310 * i, 30];
    n.collapsed = n !== keyer;
  });
  connect(layer, image.id, first.id, 'path');
  connect(layer, first.id, keyer.id, 'path');
  connect(layer, keyer.id, out.id, 'path');
  return { doc, layer, image, first, keyer, out, rgba };
}
