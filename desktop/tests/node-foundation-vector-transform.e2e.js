import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  vector = node('Vector', 40, 80),
  transform = node('Vector Transform', 340, 80);
layer.nodes.push(vector, transform);
connect(layer, vector.id, transform.id, 'vector');
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
  await ensureWorkspacePanel(page, 'inspector');
  const card = page.locator(`.graph .node[data-id="${transform.id}"]`);
  await card.locator('.node-head strong').click();
  const input = page.locator(
    `.inspector-body .field[data-node="${transform.id}"][data-key="rotation"] [data-value]`,
  );
  assert.equal(await input.inputValue(), '0');
  assert.match(
    await card.locator(`[data-source="${transform.id}"]`).getAttribute('title'),
    /Vector2 output/,
  );
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), transform.id),
    beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await input.fill('90');
  await input.press('Tab');
  const afterResult = await result(),
    after = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.notDeepEqual(afterResult, beforeResult);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await result(), beforeResult);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-vector-transform-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((value) => window.nect.load(value), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Vector Transform rendering, rotation, Undo/Redo and reload');
} finally {
  await browser.close();
}
