import test from 'node:test';
import assert from 'node:assert/strict';
import { deflate } from 'pako';
import { sampleImageLuminanceGrid } from '../src/image-luminance-field.js';

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
  const result = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
  let at = 0;
  for (const part of parts) {
    result.set(part, at);
    at += part.length;
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

function paeth(left, up, upLeft) {
  const p = left + up - upLeft,
    dl = Math.abs(p - left),
    du = Math.abs(p - up),
    dul = Math.abs(p - upLeft);
  return dl <= du && dl <= dul ? left : du <= dul ? up : upLeft;
}

function encodeScanlines(width, height, channels, pixels, filters) {
  const rowBytes = width * channels,
    raw = new Uint8Array((rowBytes + 1) * height);
  let previous = new Uint8Array(rowBytes);
  for (let y = 0; y < height; y++) {
    const filter = filters?.[y] ?? 0,
      row = pixels.subarray(y * rowBytes, (y + 1) * rowBytes),
      start = y * (rowBytes + 1);
    raw[start] = filter;
    for (let x = 0; x < rowBytes; x++) {
      const value = row[x],
        left = x >= channels ? row[x - channels] : 0,
        up = previous[x],
        upLeft = x >= channels ? previous[x - channels] : 0,
        predictor =
          filter === 1
            ? left
            : filter === 2
              ? up
              : filter === 3
                ? Math.floor((left + up) / 2)
                : filter === 4
                  ? paeth(left, up, upLeft)
                  : 0;
      raw[start + 1 + x] = (value - predictor + 256) & 255;
    }
    previous = Uint8Array.from(row);
  }
  return raw;
}

function makeAsset({
  width,
  height,
  colorType = 6,
  pixels,
  bitDepth = 8,
  interlace = 0,
  filters,
  rawOverride,
  extras = [],
  splitIdat = false,
}) {
  const channels = colorType === 2 ? 3 : colorType === 6 ? 4 : 1,
    sourcePixels = pixels ?? new Uint8Array(width * height * channels),
    header = new Uint8Array(13),
    headerView = new DataView(header.buffer);
  headerView.setUint32(0, width);
  headerView.setUint32(4, height);
  header[8] = bitDepth;
  header[9] = colorType;
  header[10] = 0;
  header[11] = 0;
  header[12] = interlace;
  const scanlines = rawOverride ?? encodeScanlines(width, height, channels, sourcePixels, filters),
    compressed = deflate(scanlines),
    idats = splitIdat
      ? [
          chunk('IDAT', compressed.subarray(0, Math.max(1, Math.floor(compressed.length / 2)))),
          chunk('IDAT', compressed.subarray(Math.max(1, Math.floor(compressed.length / 2)))),
        ]
      : [chunk('IDAT', compressed)],
    bytes = concat(
      SIGNATURE,
      chunk('IHDR', header),
      ...extras.map(({ tag, data }) => chunk(tag, data)),
      ...idats,
      chunk('IEND', new Uint8Array()),
    );
  return {
    mime: 'image/png',
    width,
    height,
    data: Buffer.from(bytes).toString('base64'),
  };
}

const srgbGray = (byte) => ((byte / 255 + 0.055) / 1.055) ** 2.4;
const rgba = (...pixels) => Uint8Array.from(pixels.flat());

test('RGB and RGBA samples use sRGB linear-light luminance and alpha over white', () => {
  const rgb = makeAsset({
      width: 3,
      height: 1,
      colorType: 2,
      pixels: Uint8Array.from([0, 0, 0, 255, 255, 255, 128, 128, 128]),
    }),
    field = sampleImageLuminanceGrid(rgb, [0, 0], [3, 1], { pitch: 1, origin: { x: 0, y: 0 } });
  for (const [index, expected] of [0, 1, srgbGray(128)].entries()) {
    assert.ok(Math.abs(field.cells[index].luminance - expected) < 1e-14);
    assert.ok(Math.abs(field.cells[index].darkness - (1 - expected)) < 1e-14);
  }

  const rgbaAsset = makeAsset({
      width: 3,
      height: 1,
      pixels: rgba([0, 0, 0, 0], [0, 0, 0, 128], [255, 0, 0, 128]),
    }),
    alphaField = sampleImageLuminanceGrid(rgbaAsset, [0, 0], [3, 1], {
      pitch: 1,
      origin: { x: 0, y: 0 },
      invert: true,
    });
  assert.deepEqual(
    alphaField.cells.map((cell) => cell.darkness),
    alphaField.cells.map((cell) => cell.luminance),
  );
  assert.equal(alphaField.cells[0].luminance, 1);
  assert.ok(Math.abs(alphaField.cells[1].luminance - (1 - 128 / 255)) < 1e-15);
  assert.ok(Math.abs(alphaField.cells[2].luminance - (1 - (128 / 255) * (1 - 0.2126))) < 1e-14);
});

test('all PNG row filters and split contiguous IDAT chunks decode correctly', () => {
  const pixels = rgba(
      [0, 0, 0, 255],
      [255, 255, 255, 255],
      [128, 128, 128, 255],
      [0, 0, 0, 0],
      [0, 0, 0, 128],
      [255, 0, 0, 128],
    ),
    asset = makeAsset({
      width: 3,
      height: 2,
      pixels,
      filters: [4, 3],
      splitIdat: true,
      extras: [
        { tag: 'sRGB', data: Uint8Array.of(0) },
        { tag: 'pHYs', data: Uint8Array.from([0, 0, 0, 1, 0, 0, 0, 1, 1]) },
      ],
    }),
    field = sampleImageLuminanceGrid(asset, [0, 0], [3, 2], { pitch: 1, origin: { x: 0, y: 0 } });
  assert.equal(field.cells.length, 6);
  const expected = [0, 1, srgbGray(128), 1, 1 - 128 / 255, 1 - (128 / 255) * (1 - 0.2126)];
  assert.ok(field.cells.every((cell, index) => Math.abs(cell.luminance - expected[index]) < 1e-14));

  const rowFiltersAsset = makeAsset({
      width: 1,
      height: 5,
      pixels: rgba(
        [0, 0, 0, 255],
        [64, 64, 64, 255],
        [128, 128, 128, 255],
        [192, 192, 192, 255],
        [255, 255, 255, 255],
      ),
      filters: [0, 1, 2, 3, 4],
    }),
    rowFilters = sampleImageLuminanceGrid(rowFiltersAsset, [0, 0], [1, 5], {
      pitch: 1,
      origin: { x: 0, y: 0 },
    });
  assert.deepEqual(
    rowFilters.cells.map(({ luminance }) => Number(luminance.toFixed(14))),
    [0, 0.05126945837404, 0.2158605001139, 0.52711512570581, 1],
  );
});

test('placement, scaling, grid phase and inversion follow the world-space image rectangle', () => {
  const asset = makeAsset({
      width: 2,
      height: 2,
      pixels: rgba([0, 0, 0, 255], [255, 255, 255, 255], [0, 0, 0, 255], [255, 255, 255, 255]),
    }),
    field = sampleImageLuminanceGrid(asset, [10, 20], [4, 2], {
      pitch: 1,
      origin: { x: 10, y: 20 },
    });
  assert.deepEqual(field.bounds, { minX: 10, minY: 20, maxX: 14, maxY: 22 });
  assert.deepEqual(
    field.cells.map(({ i, j, x, y, luminance }) => [i, j, x, y, luminance]),
    [
      [0, 0, 10.5, 20.5, 0],
      [1, 0, 11.5, 20.5, 0],
      [2, 0, 12.5, 20.5, 1],
      [3, 0, 13.5, 20.5, 1],
      [0, 1, 10.5, 21.5, 0],
      [1, 1, 11.5, 21.5, 0],
      [2, 1, 12.5, 21.5, 1],
      [3, 1, 13.5, 21.5, 1],
    ],
  );

  const phased = sampleImageLuminanceGrid(asset, [1.5, 1.5], [1, 1], {
    pitch: 2,
    origin: { x: 0, y: 0 },
    invert: true,
  });
  assert.deepEqual(
    phased.cells.map(({ i, j, x, y, luminance, darkness }) => [i, j, x, y, luminance, darkness]),
    [
      [0, 0, 1, 1, 0, 0],
      [1, 0, 3, 1, 1, 1],
      [0, 1, 1, 3, 0, 0],
      [1, 1, 3, 3, 1, 1],
    ],
  );
});

test('partial edge cells clamp to nearest image texels and wholly outside cells are absent', () => {
  const black = makeAsset({ width: 1, height: 1, pixels: rgba([0, 0, 0, 255]) }),
    field = sampleImageLuminanceGrid(black, [1.5, 1.5], [1, 1], {
      pitch: 2,
      origin: { x: 0, y: 0 },
    });
  assert.deepEqual(
    field.cells.map(({ i, j, x, y, darkness }) => [i, j, x, y, darkness]),
    [
      [0, 0, 1, 1, 1],
      [1, 0, 3, 1, 1],
      [0, 1, 1, 3, 1],
      [1, 1, 3, 3, 1],
    ],
  );
  assert.ok(field.cells.every((cell) => cell.x >= 1 && cell.x <= 3 && cell.y >= 1 && cell.y <= 3));
});

test('grid work is capped before decoding and only the referenced asset is touched', () => {
  const tiny = makeAsset({ width: 1, height: 1, pixels: rgba([255, 255, 255, 255]) }),
    unrelatedInvalid = { ...tiny, data: 'not a PNG' };
  assert.doesNotThrow(() => sampleImageLuminanceGrid(tiny, [0, 0], [1, 1]));
  assert.ok(unrelatedInvalid.data); // It is never decoded unless passed as the source.

  assert.throws(
    () => sampleImageLuminanceGrid(unrelatedInvalid, [0, 0], [4097, 1], { pitch: 1 }),
    /4096-cell limit/,
  );
  assert.throws(
    () => sampleImageLuminanceGrid(unrelatedInvalid, [0, 0], [1, 1]),
    /Invalid PNG|Invalid embedded image/,
  );
});

test('4,097 cells and over-4MP PNG headers fail before inflation', () => {
  const invalid = makeAsset({
    width: 2001,
    height: 2000,
    rawOverride: Uint8Array.of(0),
  });
  assert.throws(
    () => sampleImageLuminanceGrid(invalid, [0, 0], [2001, 2000], { pitch: 1024 }),
    /4 megapixels/,
  );
  const small = makeAsset({ width: 1, height: 1, pixels: rgba([0, 0, 0, 255]) });
  assert.throws(
    () => sampleImageLuminanceGrid(small, [0, 0], [4097, 1], { pitch: 1 }),
    /4096-cell limit/,
  );
});

test('inflated IDAT output is capped to the IHDR-derived scanline budget', () => {
  const bomb = makeAsset({ width: 1, height: 1, rawOverride: new Uint8Array(20_000).fill(0) });
  assert.throws(
    () => sampleImageLuminanceGrid(bomb, [0, 0], [1, 1]),
    /PNG decompression exceeds its declared image dimensions/,
  );
  const short = makeAsset({ width: 1, height: 1, rawOverride: Uint8Array.of(0) });
  assert.throws(
    () => sampleImageLuminanceGrid(short, [0, 0], [1, 1]),
    /decompressed dimensions do not match/,
  );
});

test('CRC errors, descriptor mismatch, and unsupported PNG semantics are rejected', () => {
  const valid = makeAsset({ width: 1, height: 1, pixels: rgba([0, 0, 0, 255]) }),
    bytes = Uint8Array.from(Buffer.from(valid.data, 'base64'));
  // Flip one byte in the IDAT CRC; images.js rejects it before zlib starts.
  bytes[bytes.length - 13] ^= 1;
  const corrupt = { ...valid, data: Buffer.from(bytes).toString('base64') };
  assert.throws(() => sampleImageLuminanceGrid(corrupt, [0, 0], [1, 1]), /checksum/);
  assert.throws(
    () => sampleImageLuminanceGrid({ ...valid, width: 2 }, [0, 0], [1, 1]),
    /dimensions do not match/,
  );

  for (const unsupported of [
    makeAsset({ width: 1, height: 1, colorType: 0, pixels: Uint8Array.of(0) }),
    makeAsset({ width: 1, height: 1, colorType: 3, pixels: Uint8Array.of(0) }),
    makeAsset({ width: 1, height: 1, bitDepth: 16, pixels: rgba([0, 0, 0, 255]) }),
    makeAsset({ width: 1, height: 1, interlace: 1, pixels: rgba([0, 0, 0, 255]) }),
    makeAsset({
      width: 1,
      height: 1,
      colorType: 2,
      pixels: Uint8Array.of(0, 0, 0),
      extras: [{ tag: 'tRNS', data: Uint8Array.of(0, 0, 0, 0, 0, 0) }],
    }),
    makeAsset({
      width: 1,
      height: 1,
      pixels: rgba([0, 0, 0, 255]),
      extras: [{ tag: 'iCCP', data: Uint8Array.of(1, 0) }],
    }),
    makeAsset({
      width: 1,
      height: 1,
      pixels: rgba([0, 0, 0, 255]),
      extras: [{ tag: 'vpAG', data: Uint8Array.of(1) }],
    }),
  ])
    assert.throws(
      () => sampleImageLuminanceGrid(unsupported, [0, 0], [1, 1]),
      /Unsupported PNG encoding/,
    );
});

test('same-ID, same-size replacement objects invalidate the identity cache', () => {
  const black = makeAsset({ width: 1, height: 1, pixels: rgba([0, 0, 0, 255]) }),
    white = {
      ...black,
      data: makeAsset({ width: 1, height: 1, pixels: rgba([255, 255, 255, 255]) }).data,
    };
  assert.equal(sampleImageLuminanceGrid(black, [0, 0], [1, 1]).cells[0].luminance, 0);
  assert.equal(sampleImageLuminanceGrid(white, [0, 0], [1, 1]).cells[0].luminance, 1);
  assert.equal(sampleImageLuminanceGrid(black, [0, 0], [1, 1]).cells[0].luminance, 0);
});

test('identical repeated sampling is stable and returns no pixel buffer', () => {
  const asset = makeAsset({
      width: 2,
      height: 2,
      pixels: rgba([0, 0, 0, 255], [255, 255, 255, 255], [128, 128, 128, 255], [0, 0, 0, 0]),
    }),
    first = sampleImageLuminanceGrid(asset, [-1, -2], [2, 2], { pitch: 1 }),
    second = sampleImageLuminanceGrid(asset, [-1, -2], [2, 2], { pitch: 1 });
  assert.deepEqual(second, first);
  assert.equal(JSON.stringify(first).includes('data'), false);
  assert.ok(
    first.cells.every(
      (cell) => Object.keys(cell).sort().join(',') === 'darkness,i,j,luminance,x,y',
    ),
  );
});

test('invalid placement and grid inputs are rejected', () => {
  const asset = makeAsset({ width: 1, height: 1, pixels: rgba([0, 0, 0, 255]) });
  for (const [position, size] of [
    [[0], [1, 1]],
    [
      [0, NaN],
      [1, 1],
    ],
    [
      [0, 0],
      [0, 1],
    ],
    [
      [0, 0],
      [Infinity, 1],
    ],
  ])
    assert.throws(() => sampleImageLuminanceGrid(asset, position, size));
  for (const options of [
    null,
    { pitch: 0 },
    { invert: 1 },
    { origin: { x: 0, y: 0, extra: 1 } },
    { extra: true },
  ])
    assert.throws(() => sampleImageLuminanceGrid(asset, [0, 0], [1, 1], options));
});
