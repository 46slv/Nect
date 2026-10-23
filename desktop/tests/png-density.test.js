import assert from 'node:assert/strict';
import test from 'node:test';
import { decodeImage, pngSize } from '../src/images.js';
import {
  pngDimensions,
  pngPixelsPerMetre,
  rasterizeSVG,
  withPngPpiMetadata,
} from '../src/raster.js';

const onePixelPng = decodeImage(
  'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==',
);

function chunks(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength),
    result = [];
  for (let at = 8; at < bytes.length; ) {
    const length = view.getUint32(at),
      end = at + 12 + length;
    result.push({
      type: String.fromCharCode(...bytes.subarray(at + 4, at + 8)),
      data: bytes.slice(at + 8, at + 8 + length),
      raw: bytes.slice(at, end),
    });
    at = end;
  }
  return result;
}

function density(bytes) {
  const chunk = chunks(bytes).find(({ type }) => type === 'pHYs');
  if (!chunk) return null;
  const view = new DataView(chunk.data.buffer, chunk.data.byteOffset, chunk.data.byteLength);
  return { x: view.getUint32(0), y: view.getUint32(4), unit: chunk.data[8] };
}

test('selected PPI is stored as rounded pixels per metre in one valid pHYs chunk', () => {
  const original = onePixelPng.slice(),
    encoded = withPngPpiMetadata(original, 300),
    encodedChunks = chunks(encoded);

  assert.deepEqual(original, onePixelPng, 'the source PNG bytes are left unchanged');
  assert.deepEqual(pngSize(encoded), { width: 1, height: 1 });
  assert.deepEqual(density(encoded), { x: 11811, y: 11811, unit: 1 });
  assert.equal(encodedChunks.filter(({ type }) => type === 'pHYs').length, 1);
  assert.deepEqual(
    encodedChunks.filter(({ type }) => type !== 'pHYs').map(({ raw }) => raw),
    chunks(onePixelPng).map(({ raw }) => raw),
    'the signature and all non-density chunks stay byte-for-byte identical',
  );
  assert.ok(Math.abs(density(encoded).x * 0.0254 - 300) < 0.001);
});

test('PPI metadata replaces a canvas density chunk and rejects unrepresentable settings', () => {
  const with300 = withPngPpiMetadata(onePixelPng, 300),
    with150 = withPngPpiMetadata(with300, 150);

  assert.deepEqual(density(with150), { x: 5906, y: 5906, unit: 1 });
  assert.equal(chunks(with150).filter(({ type }) => type === 'pHYs').length, 1);
  assert.equal(pngPixelsPerMetre(300), 11811);
  for (const ppi of [NaN, Infinity, 0, -1, 0.001, Number.MAX_VALUE])
    assert.throws(() => withPngPpiMetadata(onePixelPng, ppi));
  assert.throws(() => pngDimensions({ width: 1, height: 1 }, { mode: 'ppi', ppi: 0.001 }));
});

test('PPI export writes metadata while scale export preserves canvas PNG density', async (t) => {
  const prior = new Map(
      ['Image', 'document'].map((key) => [key, Object.getOwnPropertyDescriptor(globalThis, key)]),
    ),
    priorCreate = Object.getOwnPropertyDescriptor(URL, 'createObjectURL'),
    priorRevoke = Object.getOwnPropertyDescriptor(URL, 'revokeObjectURL');
  t.after(() => {
    for (const [key, descriptor] of prior) {
      if (descriptor) Object.defineProperty(globalThis, key, descriptor);
      else delete globalThis[key];
    }
    for (const [key, descriptor] of [
      ['createObjectURL', priorCreate],
      ['revokeObjectURL', priorRevoke],
    ])
      if (descriptor) Object.defineProperty(URL, key, descriptor);
      else delete URL[key];
  });

  globalThis.Image = class {
    naturalWidth = 1;
    naturalHeight = 1;
    async decode() {}
  };
  globalThis.document = {
    createElement: () => ({
      getContext: () => ({ drawImage() {} }),
      toBlob: (callback) => callback(new Blob([onePixelPng], { type: 'image/png' })),
    }),
  };
  URL.createObjectURL = () => 'blob:test';
  URL.revokeObjectURL = () => {};

  const svg = '<svg width="1px" height="1px" viewBox="0 0 1 1"></svg>',
    scalePng = await rasterizeSVG(svg, { width: 1, height: 1 }),
    ppiPng = await rasterizeSVG(svg, { width: 1, height: 1 }, 300);

  assert.equal(scalePng.type, 'image/png');
  assert.equal(density(new Uint8Array(await scalePng.arrayBuffer())), null);
  assert.deepEqual(density(new Uint8Array(await ppiPng.arrayBuffer())), {
    x: 11811,
    y: 11811,
    unit: 1,
  });
});
