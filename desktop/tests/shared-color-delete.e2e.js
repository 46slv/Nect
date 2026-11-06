import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/shared-color-delete');
await mkdir(output, { recursive: true });
function artwork(name) {
  const doc = blankDocument();
  const layer = doc.layers[0];
  const shape = node('Rectangle', 40, 100),
    fill = node('Fill', 340, 100),
    out = layer.nodes[0];
  layer.name = name;
  layer.nodes = [shape, fill, out];
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  return { doc, layer, fill };
}
const a = artwork('First'),
  b = artwork('Second');
a.doc.schemaVersion = 54;
a.doc.colorSlots = [
  { id: 'used', name: 'Brand', value: { hex: '#112233', alpha: 0.4 } },
  { id: 'unused', name: 'Brand', value: { hex: '#112233', alpha: 0.4 } },
];
a.fill.params.color.colorSlotId = 'used';
const project = migrateLegacyDocumentToCompositionProject(a.doc);
project.compositions[0].id = 'composition-a';
project.compositions[0].name = 'First';
const other = migrateLegacyDocumentToCompositionProject(b.doc).compositions[0];
other.id = 'composition-b';
other.name = 'Second';
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
await page.emulateMedia({ reducedMotion: 'reduce' });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () => page.evaluate(() => window.nect.commandState);
const row = (id) => page.locator(`[data-shared-color-row="${id}"]`);
async function colors() {
  await ensureWorkspacePanel(page, 'colors');
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((element) => element.open)))
    await advanced.locator('summary').click();
}
async function menu(id) {
  await colors();
  const toggle = row(id).locator('[data-shared-color-copy-toggle]');
  if ((await toggle.getAttribute('aria-expanded')) !== 'true') await toggle.click();
  return toggle;
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
  await page.locator('[role="tab"][data-active-composition="composition-b"]').click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const before = await snapshot(),
    beforeState = await state();
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  // The first Composition still uses this color; reject before touching the active Session.
  await menu('used');
  await row('used').locator('[data-shared-color-delete]').focus();
  await page.keyboard.press('Enter');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  assert.match(await page.locator('.status .message').textContent(), /in use/i);
  assert.match(await page.locator('[data-shared-color-action-status]').textContent(), /in use/i);
  assert.equal(
    await row('used')
      .locator('[data-shared-color-copy-toggle]')
      .evaluate((el) => el === document.activeElement),
    true,
  );

  // Copy a reference, then delete only the equal-looking unused slot.
  await menu('unused');
  await row('unused').locator('[data-shared-color-copy="reference"]').click();
  await menu('unused');
  await page.screenshot({ path: resolve(output, 'actions.png') });
  await row('unused').locator('[data-shared-color-delete]').focus();
  await page.keyboard.press('Enter');
  const removed = await snapshot();
  assert.equal(
    removed.colorSlots.some((slot) => slot.id === 'unused'),
    false,
  );
  assert.equal(removed.colorSlots.find((slot) => slot.id === 'used').name, 'Brand');
  assert.equal((await state()).expectedRevision, beforeState.expectedRevision + 1);
  assert.equal(
    await page.locator('[data-shared-color-add]').evaluate((el) => el === document.activeElement),
    true,
  );
  assert.equal(
    await page.locator('[data-shared-color-action-status]').textContent(),
    'Color deleted',
  );
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  await page.locator(`.graph .node[data-id="${b.fill.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const paste = page.locator(
    `.inspector-body .field[data-node="${b.fill.id}"][data-key="color"] [data-shared-color-paste]`,
  );
  assert.equal(await paste.isDisabled(), true, 'removed references do not revive on Undo');
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), removed);

  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(output, 'deleted-color-project.json');
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), removed);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(removed),
  );
  await page.setViewportSize({ width: 1000, height: 650 });
  await menu('used');
  const deleteButton = row('used').locator('[data-shared-color-delete]');
  await deleteButton.scrollIntoViewIfNeeded();
  const box = await deleteButton.boundingBox();
  assert.ok(box);
  assert.equal(
    await deleteButton.evaluate(
      (element, point) => document.elementFromPoint(point.x, point.y) === element,
      { x: box.x + box.width / 2, y: box.y + box.height / 2 },
    ),
    true,
  );
  await page.screenshot({ path: resolve(output, 'actions-short.png') });

  // A pending name must not make a different row's Delete lose the first click.
  await page.evaluate((value) => window.nect.load(JSON.stringify(value)), project);
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await menu('unused');
  const name = row('used').locator('[data-shared-color-edit="name"]');
  await name.focus();
  await name.fill('   ');
  const beforeInvalid = await snapshot(),
    beforeInvalidState = await state();
  await row('unused').locator('[data-shared-color-delete]').click();
  assert.deepEqual(await snapshot(), beforeInvalid);
  assert.deepEqual(await state(), beforeInvalidState);
  assert.equal(await name.inputValue(), '   ');
  assert.equal(await name.evaluate((element) => element === document.activeElement), true);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  assert.match(await page.locator('.status .message').textContent(), /name/i);
  assert.match(await page.locator('[data-shared-color-action-status]').textContent(), /name/i);

  await name.fill('Renamed');
  await row('unused').locator('[data-shared-color-delete]').click();
  const renamedAndDeleted = await snapshot();
  assert.equal(renamedAndDeleted.colorSlots.find((slot) => slot.id === 'used').name, 'Renamed');
  assert.equal(
    renamedAndDeleted.colorSlots.some((slot) => slot.id === 'unused'),
    false,
  );
  await activateCommand(page, 'undo');
  const renamed = await snapshot();
  assert.equal(renamed.colorSlots.find((slot) => slot.id === 'used').name, 'Renamed');
  assert.equal(
    renamed.colorSlots.some((slot) => slot.id === 'unused'),
    true,
  );
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeInvalid);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), renamed);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), renamedAndDeleted);
  assert.deepEqual(errors, []);
  console.log(
    'Shared color deletion: reference guards, Undo/Redo, clipboard and browser reopen passed',
  );
} finally {
  await browser.close();
}
