import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

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
  await ensureWorkspacePanel(page, 'inspector');
  for (const { type, key, value } of [
    { type: 'Trim Path', key: 'start', value: 0.25 },
    { type: 'Set Path Start', key: 'start', value: 0.25 },
    { type: 'Dash Path', key: 'dashLength', value: 30 },
  ]) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      path = node('Imported Path', 40, 80),
      effect = node(type, 340, 80),
      stroke = node('Stroke', 640, 80);
    path.rawPath = 'M100 100 L300 100 L300 300 L100 300 Z';
    layer.nodes.push(path, effect, stroke);
    connect(layer, path.id, effect.id, 'path');
    connect(layer, effect.id, stroke.id, 'path');
    connect(layer, stroke.id, layer.output, 'path');
    await page.evaluate((value) => window.nect.load(value), serialize(doc));
    const card = page.locator(`.graph .node[data-id="${effect.id}"]`);
    await card.locator('.node-head strong').click();
    const input = page.locator(
        `.inspector-body .field[data-node="${effect.id}"][data-key="${key}"] [data-value]`,
      ),
      result = () => page.evaluate((id) => window.nect.evaluate().run(id), effect.id),
      beforeResult = await result(),
      before = await page.evaluate(() => JSON.stringify(window.nect.document));
    await input.fill(String(value));
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
      path: `test-results/node-foundation-${type.toLowerCase().replaceAll(' ', '-')}-${process.env.EVIDENCE_LABEL || 'current'}.png`,
    });
    await page.evaluate((value) => window.nect.load(value), after);
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
    assert.deepEqual(await result(), afterResult);
  }
  assert.deepEqual(errors, []);
  console.log('PASS Trim, Set Path Start and Dash rendering, edits, Undo/Redo and reload');
} finally {
  await browser.close();
}
