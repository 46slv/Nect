import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  path = node('Imported Path', 40, 80),
  wave = node('Wave Points', 340, 80),
  stroke = node('Stroke', 640, 80);
path.rawPath = 'M100 200 L175 200 L250 200 L325 200 L400 200';
wave.params.amplitude.value = 35;
wave.params.wavelength.value = 150;
wave.params.phase.value = 45;
stroke.params.width.value = 5;
layer.nodes.push(path, wave, stroke);
connect(layer, path.id, wave.id, 'path');
connect(layer, wave.id, stroke.id, 'path');
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
  const card = page.locator(`.graph .node[data-id="${wave.id}"]`);
  await card.locator('.node-head strong').click();
  const keep = page.locator(
    `.inspector-body .field[data-node="${wave.id}"][data-key="keepEndpoints"] [data-value]`,
  );
  assert.equal(await keep.isChecked(), false);
  await keep.check();
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((item) => item.id === id).params.keepEndpoints
        .value,
    wave.id,
  );
  const direction = page.locator(`.graph [data-operation="${wave.id}"]`),
    result = () => page.evaluate((id) => window.nect.evaluate().run(id), wave.id);
  assert.equal(await direction.inputValue(), 'vertical');
  const beforeResult = await result(),
    before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await direction.selectOption('horizontal');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((item) => item.id === id).operation ===
      'horizontal',
    wave.id,
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
    path: `test-results/node-foundation-wave-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Wave Points rendering, direction, endpoints, Undo/Redo and reload');
} finally {
  await browser.close();
}
