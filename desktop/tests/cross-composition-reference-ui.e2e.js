import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, connect, node } from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

function fixture(compositionId, artworkId, nodeId, name, value) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    scalar = node('Value', 40, 100);
  layer.id = artworkId;
  layer.name = 'Variables';
  scalar.id = nodeId;
  scalar.name = name;
  scalar.params.value.value = value;
  layer.nodes.push(scalar);
  if (compositionId === 'target-comp') {
    const circle = node('Circle', 350, 100),
      fill = node('Fill', 650, 100),
      output = layer.nodes[0];
    circle.id = 'circle';
    fill.id = 'fill';
    circle.params.radius.expression = 'ref("sink", "value") * 100';
    layer.nodes.push(circle, fill);
    connect(layer, circle.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    const paletteLayer = blankLayer('Palette'),
      paletteCircle = node('Circle', 40, 100),
      paletteFill = node('Fill', 350, 100),
      paletteOutput = paletteLayer.nodes[0];
    paletteLayer.id = 'palette-artwork';
    paletteCircle.id = 'palette-circle';
    paletteFill.id = 'palette-fill';
    paletteCircle.params.center.value = [700, 250];
    paletteLayer.nodes.push(paletteCircle, paletteFill);
    connect(paletteLayer, paletteCircle.id, paletteFill.id, 'path');
    connect(paletteLayer, paletteFill.id, paletteOutput.id, 'path');
    doc.layers.push(paletteLayer);
  }
  const project = migrateLegacyDocumentToCompositionProject(doc);
  project.compositions[0].id = compositionId;
  project.compositions[0].name = compositionId === 'target-comp' ? 'Target' : 'Controllers';
  return project;
}
const project = fixture('target-comp', 'target-artwork', 'sink', 'Result', 0),
  sourceA = fixture('source-comp-a', 'source-artwork-a', 'driver-a', 'Driver', 0.4),
  sourceB = fixture('source-comp-b', 'source-artwork-b', 'driver-b', 'Driver', 0.7);
project.compositions.push(sourceA.compositions[0], sourceB.compositions[0]);

// Vite discovers the worker-only dependency lazily and reloads existing pages
// when it first optimizes it. Warm that dev-server module before the scenario.
const baseUrl = process.env.BASE_URL || 'http://127.0.0.1:5173',
  backendResponse = await fetch(new URL('/src/variable-font/backend.js', baseUrl));
assert.equal(backendResponse.ok, true);
const optimizedFontkit = (await backendResponse.text()).match(
  /from\s+["']([^"']*\/fontkit\.js(?:\?[^"']*)?)["']/,
)?.[1];
if (optimizedFontkit) {
  const dependency = await fetch(new URL(optimizedFontkit, baseUrl));
  assert.equal(dependency.ok, true);
  await dependency.arrayBuffer();
}

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
const field = (id, key = 'value') =>
  page.locator(`.inspector-body .field[data-node="${id}"][data-key="${key}"]`);
const evaluated = (id, key = 'value') =>
  page.evaluate(({ nodeId, key }) => window.nect.evaluate().parameterValue(nodeId, key), {
    nodeId: id,
    key,
  });
async function download(action, name) {
  const pending = page.waitForEvent('download');
  await activateCommand(page, action);
  const file = resolve(`test-results/cross-composition-reference/${name}`);
  await (await pending).saveAs(file);
  return readFile(file);
}
async function selectNode(id) {
  await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
}
async function switchComposition(id) {
  await page.locator(`[role="tab"][data-active-composition="${id}"]`).click();
}

