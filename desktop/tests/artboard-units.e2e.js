import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { join } from 'node:path';
import { chromium } from 'playwright-core';
import { displayLengthToDocumentUnits, formatDocumentLength } from '../src/artboard-units.js';

const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const CRC_TABLE = new Uint32Array(256);
for (let i = 0; i < CRC_TABLE.length; i++) {
  let value = i;
  for (let bit = 0; bit < 8; bit++) value = value & 1 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
  CRC_TABLE[i] = value >>> 0;
}

function pngCrc32(type, data) {
  let crc = 0xffffffff;
  for (const byte of Buffer.from(type, 'ascii')) crc = CRC_TABLE[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  for (const byte of data) crc = CRC_TABLE[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunks(bytes) {
  assert.ok(bytes.subarray(0, PNG_SIGNATURE.length).equals(PNG_SIGNATURE), 'PNG signature');
  const chunks = [];
  let offset = PNG_SIGNATURE.length;
  while (offset < bytes.length) {
    assert.ok(offset + 12 <= bytes.length, 'complete PNG chunk header');
    const length = bytes.readUInt32BE(offset),
      type = bytes.toString('ascii', offset + 4, offset + 8),
      dataStart = offset + 8,
      dataEnd = dataStart + length;
    assert.ok(dataEnd + 4 <= bytes.length, `complete ${type} chunk`);
    const data = bytes.subarray(dataStart, dataEnd),
      expectedCrc = bytes.readUInt32BE(dataEnd);
    assert.equal(expectedCrc, pngCrc32(type, data), `${type} CRC`);
    chunks.push({ type, data });
    offset = dataEnd + 4;
    if (type === 'IEND') break;
  }
  assert.equal(offset, bytes.length, 'PNG ends immediately after IEND');
  return chunks;
}

function captureBrowserDownloads() {
  const objectUrls = new Map(),
    originalCreateObjectURL = URL.createObjectURL.bind(URL),
    originalAnchorClick = HTMLAnchorElement.prototype.click;
  URL.createObjectURL = (object) => {
    const url = originalCreateObjectURL(object);
    objectUrls.set(url, object);
    return url;
  };
  HTMLAnchorElement.prototype.click = function () {
    const blob = objectUrls.get(this.href);
    if (this.download && blob instanceof Blob) {
      const capture = { filename: this.download, type: blob.type, ready: false };
      window.__nectDownloadCapture = capture;
      blob.arrayBuffer().then(
        (buffer) => {
          const bytes = new Uint8Array(buffer);
          let binary = '';
          for (let index = 0; index < bytes.length; index += 0x8000)
            binary += String.fromCharCode(...bytes.subarray(index, index + 0x8000));
          capture.base64 = btoa(binary);
          capture.ready = true;
        },
        (error) => {
          capture.error = String(error);
          capture.ready = true;
        },
      );
    }
    return originalAnchorClick.call(this);
  };
}

async function exportPngAndInspect(page, deviceScaleFactor) {
  await page.locator('#png-export-mode').selectOption('ppi');
  await page.locator('#png-ppi').fill('300');
  await page.waitForFunction(
    () => document.querySelector('[data-png-dimensions]')?.dataset.valid === 'true',
  );
  await page
    .locator('summary')
    .filter({ hasText: /^Export$/ })
    .click();
  const [download] = await Promise.all([
    page.waitForEvent('download', { timeout: 5000 }).catch(() => null),
    page.locator('[data-action="export-png"]').click(),
  ]);
  await page.waitForFunction(() => window.__nectDownloadCapture?.ready === true);
  const capture = await page.evaluate(() => ({
    filename: window.__nectDownloadCapture.filename,
    type: window.__nectDownloadCapture.type,
    base64: window.__nectDownloadCapture.base64,
    error: window.__nectDownloadCapture.error,
    notification: document.querySelector('.status')?.innerText,
  }));
  assert.equal(capture.error, undefined, 'download Blob bytes can be read');
  assert.equal(capture.filename, 'nect-artwork.png');
  assert.equal(capture.type, 'image/png');
  assert.match(capture.notification || '', /PNG exported/);
  assert.ok(download, 'Export PNG starts a browser download');
  assert.equal(download.suggestedFilename(), capture.filename);
  const relativePath = `test-results/png-100x50mm-300ppi-dpr${deviceScaleFactor}.png`,
    path = join(process.cwd(), relativePath);
  await fs.mkdir(join(process.cwd(), 'test-results'), { recursive: true });
  const bytes = Buffer.from(capture.base64, 'base64');
  await download.saveAs(path);
  const savedBytes = await fs.readFile(path),
    chunks = pngChunks(savedBytes),
    headers = chunks.filter((chunk) => chunk.type === 'IHDR'),
    densities = chunks.filter((chunk) => chunk.type === 'pHYs'),
    imageData = chunks.filter((chunk) => chunk.type === 'IDAT');
  assert.equal(headers.length, 1);
  assert.equal(headers[0].data.length, 13);
  assert.equal(headers[0].data.readUInt32BE(0), 1181);
  assert.equal(headers[0].data.readUInt32BE(4), 591);
  assert.equal(densities.length, 1);
  assert.equal(densities[0].data.length, 9);
  assert.equal(densities[0].data.readUInt32BE(0), 11811);
  assert.equal(densities[0].data.readUInt32BE(4), 11811);
  assert.equal(densities[0].data[8], 1, 'pHYs unit is pixels per metre');
  assert.ok(imageData.length > 0, 'encoded image data exists');
  assert.deepEqual(savedBytes, bytes, 'download bytes match the encoder Blob');
  const decoded = await page.evaluate(async (base64) => {
    const binary = atob(base64),
      imageBytes = Uint8Array.from(binary, (character) => character.charCodeAt(0)),
      bitmap = await createImageBitmap(new Blob([imageBytes], { type: 'image/png' }));
    return { width: bitmap.width, height: bitmap.height };
  }, savedBytes.toString('base64'));
  assert.deepEqual(decoded, { width: 1181, height: 591 }, 'browser decodes the saved PNG');
  return {
    path: relativePath,
    sizeBytes: savedBytes.length,
    width: decoded.width,
    height: decoded.height,
  };
}

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const pageContext = await browser.newContext({
  viewport: { width: 1280, height: 900 },
  acceptDownloads: true,
  deviceScaleFactor: 1,
});
const page = await pageContext.newPage();
await pageContext.addInitScript(captureBrowserDownloads);
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForSelector('[data-artboard-unit]');
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const original = await saved();
  const originalDocument = JSON.parse(original);
  const width = page.locator('[data-dimension="width"]');

  await page.locator('[data-artboard-unit]').selectOption('mm');
  assert.equal(await saved(), original, 'changing units does not edit the document');
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  assert.equal(await width.inputValue(), formatDocumentLength(originalDocument.width, 'mm'));
  assert.match(await page.locator('[data-artboard-size]').textContent(), /mm$/);

  await page.locator('[data-artboard-unit]').selectOption('cm');
  assert.equal(await saved(), original, 'a second unit change also preserves the document');
  assert.equal(await width.inputValue(), formatDocumentLength(originalDocument.width, 'cm'));

  await width.fill('10');
  await width.press('Tab');
  const expectedCentimetres = displayLengthToDocumentUnits(10, 'cm');
  await page.waitForFunction(
    (expected) => Math.abs(window.nect.document.width - expected) < 1e-10,
    expectedCentimetres,
  );
  assert.equal(
    await page.evaluate(async () => {
      const { serialize, deserialize } = await import('/src/core.js');
      return deserialize(serialize(window.nect.document)).width;
    }),
    expectedCentimetres,
    'cm input serializes in canonical px',
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'Undo restores the exact original after cm input');

  await page.locator('[data-artboard-unit]').selectOption('mm');
  await width.fill('100');
  await width.press('Tab');
  const expectedWidth = displayLengthToDocumentUnits(100, 'mm');
  await page.waitForFunction(
    (expected) => Math.abs(window.nect.document.width - expected) < 1e-10,
    expectedWidth,
  );
  const uiResult = await saved();
  assert.equal(JSON.parse(uiResult).height, originalDocument.height);
  const serializedWidth = await page.evaluate(async () => {
    const { serialize, deserialize } = await import('/src/core.js');
    return deserialize(serialize(window.nect.document)).width;
  });
  assert.equal(serializedWidth, expectedWidth, 'project serialization keeps canonical px');

  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'Undo restores the exact original dimensions');

  const widthBeforeScrub = originalDocument.width,
    widthDisplayBeforeScrub = Number(await width.inputValue()),
    widthBox = await width.boundingBox();
  assert.ok(widthBox);
  const scrubX = widthBox.x + widthBox.width / 2,
    scrubY = widthBox.y + widthBox.height / 2;
  await page.mouse.move(scrubX, scrubY);
  await page.mouse.down();
  await page.mouse.move(scrubX + 9, scrubY, { steps: 1 });
  await page.mouse.up();
  const expectedScrubWidth = widthBeforeScrub + displayLengthToDocumentUnits(5, 'mm');
  await page.waitForFunction(
    (expected) => Math.abs(window.nect.document.width - expected) < 1e-10,
    expectedScrubWidth,
  );
  assert.equal(JSON.parse(await saved()).height, originalDocument.height);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'mm scrubbing commits canonical px and Undo restores it');
  assert.equal(Number(await width.inputValue()), widthDisplayBeforeScrub);

  await page.evaluate((value) => {
    const state = window.nect.commandState;
    window.nect.dispatch({
      ...state,
      type: 'set-document-settings',
      payload: { key: 'width', value },
    });
  }, expectedWidth);
  assert.equal(await saved(), uiResult, 'the existing px command matches unit-aware UI input');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'the existing command has the same Undo result');

  const exportWidth = displayLengthToDocumentUnits(100, 'mm'),
    exportHeight = displayLengthToDocumentUnits(50, 'mm'),
    exportDocument = await page.evaluate(
      ({ width, height }) => {
        const doc = structuredClone(window.nect.document);
        doc.width = width;
        doc.height = height;
        window.nect.load(JSON.stringify(doc));
        return JSON.stringify(window.nect.document);
      },
      { width: exportWidth, height: exportHeight },
    );
  assert.equal(JSON.parse(exportDocument).width, exportWidth);
  assert.equal(JSON.parse(exportDocument).height, exportHeight);
  const dpr1 = await exportPngAndInspect(page, 1),
    dpr2Context = await browser.newContext({
      viewport: { width: 1280, height: 900 },
      acceptDownloads: true,
      deviceScaleFactor: 2,
    });
  await dpr2Context.addInitScript(captureBrowserDownloads);
  try {
    const dpr2Page = await dpr2Context.newPage();
    await dpr2Page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
    await dpr2Page.waitForSelector('[data-artboard-unit]');
    await dpr2Page.evaluate((doc) => window.nect.load(doc), exportDocument);
    const dpr2 = await exportPngAndInspect(dpr2Page, 2);
    assert.deepEqual(
      { width: dpr2.width, height: dpr2.height },
      { width: dpr1.width, height: dpr1.height },
      'PNG dimensions are independent of device-pixel ratio',
    );
    const summary = {
      source: 'browser UI Export PNG action and actual canvas encoder',
      input: { artboardWidth: '100mm', artboardHeight: '50mm', ppi: 300 },
      expected: { width: 1181, height: 591, pixelsPerMetre: 11811 },
      outputs: [dpr1, dpr2],
    };
    await fs.writeFile(
      join(process.cwd(), 'test-results/png-100x50mm-300ppi-summary.json'),
      JSON.stringify(summary, null, 2),
    );
  } finally {
    await dpr2Context.close();
  }
} finally {
  await browser.close();
}
