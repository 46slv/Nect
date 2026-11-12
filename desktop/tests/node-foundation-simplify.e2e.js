import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  shape = node('Rectangle', 40, 80),
  resample = node('Resample Path', 340, 80),
  simplify = node('Simplify Path', 640, 80),
  fill = node('Fill', 940, 80);
layer.nodes.push(shape, resample, simplify, fill);
connect(layer, shape.id, resample.id, 'path');
connect(layer, resample.id, simplify.id, 'path');
connect(layer, simplify.id, fill.id, 'path');
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
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  await ensureWorkspacePanel(page, 'inspector');
  const card = page.locator(`.graph .node[data-id="${simplify.id}"]`);
  await card.locator('.node-head strong').click();
  const field = page.locator(
      `.inspector-body .field[data-node="${simplify.id}"][data-key="tolerance"]`,
    ),
    input = field.locator('[data-value]');
  assert.equal(await input.inputValue(), '1');
  assert.match(
    await card.locator(`[data-source="${simplify.id}"]`).getAttribute('title'),
    /Path output/,
  );
  const count = () =>
    page.evaluate((id) => window.nect.evaluate().run(id).contours[0].points.length, simplify.id);
  assert.equal(await count(), 6);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await input.fill('0');
  await input.press('Tab');
  await page.waitForFunction(
    (id) => window.nect.evaluate().run(id).contours[0].points.length === 32,
    simplify.id,
  );
  const after = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.equal(await count(), 6);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.equal(await count(), 32);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-simplify-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await count(), 32);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(errors, []);
  console.log('PASS Simplify Path rendering, tolerance, Undo/Redo and reload');
} finally {
  await browser.close();
}
