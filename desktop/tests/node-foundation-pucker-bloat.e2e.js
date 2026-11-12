import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  shape = node('Rectangle', 40, 80),
  effect = node('Pucker & Bloat', 340, 80),
  fill = node('Fill', 640, 80);
effect.params.amount.value = 30;
layer.nodes.push(shape, effect, fill);
connect(layer, shape.id, effect.id, 'path');
connect(layer, effect.id, fill.id, 'path');
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
  const card = page.locator(`.graph .node[data-id="${effect.id}"]`);
  await card.locator('.node-head strong').click();
  const field = (key) =>
    page.locator(`.inspector-body .field[data-node="${effect.id}"][data-key="${key}"]`);
  assert.equal(await field('amount').locator('[data-value]').inputValue(), '30');
  assert.equal(await field('center').count(), 0);
  const autoCenter = field('autoCenter').locator('[data-value]');
  assert.equal(await autoCenter.isChecked(), true);
  await autoCenter.uncheck();
  const center = field('center').locator('[data-value][data-axis="0"]');
  assert.equal(await center.inputValue(), '0');
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), effect.id),
    beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await center.fill('50');
  await center.press('Tab');
  await page.waitForFunction(
    ([id, fingerprint]) => window.nect.evaluate().run(id).fingerprint !== fingerprint,
    [effect.id, beforeResult.fingerprint],
  );
  const afterResult = await result(),
    after = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.notDeepEqual(afterResult.contours, beforeResult.contours);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await result(), beforeResult);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-pucker-bloat-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Pucker & Bloat rendering, manual center, Undo/Redo and reload');
} finally {
  await browser.close();
}
