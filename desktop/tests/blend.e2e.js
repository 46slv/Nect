import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0];
const a = node('Rectangle', 40, 40),
  b = node('Rectangle', 40, 300),
  fa = node('Fill', 240, 40),
  fb = node('Fill', 240, 300),
  blend = node('Blend', 440, 100);
a.params.center.value = [120, 160];
b.params.center.value = [420, 160];
fa.params.color.value = { hex: '#000000', alpha: 1 };
fb.params.color.value = { hex: '#ffffff', alpha: 1 };
blend.params.steps.value = 1;
layer.nodes.push(a, b, fa, fb, blend);
connect(layer, a.id, fa.id, 'path');
connect(layer, b.id, fb.id, 'path');
connect(layer, fa.id, blend.id, 'a');
connect(layer, fb.id, blend.id, 'b');
connect(layer, blend.id, layer.output, 'path');
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate((value) => window.nect.load(value), serialize(doc));
  await ensureWorkspacePanel(page, 'inspector');
  await page.locator(`.graph .node[data-id="${blend.id}"] .node-head strong`).click();
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), blend.id);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  const initial = await result();
  assert.equal(initial.children.length, 3);
  assert.equal(initial.children[1].fill.hex, '#bcbcbc');
  const input = page.locator(
    `.inspector-body .field[data-node="${blend.id}"][data-key="steps"] [data-value]`,
  );
  await input.fill('4');
  await input.press('Tab');
  const after = await page.evaluate(() => JSON.stringify(window.nect.document));
  const changed = await result();
  assert.equal(changed.children.length, 6);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await result(), initial);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), changed);
  await page.evaluate((value) => window.nect.load(value), after);
  assert.deepEqual(await result(), changed);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/blend-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  assert.deepEqual(errors, []);
  console.log(
    'PASS Blend inspector steps, linear-light fill, Undo/Redo and reload (headless Chrome)',
  );
} finally {
  await browser.close();
}
