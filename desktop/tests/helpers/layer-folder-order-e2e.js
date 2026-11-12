import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { activateCommand } from '../command-menu-actions.js';
import { ensureWorkspacePanel } from './workspace-panels.js';

export async function verifyFolderBlockOrder(page) {
  const ids = await page.evaluate(async () => {
    const { blankDocument, blankLayer, node } = await import('/src/core.js');
    const { migrateLegacyDocumentToCompositionProject } = await import(
      '/src/composition-project-model.js'
    );
    const doc = blankDocument();
    doc.layers = ['Back', 'Duplicate', 'Duplicate', 'Front 1', 'Front 2'].map((name, index) => {
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
    window.nect.load(JSON.stringify(migrateLegacyDocumentToCompositionProject(doc)));
    const send = (type, payload) =>
      window.nect.dispatch({ ...window.nect.commandState, type, payload });
    send('create-layer-folder', {
      folderId: 'moving-folder',
      name: 'Shapes',
      layerIds: [doc.layers[1].id, doc.layers[2].id],
    });
    send('create-layer-folder', {
      folderId: 'neighbor-folder',
      name: 'Front',
      layerIds: [doc.layers[3].id, doc.layers[4].id],
    });
    send('rename-layer-folder', { folderId: 'neighbor-folder', name: 'Redo target' });
    send('undo', {});
    return doc.layers.map((layer) => layer.id);
  });
  const project = () => page.evaluate(() => window.nect.projectSnapshot()),
    svg = () => page.evaluate(() => window.nect.export()),
    folder = () => page.locator('[data-layer-folder="moving-folder"]'),
    orderButton = (direction) => folder().locator(`[data-folder-direction="${direction}"]`),
    dialog = () => page.locator('.layer-folder-move-dialog'),
    before = await project(),
    beforeSvg = await svg();
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${ids[0]}"]`).click();
  await folder().locator('[data-folder-toggle]').click();
  assert.equal(
    await folder().locator('[data-folder-toggle]').getAttribute('aria-expanded'),
    'false',
  );
  assert.equal(await orderButton('forward').isEnabled(), true);
  assert.equal(await orderButton('backward').isEnabled(), true);

  await page.setViewportSize({ width: 1000, height: 800 });
  await page.screenshot({ path: 'test-results/layer-folder-order-narrow.png' });
  assert.equal(
    await folder().evaluate((el) => el.scrollWidth <= el.clientWidth + 1),
    true,
    'Folder controls fit in the narrow panel',
  );
  await page.setViewportSize({ width: 1600, height: 1000 });
  await orderButton('forward').click();
  await dialog().waitFor({ state: 'visible' });
  assert.equal(await dialog().locator('h3').innerText(), 'Move Shapes forward?');
  assert.deepEqual(await dialog().locator('[data-current-order] li').allTextContents(), [
    'Front 2',
    'Front 1',
    'Duplicate · 1',
    'Duplicate · 2',
    'Back',
  ]);
  assert.deepEqual(await dialog().locator('[data-new-order] li').allTextContents(), [
    'Duplicate · 1',
    'Duplicate · 2',
    'Front 2',
    'Front 1',
    'Back',
  ]);
  assert.match(
    await dialog().locator(`li[data-layer-id="${ids[1]}"]`).first().getAttribute('aria-label'),
    /moving/,
  );
  await page.screenshot({ path: 'test-results/layer-folder-order-confirmation.png' });
  await dialog().locator('[data-cancel]').click();
  assert.deepEqual(await project(), before);
  assert.equal(await svg(), beforeSvg);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.equal((await project()).compositions[0].editor.layerFolders[1].name, 'Redo target');
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await project(), before);

  await orderButton('forward').click();
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'rename-layer-folder',
      payload: { folderId: 'neighbor-folder', name: 'Changed while open' },
    }),
  );
  const intervening = await project();
  await dialog().locator('button[type="submit"]').click();
  assert.match(await dialog().locator('[role="alert"]').innerText(), /changed/i);
  assert.deepEqual(await project(), intervening);
  await dialog().locator('[data-cancel]').click();
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await project(), before);

  await orderButton('forward').click();
  await page.evaluate(
    (id) => document.querySelector(`[data-select-layer="${id}"]`).click(),
    ids[3],
  );
  await dialog().locator('button[type="submit"]').click();
  assert.match(await dialog().locator('[role="alert"]').innerText(), /selection changed/i);
  assert.deepEqual(await project(), before);
  await dialog().locator('[data-cancel]').click();
  await page.locator(`[data-select-layer="${ids[0]}"]`).click();

  await orderButton('forward').click();
  await page.evaluate((replacement) => window.nect.load(JSON.stringify(replacement)), before);
  if (await dialog().count()) {
    await dialog().locator('button[type="submit"]').click();
    assert.match(await dialog().locator('[role="alert"]').innerText(), /changed/i);
    await dialog().locator('[data-cancel]').click();
  }
  assert.deepEqual(await project(), before);
  await ensureWorkspacePanel(page, 'layers');
  if ((await folder().locator('[data-folder-toggle]').getAttribute('aria-expanded')) === 'true')
    await folder().locator('[data-folder-toggle]').click();

  await orderButton('backward').click();
  await dialog().locator('button[type="submit"]').click();
  assert.deepEqual(
    (await project()).compositions[0].layers.map(({ id }) => id),
    [ids[1], ids[2], ids[0], ids[3], ids[4]],
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await project(), before, 'one Undo restores a move across a root Layer');

  const revision = await page.evaluate(() => window.nect.commandState.expectedRevision);
  await orderButton('forward').click();
  await dialog().locator('button[type="submit"]').click();
  const after = await project();
  assert.deepEqual(
    after.compositions[0].layers.map(({ id }) => id),
    [ids[0], ids[3], ids[4], ids[1], ids[2]],
  );
  assert.deepEqual(
    after.compositions[0].editor.layerFolders,
    before.compositions[0].editor.layerFolders,
  );
  assert.notEqual(await svg(), beforeSvg);
  assert.equal(await page.evaluate(() => window.nect.commandState.expectedRevision), revision + 1);
  assert.equal(
    await folder().locator('[data-folder-toggle]').getAttribute('aria-expanded'),
    'false',
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await project(), before);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await project(), after);

  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve('test-results/layer-folder-order-project.json');
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), after);
  await page.reload();
  await page.waitForFunction(() => !!window.nect);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  const discard = page.locator('dialog[open] button[value="discard"]');
  if (await discard.isVisible()) await discard.click();
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(after),
  );
  await ensureWorkspacePanel(page, 'layers');
  assert.equal(await orderButton('forward').isDisabled(), true);
  assert.equal(await orderButton('backward').isEnabled(), true);
  if ((await folder().locator('[data-folder-toggle]').getAttribute('aria-expanded')) === 'false')
    await folder().locator('[data-folder-toggle]').click();
  await page.locator(`[data-layer-lock="${ids[1]}"]`).click();
  assert.equal(await orderButton('forward').isDisabled(), true);
  assert.equal(await orderButton('backward').isDisabled(), true);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'create-layer-folder',
      payload: { folderId: 'empty-folder', name: 'Empty', layerIds: [] },
    }),
  );
  const empty = page.locator('[data-layer-folder="empty-folder"]');
  assert.equal(await empty.locator('[data-folder-direction="forward"]').isDisabled(), true);
  assert.equal(await empty.locator('[data-folder-direction="backward"]').isDisabled(), true);
}
