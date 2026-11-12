import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
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
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate(async () => {
    const { blankDocument } = await import('/src/core.js');
    window.nect.load(JSON.stringify(blankDocument()));
  });
  await ensureWorkspacePanel(page, 'inspector');
  const addToggle = async () => {
    await page.locator('#add-node').click();
    const search = page.locator('.search-panel input').first();
    await search.fill('Toggle');
    await page.locator('.picker-option[data-id="Toggle"]').click();
    await search.press('Enter');
    await page.waitForFunction(() => !document.querySelector('.search-panel'));
  };
  await addToggle();
  const id = await page.evaluate(
    () => window.nect.document.layers[0].nodes.find((entry) => entry.type === 'Toggle').id,
  );
  const card = page.locator(`.graph .node[data-id="${id}"]`);
  await card.locator('.node-head strong').click();
  const field = page.locator(`.inspector-body .field[data-node="${id}"][data-key="value"]`);
  const input = field.locator('[data-value]');
  assert.equal(await input.getAttribute('type'), 'checkbox');
  assert.equal(await input.isChecked(), true);
  const output = card.locator(`[data-source="${id}"]:not([data-source-port])`);
  assert.match(await output.getAttribute('title'), /Boolean output/);
  assert.equal(await page.evaluate((nodeId) => window.nect.evaluate().run(nodeId), id), true);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await input.uncheck();
  assert.equal(await page.evaluate((nodeId) => window.nect.evaluate().run(nodeId), id), false);
  const after = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.equal(await input.isChecked(), true);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.equal(await input.isChecked(), false);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-toggle-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await addToggle();
  const targetId = await page.evaluate(
    () => window.nect.document.layers[0].nodes.filter((entry) => entry.type === 'Toggle').at(-1).id,
  );
  const target = page.locator(`.graph .field[data-node="${targetId}"][data-key="value"]`);
  const beforeConnect = await page.evaluate(() => JSON.stringify(window.nect.document));
  const sourceBox = await output.boundingBox(),
    targetBox = await target.boundingBox();
  assert.ok(sourceBox && targetBox);
  await page.mouse.move(sourceBox.x + sourceBox.width / 2, sourceBox.y + sourceBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(targetBox.x + 40, targetBox.y + targetBox.height / 2, { steps: 5 });
  assert.equal(await target.evaluate((element) => element.classList.contains('compatible')), true);
  await page.mouse.up();
  const connected = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.equal(
    await page.evaluate(
      (nodeId) =>
        window.nect.document.layers[0].nodes.find((entry) => entry.id === nodeId).params.value
          .source,
      targetId,
    ),
    id,
  );
  assert.equal(
    await page.evaluate((nodeId) => window.nect.evaluate().run(nodeId), targetId),
    false,
  );
  assert.equal(await page.locator('.graph .wire').count(), 1);
  await page.screenshot({
    path: `test-results/node-foundation-toggle-wire-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), beforeConnect);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), connected);
  await page.evaluate((text) => window.nect.load(text), connected);
  assert.equal(
    await page.evaluate((nodeId) => window.nect.evaluate().run(nodeId), targetId),
    false,
  );
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), connected);
  assert.deepEqual(errors, []);
  console.log('PASS Toggle picker, Boolean wiring, Inspector, evaluation, Undo/Redo and reload');
} finally {
  await browser.close();
}
