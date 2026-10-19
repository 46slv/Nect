import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { blankDocument } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  validateCompositionProject,
} from '../src/composition-project-model.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 900, height: 620 } });
const result = { steps: [], errors: [] };
page.on('pageerror', (error) => result.errors.push(error.message));
const record = (name, pass, detail = '') => result.steps.push({ name, pass, detail });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:4173');
  await page.waitForSelector('.node');
  const wide = migrateLegacyDocumentToCompositionProject(blankDocument()),
    tall = migrateLegacyDocumentToCompositionProject(blankDocument());
  wide.name = 'Import source';
  wide.compositions[0].id = 'source-wide';
  wide.compositions[0].name = 'Wide source';
  wide.compositions[0].width = 1280;
  wide.compositions[0].height = 720;
  tall.compositions[0].id = 'source-tall';
  tall.compositions[0].name = 'Tall source';
  tall.compositions[0].width = 320;
  tall.compositions[0].height = 960;
  const sourceProject = validateCompositionProject({
    ...wide,
    compositions: [wide.compositions[0], tall.compositions[0]],
  });
  const sourceText = serializeCompositionProject(sourceProject);
  const initial = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(initial.compositions.length, 1);
  const openImportDialog = async () => {
    await page.getByText('Import', { exact: true }).click();
    await page.getByText('Project composition', { exact: true }).click();
    await page.locator('body > input[type="file"]').last().setInputFiles({
      name: 'source.json',
      mimeType: 'application/json',
      buffer: Buffer.from(sourceText),
    });
    await page.waitForSelector('dialog[open] [data-composition-choice]');
  };

  await openImportDialog();
  const choices = await page.locator('dialog[open] [data-composition-choice] option').count(),
    defaultChoice = await page.locator('dialog[open] [data-composition-choice]').inputValue();
  assert.equal(choices, 3);
  assert.equal(defaultChoice, '__all__');
  await page.locator('dialog[open] button[value="import"]').click();
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 3);
  const first = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    activeCompositionId: window.nect.activeCompositionId,
    openDialogs: document.querySelectorAll('dialog[open]').length,
  }));
  const firstImported = first.project.compositions.slice(1);
  record(
    'import all complete outer Compositions',
    first.activeCompositionId === firstImported[0].id &&
      firstImported.map(({ name, width, height }) => [name, width, height]).toString() ===
        [['Wide source', 1280, 720], ['Tall source', 320, 960]].toString() &&
      firstImported.every((composition) => composition.layers.length === 1) &&
      first.openDialogs === 0,
  );

  const beforeSecondImport = JSON.stringify(first.project);
  await openImportDialog();
  await page.locator('dialog[open] button[value="import"]').click();
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 5);
  const second = await page.evaluate(() => window.nect.projectSnapshot());
  const compositionIds = second.compositions.map((composition) => composition.id),
    layerIds = second.compositions.flatMap((composition) =>
      composition.layers.map((layer) => layer.id),
    );
  record(
    'repeat import remaps outer and Layer IDs',
    new Set(compositionIds).size === compositionIds.length &&
      new Set(layerIds).size === layerIds.length,
  );

  const beforeUndo = JSON.stringify(second);
  await page.locator('[data-action="undo"]').click();
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 3);
  record(
    'one Undo removes exactly the second full import',
    JSON.stringify(await page.evaluate(() => window.nect.projectSnapshot())) === beforeSecondImport,
  );
  await page.keyboard.press('Control+Shift+z');
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 5);
  record(
    'keyboard Redo restores the whole import batch',
    JSON.stringify(await page.evaluate(() => window.nect.projectSnapshot())) === beforeUndo,
  );
  await page.keyboard.press('Control+z');
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 3);
  record(
    'keyboard Undo removes the whole import batch',
    JSON.stringify(await page.evaluate(() => window.nect.projectSnapshot())) === beforeSecondImport,
  );
  await page.locator('[data-action="redo"]').click();
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 5);
  record(
    'Redo restores the whole import batch',
    JSON.stringify(await page.evaluate(() => window.nect.projectSnapshot())) === beforeUndo,
  );

  const saved = await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  await page.evaluate((text) => window.nect.load(text), saved);
  record(
    'project save/load model roundtrip',
    (await page.evaluate(() => window.nect.projectSnapshot().compositions.length)) === 5,
  );
  await page.evaluate((text) => window.nect.load(text), sourceText);
  record(
    'public load replaces the project',
    (await page.evaluate(() => window.nect.projectSnapshot().compositions.length)) === 2,
  );

  const beforeInvalid = JSON.stringify(await page.evaluate(() => window.nect.projectSnapshot()));
  await page.getByText('Import', { exact: true }).click();
  await page.getByText('Project composition', { exact: true }).click();
  await page.locator('body > input[type="file"]').last().setInputFiles({
    name: 'invalid.json',
    mimeType: 'application/json',
    buffer: Buffer.from('{}'),
  });
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('rejected'),
  );
  record(
    'invalid project is rejected atomically',
    JSON.stringify(await page.evaluate(() => window.nect.projectSnapshot())) === beforeInvalid,
  );
  assert.equal(result.errors.length, 0, result.errors.join('; '));
  assert.ok(result.steps.every((step) => step.pass), JSON.stringify(result.steps));
} catch (error) {
  result.failure = error.stack || String(error);
  throw error;
} finally {
  await fs.mkdir('test-results', { recursive: true });
  await fs.writeFile('test-results/project-composition-import-ui.json', JSON.stringify(result, null, 2));
  await browser.close();
}
