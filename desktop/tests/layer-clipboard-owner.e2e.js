import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const origin = migrateLegacyDocumentToCompositionProject(blankDocument());
origin.compositions[0].id = 'clipboard-origin';
origin.compositions[0].name = 'Origin';
origin.compositions[0].layers[0].id = 'clipboard-layer';
origin.compositions[0].layers[0].name = 'Original';
const rectangle = node('Rectangle'),
  fill = node('Fill');
rectangle.id = 'clipboard-rectangle';
rectangle.params.center.value = [100, 100];
fill.id = 'clipboard-fill';
fill.params.path.source = rectangle.id;
fill.params.color.value = { hex: '#ff0000', alpha: 0.5 };
origin.compositions[0].layers[0].nodes.unshift(rectangle, fill);
origin.compositions[0].layers[0].nodes.find((item) => item.type === 'Output').params.path.source =
  fill.id;
const replacement = structuredClone(origin);
replacement.compositions[0].name = 'Replacement';
replacement.compositions[0].layers[0].name = 'Foreign layer';
const project = structuredClone(origin);
const keeper = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
keeper.id = 'clipboard-keeper';
project.compositions.push(keeper);

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
const load = (value) => page.evaluate((p) => window.nect.load(JSON.stringify(p)), value);
const focusLayer = async (id) => {
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${id}"]`).focus();
};
const copy = async (id) => {
  await focusLayer(id);
  await page.keyboard.press('Control+c');
};
const paste = async (id) => {
  await focusLayer(id);
  await page.keyboard.press('Control+v');
};
const undo = () =>
  page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
