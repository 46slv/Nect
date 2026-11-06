import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/shared-color-order');
await mkdir(output, { recursive: true });
const doc = blankDocument(),
  layer = doc.layers[0];
const shape = node('Rectangle', 40, 100),
  fill = node('Fill', 340, 100),
  out = layer.nodes[0];
layer.nodes = [shape, fill, out];
connect(layer, shape.id, fill.id, 'path');
connect(layer, fill.id, out.id, 'path');
doc.schemaVersion = 54;
doc.colorSlots = [
  { id: 'z', name: 'Zeta', value: { hex: '#112233', alpha: 1 } },
  { id: 'b', name: 'Beta', value: { hex: '#445566', alpha: 1 } },
  { id: 'a', name: 'Beta', value: { hex: '#445566', alpha: 1 } },
];
fill.params.color.colorSlotId = 'a';
const project = migrateLegacyDocumentToCompositionProject(doc);
project.compositions[0].id = 'composition-a';
const other = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
other.id = 'composition-b';
project.compositions.push(other);

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1000, height: 650 } });
await page.emulateMedia({ reducedMotion: 'reduce' });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () => page.evaluate(() => window.nect.commandState);
const row = (id) => page.locator(`[data-shared-color-row="${id}"]`);
const order = () =>
  page
    .locator('[data-shared-color-row]')
    .evaluateAll((elements) => elements.map((element) => element.dataset.sharedColorRow));
const sort = () => page.locator('[data-shared-color-sort]');
const find = () => page.locator('[data-shared-color-find]');
const filter = () => page.locator('[data-shared-color-filter]');
const visibleRows = () =>
  page
    .locator('[data-shared-color-row]:visible')
    .evaluateAll((elements) => elements.map((element) => element.dataset.sharedColorRow));
