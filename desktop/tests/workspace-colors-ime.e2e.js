import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';

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
await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (route) => route.abort());

try {
  const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-shared-color-add]').click();
  await page.locator('[data-color-advanced] summary').click();
  const input = page.locator('[data-shared-color-edit="name"]').last();
  const id = await input.getAttribute('data-shared-color-id');
  const initial = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  await input.fill('編集中');
  await input.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  for (const selector of [
    '[data-workspace-panel="assets"]',
    '[data-workspace-panel="layers"]',
    '[data-workspace-panel="colors"]',
    '[data-shared-color-add]',
    '[data-shared-color-close]',
    '.viewer-tabs [data-viewer-mode="viewers"]',
    '[data-key-color-recipe-open]',
  ]) {
    const target = page.locator(selector);
    await target.click(); // Real pointer events must not cause native focusout first.
    await target.evaluate((element) => element.click()); // Keyboard-style activation has no pointerdown.
    assert.equal(
      await input.evaluate((element) => element === document.activeElement),
      true,
      selector,
    );
    assert.equal(await input.inputValue(), '編集中', selector);
    assert.equal(
      await page.locator('[data-workspace-panel="colors"]').getAttribute('aria-expanded'),
      'true',
    );
    assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'artwork');
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), initial.state);
    assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), initial.project);
  }
  await input.evaluate((element) => {
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true }));
    element.dispatchEvent(new InputEvent('input', { bubbles: true, data: '中' }));
  });
  await page.locator('[data-workspace-panel="assets"]').click();
  assert.equal(
    await page.locator('[data-workspace-panel="assets"]').getAttribute('aria-expanded'),
    'true',
  );
  assert.equal(
    await page.evaluate(
      (slotId) => window.nect.projectSnapshot().colorSlots.find((slot) => slot.id === slotId).name,
      id,
    ),
    '編集中',
  );
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    initial.state.expectedRevision + 1,
  );
  await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo', payload: {} }),
  );
  assert.notEqual(
    await page.evaluate(
      (slotId) => window.nect.projectSnapshot().colorSlots.find((slot) => slot.id === slotId).name,
      id,
    ),
    '編集中',
  );
  await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'redo', payload: {} }),
  );
  assert.equal(
    await page.evaluate(
      (slotId) => window.nect.projectSnapshot().colorSlots.find((slot) => slot.id === slotId).name,
      id,
    ),
    '編集中',
  );
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('.viewer-tabs [data-viewer-mode="viewers"]').click();
  assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'viewers');
  await page.locator('.viewer-tabs [data-viewer-mode="artwork"]').click();
  assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'artwork');

  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-shared-color-add]').click();
  const secondId = (await page.evaluate(() => window.nect.projectSnapshot())).colorSlots.at(-1).id;
  const firstName = page.locator(`[data-shared-color-edit="name"][data-shared-color-id="${id}"]`);
  const secondName = page.locator(
    `[data-shared-color-edit="name"][data-shared-color-id="${secondId}"]`,
  );
  if (!(await page.locator('[data-color-advanced]').evaluate((element) => element.open)))
    await page.locator('[data-color-advanced] summary').click();
  const beforeSibling = (await page.evaluate(() => window.nect.commandState)).expectedRevision;
  await firstName.fill('First edited');
  await secondName.click();
  await page.evaluate(
    () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
  );
  assert.equal(await secondName.evaluate((element) => element === document.activeElement), true);
  assert.equal(await secondName.inputValue(), 'Color 2');
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    beforeSibling + 1,
  );
  await secondName.fill('Second edited');
  await page.locator('[data-workspace-panel="assets"]').click();
  let project = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(project.colorSlots.find((slot) => slot.id === id).name, 'First edited');
  assert.equal(project.colorSlots.find((slot) => slot.id === secondId).name, 'Second edited');
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    beforeSibling + 2,
  );

  await ensureWorkspacePanel(page, 'colors');
  if (!(await page.locator('[data-color-advanced]').evaluate((element) => element.open)))
    await page.locator('[data-color-advanced] summary').click();
  await firstName.focus();
  const beforeUnchanged = (await page.evaluate(() => window.nect.commandState)).expectedRevision;
  await page.locator('[data-workspace-panel="colors"]').click();
  assert.equal(
    await page
      .locator('[data-workspace-panel="colors"]')
      .evaluate((element) => element === document.activeElement),
    true,
  );
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    beforeUnchanged,
  );

  await ensureWorkspacePanel(page, 'colors');
  if (!(await page.locator('[data-color-advanced]').evaluate((element) => element.open)))
    await page.locator('[data-color-advanced] summary').click();
  await firstName.fill('');
  await page.locator('[data-workspace-panel="assets"]').click();
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    beforeUnchanged,
  );
  project = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(project.colorSlots.find((slot) => slot.id === id).name, 'First edited');
  assert.match(await page.locator('.status .message').textContent(), /Invalid Color slot name/);
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(project));
  const reopened = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(reopened.colorSlots.find((slot) => slot.id === id).name, 'First edited');
  assert.equal(reopened.colorSlots.find((slot) => slot.id === secondId).name, 'Second edited');
  assert.deepEqual(errors, []);
  console.log(
    'Shared Colors: synthetic composition, pointer protection, commit and Undo/Redo passed.',
  );
} finally {
  await browser.close();
}
