import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  output = layer.nodes[0],
  shape = node('Rectangle', 40, 80),
  stroke = node('Stroke', 340, 80),
  fill = node('Fill', 640, 80),
  first = node('Color', 340, 420),
  second = node('Color', 640, 420);
doc.schemaVersion = 54;
doc.colorSlots = ['first', 'second', 'stroke'].map((id) => ({
  id,
  name: id,
  value: { hex: '#123456', alpha: 0.5 },
}));
first.params.value.colorSlotId = 'first';
second.params.value.colorSlotId = 'second';
stroke.params.color.colorSlotId = 'stroke';
layer.nodes = [shape, stroke, fill, first, second, output];
connect(layer, shape.id, stroke.id, 'path');
connect(layer, stroke.id, fill.id, 'path');
connect(layer, first.id, fill.id, 'color');
connect(layer, fill.id, output.id, 'path');

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
  const card = page.locator(`.graph .node[data-id="${fill.id}"]`),
    field = page.locator(`.inspector-body .field[data-node="${fill.id}"][data-key="color"]`),
    saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    scene = () =>
      page.evaluate(async () => {
        const { exportScene } = await import('/src/core.js');
        const result = exportScene(window.nect.document)[0];
        return { value: result.value, paintProvenance: result.paintProvenance };
      });
  await card.locator('.node-head strong').click();
  const before = await saved(),
    painted = await scene();
  assert.deepEqual(painted.paintProvenance, { stroke: 'stroke', fill: 'first' });
  await field.locator('[data-connect-menu]').click();
  await page.locator('dialog select').selectOption(second.id);
  await page.locator('dialog [data-confirm]').click();
  const rewired = await saved(),
    changed = await scene();
  assert.deepEqual(changed.value, painted.value);
  assert.deepEqual(changed.paintProvenance, { stroke: 'stroke', fill: 'second' });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  assert.deepEqual((await scene()).paintProvenance, { stroke: 'stroke', fill: 'first' });
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), rewired);
  assert.deepEqual((await scene()).paintProvenance, { stroke: 'stroke', fill: 'second' });
  await card.locator(`[data-bypass="${fill.id}"]`).click();
  const bypassed = await scene();
  assert.equal(bypassed.value.fill, undefined);
  assert.deepEqual(bypassed.value.stroke, changed.value.stroke);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), rewired);
  assert.deepEqual(await scene(), changed);
  await page.evaluate((value) => window.nect.load(value), rewired);
  assert.equal(await saved(), rewired);
  assert.deepEqual(await scene(), changed);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-fill-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  assert.deepEqual(errors, []);
  console.log('PASS Fill source, equal-color slot identity, bypass, Undo/Redo and reload');
} finally {
  await browser.close();
}
