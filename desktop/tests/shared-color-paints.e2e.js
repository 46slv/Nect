import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/shared-color-paints');
await mkdir(output, { recursive: true });
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
const row = () => page.locator('[data-shared-color-row="paint-slot"]');
async function colors() {
  await ensureWorkspacePanel(page, 'colors');
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((el) => el.open))) await advanced.locator('summary').click();
}
async function copy(kind) {
  await colors();
  await row().locator('[data-shared-color-copy-toggle]').click();
  await row().locator(`[data-shared-color-copy="${kind}"]`).click();
}
function fixture(type) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    geometry = node(type === 'Text' ? 'Text' : 'Rectangle'),
    fill = node('Fill'),
    outputNode = layer.nodes[0];
  geometry.id = 'paint-geometry';
  geometry.text = 'Color';
  fill.id = 'paint-fill';
  outputNode.position = [900, 100];
  geometry.position = [40, 100];
  fill.position = [320, 100];
  layer.nodes.push(geometry, fill);
  connect(layer, geometry.id, fill.id, 'path');
  let paint = fill;
  if (type !== 'Text') {
    paint = node(type, 610, 100);
    paint.id = 'gradient';
    layer.nodes.push(paint);
    connect(layer, fill.id, paint.id, 'path');
  }
  connect(layer, paint.id, outputNode.id, 'path');
  doc.schemaVersion = 54;
  doc.colorSlots = [{ id: 'paint-slot', name: 'Studio', value: { hex: '#336699', alpha: 0.4 } }];
  return { doc, nodeId: paint.id };
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  for (const type of [
    'Linear Gradient Fill',
    'Radial Gradient Fill',
    'Angular Gradient Fill',
    'Text',
  ]) {
    const { doc, nodeId } = fixture(type);
    await page.evaluate((text) => window.nect.load(text), serialize(doc));
    await copy('reference');
    await page.locator(`.graph .node[data-id="${nodeId}"] .node-head strong`).click();
    await ensureWorkspacePanel(page, 'inspector');
    const key = type === 'Text' ? 'color' : 'startColor',
      valueKey =
        type === 'Angular Gradient Fill' ? 'color2' : type === 'Text' ? 'color' : 'endColor',
      field = (name) =>
        page.locator(`.inspector-body .field[data-node="${nodeId}"][data-key="${name}"]`),
      parameter = async (name) =>
        (await snapshot()).compositions[0].layers[0].nodes.find((item) => item.id === nodeId)
          .params[name];
    const beforeLink = await snapshot(),
      beforeLinkState = await state();
    await field(key)
      .getByRole('button', { name: /Paste link into/ })
      .click();
    assert.equal((await parameter(key)).colorSlotId, 'paint-slot');
    assert.equal((await state()).expectedRevision, beforeLinkState.expectedRevision + 1);
    const linked = await snapshot();
    await activateCommand(page, 'undo');
    assert.deepEqual(await snapshot(), beforeLink);
    await activateCommand(page, 'redo');
    assert.deepEqual(await snapshot(), linked);
    await colors();
    await row().locator('[data-shared-color-edit="alpha"]').fill('0.8');
    await row().locator('[data-shared-color-edit="alpha"]').press('Tab');
    await page.waitForFunction(() => !window.nect.evaluate().error);
    const resolved = await page.evaluate(() => window.nect.evaluate().path.fill);
    assert.equal(type === 'Text' ? resolved.alpha : resolved.startColor.alpha, 0.8);
    await copy('value');
    await row().locator('[data-shared-color-edit="alpha"]').fill('0.2');
    await row().locator('[data-shared-color-edit="alpha"]').press('Tab');
    const beforeValue = await snapshot(),
      beforeValueState = await state();
    await ensureWorkspacePanel(page, 'inspector');
    await field(valueKey)
      .getByRole('button', { name: /Paste value into/ })
      .click();
    assert.equal((await parameter(valueKey)).colorSlotId, undefined);
    assert.deepEqual((await parameter(valueKey)).value, { hex: '#336699', alpha: 0.8 });
    assert.equal((await state()).expectedRevision, beforeValueState.expectedRevision + 1);
    const afterValue = await snapshot();
    await activateCommand(page, 'undo');
    assert.deepEqual(await snapshot(), beforeValue);
    await activateCommand(page, 'redo');
    assert.deepEqual(await snapshot(), afterValue);
    if (type === 'Angular Gradient Fill') {
      for (const extra of ['color1', 'endColor']) {
        const before = await snapshot(),
          revision = (await state()).expectedRevision;
        await field(extra)
          .getByRole('button', { name: /Paste value into/ })
          .click();
        assert.deepEqual((await parameter(extra)).value, { hex: '#336699', alpha: 0.8 });
        assert.equal((await state()).expectedRevision, revision + 1);
        const after = await snapshot();
        await activateCommand(page, 'undo');
        assert.deepEqual(await snapshot(), before);
        await activateCommand(page, 'redo');
        assert.deepEqual(await snapshot(), after);
      }
    }
    const savedProject = await snapshot();
    const downloading = page.waitForEvent('download');
    await activateCommand(page, 'save-as');
    const file = resolve(output, `${type.replaceAll(' ', '-')}.json`);
    await (await downloading).saveAs(file);
    assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), savedProject);
    await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
    const chooser = page.waitForEvent('filechooser');
    await activateCommand(page, 'open');
    await (await chooser).setFiles(file);
    await page.waitForFunction(
      (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
      JSON.stringify(savedProject),
    );
    assert.deepEqual((await parameter(valueKey)).value, { hex: '#336699', alpha: 0.8 });
    console.log(`PASS ${type}: copied link/value, live result, Undo/Redo and browser reopen`);
  }
  assert.deepEqual(errors, []);
} finally {
  await browser.close();
}
