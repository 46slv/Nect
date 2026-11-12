import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { blankDocument } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const origin = migrateLegacyDocumentToCompositionProject(blankDocument());
origin.compositions[0].id = 'clipboard-origin';
origin.compositions[0].name = 'Origin';
origin.compositions[0].layers[0].id = 'clipboard-layer';
origin.compositions[0].layers[0].name = 'Original';
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
  await page.locator('[role="tab"][data-active-composition="clipboard-keeper"]').click();
  const otherTab = await snapshot();
  await paste(keeper.layers[0].id);
  assert.equal(await snapshot(), otherTab, 'another Composition is not a Layer paste target');
  await page.locator('[role="tab"][data-active-composition="clipboard-origin"]').click();
  const beforePaste = await snapshot();
  await paste('clipboard-layer');
  const layers = await page.evaluate(() => window.nect.document.layers);
  assert.equal(layers.length, 2);
  assert.equal(layers[1].name, 'Edited copy', 'Copy retains the existing live-ID behavior');
  assert.notEqual(layers[1].id, layers[0].id);
  await undo();
  assert.equal(await snapshot(), beforePaste, 'Paste is one Undo');

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
  assert.deepEqual(errors, []);
  console.log('PASS: Layer clipboard owner, live edit, Undo, Open and reused-ID import');
} finally {
  await browser.close();
}
