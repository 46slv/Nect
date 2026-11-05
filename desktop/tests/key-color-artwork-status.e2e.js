import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';

const output = resolve('test-results/key-color-artwork-status');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
// Only synthetic data is loaded; font bytes and user documents are not accessed.
await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (route) => route.abort());

try {
  const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);
  const fixture = retainedRecipeFixture();
  delete fixture.keyColorRecipe;
  await page.evaluate((text) => window.nect.load(text), serialize(fixture));
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-key-color-recipe-open]').click();
  await page.locator('[data-research-hex]').fill('#3273c7');
  await page.locator('[data-research-alpha]').fill('1');
  await page.locator('[data-research-plan]').selectOption('research-neutral-start');
  await page.locator('[data-research-preview]').click();
  await page.locator('[data-research-create]').click();

  const project = await page.evaluate(() => window.nect.projectSnapshot());
  const slotId = project.keyColorRecipe.committedSnapshot.intent.slots[0].id;
  const boundLayer = project.compositions[0].layers[0];
  boundLayer.nodes.find((node) => node.type === 'Fill').params.color.colorSlotId = slotId;
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(project));
  const before = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  const artboard = page.locator('.artboard');
  const saved = await artboard.innerHTML();
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-key-color-recipe-open]').click();
  const delta = page.locator(`[data-key-color-input="deltaL"][data-key-color-slot="${slotId}"]`);
  await delta.fill('0.2');
  await page.locator('[data-key-color-preview]').click();
  await page.locator('[data-artwork-show]').click();
  const projected = await artboard.innerHTML();
  assert.notEqual(projected, saved);
  await page.locator('[data-artwork-restore]').click();
  assert.equal(await artboard.innerHTML(), saved);
  assert.equal(await page.locator('[data-artwork-show]').getAttribute('aria-pressed'), 'false');
  assert.match(await page.locator('[data-key-color-status]').textContent(), /artwork preview off/i);
  await page.locator('[data-key-color-status]').scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'restored.png') });

  await page.locator('[data-artwork-show]').click();
  assert.equal(await artboard.innerHTML(), projected);
  await delta.evaluate((input) => {
    input.value = '0.4'; // Deliberately omit input/change before a real resize.
  });
  await page.setViewportSize({ width: 1400, height: 1000 });
  await page.waitForFunction(() => document.querySelector('[data-key-color-apply]').disabled);
  await page.waitForFunction(
    (original) => document.querySelector('.artboard').innerHTML === original,
    saved,
  );
  assert.equal(await page.locator('[data-artwork-show]').getAttribute('aria-pressed'), 'false');
  assert.equal(await page.locator('[data-key-color-comparison]').textContent(), '');
  assert.match(await page.locator('[data-key-color-status]').textContent(), /draft only/i);
  await page.locator('[data-key-color-status]').scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'expired.png') });
  await page.locator('dialog footer [data-key-color-cancel]').click();
  assert.deepEqual(
    await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      state: window.nect.commandState,
    })),
    before,
  );
  assert.deepEqual(errors, []);
  console.log('Artwork status: browser Restore and silent resize expiration passed.');
} finally {
  await browser.close();
}
