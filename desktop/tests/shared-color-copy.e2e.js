import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/shared-color-copy');
await mkdir(output, { recursive: true });
function artwork(name) {
  const doc = blankDocument(),
    layer = doc.layers[0];
  const shape = node('Rectangle', 40, 100),
    fill = node('Fill', 340, 100),
    out = layer.nodes[0];
  out.position = [640, 100];
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
  { id: 'shared-brand', name: 'Brand', value: { hex: '#112233', alpha: 0.4 } },
  { id: 'shared-other', name: 'Brand', value: { hex: '#112233', alpha: 0.4 } },
];
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
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () => page.evaluate(() => window.nect.commandState);
const slot = () => page.locator('[data-shared-color-row="shared-brand"]');
const target = () =>
  page.locator(`.inspector-body .field[data-node="${b.fill.id}"][data-key="color"]`);
const parameter = async () =>
  (await snapshot()).compositions
    .find((c) => c.id === 'composition-b')
    .layers.find((l) => l.id === b.layer.id)
    .nodes.find((n) => n.id === b.fill.id).params.color;
async function colors() {
  await ensureWorkspacePanel(page, 'colors');
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((el) => el.open))) await advanced.locator('summary').click();
}
async function copy(kind) {
  await colors();
  await slot().locator('[data-shared-color-copy-menu]').selectOption(kind);
}
async function inspectB() {
  await page.locator('[role="tab"][data-active-composition="composition-b"]').click();
  await page.locator(`.graph .node[data-id="${b.fill.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((p) => window.nect.load(JSON.stringify(p)), project);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-color-slot',
      payload: { slotId: 'shared-brand', value: { hex: '#123456', alpha: 0.4 } },
    }),
  );
  await activateCommand(page, 'undo');
  const beforeCopy = await snapshot(),
    copyState = await state();
  await colors();
  assert.notEqual(
    await slot().locator('.shared-color-identity').textContent(),
    await page
      .locator('[data-shared-color-row="shared-other"] .shared-color-identity')
      .textContent(),
  );
  assert.deepEqual(await slot().locator('[data-shared-color-copy-menu] option').allTextContents(), [
    'Copy…',
    'Copy value in Nect',
    'Copy reference in Nect',
  ]);
  await page.screenshot({ path: resolve(output, 'copy-menu.png') });
  await slot().locator('[data-shared-color-copy-menu]').focus();
  await page.keyboard.press('ArrowDown');
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('Color value copied in Nect'),
  );
  assert.deepEqual(await snapshot(), beforeCopy);
  assert.deepEqual(await state(), copyState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await slot().locator('[data-shared-color-edit="alpha"]').fill('0.8');
  await slot().locator('[data-shared-color-edit="alpha"]').press('Tab');
  assert.equal((await snapshot()).colorSlots[0].value.alpha, 0.8);
  await inspectB();
  await target().locator('[data-shared-color-binding]').selectOption('shared-brand');
  const beforeValue = await snapshot(),
    beforeState = await state();
  await target()
    .getByRole('button', { name: /Paste value into/ })
    .scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'paste-value.png') });
  await target()
    .getByRole('button', { name: /Paste value into/ })
    .click();
  assert.deepEqual((await parameter()).value, { hex: '#112233', alpha: 0.4 });
  assert.equal((await parameter()).colorSlotId, undefined);
  assert.equal((await state()).expectedRevision, beforeState.expectedRevision + 1);
  const afterValue = await snapshot();
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeValue);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), afterValue);

  await page.locator('[role="tab"][data-active-composition="composition-a"]').click();
  await copy('reference');
  await inspectB();
  const beforeLink = await snapshot(),
    beforeLinkState = await state();
  await target()
    .getByRole('button', { name: /Paste link into/ })
    .click();
  assert.equal((await parameter()).colorSlotId, 'shared-brand');
  assert.equal((await state()).expectedRevision, beforeLinkState.expectedRevision + 1);
  const afterLink = await snapshot();
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeLink);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), afterLink);
  await colors();
  await slot().locator('[data-shared-color-edit="alpha"]').fill('0.2');
  await slot().locator('[data-shared-color-edit="alpha"]').press('Tab');
  assert.equal((await parameter()).colorSlotId, 'shared-brand');
  await page.waitForFunction(() =>
    [...document.querySelectorAll('.shape')].some(
      (el) =>
        el.getAttribute('fill') === '#112233' && Number(el.getAttribute('fill-opacity')) === 0.2,
    ),
  );

  const expected = await snapshot(),
    downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(output, 'copy-link-project.json');
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), expected);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(expected),
  );
  await inspectB();
  assert.equal((await parameter()).colorSlotId, 'shared-brand');
  assert.equal(
    await target()
      .getByRole('button', { name: /Paste link into/ })
      .isDisabled(),
    true,
    'a copied reference cannot target a replaced project even if its slot ID matches',
  );
  await page.setViewportSize({ width: 900, height: 720 });
  await colors();
  await slot().scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'copy-menu-narrow.png') });
  assert.deepEqual(errors, []);
  console.log(
    'Shared color copy: value snapshot, project-bound reference, Undo/Redo and browser reopen passed',
  );
} finally {
  await browser.close();
}
