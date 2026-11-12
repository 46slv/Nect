import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  path = node('Imported Path', 40, 80),
  stroke = node('Stroke', 340, 80),
  expand = node('Expand Stroke', 640, 80),
  fill = node('Fill', 940, 80);
path.rawPath = 'M100 200 L300 200 L400 250';
stroke.params.width.value = 20;
layer.nodes.push(path, stroke, expand, fill);
connect(layer, path.id, stroke.id, 'path');
connect(layer, stroke.id, expand.id, 'path');
connect(layer, expand.id, fill.id, 'path');
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
  const card = page.locator(`.graph .node[data-id="${expand.id}"]`);
  await card.locator('.node-head strong').click();
  const field = page.locator(
      `.inspector-body .field[data-node="${expand.id}"][data-key="startWidthMultiplier"]`,
    ),
    input = field.locator('[data-value]');
  assert.equal(await input.inputValue(), '1');
  assert.equal(await input.getAttribute('min'), '0');
  assert.equal(await input.getAttribute('max'), '4');
  assert.match(
    await card.locator(`[data-source="${expand.id}"]`).getAttribute('title'),
    /Path output/,
  );
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), expand.id);
  const beforeResult = await result();
  assert.ok(beforeResult.contours.length);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await input.fill('0.5');
  await input.press('Tab');
  await page.waitForFunction(
    ([id, fingerprint]) => window.nect.evaluate().run(id).fingerprint !== fingerprint,
    [expand.id, beforeResult.fingerprint],
  );
  const afterResult = await result();
  const after = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.notDeepEqual(afterResult.contours, beforeResult.contours);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await result(), beforeResult);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-expand-stroke-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.evaluate((text) => window.nect.load(text), after);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), afterResult);
  assert.deepEqual(errors, []);
  console.log('PASS Expand Stroke rendering, width, Undo/Redo and reload');
} finally {
  await browser.close();
}
