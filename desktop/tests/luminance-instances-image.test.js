import test from 'node:test';
import assert from 'node:assert/strict';
import { deflate } from 'pako';
import {
  document,
  node,
  evaluate,
  connect,
  serialize,
  deserialize,
  setNodeBypassed,
} from '../src/core.js';
const SIGNATURE = Uint8Array.from([137, 80, 78, 71, 13, 10, 26, 10]);
function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = crc & 1 ? 0xedb88320 ^ (crc >>> 1) : crc >>> 1;
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function concat(...parts) {
  const result = new Uint8Array(parts.reduce((total, part) => total + part.length, 0));
  let offset = 0;
  for (const part of parts) {
    result.set(part, offset);
    offset += part.length;
  }
  return result;
}

function chunk(tag, data) {
  const tagBytes = new TextEncoder().encode(tag),
    body = concat(tagBytes, data),
    result = new Uint8Array(body.length + 8),
    view = new DataView(result.buffer);
  view.setUint32(0, data.length);
  result.set(body, 4);
  view.setUint32(4 + body.length, crc32(body));
  return result;
}

function pngAsset(width, height, rgba, { colorType = 6 } = {}) {
  const channels = colorType === 6 ? 4 : colorType === 2 ? 3 : 1,
    header = new Uint8Array(13),
    view = new DataView(header.buffer),
    rows = new Uint8Array(height * (width * channels + 1));
  view.setUint32(0, width);
  view.setUint32(4, height);
  header[8] = 8;
  header[9] = colorType;
  for (let y = 0; y < height; y++) {
    const stride = width * channels,
      start = y * (stride + 1);
    rows[start] = 0;
    if (colorType === 6) rows.set(rgba.subarray(y * stride, (y + 1) * stride), start + 1);
    else if (colorType === 2) rows.set(rgba.subarray(y * stride, (y + 1) * stride), start + 1);
    else rows.fill(0, start + 1, start + 1 + stride);
  }
  const bytes = concat(
    SIGNATURE,
    chunk('IHDR', header),
    chunk('IDAT', deflate(rows)),
    chunk('IEND', new Uint8Array()),
  );
  return {
    name: 'pixels.png',
    mime: 'image/png',
    width,
    height,
    data: Buffer.from(bytes).toString('base64'),
  };
}

test('PNG samples drive scale/angle; partial cells excluded, transparent pixels map to white, exact reopen', () => {
  const doc = document(),
    layer = doc.layers[0],
    image = node('Image'),
    motif = node('Imported Path'),
    effect = node('Luminance Instances'),
    fill = node('Fill'),
    out = node('Output');
  doc.imageAssets = { 'image-test': pngAsset(2, 1, Uint8Array.from([0, 0, 0, 0, 0, 0, 0, 255])) };
  image.asset = 'image-test';
  image.params.position.value = [0, 0];
  image.params.size.value = [32, 16];
  motif.rawPath = 'M-4 -2H4V2H-4Z';
  effect.params.pitch.value = 16;
  layer.nodes = [image, motif, effect, fill, out];
  layer.output = out.id;
  connect(layer, image.id, effect.id, 'path');
  connect(layer, motif.id, effect.id, 'motif');
  connect(layer, effect.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  const run = () => evaluate(layer, {}, doc.imageAssets).run(effect.id),
    result = run();
  assert.equal(result.contours.length, 2);
  const a = result.contours[0].points,
    b = result.contours[1].points;
  assert.ok(Math.abs(a[1].position[0] - a[0].position[0] - 1.6) < 1e-12);
  assert.ok(Math.abs(b[1].position[1] - b[0].position[1] - 8) < 1e-12);
  const reopened = deserialize(serialize(doc));
  assert.equal(reopened.schemaVersion, 21);
  assert.deepEqual(evaluate(reopened.layers[0], {}, reopened.imageAssets).run(effect.id), result);
  image.params.size.value = [20, 16];
  assert.equal(run().contours.length, 1);
  effect.params.invert.value = true;
  assert.ok(
    Math.abs(
      run().contours[0].points[1].position[1] - run().contours[0].points[0].position[1] - 8,
    ) < 1e-12,
  );
  setNodeBypassed(layer, effect.id, true);
  connect(layer, image.id, effect.id, 'path');
  assert.deepEqual(run(), evaluate(layer).run(motif.id));
});