try {
  await mkdir('test-results/cross-composition-reference', { recursive: true });
  await page.goto(baseUrl);
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(project));
  await selectNode('sink');
  await field('sink').locator('[data-fx]').click();
  const dialog = page
      .locator('dialog[open]')
      .filter({ has: page.locator('[data-insert-reference]') }),
    draft = dialog.locator('[aria-label="Parameter expression"]');
  const expression = 'ref(composition, "source-comp-a", "source-artwork-a", "driver-a", "value")';
  await draft.fill('Controllers');
  const completion = dialog
    .locator('.expression-completions [role="option"]')
    .filter({ hasText: 'driver-a' });
  assert.match(await completion.innerText(), /Controllers/);
  await completion.click();
  assert.equal(await draft.inputValue(), expression);
  await draft.fill('');
  await dialog.locator('[data-insert-reference]').click();
  const picker = page.getByRole('dialog', { name: 'Parameter references' });
  await picker.getByRole('combobox').fill('Controllers Variables Driver');
  const choices = picker.locator('.picker-option');
  assert.equal(await choices.count(), 2);
  const labels = await choices.locator(':scope > span').allTextContents();
  assert.notEqual(labels[0].normalize('NFC'), labels[1].normalize('NFC'));
  await page.setViewportSize({ width: 1000, height: 650 });
  await picker.screenshot({ path: resolve('test-results/cross-composition-reference/picker.png') });
  const applyBounds = await picker.locator('.picker-apply').boundingBox();
  assert.ok(applyBounds && applyBounds.width > 0 && applyBounds.height > 0);
  assert.equal(
    await picker.locator('.picker-apply').evaluate((element) => {
      const box = element.getBoundingClientRect(),
        target = document.elementFromPoint(box.x + box.width / 2, box.y + box.height / 2);
      return target === element || element.contains(target);
    }),
    true,
  );
  await page.setViewportSize({ width: 1440, height: 960 });
  await picker.getByRole('combobox').fill('source-comp-a driver-a value');
  assert.equal(await choices.count(), 1);
  await choices.first().click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const beforeStale = await snapshot(),
    beforeStaleState = await state();
  await picker.locator('.picker-apply').click();
  assert.equal(await draft.inputValue(), '');
  assert.match(await dialog.getByRole('alert').innerText(), /Close and reopen/);
  assert.deepEqual(await snapshot(), beforeStale);
  assert.deepEqual(await state(), beforeStaleState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await dialog.locator('[data-cancel]').click();
  await field('sink').locator('[data-fx]').click();
  await dialog.locator('[data-insert-reference]').click();
  const search = picker.getByRole('combobox');
  await search.fill('source-comp-a driver-a value');
  await search.press('ArrowDown');
  await search.press('Enter');
  assert.equal(await draft.inputValue(), expression);
  await dialog.locator('[data-confirm]').click();
  await dialog.waitFor({ state: 'detached' });
  assert.equal((await snapshot()).schemaVersion, 63);
  assert.equal(await evaluated('sink'), 0.4);
  assert.equal(await evaluated('circle', 'radius'), 40);
  const svgBefore = await download('export', 'before.svg'),
    pngBefore = await download('export-png', 'before.png');
  assert.match(svgBefore.toString('utf8'), /<svg/);
  assert.deepEqual([...pngBefore.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);

  await switchComposition('source-comp-a');
  await selectNode('driver-a');
  const input = field('driver-a').locator('[data-value]');
  await input.fill('0.65');
  await input.press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((entry) => entry.id === 'driver-a').params.value
        .value === 0.65,
  );
  await switchComposition('target-comp');
  assert.equal(await evaluated('sink'), 0.65);
  assert.equal(await evaluated('circle', 'radius'), 65);
  const svgAfter = await download('export', 'after.svg'),
    pngAfter = await download('export-png', 'after.png');
  assert.notDeepEqual(svgAfter, svgBefore);
  assert.notDeepEqual(pngAfter, pngBefore);
  await activateCommand(page, 'undo');
  assert.equal(await evaluated('sink'), 0.4);
  await activateCommand(page, 'redo');
  assert.equal(await evaluated('sink'), 0.65);

  // A Color operation uses the same Scalar source without copying its value.
  await selectNode('fill');
  await field('fill', 'color').locator('[data-fx]').click();
  const prefix = 'alpha(value, ';
  await draft.fill(`${prefix})`);
  await draft.evaluate(
    (input, position) => input.setSelectionRange(position, position),
    prefix.length,
  );
  await dialog.locator('[data-insert-reference]').click();
  await picker.getByRole('combobox').fill('source-comp-a driver-a value');
  await choices.first().click();
  await picker.locator('.picker-apply').click();
  assert.equal(await draft.inputValue(), `${prefix}${expression})`);
  await dialog.locator('[data-confirm]').click();
  await dialog.waitFor({ state: 'detached' });
  assert.equal((await evaluated('fill', 'color')).alpha, 0.65);

  await switchComposition('source-comp-a');
  await selectNode('driver-a');
  await input.fill('0.3');
  await input.press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((entry) => entry.id === 'driver-a').params.value
        .value === 0.3,
  );
  await switchComposition('target-comp');
  assert.equal(await evaluated('sink'), 0.3);
  assert.equal((await evaluated('fill', 'color')).alpha, 0.3);
  await activateCommand(page, 'undo');
  assert.equal(await evaluated('sink'), 0.65);
  assert.equal((await evaluated('fill', 'color')).alpha, 0.65);
  await activateCommand(page, 'redo');
  assert.equal(await evaluated('sink'), 0.3);
  assert.equal((await evaluated('fill', 'color')).alpha, 0.3);

  const beforeSave = await snapshot();
  const pendingSave = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve('test-results/cross-composition-reference/project.json');
  await (await pendingSave).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), beforeSave);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page.locator('#open-file').setInputFiles(file);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(beforeSave),
  );
  await switchComposition('target-comp');
  assert.equal(await evaluated('sink'), 0.3);
  assert.equal((await evaluated('fill', 'color')).alpha, 0.3);
  await page.waitForFunction(() => {
    const shape = document.querySelector('.shape[data-layer="target-artwork"]');
    return shape && Math.abs(shape.getBBox().width - 60) < 0.001;
  });
  await selectNode('circle');
  const reopenedRadius = field('circle', 'radius').locator('.parameter-result');
  await reopenedRadius.scrollIntoViewIfNeeded();
  assert.equal(await reopenedRadius.isVisible(), true);
  assert.match(await reopenedRadius.innerText(), /Result\s*·\s*30/);
  await page.screenshot({
    path: resolve('test-results/cross-composition-reference/reopened-canvas.png'),
  });

  // The detached Key Color artwork preview must retain the current project's
  // cross-Composition context while leaving authored state untouched.
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-key-color-recipe-open]').click();
  await page.locator('[data-research-hex]').fill('#3273c7');
  await page.locator('[data-research-alpha]').fill('1');
  await page.locator('[data-research-plan]').selectOption('research-neutral-start');
  await page.locator('[data-research-preview]').click();
  await page.locator('[data-research-create]').click();
  const trial = await snapshot(),
    slotId = trial.keyColorRecipe.committedSnapshot.intent.slots[0].id;
  await page.evaluate(
    (id) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'bind-color-slot',
        payload: { layerId: 'palette-artwork', nodeId: 'palette-fill', key: 'color', slotId: id },
      }),
    slotId,
  );
  const beforePreview = await snapshot(),
    previewState = await state();
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-key-color-recipe-open]').click();
  await page
    .locator(`[data-key-color-input="deltaL"][data-key-color-slot="${slotId}"]`)
    .fill('0.2');
  await page.locator('[data-key-color-preview]').click();
  const artboard = page.locator('.artboard'),
    savedArtwork = await artboard.innerHTML();
  await page.locator('[data-artwork-show]').click();
  await page.waitForFunction(
    (saved) => document.querySelector('.artboard').innerHTML !== saved,
    savedArtwork,
  );
  assert.equal(await page.locator('.shape[data-layer="target-artwork"]').count(), 1);
  await page.locator('[data-artwork-restore]').click();
  assert.equal(await artboard.innerHTML(), savedArtwork);
  await page.locator('dialog footer [data-key-color-cancel]').click();
  assert.deepEqual(await snapshot(), beforePreview);
  assert.deepEqual(await state(), previewState);

  // Opening and editing a variable-font project also prepares a detached
  // document; it must retain the project context for cross-Composition refs.
  const bytes = await readFile(
      new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
    ),
    binary = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    asset = makeVariableFontAsset(binary, 'Roboto-variable.ttf'),
    variableProject = structuredClone(beforeSave),
    variableText = node('Text');
  variableText.id = 'variable-text';
  variableText.font = asset.id;
  variableText.text = 'AV';
  variableText.variation = { version: 1, fontId: asset.id, axes: {} };
  variableProject.fontAssets = { ...(variableProject.fontAssets || {}), [asset.id]: asset.asset };
  variableProject.compositions
    .find((entry) => entry.id === 'target-comp')
    .layers[0].nodes.push(variableText);
  const variableFile = resolve('test-results/cross-composition-reference/variable-project.json');
  await writeFile(variableFile, serializeCompositionProject(variableProject));
  await page.locator('#open-file').setInputFiles(variableFile);
  await page.waitForFunction(() =>
    window.nect.document.layers[0].nodes.some((entry) => entry.id === 'variable-text'),
  );
  assert.equal(await evaluated('sink'), 0.3);
  await selectNode('variable-text');
  await page
    .locator('.inspector-body [data-text-inspector-section="typography"] > summary')
    .click();
  const weight = page.locator('.inspector-body [data-variable-axis="wght"]');
  await weight.scrollIntoViewIfNeeded();
  await weight.fill('650');
  await weight.press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((entry) => entry.id === 'variable-text').variation
        .axes.wght === 650,
  );
  assert.equal(await evaluated('sink'), 0.3);
  await activateCommand(page, 'undo');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((entry) => entry.id === 'variable-text').variation
        .axes.wght === undefined,
  );
  await activateCommand(page, 'redo');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((entry) => entry.id === 'variable-text').variation
        .axes.wght === 650,
  );
  assert.equal(await evaluated('sink'), 0.3);
  assert.deepEqual(errors, []);
  console.log(
    'PASS cross-Composition picker, source edit, exports, preview, JSON reopen and variable font',
  );
} finally {
  await browser.close();
}
