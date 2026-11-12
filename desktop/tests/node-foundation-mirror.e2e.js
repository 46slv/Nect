import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  path = node('Imported Path', 40, 80),
  mirror = node('Mirror', 340, 80),
  stroke = node('Stroke', 640, 80);
path.rawPath = 'M360 200 L420 220 L460 260 L500 220 L540 200';
stroke.params.width.value = 5;
layer.nodes.push(path, mirror, stroke);
connect(layer, path.id, mirror.id, 'path');
connect(layer, mirror.id, stroke.id, 'path');
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
  const card = page.locator(`.graph .node[data-id="${mirror.id}"]`);
  await card.locator('.node-head strong').click();
  const keep = page.locator(
    `.inspector-body .field[data-node="${mirror.id}"][data-key="keepOriginal"] [data-value]`,
  );
  assert.equal(await keep.isChecked(), false);
  await keep.check();
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((item) => item.id === id).params.keepOriginal.value,
    mirror.id,
  );
  const direction = page.locator(`.graph [data-operation="${mirror.id}"]`),
    result = () => page.evaluate((id) => window.nect.evaluate().run(id), mirror.id);
  assert.equal(await direction.inputValue(), 'left-right');
  const beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await direction.selectOption('angle');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((item) => item.id === id).operation === 'angle',
    mirror.id,
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
    path: `test-results/node-foundation-mirror-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Mirror rendering, direction, live copy, Undo/Redo and reload');
} finally {
  await browser.close();
}
