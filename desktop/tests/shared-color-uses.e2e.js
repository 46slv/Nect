import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/shared-color-uses');
await mkdir(output, { recursive: true });
function artwork(name) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle', 40, 100),
    fill = node('Fill', 600, 100);
  layer.name = name;
  layer.nodes[0].position = [900, 100];
  layer.nodes.push(shape, fill);
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  return { doc, layer, fill, shape };
}
const a = artwork('Artwork A'),
  b = artwork('Artwork B');
b.fill.name = 'Fill <img src=x onerror=alert(1)>';
const remap = node('Path Remap', 320, 100);
a.layer.nodes.push(remap);
connect(a.layer, a.shape.id, remap.id, 'path');
connect(a.layer, remap.id, a.fill.id, 'path');
a.doc.schemaVersion = 54;
a.doc.colorSlots = [
  { id: 'brand', name: 'Brand', value: { hex: '#112233', alpha: 0.5 } },
  { id: 'same-value', name: 'Same value', value: { hex: '#112233', alpha: 0.5 } },
];
a.fill.params.color.colorSlotId = 'brand';
const project = migrateLegacyDocumentToCompositionProject(a.doc);
project.compositions[0].id = 'comp-a';
project.compositions[0].name = 'First';
const other = migrateLegacyDocumentToCompositionProject(b.doc).compositions[0];
other.id = 'comp-b';
other.name = 'Second';
const lockedColor = other.layers[0].nodes.find((entry) => entry.id === b.fill.id).params.color;
lockedColor.colorSlotId = 'brand';
lockedColor.locked = true;
const extras = Array.from({ length: 101 }, () => node('Color'));
for (const extra of extras) {
  extra.name = 'Duplicate';
  extra.params.value.colorSlotId = 'brand';
  other.layers[0].nodes.push(extra);
}
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
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const slot = (id) => page.locator(`[data-shared-color-row="${id}"]`);
const dialog = page.locator('.shared-color-uses-dialog[open]');
const rows = () => dialog.locator('[data-color-uses-list] button');
async function colors() {
  await ensureWorkspacePanel(page, 'colors');
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((element) => element.open)))
    await advanced.locator('summary').click();
}
async function openUses(id) {
  await colors();
  const toggle = slot(id).locator('[data-shared-color-copy-toggle]');
  if ((await toggle.getAttribute('aria-expanded')) !== 'true') await toggle.click();
  await slot(id).locator('[data-shared-color-uses]').click();
  await dialog.waitFor();
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-color-slot',
      payload: { slotId: 'brand', value: { hex: '#abcdef', alpha: 0.5 } },
    }),
  );
  await activateCommand(page, 'undo');
  const before = await snapshot();
  const remapNode = page.locator(`.graph .node[data-id="${remap.id}"]`);
  await remapNode.getByRole('button', { name: 'Browse all points', exact: true }).click();
  await remapNode.locator('[data-batch-point]').first().check();
  const batchDelta = remapNode.getByLabel('Batch delta X', { exact: true });
  await batchDelta.fill('7');
  await batchDelta.press('Tab');
  await colors();
  await slot('brand').locator('[data-shared-color-copy-toggle]').click();
  await slot('brand').locator('[data-shared-color-uses]').click();
  assert.equal(await dialog.count(), 0);
  assert.equal(await remapNode.locator('[data-batch-point]').first().isChecked(), true);
  assert.equal(await batchDelta.inputValue(), '7');
  assert.deepEqual(await snapshot(), before);
  await remapNode.getByRole('button', { name: 'Clear batch selection', exact: true }).click();
  await colors();
  await slot('brand').locator('[data-shared-color-copy-toggle]').click();
  const hue = slot('brand').locator('[data-shared-color-edit="hsl-h"]');
  await hue.fill('190');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  const preview = await page.evaluate(() => window.nect.colorPreviewSnapshot());
  await slot('brand').locator('[data-shared-color-uses]').click();
  assert.equal(await dialog.count(), 0);
  assert.equal(await hue.evaluate((element) => element === document.activeElement), true);
  assert.equal(await hue.inputValue(), '190');
  assert.deepEqual(await page.evaluate(() => window.nect.colorPreviewSnapshot()), preview);
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await hue.press('Escape');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
  await colors();
  const toggle = slot('brand').locator('[data-shared-color-copy-toggle]');
  if ((await toggle.getAttribute('aria-expanded')) !== 'true') await toggle.click();
  const name = slot('brand').locator('[data-shared-color-edit="name"]');
  await name.fill('Uncommitted');
  await slot('brand').locator('[data-shared-color-uses]').click();
  assert.equal(await dialog.count(), 0);
  assert.equal(await name.evaluate((element) => element === document.activeElement), true);
  assert.equal(await name.inputValue(), 'Uncommitted');
  assert.deepEqual(await snapshot(), before);
  await name.fill('編集中');
  await name.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await slot('brand').locator('[data-shared-color-uses]').click();
  assert.equal(await dialog.count(), 0);
  assert.equal(await name.evaluate((element) => element === document.activeElement), true);
  assert.equal(await name.inputValue(), '編集中');
  assert.deepEqual(await snapshot(), before);
  await name.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  await name.fill('Brand');
  await name.press('Tab');
  await openUses('brand');
  assert.equal(await rows().count(), 100);
  assert.equal(await dialog.locator('[data-color-uses-page]').textContent(), '1–100 / 103');
  assert.match(await rows().first().textContent(), /First \/ Artwork A \/ Fill \/ color/);
  assert.match(await rows().nth(1).textContent(), /<img src=x onerror=alert\(1\)>/);
  assert.equal(await dialog.locator('img').count(), 0);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.screenshot({ path: resolve(output, 'direct-uses.png') });
  await page.setViewportSize({ width: 420, height: 520 });
  await page.screenshot({ path: resolve(output, 'direct-uses-narrow.png') });
  for (const selector of ['[data-color-uses-next]', '[data-color-uses-close]']) {
    const hit = await dialog.locator(selector).evaluate((element) => {
      const rect = element.getBoundingClientRect();
      const target = document.elementFromPoint(
        rect.left + rect.width / 2,
        rect.top + rect.height / 2,
      );
      return target === element || element.contains(target);
    });
    assert.equal(hit, true, selector);
  }
  await page.setViewportSize({ width: 1440, height: 960 });
  await dialog.locator('[data-color-uses-next]').focus();
  await page.keyboard.press('Enter');
  assert.equal(await rows().count(), 3);
  assert.equal(await dialog.locator('[data-color-uses-page]').textContent(), '101–103 / 103');
  assert.equal(
    await dialog
      .locator('[data-color-uses-previous]')
      .evaluate((el) => el === document.activeElement),
    true,
  );
  await page.keyboard.press('Enter');
  assert.equal(await rows().count(), 100);
  assert.equal(
    await dialog.locator('[data-color-uses-next]').evaluate((el) => el === document.activeElement),
    true,
  );
  await page.keyboard.press('Enter');
  assert.equal(await rows().count(), 3);
  await rows().last().focus();
  await page.keyboard.press('Enter');
  await dialog.waitFor({ state: 'detached' });
  await page.waitForFunction((id) => window.nect.activeCompositionId === id, 'comp-b');
  const target = page.locator(
    `.inspector .field[data-node="${extras.at(-1).id}"][data-key="value"] [data-shared-color-binding]`,
  );
  await target.waitFor({ state: 'visible' });
  await page.waitForFunction(
    (id) => document.activeElement?.dataset.sharedColorNode === id,
    extras.at(-1).id,
  );
  assert.equal(await target.inputValue(), 'brand');
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await activateCommand(page, 'redo');
  assert.equal(
    (await snapshot()).colorSlots.find((entry) => entry.id === 'brand').value.hex,
    '#abcdef',
  );
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);

  await openUses('brand');
  await rows().nth(1).click();
  await dialog.waitFor({ state: 'detached' });
  const lockedField = page.locator(`.inspector .field[data-node="${b.fill.id}"][data-key="color"]`);
  await lockedField.waitFor({ state: 'visible' });
  assert.equal(await lockedField.locator('[data-shared-color-binding]').isDisabled(), true);
  await page.waitForFunction(
    (id) => document.activeElement?.closest('.field')?.dataset.node === id,
    b.fill.id,
  );
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width },
    }),
  );
  await page.waitForFunction(
    (id) => document.activeElement?.closest('.field')?.dataset.node === id,
    b.fill.id,
  );
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await openUses('brand');
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-color-slot',
      payload: { slotId: 'brand', value: { hex: '#778899', alpha: 0.5 } },
    }),
  );
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  const stale = await snapshot(),
    active = await page.evaluate(() => window.nect.activeCompositionId);
  await rows().first().click();
  assert.match(
    await dialog.locator('[data-color-uses-status]').textContent(),
    /project or current edit changed/i,
  );
  assert.deepEqual(await snapshot(), stale);
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), active);
  await dialog.locator('[data-color-uses-close]').click();
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await openUses('brand');
  await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
  const replacement = await snapshot();
  await rows().first().click();
  assert.match(
    await dialog.locator('[data-color-uses-status]').textContent(),
    /project or current edit changed/i,
  );
  assert.deepEqual(await snapshot(), replacement);
  await dialog.locator('[data-color-uses-close]').click();
  await openUses('same-value');
  assert.equal(await rows().count(), 0);
  assert.equal(await dialog.locator('[data-color-uses-status]').textContent(), 'No direct uses.');
  await page.keyboard.press('Escape');
  await dialog.waitFor({ state: 'detached' });
  await page.waitForFunction(() => document.activeElement?.hasAttribute('data-shared-color-uses'));
  assert.deepEqual(await snapshot(), replacement);
  // Deleting a Remap must not leave an unreachable batch draft blocking navigation.
  await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
  await remapNode.getByRole('button', { name: 'Browse all points', exact: true }).click();
  await remapNode.locator('[data-batch-point]').first().check();
  await remapNode.locator(`[data-delete="${remap.id}"]`).click();
  await remapNode.waitFor({ state: 'detached' });
  const afterDelete = await snapshot();
  await openUses('brand');
  assert.equal(await dialog.count(), 1, 'deleting the Remap releases the navigation blocker');
  await dialog.locator('[data-color-uses-close]').click();
  assert.deepEqual(await snapshot(), afterDelete);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 1001 },
    }),
  );
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  const deleted = await snapshot();
  await colors();
  const orphanToggle = slot('brand').locator('[data-shared-color-copy-toggle]');
  if ((await orphanToggle.getAttribute('aria-expanded')) !== 'true') await orphanToggle.click();
  await slot('brand').locator('[data-shared-color-uses]').click();
  assert.equal(await dialog.count(), 1, 'deleted Remap cannot leave an orphan navigation blocker');
  await rows().first().click();
  await dialog.waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), deleted);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await activateCommand(page, 'redo');
  assert.equal((await snapshot()).compositions[0].width, 1001);
  assert.deepEqual(errors, []);
  console.log(
    'PASS direct shared Color uses: cross-Composition navigation, paging, IME and stale-owner guards',
  );
} finally {
  await browser.close();
}