async function colors() {
  await ensureWorkspacePanel(page, 'colors');
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((element) => element.open)))
    await advanced.locator('summary').click();
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await colors();
  await page.waitForFunction(() =>
    [...document.querySelectorAll('[data-color-coverage-id]')].every((el) =>
      el.textContent.endsWith('%'),
    ),
  );
  const coverage = () =>
    page
      .locator('[data-color-coverage-id]')
      .evaluateAll((els) =>
        Object.fromEntries(els.map((el) => [el.dataset.colorCoverageId, el.textContent])),
      );
  const before = await snapshot(),
    beforeState = await state(),
    beforeCoverage = await coverage();
  assert.deepEqual(await order(), ['z', 'b', 'a']);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await find().focus();
  await page.keyboard.press('Enter');
  assert.equal(await filter().evaluate((element) => element === document.activeElement), true);
  await filter().fill('BETA');
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  const duplicateLabels = await page
    .locator('[data-shared-color-row]:visible [data-shared-color-copy-toggle]')
    .evaluateAll((elements) => elements.map((element) => element.getAttribute('aria-label')));
  assert.equal(new Set(duplicateLabels).size, 2);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.screenshot({ path: resolve(output, 'filtered-1000x650.png') });
  await page.setViewportSize({ width: 420, height: 520 });
  await filter().scrollIntoViewIfNeeded();
  assert.equal(
    await filter().evaluate((element) => {
      const rect = element.getBoundingClientRect();
      return (
        document.elementFromPoint(rect.left + rect.width / 2, rect.top + rect.height / 2) ===
        element
      );
    }),
    true,
  );
  await page.screenshot({ path: resolve(output, 'filtered-narrow-stress.png') });
  await page.setViewportSize({ width: 1000, height: 650 });
  await filter().fill('no match');
  assert.equal(await page.locator('[data-shared-color-filter-empty]').isVisible(), true);
  assert.equal(await page.getByText('No saved colors.').count(), 0);
  await filter().fill('Beta');
  await filter().evaluate((element) => {
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
    element.value = 'Zeta';
    element.dispatchEvent(new InputEvent('input', { bubbles: true, isComposing: true }));
  });
  await sort().click();
  assert.equal(await filter().evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await filter().evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  assert.deepEqual(await visibleRows(), ['z']);
  await page.keyboard.press('Escape');
  assert.equal(await filter().inputValue(), '');
  assert.equal(await filter().evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(await visibleRows(), ['z', 'b', 'a']);
  await page.keyboard.press('Escape');
  assert.equal(await find().evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(await visibleRows(), ['z', 'b', 'a']);
  const sortButton = sort();
  const inactiveColor = await sortButton.evaluate(
    (element) => getComputedStyle(element).backgroundColor,
  );
  await sortButton.scrollIntoViewIfNeeded();
  const box = await sortButton.boundingBox();
  assert.ok(box);
  assert.equal(
    await sortButton.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, {
      x: box.x + box.width / 2,
      y: box.y + box.height / 2,
    }),
    true,
  );
  await page.screenshot({ path: resolve(output, 'before-1000x650.png') });
  await sortButton.focus();
  await page.keyboard.press('Enter');
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  assert.equal(await sort().getAttribute('aria-pressed'), 'true');
  assert.equal(await sort().evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.deepEqual(await coverage(), beforeCoverage);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  assert.notEqual(
    await sort().evaluate((element) => getComputedStyle(element).backgroundColor),
    inactiveColor,
  );
  await sort().evaluate((element) => element.blur());
  await page.screenshot({ path: resolve(output, 'sorted-1000x650.png') });
  await page.setViewportSize({ width: 420, height: 520 });
  await sort().scrollIntoViewIfNeeded();
  const narrow = await sort().boundingBox();
  assert.ok(narrow);
  assert.equal(
    await sort().evaluate(
      (element, point) => document.elementFromPoint(point.x, point.y) === element,
      { x: narrow.x + narrow.width / 2, y: narrow.y + narrow.height / 2 },
    ),
    true,
  );
  await page.screenshot({ path: resolve(output, 'sorted-narrow-stress.png') });
  await page.setViewportSize({ width: 1000, height: 650 });
  const labels = await page
    .locator('[data-shared-color-copy-toggle]')
    .evaluateAll((els) => els.map((el) => el.getAttribute('aria-label')));
  assert.equal(new Set(labels).size, 3);
  await find().click();
  await filter().fill('Beta');
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await activateCommand(page, 'redo');
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  assert.equal(await filter().inputValue(), 'Beta');
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  assert.equal(await filter().inputValue(), 'Beta');
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await page.locator('[data-shared-color-close]').click();
  await colors();
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  assert.equal(await filter().inputValue(), 'Beta');
  await find().click();

  // A synthetic composition cannot be displaced by a real pointer click on Sort.
  const name = row('b').locator('[data-shared-color-edit="name"]');
  await name.focus();
  await name.evaluate((el) => {
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
    el.value = 'draft';
    el.dispatchEvent(new InputEvent('input', { bubbles: true, isComposing: true }));
  });
  await sort().click();
  assert.equal(await name.evaluate((el) => el === document.activeElement), true);
  assert.equal(await name.inputValue(), 'draft');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  await name.evaluate((el) => {
    el.value = 'Beta';
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true }));
  });

  await find().click();
  await filter().fill('Beta');
  await page.locator('[role="tab"][data-active-composition="composition-b"]').click();
  await colors();
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  assert.equal(await filter().inputValue(), 'Beta');
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await find().click();
  await sort().click();
  assert.deepEqual(await order(), ['z', 'b', 'a']);
  await sort().click();
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  assert.equal(
    (await snapshot()).compositions[0].layers[0].nodes.find((entry) => entry.id === fill.id).params
      .color.colorSlotId,
    'a',
  );

  // The view choice does not enter the saved project and resets on replacement.
  await find().click();
  await filter().fill('Beta');
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(output, 'unchanged-colors.json');
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), before);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(before),
  );
  await colors();
  assert.deepEqual(await order(), ['z', 'b', 'a']);
  assert.equal(await sort().getAttribute('aria-pressed'), 'false');
  assert.equal(await filter().isVisible(), false);
  assert.equal(await filter().inputValue(), '');
  await find().click();
  await filter().fill('Beta');
  await row('b').locator('[data-shared-color-copy-toggle]').click();
  await row('b').locator('[data-shared-color-delete]').click();
  assert.deepEqual(await visibleRows(), ['a']);
  await activateCommand(page, 'undo');
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await activateCommand(page, 'redo');
  assert.deepEqual(await visibleRows(), ['a']);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await visibleRows(), ['b', 'a']);
  await find().click();
  const renamed = row('z').locator('[data-shared-color-edit="name"]');
  await renamed.focus();
  await renamed.fill('   ');
  const beforeInvalid = await snapshot(),
    beforeInvalidState = await state();
  await sort().click();
  assert.deepEqual(await snapshot(), beforeInvalid);
  assert.deepEqual(await state(), beforeInvalidState);
  assert.equal(await renamed.inputValue(), '   ');
  assert.equal(await renamed.evaluate((element) => element === document.activeElement), true);
  await renamed.fill('Alpha');
  await sort().click();
  assert.equal((await snapshot()).colorSlots.find((slot) => slot.id === 'z').name, 'Alpha');
  assert.equal(await sort().getAttribute('aria-pressed'), 'true');
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeInvalid);
  assert.deepEqual(await order(), ['b', 'a', 'z']);
  await page.evaluate(
    (value) => window.nect.load(JSON.stringify(value)),
    migrateLegacyDocumentToCompositionProject(blankDocument()),
  );
  await ensureWorkspacePanel(page, 'colors');
  assert.equal(await page.locator('[data-shared-color-filter-empty]').count(), 0);
  assert.equal(await page.getByText('No saved colors.').isVisible(), true);
  assert.deepEqual(errors, []);
  console.log(
    'Shared colors: view-only Find and sorting, identity, coverage, IME and reopen passed',
  );
} finally {
  await browser.close();
}
