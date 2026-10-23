import { writeFile } from 'node:fs/promises';
import { blankDocument, layer, node, serialize, validate } from '../src/core.js';
import { encodeImage } from '../src/images.js';
import { encodeRgbaPng } from '../src/image-keyer.js';

const doc = blankDocument();
doc.name = 'Image and Shape Dither';
doc.width = 328;
doc.height = 328;
doc.schemaVersion = 41;
doc.layers = [];
const pixels = new Uint8Array(64 * 64 * 4);
for (let y = 0; y < 64; y++)
  for (let x = 0; x < 64; x++) {
    const gray = Math.round((x * 255) / 63),
      alpha = y < 16 ? 0 : y < 32 ? 128 : 255;
    pixels.set([gray, gray, gray, alpha], (y * 64 + x) * 4);
  }
doc.imageAssets = {
  'image-dither-example': {
    name: 'synthetic-alpha-gradient.png',
    mime: 'image/png',
    width: 64,
    height: 64,
    data: encodeImage(encodeRgbaPng(64, 64, pixels)),
  },
};
for (const [index, algorithm] of ['Ordered Dither', 'Error Diffusion Dither'].entries())
  for (const imageOutput of [true, false]) {
    const suffix = `${index}-${imageOutput ? 'image' : 'shape'}`,
      comp = layer(`${algorithm} ${imageOutput ? 'Image' : 'Shape'}`),
      source = node('Image'),
      effect = node(algorithm + (imageOutput ? ' Image' : '')),
      output = node(imageOutput ? 'Image Output' : 'Output');
    comp.id = `layer-dither-${suffix}`;
    source.id = `source-dither-${suffix}`;
    effect.id = `effect-dither-${suffix}`;
    output.id = `output-dither-${suffix}`;
    source.asset = 'image-dither-example';
    source.params.position.value = [24 + index * 152, imageOutput ? 24 : 176];
    source.params.size.value = [128, 128];
    effect.params.path.source = source.id;
    effect.params.pitch.value = 8;
    effect.params.dotSize.value = 12;
    effect.operation = index === 0 ? 'star' : 'square';
    effect.params.dotSides.value = 4;
    effect.params.starInnerRatio.value = 0.18;
    effect.params.dotRotation.value = index === 0 ? 0 : 45;
    output.params.path.source = effect.id;
    comp.nodes = [source, effect, output];
    comp.output = output.id;
    doc.layers.push(comp);
  }
await writeFile(
  new URL('../examples/image-dither-output.json', import.meta.url),
  serialize(validate(doc)) + '\n',
);
