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
  await page.evaluate((slotId) => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-color-slot',
      payload: { slotId, value: { hex: '#abcdef', alpha: 0.5 } },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo', payload: {} });
  }, id);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  const initial = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  await input.fill('編集中');
  await input.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await input.click(); // Repositioning the caret in the composing field stays available.
  assert.equal(await input.evaluate((element) => element === document.activeElement), true);
  // A synthetic composition plus real keyboard navigation exercises the browser's
  // focus behavior without claiming physical IME coverage.
  for (const key of ['Tab', 'Shift+Tab']) {
    await page.keyboard.press(key);
    assert.equal(await input.evaluate((element) => element === document.activeElement), true, key);
    assert.equal(await input.inputValue(), '編集中', key);
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), initial.state);
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  }
  for (const selector of [
    '[data-workspace-panel="assets"]',
    '[data-workspace-panel="layers"]',
    '[data-workspace-panel="colors"]',
    '[data-shared-color-add]',
    '[data-shared-color-close]',
    '[data-shared-color-copy-toggle]',
    '[data-shared-color-edit="alpha"]',
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
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  }
  await input.evaluate((element) => {
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true }));
    element.dispatchEvent(new InputEvent('input', { bubbles: true, data: '中' }));
  });
  await page.keyboard.press('Tab');
  await page.waitForFunction(
    (revision) => window.nect.commandState.expectedRevision === revision + 1,
    initial.state.expectedRevision,
  );
  assert.equal(await input.evaluate((element) => element === document.activeElement), false);
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
  assert.equal(await firstName.evaluate((element) => element === document.activeElement), true);
  assert.equal(await firstName.inputValue(), '');
  assert.equal(
    await page.locator('[data-workspace-panel="colors"]').getAttribute('aria-expanded'),
    'true',
  );
  assert.match(await page.locator('.status .message').textContent(), /Enter a shared color name/);
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(project));
  const reopened = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(reopened.colorSlots.find((slot) => slot.id === id).name, 'First edited');
  assert.equal(reopened.colorSlots.find((slot) => slot.id === secondId).name, 'Second edited');

  // A keyboard-style activation must not hide the owner while a real scrub is active.
  await ensureWorkspacePanel(page, 'colors');
  if (!(await page.locator('[data-color-advanced]').evaluate((element) => element.open)))
    await page.locator('[data-color-advanced] summary').click();
  const alpha = page.locator(`[data-shared-color-edit="alpha"][data-shared-color-id="${id}"]`);
  await alpha.scrollIntoViewIfNeeded();
  const box = await alpha.boundingBox();
  const x = box.x + box.width * 0.4;
  const y = box.y + box.height / 2;
  const beforeScrub = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    state: window.nect.commandState,
  }));
  await page.mouse.move(x, y);
  await page.mouse.down();
  await page.mouse.move(x - 24, y, { steps: 5 });
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  const focusedDuringScrub = await alpha.evaluate((element) => document.activeElement === element);
  assert.equal(focusedDuringScrub, true);
  await page.keyboard.press('Tab');
  assert.equal(
    await alpha.evaluate((element) => document.activeElement === element),
    focusedDuringScrub,
  );
  for (const selector of [
    '[data-workspace-panel="assets"]',
    '[data-workspace-panel="colors"]',
    '[data-shared-color-close]',
    '.viewer-tabs [data-viewer-mode="viewers"]',
  ]) {
    await page.locator(selector).evaluate((element) => element.click());
    assert.equal(
      await page.locator('[data-workspace-panel="colors"]').getAttribute('aria-expanded'),
      'true',
    );
    assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'artwork');
    assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), beforeScrub.project);
  }
  await page.mouse.up();
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
  const scrubbed = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    beforeScrub.state.expectedRevision + 1,
  );
  await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo', payload: {} }),
  );
  assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), beforeScrub.project);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await alpha.scrollIntoViewIfNeeded();
  const retryBox = await alpha.boundingBox();
  const retryX = retryBox.x + retryBox.width * 0.4;
  const retryY = retryBox.y + retryBox.height / 2;
  await page.mouse.move(retryX, retryY);
  await page.mouse.down();
  await page.mouse.move(retryX - 24, retryY, { steps: 5 });
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
  assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), beforeScrub.project);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-workspace-panel="assets"]').click();
  assert.equal(
    await page.locator('[data-workspace-panel="assets"]').getAttribute('aria-expanded'),
    'true',
  );
  await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'redo', payload: {} }),
  );
  assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), scrubbed);
  // A release outside #app must not leave the owner permanently busy.
  await ensureWorkspacePanel(page, 'colors');
  await page.evaluate(() => {
    const slider = document.querySelector('[data-chroma-strength-range]');
    slider.dispatchEvent(
      new PointerEvent('pointerdown', { bubbles: true, button: 0, isPrimary: true, pointerId: 91 }),
    );
    document.body.dispatchEvent(new PointerEvent('pointerup', { bubbles: true, pointerId: 91 }));
  });
  await page.locator('[data-workspace-panel="assets"]').click();
  assert.equal(
    await page.locator('[data-workspace-panel="assets"]').getAttribute('aria-expanded'),
    'true',
  );
  assert.deepEqual(errors, []);
  console.log(
    'Shared Colors: synthetic composition, pointer protection, commit and Undo/Redo passed.',
  );
} finally {
  await browser.close();
}
