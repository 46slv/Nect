import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs/promises';
import { join } from 'node:path';
import { chromium } from 'playwright-core';

// No editor, document model, fonts, imported assets or external SVG references.
const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="32px" height="16px" viewBox="0 0 32 16">
  <rect width="16" height="8" fill="#ff0000"/>
  <rect x="16" width="16" height="8" fill="#00ff00" fill-opacity="0.5"/>
  <rect y="8" width="16" height="8" fill="#0000ff"/>
</svg>`;
const cases = [
  { id: 'scale-1', sizing: 1, ppi: null, width: 32, height: 16 },
  { id: 'fixed-300ppi', sizing: 1, ppi: 300, density: 11811, width: 32, height: 16 },
  { id: 'fixed-150ppi', sizing: 1, ppi: 150, density: 5906, width: 32, height: 16 },
  { id: 'scale-2', sizing: { mode: 'scale', scale: 2 }, ppi: null, width: 64, height: 32 },
  {
    id: 'sized-300ppi',
    sizing: { mode: 'ppi', ppi: 300 },
    ppi: 300,
    density: 11811,
    width: 100,
    height: 50,
  },
];

// Independent test parser: check actual encoder bytes, including each chunk CRC.
function pngChunks(bytes) {
  assert.deepEqual([...bytes.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  const chunks = [];
  let at = 8;
  while (at < bytes.length) {
    assert.ok(at + 12 <= bytes.length, 'complete PNG chunk header');
    const length = bytes.readUInt32BE(at),
      end = at + 12 + length,
      type = bytes.toString('ascii', at + 4, at + 8);
    assert.ok(end <= bytes.length, `complete ${type} chunk`);
    let crc = 0xffffffff;
    for (const byte of bytes.subarray(at + 4, end - 4)) {
      crc ^= byte;
      for (let bit = 0; bit < 8; bit++) crc = crc & 1 ? 0xedb88320 ^ (crc >>> 1) : crc >>> 1;
    }
    assert.equal(bytes.readUInt32BE(end - 4), (crc ^ 0xffffffff) >>> 0, `${type} CRC`);
    chunks.push({ type, data: bytes.subarray(at + 8, end - 4) });
    at = end;
    if (type === 'IEND') break;
  }
  assert.equal(chunks[0]?.type, 'IHDR');
  assert.equal(chunks.at(-1)?.type, 'IEND');
  assert.equal(at, bytes.length, 'no bytes after IEND');
  return chunks;
}

function expectedRgba(width, height) {
  const pixels = Buffer.alloc(width * height * 4);
  for (let y = 0; y < height; y++)
    for (let x = 0; x < width; x++) {
      const color =
        y < height / 2
          ? x < width / 2
            ? [255, 0, 0, 255]
            : [0, 255, 0, 128]
          : x < width / 2
            ? [0, 0, 255, 255]
            : [0, 0, 0, 0];
      pixels.set(color, (y * width + x) * 4);
    }
  return pixels;
}

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
try {
  const page = await browser.newPage(),
    errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  // Same-origin module imports use the existing Vite server, not the editor UI.
  await page.route('**/__raster-direct-test', (route) =>
    route.fulfill({
      contentType: 'text/html',
      body: '<!doctype html><title>Direct raster test</title>',
    }),
  );
  await page.goto(`${process.env.BASE_URL || 'http://127.0.0.1:5173'}/__raster-direct-test`);
  const outputs = await page.evaluate(
    async ({ svg, cases }) => {
      const { pngDimensions, rasterizeSVG } = await import('/src/raster.js'),
        results = [];
      for (const fixture of cases) {
        const size = pngDimensions({ width: 32, height: 16 }, fixture.sizing),
          blob = await rasterizeSVG(svg, size, fixture.ppi),
          bytes = new Uint8Array(await blob.arrayBuffer()),
          url = URL.createObjectURL(blob);
        try {
          const image = new Image();
          image.src = url;
          await image.decode();
          const canvas = document.createElement('canvas');
          canvas.width = image.naturalWidth;
          canvas.height = image.naturalHeight;
          const context = canvas.getContext('2d');
          context.drawImage(image, 0, 0);
          results.push({
            id: fixture.id,
            type: blob.type,
            size,
            decodedSize: { width: canvas.width, height: canvas.height },
            png: Array.from(bytes),
            rgba: Array.from(context.getImageData(0, 0, canvas.width, canvas.height).data),
          });
        } finally {
          URL.revokeObjectURL(url);
        }
      }
      return results;
    },
    { svg, cases },
  );
  const output = join(process.cwd(), 'test-results'),
    results = [];
  await fs.mkdir(output, { recursive: true });
  for (const [index, fixture] of cases.entries()) {
    const result = outputs[index],
      size = { width: fixture.width, height: fixture.height },
      png = Buffer.from(result.png),
      rgba = Buffer.from(result.rgba),
      chunks = pngChunks(png),
      headers = chunks.filter(({ type }) => type === 'IHDR'),
      densities = chunks.filter(({ type }) => type === 'pHYs'),
      firstData = chunks.findIndex(({ type }) => type === 'IDAT');
    assert.equal(result.id, fixture.id);
    assert.equal(result.type, 'image/png');
    assert.deepEqual(result.size, size);
    assert.deepEqual(result.decodedSize, size);
    assert.equal(headers.length, 1);
    assert.equal(headers[0].data.length, 13);
    assert.equal(headers[0].data.readUInt32BE(0), size.width);
    assert.equal(headers[0].data.readUInt32BE(4), size.height);
    assert.ok(firstData > 0, 'encoded image data exists');
    assert.deepEqual(
      rgba,
      expectedRgba(size.width, size.height),
      `${fixture.id}: every RGBA pixel`,
    );
    let density = null;
    if (densities.length) {
      assert.equal(densities.length, 1);
      assert.equal(densities[0].data.length, 9);
      assert.ok(chunks.findIndex(({ type }) => type === 'pHYs') < firstData);
      density = {
        x: densities[0].data.readUInt32BE(0),
        y: densities[0].data.readUInt32BE(4),
        unit: densities[0].data[8],
      };
    }
    if (fixture.ppi !== null)
      assert.deepEqual(density, { x: fixture.density, y: fixture.density, unit: 1 });
    const file = `workflow-raster-direct-${fixture.id}.png`;
    await fs.writeFile(join(output, file), png);
    results.push({
      id: fixture.id,
      file,
      sizing: fixture.sizing,
      requestedPpi: fixture.ppi,
      size,
      density,
      rgbaSha256: createHash('sha256').update(rgba).digest('hex'),
      pngSha256: createHash('sha256').update(png).digest('hex'),
      allPixelsMatch: true,
    });
  }
  assert.equal(results[0].rgbaSha256, results[1].rgbaSha256);
  assert.equal(results[0].rgbaSha256, results[2].rgbaSha256);
  assert.deepEqual(results[0].density, results[3].density, 'scale retains canvas default density');
  assert.deepEqual(
    results[1].density,
    results[4].density,
    'PPI is independent of raster dimensions',
  );
  assert.deepEqual(errors, []);
  const receipt = {
    route: 'Direct existing rasterizeSVG in actual Chrome; no editor UI or new export API',
    commit: process.env.GITHUB_SHA || null,
    browser: browser.version(),
    fixture: { width: 32, height: 16, externalReferences: false },
    results,
    pageErrors: errors,
    ok: true,
    notRun: ['PNG CLI product', 'native WebView2/OS save', 'physical print or color management'],
  };
  await fs.writeFile(
    join(output, 'workflow-raster-direct-png.json'),
    JSON.stringify(receipt, null, 2),
  );
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser.close();
}
