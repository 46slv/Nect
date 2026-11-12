import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const ids = await page.evaluate(async () => {
    const { blankDocument, blankLayer, node } = await import('/src/core.js');
    const { migrateLegacyDocumentToCompositionProject } = await import(
      '/src/composition-project-model.js'
    );
    const doc = blankDocument();
    doc.layers = ['Back', 'Middle', 'Front', 'Top'].map((name, index) => {
      const layer = blankLayer(name),
        rectangle = node('Rectangle'),
        fill = node('Fill');
      rectangle.params.center.value = [100, 100];
      rectangle.params.size.value = [100, 100];
      fill.params.path.source = rectangle.id;
      fill.params.color.value = {
        hex: ['#ff0000', '#00ff00', '#0000ff', '#ffff00'][index],
        alpha: 0.5,
      };
      layer.nodes.unshift(rectangle, fill);
      layer.nodes.find((item) => item.type === 'Output').params.path.source = fill.id;
      return layer;
    });
    const project = migrateLegacyDocumentToCompositionProject(doc);
    const other = migrateLegacyDocumentToCompositionProject(blankDocument());
    project.compositions[0].id = 'folder-a';
    other.compositions[0].id = 'folder-b';
    project.compositions.push(other.compositions[0]);
    window.nect.load(JSON.stringify(project));
    return doc.layers.map((layer) => layer.id);
  });
  const project = () => page.evaluate(() => window.nect.projectSnapshot());
  const artwork = () => page.evaluate(() => window.nect.export());
  const select = async (id, shift = false) =>
    page.locator(`[data-select-layer="${id}"]`).click({ modifiers: shift ? ['Shift'] : [] });
  const before = await project(),
    beforeSvg = await artwork();

  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-action="add-layer-folder"]').click();
  assert.equal(await page.locator('[data-layer-folder]').count(), 1);
  let folder = page.locator('[data-layer-folder]').first();
  assert.deepEqual((await project()).compositions[0].editor.layerFolders[0].members, []);
  await select(ids[1]);
  await select(ids[2], true);
  await folder.locator('[data-folder-add]').click();
  const grouped = await project(),
    entry = grouped.compositions[0].editor.layerFolders[0];
  assert.deepEqual(entry.members, [{ layerId: ids[1] }, { layerId: ids[2] }]);
  assert.deepEqual(grouped.compositions[0].layers, before.compositions[0].layers);
  assert.equal(await artwork(), beforeSvg);
  assert.equal(await page.locator('.layer.folder-member').count(), 2);
  assert.equal(await page.locator('.layer.folder-member.selected').count(), 2);
  await mkdir('test-results', { recursive: true });
  await page.locator('[data-workspace-split="editing"]').focus();
  await page.keyboard.press('End');
  await page.screenshot({ path: 'test-results/layer-folders-expanded.png' });
  await page.setViewportSize({ width: 1000, height: 800 });
  await page.screenshot({ path: 'test-results/layer-folders-narrow.png' });
  await page.setViewportSize({ width: 1600, height: 1000 });

  const beforeCollapse = await project();
  await folder.locator('[data-folder-toggle]').click();
  folder = page.locator('[data-layer-folder]').first();
  assert.equal(await folder.locator('[data-folder-toggle]').getAttribute('aria-expanded'), 'false');
  assert.equal(await page.locator('.layer.folder-member').count(), 0);
  assert.equal(await page.evaluate(() => document.activeElement?.dataset.folderToggle), entry.id);
  assert.deepEqual(await project(), beforeCollapse);
  assert.equal(await artwork(), beforeSvg);
  await page.locator('[role="tab"][data-active-composition="folder-b"]').click();
  await page.locator('[role="tab"][data-active-composition="folder-a"]').click();
  folder = page.locator('[data-layer-folder]').first();
  assert.equal(await folder.locator('[data-folder-toggle]').getAttribute('aria-expanded'), 'false');
  assert.deepEqual(await project(), beforeCollapse);
  await folder.locator('[data-folder-toggle]').click();
  folder = page.locator('[data-layer-folder]').first();
  assert.equal(await page.locator('.layer.folder-member.selected').count(), 2);
  await folder.locator('[data-folder-name]').focus();
  await page.keyboard.press('F2');
  await page.locator('[aria-label="Folder name"]').fill('Shapes');
  await page.locator('[aria-label="Folder name"]').press('Enter');
  await page.waitForFunction(
    () => window.nect.projectSnapshot().compositions[0].editor.layerFolders[0].name === 'Shapes',
  );
  assert.equal(await artwork(), beforeSvg);

  await select(ids[3]);
  await page.locator('[data-folder-add]').click();
  assert.equal((await project()).compositions[0].editor.layerFolders[0].members.length, 3);
  await select(ids[3]);
  await page.locator('[data-folder-out]').click();
  const saved = await project();
  assert.deepEqual(saved.compositions[0].editor.layerFolders[0].members, [
    { layerId: ids[1] },
    { layerId: ids[2] },
  ]);
  assert.equal(await artwork(), beforeSvg);
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const savedFile = resolve('test-results/layer-folders-project.json');
  await (await downloading).saveAs(savedFile);
  assert.deepEqual(JSON.parse(await readFile(savedFile, 'utf8')), saved);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  const discard = page.locator('dialog[open] button[value="discard"]');
  if (await discard.isVisible()) await discard.click();
  await (await chooser).setFiles(savedFile);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(saved),
  );
  await ensureWorkspacePanel(page, 'layers');
  assert.deepEqual(await project(), saved);
  assert.equal(await page.locator('.layer.folder-member').count(), 2);
  assert.equal(await artwork(), beforeSvg);
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  await page.locator('[data-folder-name]').focus();
  await page.keyboard.press('F2');
  await page.locator('[aria-label="Folder name"]').fill('Pending Folder name');
  const warning = new Promise((resolveWarning, reject) => {
    const timer = setTimeout(() => reject(Error('Folder name reload did not warn')), 10000);
    page.once('dialog', async (dialog) => {
      clearTimeout(timer);
      const type = dialog.type();
      await dialog.dismiss();
      resolveWarning(type);
    });
  });
  await page.evaluate(() => setTimeout(() => window.location.reload(), 0));
  assert.equal(await warning, 'beforeunload');
  assert.equal(
    await page.locator('[aria-label="Folder name"]').inputValue(),
    'Pending Folder name',
  );
  await page.locator('[aria-label="Folder name"]').press('Escape');
  assert.deepEqual(await project(), saved);

  const historyMismatches = [];
  for (const action of ['create', 'add', 'remove', 'rename', 'ungroup']) {
    const nextIds = await page.evaluate(async (action) => {
      const { blankDocument, blankLayer, node } = await import('/src/core.js');
      const { migrateLegacyDocumentToCompositionProject } = await import(
        '/src/composition-project-model.js'
      );
      const doc = blankDocument();
      doc.layers = ['Back', 'Middle', 'Front'].map((name) => {
        const layer = blankLayer(name),
          rectangle = node('Rectangle'),
          fill = node('Fill');
        fill.params.path.source = rectangle.id;
        layer.nodes.unshift(rectangle, fill);
        layer.nodes.find((item) => item.type === 'Output').params.path.source = fill.id;
        return layer;
      });
      window.nect.load(JSON.stringify(migrateLegacyDocumentToCompositionProject(doc)));
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'create-layer-folder',
        payload: {
          folderId: 'history-folder',
          name: 'Source',
          layerIds:
            action === 'create'
              ? []
              : action === 'remove'
                ? doc.layers.map((l) => l.id)
                : [doc.layers[0].id],
        },
      });
      return doc.layers.map((layer) => layer.id);
    }, action);
    await ensureWorkspacePanel(page, 'layers');
    await select(nextIds[0]);
    await page.evaluate(() =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'rename-layer-folder',
        payload: { folderId: 'history-folder', name: 'Temporary history' },
      }),
    );
    await page.locator('[data-action="undo"]').click();
    await page.locator('[data-action="redo"]').click();
    await page.locator('[data-action="undo"]').click();
    await select(nextIds[1]);
    const expectedSelection = [nextIds[1]];
    if (action !== 'add') {
      await select(nextIds[2], true);
      expectedSelection.push(nextIds[2]);
    }
    const beforeAction = await project();
    if (action === 'create') await page.locator('[data-action="add-layer-folder"]').click();
    else if (action === 'add') await page.locator('[data-folder-add="history-folder"]').click();
    else if (action === 'remove') await page.locator('[data-folder-out="history-folder"]').click();
    else if (action === 'ungroup')
      await page.locator('[data-folder-dissolve="history-folder"]').click();
    else {
      await page.locator('[data-folder-name="history-folder"]').focus();
      await page.keyboard.press('F2');
      await page.locator('[aria-label="Folder name"]').fill('Renamed');
      await page.locator('[aria-label="Folder name"]').press('Enter');
    }
    const afterAction = await project();
    assert.notDeepEqual(afterAction, beforeAction, action);
    for (const [direction, expected] of [
      ['undo', beforeAction],
      ['redo', afterAction],
    ]) {
      await page.locator(`[data-action="${direction}"]`).click();
      assert.deepEqual(await project(), expected, `${action} ${direction} document`);
      const actual = (
        await page
          .locator('.layer.selected')
          .evaluateAll((rows) => rows.map((row) => row.dataset.layer))
      ).sort();
      if (JSON.stringify(actual) !== JSON.stringify([...expectedSelection].sort()))
        historyMismatches.push(`${action} ${direction}: selection changed`);
    }
  }
  assert.deepEqual(historyMismatches, []);
  assert.deepEqual(errors, []);
  console.log('PASS Layer Folder create, membership, disclosure, rename and project reopen');
} finally {
  await browser.close();
}
