import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, connect, node } from '../src/core.js';
import { directSharedColorUses } from '../src/shared-color-uses.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  targetLayer = doc.layers[0],
  localLayer = blankLayer('Local source'),
  foreign = blankDocument(),
  foreignLayer = foreign.layers[0],
  circle = node('Circle'),
  fill = node('Fill'),
  localColor = node('Color'),
  foreignColor = node('Color');
targetLayer.id = 'target-art';
targetLayer.name = 'Artwork';
localLayer.id = 'local-art';
foreignLayer.id = 'foreign-art';
foreignLayer.name = 'Artwork';
circle.id = 'target-circle';
fill.id = 'target-fill';
localColor.id = 'local-color';
localColor.name = 'Source color';
localColor.params.value.value = { hex: '#1248ac', alpha: 0.7 };
foreignColor.id = 'foreign-color';
foreignColor.name = 'Source color';
circle.params.center.value = [350, 250];
targetLayer.nodes.push(circle, fill);
connect(targetLayer, circle.id, fill.id, 'path');
connect(targetLayer, fill.id, targetLayer.nodes[0].id, 'path');
localLayer.nodes.push(localColor);
foreignLayer.nodes.push(foreignColor);
doc.layers.push(localLayer);
const project = migrateLegacyDocumentToCompositionProject(doc);
project.compositions[0].id = 'target-comp';
project.compositions[0].name = 'Design';
project.compositions.push({
  id: 'source-comp',
  name: 'Design',
  width: foreign.width,
  height: foreign.height,
  layers: foreign.layers,
  editor: foreign.editor,
});

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
page.setDefaultTimeout(15000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () => page.evaluate(() => window.nect.commandState);
const resultColor = () =>
  page.evaluate(() => window.nect.evaluate().parameterValue('target-fill', 'color'));
const field = (id, key) =>
  page.locator(`.inspector-body .field[data-node="${id}"][data-key="${key}"]`);
async function selectNode(id) {
  await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
}
async function download(action, name) {
  const pending = page.waitForEvent('download');
  await activateCommand(page, action);
  const path = resolve(`test-results/qualified-color-reference/${name}`);
  await (await pending).saveAs(path);
  return readFile(path);
}
async function chooseReference(search, { inspectContext = false, keyboard = false } = {}) {
  await field('target-fill', 'color').locator('[data-fx]').click();
  const dialog = page.locator('dialog[open]').filter({
      has: page.locator('[data-insert-reference]'),
    }),
    draft = dialog.locator('[aria-label="Parameter expression"]');
  await draft.fill('');
  await dialog.locator('[data-insert-reference]').click();
  const picker = page.getByRole('dialog', { name: 'Parameter references' });
  const input = picker.getByRole('combobox'),
    rows = picker.locator('.picker-option');
  if (inspectContext) {
    await input.fill('Source color');
    assert.equal(await rows.count(), 2);
    const labels = await rows.locator(':scope > span').allTextContents();
    assert.notEqual(labels[0].normalize('NFC'), labels[1].normalize('NFC'));
    await page.setViewportSize({ width: 1000, height: 650 });
    await picker.screenshot({ path: resolve('test-results/qualified-color-reference/picker.png') });
    assert.equal(
      await picker.locator('.picker-apply').evaluate((element) => {
        const box = element.getBoundingClientRect(),
          hit = document.elementFromPoint(box.x + box.width / 2, box.y + box.height / 2);
        return hit === element || element.contains(hit);
      }),
      true,
    );
    await page.setViewportSize({ width: 1440, height: 960 });
  }
  await input.fill(search);
  assert.equal(await rows.count(), 1);
  if (keyboard) {
    await input.press('ArrowDown');
    await input.press('Enter');
  } else {
    await rows.first().click();
    await picker.locator('.picker-apply').click();
  }
  const expression = await draft.inputValue();
  await dialog.locator('[data-confirm]').click();
  await dialog.waitFor({ state: 'detached' });
  return expression;
}

try {
  await mkdir('test-results/qualified-color-reference', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(project));

  // The linked palette is an explicit B trial, not a new product default.
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-key-color-recipe-open]').click();
  await page.locator('[data-research-hex]').fill('#3273c7');
  await page.locator('[data-research-alpha]').fill('1');
  await page.locator('[data-research-plan]').selectOption('research-neutral-start');
  await page.locator('[data-research-live]').check();
  await page.locator('[data-research-preview]').click();
  await page.locator('[data-research-create]').click();
  const linked = await snapshot(),
    slotId = linked.keyColorRecipe.committedSnapshot.intent.preset.slots[0].id;
  assert.equal(linked.keyColorRecipe.mode, 'live');

  await page.locator('[role="tab"][data-active-composition="source-comp"]').click();
  await selectNode('foreign-color');
  await field('foreign-color', 'value').locator('[data-shared-color-binding]').selectOption(slotId);
  await page.locator('[role="tab"][data-active-composition="target-comp"]').click();
  await selectNode('target-fill');
  const local = await chooseReference('local-art local-color value', { inspectContext: true });
  assert.equal(local, 'ref(colorArtwork, "local-art", "local-color", "value")');
  assert.deepEqual(await resultColor(), { hex: '#1248ac', alpha: 0.7 });

  await selectNode('target-fill');
  const remote = await chooseReference('source-comp foreign-art foreign-color value', {
    keyboard: true,
  });
  assert.equal(
    remote,
    'ref(colorComposition, "source-comp", "foreign-art", "foreign-color", "value")',
  );
  const bound = await snapshot(),
    target = bound.compositions[0].layers[0].nodes.find((entry) => entry.id === 'target-fill'),
    oldColor = bound.colorSlots.find((entry) => entry.id === slotId).value;
  assert.equal(bound.schemaVersion, 64);
  assert.equal(target.params.color.colorSlotId, undefined);
  assert.deepEqual(
    directSharedColorUses(bound, slotId).map((use) => use.nodeId),
    ['foreign-color'],
  );
  assert.deepEqual(await resultColor(), oldColor);
  const oldSvg = await download('export', 'before.svg'),
    oldPng = await download('export-png', 'before.png');
  assert.match(oldSvg.toString('utf8'), /<svg/);
  assert.ok(oldSvg.toString('utf8').includes(`fill="${oldColor.hex}"`));
  assert.deepEqual([...oldPng.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);

  // A proposed live palette affects the dependent artwork without committing.
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-key-color-recipe-open]').click();
  await page.locator('[data-key-color-input="hex"]').fill('#e43f74');
  await page.locator('[data-key-color-preview]').click();
  const proposed = await page
    .locator(`[data-key-color-compare="${slotId}"] [data-key-color-after] code`)
    .textContent();
  const beforePreviewState = await state();
  await page.locator('[data-artwork-show]').click();
  await page.waitForFunction(
    (hex) =>
      document.querySelector('.shape[data-layer="target-art"]')?.getAttribute('fill') === hex,
    proposed,
  );
  assert.deepEqual(await snapshot(), bound);
  assert.deepEqual(await state(), beforePreviewState);
  await page.locator('[data-artwork-restore]').click();
  await page.waitForFunction(
    (hex) =>
      document.querySelector('.shape[data-layer="target-art"]')?.getAttribute('fill') === hex,
    oldColor.hex,
  );
  await page.locator('[data-key-color-apply]').click();
  const edited = await snapshot(),
    newColor = edited.colorSlots.find((entry) => entry.id === slotId).value;
  assert.notDeepEqual(newColor, oldColor);
  assert.deepEqual(await resultColor(), newColor);
  const newSvg = await download('export', 'after.svg'),
    newPng = await download('export-png', 'after.png');
  assert.notDeepEqual(newSvg, oldSvg);
  assert.notDeepEqual(newPng, oldPng);
  assert.ok(newSvg.toString('utf8').includes(`fill="${newColor.hex}"`));
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), bound);
  assert.deepEqual(await resultColor(), oldColor);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), edited);
  assert.deepEqual(await resultColor(), newColor);

  // A missing Color source exposes the same diagnostic as numeric references.
  await page.locator('[role="tab"][data-active-composition="source-comp"]').click();
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'delete-node',
      payload: { layerId: 'foreign-art', nodeId: 'foreign-color' },
    }),
  );
  await page.locator('[role="tab"][data-active-composition="target-comp"]').click();
  await selectNode('target-fill');
  const unavailable = field('target-fill', 'color').locator('.parameter-result');
  assert.match(await unavailable.innerText(), /Cannot evaluate/);
  const diagnostic = await unavailable.getAttribute('title');
  assert.match(diagnostic, /missing/i);
  assert.equal(await unavailable.getAttribute('aria-description'), diagnostic);
  assert.equal(
    await field('target-fill', 'color').locator('[data-expression]').inputValue(),
    remote,
  );
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), edited);
  assert.deepEqual(await resultColor(), newColor);
  assert.equal(await unavailable.getAttribute('title'), null);
  await activateCommand(page, 'redo');
  assert.match(await unavailable.getAttribute('aria-description'), /missing/i);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), edited);
  assert.deepEqual(await resultColor(), newColor);

  const pending = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const path = resolve('test-results/qualified-color-reference/project.json');
  await (await pending).saveAs(path);
  assert.deepEqual(JSON.parse(await readFile(path, 'utf8')), edited);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page.locator('#open-file').setInputFiles(path);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(edited),
  );
  assert.deepEqual(await resultColor(), newColor);
  await page.setViewportSize({ width: 1000, height: 650 });
  await selectNode('target-fill');
  await field('target-fill', 'color').locator('.parameter-result').scrollIntoViewIfNeeded();
  assert.match(
    await field('target-fill', 'color').locator('.parameter-result').innerText(),
    /Result/,
  );
  await page.waitForFunction(() =>
    [...document.querySelectorAll('.workspace-drawer:not([hidden])')].every(
      (drawer) => Number(getComputedStyle(drawer).opacity) === 1,
    ),
  );
  await page.screenshot({ path: resolve('test-results/qualified-color-reference/reopened.png') });
  assert.deepEqual(errors, []);
  console.log('PASS qualified Color picker, linked palette preview, history and JSON reopen');
} finally {
  await browser.close();
}
