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
  for (const { type, key, value, points } of [
    { type: 'Polygon', key: 'sides', value: 8, points: 8 },
    { type: 'Star', key: 'points', value: 6, points: 12 },
  ]) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node(type, 40, 80),
      fill = node('Fill', 340, 80);
    layer.nodes.push(shape, fill);
    connect(layer, shape.id, fill.id, 'path');
    connect(layer, fill.id, layer.output, 'path');
    await page.evaluate((value) => window.nect.load(value), serialize(doc));
    const card = page.locator(`.graph .node[data-id="${shape.id}"]`);
    await card.locator('.node-head strong').click();
    const input = page.locator(
        `.inspector-body .field[data-node="${shape.id}"][data-key="${key}"] [data-value]`,
      ),
      result = () => page.evaluate((id) => window.nect.evaluate().run(id), shape.id),
      beforeResult = await result(),
      before = await page.evaluate(() => JSON.stringify(window.nect.document));
    await input.fill(String(value));
    await input.press('Tab');
    const afterResult = await result(),
      after = await page.evaluate(() => JSON.stringify(window.nect.document));
    assert.equal(afterResult.contours[0].points.length, points);
    await page.locator('[data-action="undo"]').click();
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
    assert.deepEqual(await result(), beforeResult);
    await page.locator('[data-action="redo"]').click();
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
    assert.deepEqual(await result(), afterResult);
    await mkdir('test-results', { recursive: true });
    await page.screenshot({
      path: `test-results/node-foundation-${type.toLowerCase()}-${process.env.EVIDENCE_LABEL || 'current'}.png`,
    });
    await page.evaluate((value) => window.nect.load(value), after);
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
    assert.deepEqual(await result(), afterResult);
  }
  assert.deepEqual(errors, []);
  console.log('PASS Polygon and Star rendering, count, Undo/Redo and reload');
} finally {
  await browser.close();
}
