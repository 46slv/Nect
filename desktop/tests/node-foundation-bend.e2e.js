import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  path = node('Imported Path', 40, 80),
  bend = node('Bend Points', 340, 80),
  stroke = node('Stroke', 640, 80);
path.rawPath = 'M100 200 L175 200 L250 250 L325 200 L400 200';
stroke.params.width.value = 5;
layer.nodes.push(path, bend, stroke);
connect(layer, path.id, bend.id, 'path');
connect(layer, bend.id, stroke.id, 'path');
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
  const card = page.locator(`.graph .node[data-id="${bend.id}"]`);
  await card.locator('.node-head strong').click();
  const input = page.locator(
    `.inspector-body .field[data-node="${bend.id}"][data-key="angle"] [data-value]`,
  );
  assert.equal(await input.inputValue(), '0');
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), bend.id),
    beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await input.fill('5');
  await input.press('Tab');
  await page.waitForFunction(
    ([id, fingerprint]) => window.nect.evaluate().run(id).fingerprint !== fingerprint,
    [bend.id, beforeResult.fingerprint],
  );
  const angleResult = await result(),
    angleState = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.notDeepEqual(angleResult.contours, beforeResult.contours);
  const axis = page.locator(`.graph [data-operation="${bend.id}"]`);
  assert.equal(await axis.inputValue(), 'horizontal');
  await axis.selectOption('vertical');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((item) => item.id === id).operation === 'vertical',
    bend.id,
  );
  const afterResult = await result(),
    after = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.notDeepEqual(afterResult.contours, angleResult.contours);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), angleState);
  assert.deepEqual(await result(), angleResult);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await result(), beforeResult);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), angleState);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-bend-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Bend Points rendering, angle, Undo/Redo and reload');
} finally {
  await browser.close();
}