const deleteOrigin = () =>
  page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'delete-composition',
      payload: {
        compositionId: 'clipboard-origin',
        expectedName: 'Origin',
        expectedProjectRevision: window.nect.projectRevision,
      },
    }),
  );

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await load(project);
  await copy('clipboard-layer');
  await page.locator('[data-select-layer="clipboard-layer"]').dblclick();
  await page.locator('[data-rename-name]').fill('Edited');
  await page.locator('[data-rename-name]').press('Enter');
  await page.evaluate(() => {
    const send = (nodeId, key, value) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-parameter',
        payload: { layerId: 'clipboard-layer', nodeId, key, value },
      });
    send('clipboard-rectangle', 'center', [240, 180]);
    send('clipboard-fill', 'color', { hex: '#0000ff', alpha: 0.8 });
  });
  await page.locator('[role="tab"][data-active-composition="clipboard-keeper"]').click();
  const otherTab = await snapshot();
  await paste(keeper.layers[0].id);
  assert.equal(await snapshot(), otherTab, 'another Composition is not a Layer paste target');
  await page.locator('[role="tab"][data-active-composition="clipboard-origin"]').click();
  const beforePaste = await snapshot();
  await paste('clipboard-layer');
  const layers = await page.evaluate(() => window.nect.document.layers);
  assert.equal(layers.length, 2);
  assert.equal(layers[1].name, 'Original copy', 'Paste retains the copy-time Layer snapshot');
  assert.notEqual(layers[1].id, layers[0].id);
  assert.deepEqual(
    layers[1].nodes.find((item) => item.type === 'Rectangle').params.center.value,
    [100, 100],
  );
  assert.deepEqual(layers[1].nodes.find((item) => item.type === 'Fill').params.color.value, {
    hex: '#ff0000',
    alpha: 0.5,
  });
  const firstCopyId = layers[1].id;
  await focusLayer('clipboard-layer');
  await page.keyboard.press('Control+d');
  const currentCopy = (await page.evaluate(() => window.nect.document.layers)).find(
    (item) => item.name === 'Edited copy',
  );
  assert.ok(currentCopy, 'Ctrl+D duplicates the current Layer');
  assert.deepEqual(
    currentCopy.nodes.find((item) => item.type === 'Rectangle').params.center.value,
    [240, 180],
  );
  assert.deepEqual(currentCopy.nodes.find((item) => item.type === 'Fill').params.color.value, {
    hex: '#0000ff',
    alpha: 0.8,
  });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.locator('.layer.selected').getAttribute('data-layer'), firstCopyId);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), beforePaste, 'Paste is one Undo');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.locator('.layer.selected').getAttribute('data-layer'), firstCopyId);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), beforePaste);
  await paste('clipboard-layer');
  const secondCopy = (await page.evaluate(() => window.nect.document.layers)).find(
    (item) => item.name === 'Original copy',
  );
  assert.notEqual(secondCopy.id, firstCopyId, 'each Paste has a fresh Layer ID');
  await undo();
  await mkdir('test-results', { recursive: true });
  const originalDownload = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const originalFile = resolve('test-results/layer-snapshot-before-paste.json');
  await (await originalDownload).saveAs(originalFile);
  assert.equal(JSON.stringify(JSON.parse(await readFile(originalFile, 'utf8'))), await snapshot());
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  await paste('clipboard-layer');
  const savedWithPaste = await snapshot(),
    pastedLayer = (await page.evaluate(() => window.nect.document.layers)).find(
      (item) => item.name === 'Original copy',
    );
  const pastedDownload = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const pastedFile = resolve('test-results/layer-snapshot-after-paste.json');
  await (await pastedDownload).saveAs(pastedFile);
  assert.equal(JSON.stringify(JSON.parse(await readFile(pastedFile, 'utf8'))), savedWithPaste);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  const discard = page.locator('dialog[open] button[value="discard"]');
  if (await discard.isVisible()) await discard.click();
  await (await chooser).setFiles(pastedFile);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    savedWithPaste,
  );
  const pastedRectangle = pastedLayer.nodes.find((item) => item.type === 'Rectangle');
  await page.evaluate(
    ({ layerId, nodeId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-parameter',
        payload: { layerId, nodeId, key: 'center', value: [150, 150] },
      }),
    { layerId: pastedLayer.id, nodeId: pastedRectangle.id },
  );
  assert.deepEqual(
    (
      await page.evaluate(
        (id) => window.nect.document.layers.find((layer) => layer.id === id),
        pastedLayer.id,
      )
    ).nodes.find((item) => item.type === 'Rectangle').params.center.value,
    [150, 150],
  );

  // A real Open with the same IDs must not make the old Copy target another project.
  await load(project);
  await copy('clipboard-layer');
  await page.locator('#open-file').setInputFiles({
    name: 'replacement.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(JSON.stringify(replacement)),
  });
  await page.waitForFunction(() => window.nect.document.layers[0].name === 'Foreign layer');
  const beforeOpenPaste = await snapshot();
  await paste('clipboard-layer');
  const openRetargeted = (await snapshot()) !== beforeOpenPaste;

  // Undoing the actual deletion restores the original identity and its Copy.
  await load(project);
  await copy('clipboard-layer');
  await deleteOrigin();
  await undo();
  await page.locator('[role="tab"][data-active-composition="clipboard-origin"]').click();
  const beforeRestoredPaste = await snapshot();
  await paste('clipboard-layer');
  assert.equal((await page.evaluate(() => window.nect.document.layers)).length, 2);
  await undo();
  assert.equal(await snapshot(), beforeRestoredPaste);

  // Import may legally reuse IDs freed by deletion; those IDs are a new owner.
  await load(project);
  await copy('clipboard-layer');
  await deleteOrigin();
  await page.evaluate(
    (p) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'import-project-compositions',
        payload: { sourceProject: p, compositionIds: ['clipboard-origin'] },
      }),
    replacement,
  );
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'clipboard-origin');
  assert.equal(await page.evaluate(() => window.nect.document.layers[0].name), 'Foreign layer');
  const beforeImportPaste = await snapshot();
  await paste('clipboard-layer');
  const importRetargeted = (await snapshot()) !== beforeImportPaste;
  assert.deepEqual(
    { openRetargeted, importRetargeted },
    {
      openRetargeted: false,
      importRetargeted: false,
    },
  );
  await load(project);
  await copy('clipboard-layer');
  await activateCommand(page, 'new');
  const discardForNew = page.locator('dialog[open] button[value="discard"]');
  if (await discardForNew.isVisible()) await discardForNew.click();
  await page.waitForFunction(
    () => !window.nect.document.layers.some((layer) => layer.id === 'clipboard-layer'),
  );
  const newLayerId = await page.evaluate(() => window.nect.document.layers[0].id),
    beforeNewPaste = await snapshot();
  await paste(newLayerId);
  assert.equal(await snapshot(), beforeNewPaste, 'New clears the old Layer clipboard');
  assert.deepEqual(errors, []);
  console.log('PASS: Layer snapshot, current Duplicate, Undo, Save/Open and reused-ID import');
} finally {
  await browser.close();
}
