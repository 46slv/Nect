import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { revealNodePresets } from './helpers/boundary-controls.js';
import { activateCommand } from './command-menu-actions.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1400, height: 900 } }),
  errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const send = (type, payload = {}) =>
  page.evaluate(
    ({ type, payload }) => window.nect.dispatch({ ...window.nect.commandState, type, payload }),
    { type, payload },
  );
const project = () => page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.locator('[data-tool="Rectangle"]').click();
  const layerId = await page.evaluate(
    () => window.nect.document.layers.find((l) => l.nodes.some((n) => n.type === 'Rectangle')).id,
  );
  await page.getByRole('button', { name: 'Toggle Effects panel', exact: true }).click();
  await page.locator('[data-effects-apply]').click();
  const first = await send('create-node', { layerId, nodeType: 'Transform', position: [900, 100] }),
    second = await send('create-node', { layerId, nodeType: 'Transform', position: [1250, 100] });
  await send('connect', { layerId, sourceId: first.nodeId, nodeId: second.nodeId, key: 'path' });
  await send('set-parameter', { layerId, nodeId: first.nodeId, key: 'scale', value: 0.5 });
  await page.locator(`.graph .node[data-id="${first.nodeId}"] [data-rename-node]`).click();
  await page
    .locator(`.graph .node[data-id="${second.nodeId}"] [data-rename-node]`)
    .click({ modifiers: ['Shift'] });
  await revealNodePresets(page);
  const beforeCancel = await project();
  await page.locator('[data-action="create-macro"]').click();
  await page
    .getByRole('dialog', { name: 'Create Macro' })
    .locator('[data-macro-name]')
    .fill('Discarded');
  await page
    .getByRole('dialog', { name: 'Create Macro' })
    .getByRole('button', { name: 'Cancel' })
    .click();
  assert.equal(await project(), beforeCancel);
  await revealNodePresets(page);
  await page.locator('[data-action="create-macro"]').click();
  const staleDialog = page.getByRole('dialog', { name: 'Create Macro' });
  await staleDialog.locator('[data-macro-name]').fill('Stale');
  await staleDialog.locator('[data-macro-control="control-1"]').check();
  await send('set-document-settings', { key: 'width', value: 777 });
  const afterNested = await project();
  await staleDialog.getByRole('button', { name: 'Create', exact: true }).click();
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('Selection changed'),
  );
  assert.equal(await project(), afterNested);
  assert.equal(
    await page.evaluate(() => window.nect.projectSnapshot().macroDefinitions?.length || 0),
    0,
  );
  const previous = await page
    .locator('.graph .node.selected')
    .evaluateAll((nodes) => nodes.map((node) => node.dataset.id));
  for (const id of previous)
    await page
      .locator(`.graph .node[data-id="${id}"] .node-head .type-tag`)
      .click({ modifiers: ['Shift'] });
  await page.locator(`.graph .node[data-id="${first.nodeId}"] [data-rename-node]`).click();
  await page
    .locator(`.graph .node[data-id="${second.nodeId}"] [data-rename-node]`)
    .click({ modifiers: ['Shift'] });
  await revealNodePresets(page);
  await page.locator('[data-action="create-macro"]').click();
  const dialog = page.getByRole('dialog', { name: 'Create Macro' });
  await dialog.locator('[data-macro-name]').fill('Double');
  await dialog.locator('[data-macro-control="control-1"]').check();
  await dialog.getByRole('button', { name: 'Create', exact: true }).click();
  await page.waitForFunction(() => window.nect.projectSnapshot().macroDefinitions?.length === 1);
  const definition = await page.evaluate(() => window.nect.projectSnapshot().macroDefinitions[0]);
  assert.equal(definition.name, 'Double');
  assert.equal(definition.parameters[0].defaultValue, 0.5);
  const duplicateName = structuredClone(definition);
  duplicateName.id = 'other-definition';
  duplicateName.parameters[0].defaultValue = 0.25;
  await send('register-macro-definition', { definition: duplicateName });
  await page.locator('.artboard .shape').first().click();
  await page.getByRole('button', { name: 'Toggle Effects panel', exact: true }).click();
  assert.equal(await page.getByRole('button', { name: 'Double · 1', exact: true }).count(), 1);
  assert.equal(await page.getByRole('button', { name: 'Double · 2', exact: true }).count(), 1);
  await page.locator(`[data-effect-id="macro.${definition.id}.${definition.revision}"]`).click();
  await page.locator('[data-effects-apply]').click();
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers.flatMap((l) => l.nodes).find((n) => n.type === 'Macro').macro
          .definitionId,
    ),
    definition.id,
  );
  const inserted = await project();
  const input = page.locator('.inspector-body [data-macro-value="control-1"]');
  await input.fill('0.75');
  await input.press('Enter');
  await page.waitForFunction(() =>
    window.nect.document.layers
      .flatMap((l) => l.nodes)
      .some((n) => n.type === 'Macro' && n.macro.overrides['control-1'] === 0.75),
  );
  const edited = await project();
  assert.notEqual(edited, inserted);
  await page.setViewportSize({ width: 1000, height: 800 });
  await fs.mkdir('test-results', { recursive: true });
  const presetSummary = page
    .locator('.boundary-popover[open] > summary')
    .filter({ hasText: /^Presets$/ });
  if (await presetSummary.count()) await presetSummary.click();
  await input.scrollIntoViewIfNeeded();
  await page.screenshot({
    path: 'test-results/macro-workflow-inspector.png',
    animations: 'disabled',
  });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await project(), inserted);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await project(), edited);
  await page.locator('.inspector-body [data-macro-reset="control-1"]').click();
  await page.waitForFunction(() =>
    window.nect.document.layers
      .flatMap((l) => l.nodes)
      .some((n) => n.type === 'Macro' && !Object.hasOwn(n.macro.overrides, 'control-1')),
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await project(), edited);
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save');
  const file = 'test-results/macro-workflow-project.json';
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await fs.readFile(file, 'utf8')), JSON.parse(edited));
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const choosing = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await page.locator('dialog[open] button[value="discard"]').click();
  await (await choosing).setFiles(file);
  await page.waitForFunction(() => !window.nect.fileState.dirty);
  assert.equal(await project(), edited);
  assert.equal(await page.locator('.artboard .shape').count(), 1);
  assert.deepEqual(errors, []);
  console.log('PASS Macro cancel/create, Effects apply, control, Undo/Redo and Save/Open');
} finally {
  await browser.close();
}
