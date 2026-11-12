import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  text = node('Text', 40, 80),
  outline = node('Text to Path', 340, 80),
  fill = node('Fill', 640, 80);
text.text = 'Nect';
layer.nodes.push(text, outline, fill);
connect(layer, text.id, outline.id, 'path');
connect(layer, outline.id, fill.id, 'path');
connect(layer, fill.id, layer.output, 'path');

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
  await page.waitForFunction((id) => {
    try {
      return window.nect.evaluate().run(id).contours.length > 0;
    } catch {
      return false;
    }
  }, outline.id);
  await ensureWorkspacePanel(page, 'inspector');
  const card = page.locator(`.graph .node[data-id="${outline.id}"]`);
  await card.locator('.node-head strong').click();
  assert.match(
    await card.locator(`[data-source="${outline.id}"]`).getAttribute('title'),
    /Path output/,
  );
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), outline.id),
    beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.equal(beforeResult.characterOccurrences[0].source, text.id);
  assert.equal(beforeResult.characterOccurrences[0].reason, null);
  await card.locator(`[data-bypass="${outline.id}"]`).click();
  const bypassed = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.notEqual(bypassed, before);
  assert.deepEqual(await result(), beforeResult);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), bypassed);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-text-to-path-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((value) => window.nect.load(value), bypassed);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), bypassed);
  assert.deepEqual(await result(), beforeResult);
  assert.deepEqual(errors, []);
  console.log('PASS Text to Path rendering, bypass, Undo/Redo and reload');
} finally {
  await browser.close();
}
