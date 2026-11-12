import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  shape = node('Rectangle', 40, 80),
  paint = node('Fill', 340, 80),
  converter = node('Convert to Path', 640, 80),
  repaint = node('Fill', 940, 80);
layer.nodes.push(shape, paint, converter, repaint);
connect(layer, shape.id, paint.id, 'path');
connect(layer, shape.id, converter.id, 'path');
connect(layer, converter.id, repaint.id, 'path');
connect(layer, repaint.id, layer.output, 'path');

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
  const card = page.locator(`.graph .node[data-id="${converter.id}"]`),
    input = card.locator('.field[data-key="path"] [data-path-source]'),
    bypass = card.locator(`[data-bypass="${converter.id}"]`),
    result = () => page.evaluate((id) => window.nect.evaluate().run(id), converter.id),
    saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    before = await saved(),
    plain = await result();
  assert.ok(plain.contours.length > 0);
  assert.equal(await bypass.isDisabled(), false);
  await bypass.click();
  const bypassed = await saved();
  assert.notEqual(bypassed, before);
  assert.deepEqual(await result(), plain);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), bypassed);
  await page.locator('[data-action="undo"]').click();

  await input.selectOption(paint.id);
  const painted = await saved(),
    converted = await result();
  assert.notEqual(painted, before);
  assert.ok(converted.fingerprint);
  assert.equal(Object.hasOwn(converted, 'fill'), false);
  assert.equal(Object.hasOwn(converted, 'stroke'), false);
  assert.equal(await bypass.isDisabled(), true);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  assert.deepEqual(await result(), plain);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), painted);
  assert.deepEqual(await result(), converted);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-convert-to-path-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((value) => window.nect.load(value), painted);
  assert.equal(await saved(), painted);
  assert.deepEqual(await result(), converted);
  assert.deepEqual(errors, []);
  console.log('PASS Convert to Path source type, bypass, Undo/Redo and reload');
} finally {
  await browser.close();
}
