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

  const movedIds = await page.evaluate(async () => {
    const { blankDocument, blankLayer, node } = await import('/src/core.js');
    const { migrateLegacyDocumentToCompositionProject } = await import(
      '/src/composition-project-model.js'
    );
    const doc = blankDocument();
    doc.layers = [
      'Back',
      'Duplicate',
      'Duplicate',
      'A very long selected Layer name '.repeat(5),
      'A very long selected Layer name '.repeat(5),
    ].map((name, index) => {
      const layer = blankLayer(name),
        rectangle = node('Rectangle'),
        fill = node('Fill');
      rectangle.params.center.value = [100, 100];
      rectangle.params.size.value = [100, 100];
      fill.params.path.source = rectangle.id;
      fill.params.color.value = {
        hex: ['#ff0000', '#00ff00', '#0000ff', '#ffff00', '#ff00ff'][index],
        alpha: 0.5,
      };
      layer.nodes.unshift(rectangle, fill);
      layer.nodes.find((item) => item.type === 'Output').params.path.source = fill.id;
      return layer;
    });
    const next = migrateLegacyDocumentToCompositionProject(doc);
    const other = migrateLegacyDocumentToCompositionProject(blankDocument());
    next.compositions[0].id = 'move-main';
    other.compositions[0].id = 'move-other';
    next.compositions.push(other.compositions[0]);
    window.nect.load(JSON.stringify(next));
    const send = (type, payload) =>
      window.nect.dispatch({ ...window.nect.commandState, type, payload });
    send('create-layer-folder', {
      folderId: 'target',
      name: 'Destination',
      layerIds: [doc.layers[0].id],
    });
    send('create-layer-folder', {
      folderId: 'source',
      name: 'Source',
      layerIds: [doc.layers[1].id, doc.layers[2].id],
    });
    send('rename-layer-folder', { folderId: 'target', name: 'Redo available' });
    send('undo', {});
    return doc.layers.map((layer) => layer.id);
  });
  const pixel = () =>
    page.evaluate(async () => {
      const svg = window.nect.export();
      const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
      try {
        const image = new Image();
        image.src = url;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = canvas.height = 200;
        const context = canvas.getContext('2d');
        context.drawImage(image, 0, 0);
        return [...context.getImageData(100, 100, 1, 1).data];
      } finally {
        URL.revokeObjectURL(url);
      }
    });
  const selectMoveLayers = async () => {
    await select(movedIds[0]);
    await select(movedIds[2], true);
    await select(movedIds[3], true);
  };
  await ensureWorkspacePanel(page, 'layers');
  await selectMoveLayers();
  const beforeMove = await project(),
    beforeMovePixel = await pixel(),
    beforeMoveRevision = await page.evaluate(() => window.nect.projectRevision),
    selectedBefore = await page.locator('.layer.selected').count();
  await page.locator('[data-folder-add="target"]').click();
  let moveDialog = page.locator('.layer-folder-move-dialog');
  await moveDialog.waitFor({ state: 'visible' });
  assert.equal(
    await page.evaluate(() => document.activeElement?.hasAttribute('data-cancel')),
    true,
  );
  assert.deepEqual(await moveDialog.locator('[data-current-order] li').allTextContents(), [
    `${'A very long selected Layer name '.repeat(5)} · 1`,
    `${'A very long selected Layer name '.repeat(5)} · 2`,
    'Duplicate · 1',
    'Duplicate · 2',
    'Back',
  ]);
  assert.deepEqual(await moveDialog.locator('[data-new-order] li').allTextContents(), [
    `${'A very long selected Layer name '.repeat(5)} · 1`,
    'Duplicate · 2',
    `${'A very long selected Layer name '.repeat(5)} · 2`,
    'Duplicate · 1',
    'Back',
  ]);
  for (const [id, label] of [
    [movedIds[1], 'Duplicate · 2'],
    [movedIds[2], 'Duplicate · 1'],
  ]) {
    assert.deepEqual(await moveDialog.locator(`li[data-layer-id="${id}"]`).allTextContents(), [
      label,
      label,
    ]);
  }
  assert.match(
    await moveDialog
      .locator(`li[data-layer-id="${movedIds[2]}"]`)
      .first()
      .getAttribute('aria-label'),
    /selected/,
  );
  assert.equal(
    await moveDialog
      .locator(`li[data-layer-id="${movedIds[3]}"] .layer-folder-order-name`)
      .first()
      .getAttribute('title'),
    `${'A very long selected Layer name '.repeat(5)} · 2`,
  );
  for (const suffix of await moveDialog
    .locator(`li[data-layer-id="${movedIds[3]}"] .layer-folder-order-number`)
    .all()) {
    const box = await suffix.boundingBox(),
      parent = await suffix.locator('..').boundingBox();
    assert.ok(box.width > 0 && box.x + box.width <= parent.x + parent.width + 1);
  }
  await page.screenshot({ path: 'test-results/layer-folder-move-preview.png' });
  await page.setViewportSize({ width: 1000, height: 800 });
  await page.screenshot({ path: 'test-results/layer-folder-move-preview-narrow.png' });
  await page.setViewportSize({ width: 1600, height: 1000 });
  await page.keyboard.press('Escape');
  assert.equal(await moveDialog.count(), 0);
  assert.deepEqual(await project(), beforeMove);
  assert.equal(await page.evaluate(() => window.nect.projectRevision), beforeMoveRevision);
  assert.equal(await page.locator('.layer.selected').count(), selectedBefore);
  assert.deepEqual(await pixel(), beforeMovePixel);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.equal((await project()).compositions[0].editor.layerFolders[0].name, 'Redo available');
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  await selectMoveLayers();
  await page.locator('[data-folder-add="target"]').click();
  moveDialog = page.locator('.layer-folder-move-dialog');
  await page.evaluate(
    (id) => document.querySelector(`[data-select-layer="${id}"]`).click(),
    movedIds[4],
  );
  await moveDialog.locator('button[type="submit"]').click();
  assert.match(await moveDialog.locator('[role="alert"]').textContent(), /selection changed/i);
  assert.deepEqual(await project(), beforeMove);
  await moveDialog.locator('[data-cancel]').click();
  await selectMoveLayers();
  await page.locator('[data-folder-add="target"]').click();
  moveDialog = page.locator('.layer-folder-move-dialog');
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'rename-layer-folder',
      payload: { folderId: 'source', name: 'Changed while open' },
    }),
  );
  const afterInterveningEdit = await project();
  await moveDialog.locator('button[type="submit"]').click();
  assert.match(await moveDialog.locator('[role="alert"]').textContent(), /changed/i);
  assert.deepEqual(await project(), afterInterveningEdit);
  await moveDialog.locator('[data-cancel]').click();
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await project(), beforeMove);
  await selectMoveLayers();
  await page.locator('[data-folder-add="target"]').click();
  moveDialog = page.locator('.layer-folder-move-dialog');
  await page.evaluate((replacement) => window.nect.load(JSON.stringify(replacement)), beforeMove);
  if (await moveDialog.count()) {
    await moveDialog.locator('button[type="submit"]').click();
    assert.match(await moveDialog.locator('[role="alert"]').textContent(), /changed/i);
    await moveDialog.locator('[data-cancel]').click();
  }
  assert.deepEqual(await project(), beforeMove);
  // Seed a different selection on the exact prior History entry before moving.
  await select(movedIds[1]);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'rename-layer-folder',
      payload: { folderId: 'source', name: 'Temporary history' },
    }),
  );
  await page.locator('[data-action="undo"]').click();
  await page.locator('[data-action="redo"]').click();
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await project(), beforeMove);
  await selectMoveLayers();
  await page.locator('[data-folder-add="target"]').click();
  moveDialog = page.locator('.layer-folder-move-dialog');
  await moveDialog.locator('button[type="submit"]').click();
  const moved = await project();
  assert.deepEqual(
    moved.compositions[0].layers.map((layer) => layer.id),
    [movedIds[0], movedIds[2], movedIds[3], movedIds[1], movedIds[4]],
  );
  assert.deepEqual(moved.compositions[1], beforeMove.compositions[1]);
  assert.deepEqual(moved.compositions[0].editor.layerFolders[1].members, [
    { layerId: movedIds[1] },
  ]);
  assert.notDeepEqual(await pixel(), beforeMovePixel);
  assert.equal(await page.locator('.layer.selected').count(), selectedBefore);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await project(), beforeMove);
  assert.deepEqual(
    (
      await page
        .locator('.layer.selected')
        .evaluateAll((rows) => rows.map((row) => row.dataset.layer))
    ).sort(),
    [movedIds[0], movedIds[2], movedIds[3]].sort(),
  );
  assert.deepEqual(await pixel(), beforeMovePixel);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await project(), moved);
  assert.deepEqual(
    (
      await page
        .locator('.layer.selected')
        .evaluateAll((rows) => rows.map((row) => row.dataset.layer))
    ).sort(),
    [movedIds[0], movedIds[2], movedIds[3]].sort(),
  );
  const movingDownload = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const movedFile = resolve('test-results/layer-folder-move-project.json');
  await (await movingDownload).saveAs(movedFile);
  assert.deepEqual(JSON.parse(await readFile(movedFile, 'utf8')), moved);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const movingChooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  const movingDiscard = page.locator('dialog[open] button[value="discard"]');
  if (await movingDiscard.isVisible()) await movingDiscard.click();
  await (await movingChooser).setFiles(movedFile);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(moved),
  );
  assert.deepEqual(await project(), moved);
  assert.notDeepEqual(await pixel(), beforeMovePixel);
  assert.deepEqual(errors, []);
  console.log('PASS Layer Folder organization, confirmed order change, Undo and project reopen');
} finally {
  await browser.close();
}
