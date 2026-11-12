import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  path = node('Imported Path', 40, 80),
  auto = node('Auto Handles', 340, 80),
  stroke = node('Stroke', 640, 80);
path.rawPath = 'M100 200 L175 200 L250 250 L325 200 L400 200';
stroke.params.width.value = 5;
layer.nodes.push(path, auto, stroke);
connect(layer, path.id, auto.id, 'path');
connect(layer, auto.id, stroke.id, 'path');
connect(layer, stroke.id, layer.output, 'path');

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
  const card = page.locator(`.graph .node[data-id="${auto.id}"]`);
  await card.locator('.node-head strong').click();
  const input = page.locator(
    `.inspector-body .field[data-node="${auto.id}"][data-key="strength"] [data-value]`,
  );
  assert.equal(await input.inputValue(), '1');
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), auto.id),
    beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await input.fill('0');
  await input.press('Tab');
  await page.waitForFunction(
    ([id, fingerprint]) => window.nect.evaluate().run(id).fingerprint !== fingerprint,
    [auto.id, beforeResult.fingerprint],
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
    path: `test-results/node-foundation-auto-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Auto Handles rendering, strength, Undo/Redo and reload');
} finally {
  await browser.close();
}
