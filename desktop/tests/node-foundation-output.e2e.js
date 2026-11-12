import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  output = layer.nodes[0],
  rectangle = node('Rectangle', 40, 80),
  fill = node('Fill', 340, 80);
output.position = [640, 80];
layer.nodes.push(rectangle, fill);
connect(layer, rectangle.id, fill.id, 'path');
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } }),
  errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate((value) => window.nect.load(value), serialize(doc));
  const card = page.locator(`.graph .node[data-id="${output.id}"]`),
    input = card.locator(`.field[data-key="path"] [data-path-source]`),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.deepEqual(await page.evaluate(() => window.nect.evaluate().path), { contours: [] });
  await input.selectOption(fill.id);
  const connected = await page.evaluate(() => JSON.stringify(window.nect.document)),
    result = await page.evaluate(() => window.nect.evaluate().path);
  assert.ok(result.contours.length > 0);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await page.evaluate(() => window.nect.evaluate().path), { contours: [] });
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), connected);
  assert.deepEqual(await page.evaluate(() => window.nect.evaluate().path), result);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-output-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((value) => window.nect.load(value), connected);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), connected);
  assert.deepEqual(await page.evaluate(() => window.nect.evaluate().path), result);
  assert.deepEqual(errors, []);
  console.log('PASS Output wiring, rendering, Undo/Redo and reload');
} finally {
  await browser.close();
}
