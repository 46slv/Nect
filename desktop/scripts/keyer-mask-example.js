// Reuses the synthetic green/red/alpha fixture from tests/image-effects.e2e.js.
// Every appearance and mask is authored/evaluated/exported by Nect.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import {
  blankDocument,
  node,
  connect,
  serialize,
  deserialize,
  exportSVG,
  evaluate,
  CommandSession,
} from '../src/core.js';
import { encodeRgbaPng, imageAssetFor } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';
import { copyImageRgba } from '../src/image-luminance-field.js';
import { keyerInputPickContext, planKeyerInputPick } from '../src/keyer-input-pick.js';

export function keyerMaskExample(keyed = false) {
  const doc = blankDocument();
  Object.assign(doc, {
    schemaVersion: 49,
    name: 'Keyer + Mask — editable example',
    width: 80,
    height: 80,
  });
  const layer = doc.layers[0];
  Object.assign(layer, { id: 'keyer-mask-layer', name: 'Keyer + Mask', allowEmptyOutput: false });
  const image = node('Image', 40, 40),
    keyer = node('Keyer', 350, 40),
    shape = node('Rectangle', 40, 360),
    mask = node('Mask', 660, 40),
    output = node('Output', 970, 40);
  const nodes = [image, keyer, shape, mask, output];
  nodes.forEach((n, i) => {
    n.id = [
      'example-image',
      'example-keyer',
      'example-mask-shape',
      'example-mask',
      'example-output',
    ][i];
    n.name = ['Source image', 'Remove green', 'Mask rectangle', 'Shape mask', 'Masked output'][i];
    n.collapsed = !['Keyer', 'Mask'].includes(n.type);
  });
  const rgba = new Uint8Array(80 * 80 * 4);
  for (let y = 0; y < 80; y++)
    for (let x = 0; x < 80; x++)
      rgba.set(x < 40 ? [0, 255, 0, 255] : [255, 0, 0, y < 40 ? 128 : 255], (y * 80 + x) * 4);
  doc.imageAssets = {
    'image-keyer-mask-example': {
      name: 'synthetic-green-red-alpha.png',
      mime: 'image/png',
      width: 80,
      height: 80,
      data: encodeImage(encodeRgbaPng(80, 80, rgba)),
    },
  };
  image.asset = 'image-keyer-mask-example';
  image.params.position.value = [0, 0];
  image.params.size.value = [80, 80];
  keyer.params.keyColor.value = { hex: keyed ? '#00ff00' : '#ff00ff', alpha: 1 };
  keyer.params.tolerance.value = 0;
  keyer.params.softness.value = 0;
  shape.params.center.value = [40, 40];
  shape.params.size.value = [56, 56];
  layer.nodes = nodes;
  layer.output = output.id;
  connect(layer, image.id, keyer.id, 'path');
  connect(layer, keyer.id, mask.id, 'path');
  connect(layer, shape.id, mask.id, 'mask');
  connect(layer, mask.id, output.id, 'path');
  return doc;
}
export async function writeKeyerMaskExample(output) {
  assert.ok(output && path.isAbsolute(output), 'Pass a new absolute output directory');
  await fs.mkdir(output); // Refuse every existing directory, including an empty one.
  const before = keyerMaskExample(false),
    finished = keyerMaskExample(true),
    session = new CommandSession(before),
    context = keyerInputPickContext(session.snapshot(), 'keyer-mask-layer', 'example-keyer'),
    pick = planKeyerInputPick(session.snapshot(), context, [20, 20]);
  assert.equal(pick.hex, '#00ff00');
  session.dispatch({ ...session.state, type: pick.type, payload: pick.payload });
  assert.deepEqual(session.snapshot(), finished);
  assert.equal(session.historyView.past.length, 1);
  session.dispatch({ ...session.state, type: 'undo' });
  assert.deepEqual(session.snapshot(), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.deepEqual(session.snapshot(), finished);
  const scene = evaluate(finished.layers[0], {}, finished.imageAssets, finished);
  assert.equal(scene.error, null);
  const input = scene.run('example-keyer'),
    pixels = copyImageRgba(imageAssetFor(input, finished.imageAssets)),
    alpha = (x, y) => pixels[(y * 80 + x) * 4 + 3];
  assert.equal(alpha(20, 20), 0);
  assert.equal(alpha(50, 30), 128);
  assert.equal(alpha(50, 60), 255);
  assert.equal(scene.path.mask.shape.contours.length, 1);
  assert.equal(scene.path.mask.shape.contours[0].closed, true);
  const finalSVG = exportSVG(finished);
  for (const [prefix, doc] of [
    ['01-pick-background', before],
    ['02-keyed-mask', finished],
  ]) {
    const json = serialize(doc),
      svg = exportSVG(doc);
    assert.deepEqual(deserialize(json), doc);
    assert.equal(exportSVG(deserialize(json)), svg);
    await fs.writeFile(path.join(output, prefix + '.json'), json, { flag: 'wx' });
    await fs.writeFile(path.join(output, prefix + '.svg'), svg, { flag: 'wx' });
  }
  session.dispatch({
    ...session.state,
    type: 'set-parameter',
    payload: {
      layerId: 'keyer-mask-layer',
      nodeId: 'example-mask-shape',
      key: 'size',
      value: [40, 40],
    },
  });
  const resized = session.snapshot();
  assert.notEqual(exportSVG(resized), finalSVG);
  assert.deepEqual(resized.imageAssets, finished.imageAssets);
  assert.deepEqual(
    resized.layers[0].nodes.map((n) => n.id),
    finished.layers[0].nodes.map((n) => n.id),
  );
  session.dispatch({ ...session.state, type: 'undo' });
  assert.deepEqual(session.snapshot(), finished);
  const receipt = {
    Source:
      'Existing tests/image-effects.e2e.js synthetic image/alpha scheme; existing Nect graph/evaluator/export',
    Canvas: [80, 80],
    Nodes: 5,
    Connections: 4,
    Graph: 'Image -> Keyer -> Mask -> Output; Rectangle -> Mask.Shape',
    InitialKey: '#ff00ff',
    PickedKey: '#00ff00',
    MaskRectangle: { Center: [40, 40], Size: [56, 56], Bounds: [12, 12, 68, 68] },
    KeyerOutputAlpha: { Green: 0, UpperRed: 128, LowerRed: 255 },
    OnePickUndoRedo: true,
    MaskSizeEditableUndo: true,
    IDsAssetsPreserved: true,
    JSONSVGExact: true,
    ExternalImages: false,
    OSDialogs: false,
    NativeProcessInteraction: false,
  };
  await fs.writeFile(path.join(output, 'model-receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  console.log(JSON.stringify(receipt, null, 2));
}
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href)
  await writeKeyerMaskExample(process.argv[2]);
