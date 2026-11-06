import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, transformExtensions } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const targetDocument = blankDocument(),
  targetLayer = targetDocument.layers[0],
  circle = node('Circle', 50, 80),
  fill = node('Fill', 400, 80),
  length = node('Value', 50, 350),
  vectorComponent = node('Value', 400, 350),
  sourceDocument = blankDocument(),
  sourceLayer = sourceDocument.layers[0],
  curve = node('Curve', 50, 80),
  transform = node('Transform', 400, 80),
  sourceFill = node('Fill', 750, 80);
targetLayer.id = 'target-art';
targetLayer.name = 'Artwork';
circle.id = 'target-circle';
fill.id = 'target-fill';
length.id = 'handle-length';
vectorComponent.id = 'vector-component';
targetLayer.nodes.push(circle, fill, length, vectorComponent);
connect(targetLayer, circle.id, fill.id, 'path');
connect(targetLayer, fill.id, targetLayer.nodes[0].id, 'path');
sourceLayer.id = 'source-art';
sourceLayer.name = 'Artwork';
curve.id = 'source-curve';
curve.name = 'Outline';
curve.points[0].id = 'anchor';
curve.points[0].position.value = [30, 40];
curve.points[0].handleA.value = [3, 4];
transform.id = 'source-transform';
transform.name = 'Mover';
transform.params.offset.value = [100, 200];
transform.params.scale.value = 2;
transform.params.uniformScale = {
  ...structuredClone(transformExtensions.uniformScale),
  value: false,
};
transform.params.scaleWidth = { ...structuredClone(transformExtensions.scaleWidth), value: 3 };
sourceFill.id = 'source-fill';
sourceLayer.nodes.push(curve, transform, sourceFill);
connect(sourceLayer, curve.id, transform.id, 'path');
connect(sourceLayer, transform.id, sourceFill.id, 'path');
connect(sourceLayer, sourceFill.id, sourceLayer.nodes[0].id, 'path');
const project = migrateLegacyDocumentToCompositionProject(targetDocument),
  source = migrateLegacyDocumentToCompositionProject(sourceDocument);
project.compositions[0].id = 'target-comp';
project.compositions[0].name = 'Design';
source.compositions[0].id = 'source-comp';
source.compositions[0].name = 'Design';
project.compositions.push(source.compositions[0]);

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } }),
  errors = [];
let capturedPicker = false;
page.setDefaultTimeout(15000);
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const field = (id, key) =>
  page.locator(`.inspector-body .field[data-node="${id}"][data-key="${key}"]`);
const result = (nodeId, key) =>
  page.evaluate(({ nodeId, key }) => window.nect.evaluate().parameterValue(nodeId, key), {
    nodeId,
    key,
  });
async function selectNode(id) {
  await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
}
async function switchComposition(id) {
  await page.locator(`[role="tab"][data-active-composition="${id}"]`).click();
}
async function download(action, name) {
  const pending = page.waitForEvent('download');
  await activateCommand(page, action);
  const file = resolve(`test-results/cross-composition-vector-reference/${name}`);
  await (await pending).saveAs(file);
  return readFile(file);
}
async function openExpression(id, key) {
  await selectNode(id);
  await field(id, key).locator('[data-fx]').click();
  const dialog = page
    .locator('dialog[open]')
    .filter({ has: page.locator('[data-insert-reference]') });
  await dialog.locator('[aria-label="Parameter expression"]').fill('');
  return dialog;
}
async function insertReference(dialog, sourceId, key, label, keyboard = false) {
  await dialog.locator('[data-insert-reference]').click();
  const picker = page.getByRole('dialog', { name: 'Parameter references' }),
    search = picker.getByRole('combobox'),
    options = picker.locator('.picker-option');
  await search.fill(
    sourceId === 'source-curve' ? 'Design Artwork Outline Point 1' : 'Design Artwork Mover offset',
  );
  const choice = options.filter({ hasText: label });
  assert.equal(
    await choice.count(),
    1,
    `foreign ${key} ${label} must be selectable: ${JSON.stringify(await options.allTextContents())}`,
  );
  if (!capturedPicker) {
    await page.setViewportSize({ width: 1000, height: 650 });
    await picker.screenshot({
      path: resolve('test-results/cross-composition-vector-reference/picker.png'),
    });
    assert.equal(
      await picker.locator('.picker-apply').evaluate((button) => {
        const box = button.getBoundingClientRect(),
          target = document.elementFromPoint(box.x + box.width / 2, box.y + box.height / 2);
        return target === button || button.contains(target);
      }),
      true,
    );
    await page.setViewportSize({ width: 1440, height: 960 });
    capturedPicker = true;
  }
  if (keyboard) {
    for (let index = 0; index < (await options.count()); index++) {
      await search.press('ArrowDown');
      if ((await choice.getAttribute('aria-selected')) === 'true') break;
    }
    assert.equal(await choice.getAttribute('aria-selected'), 'true');
    await search.press('Enter');
  } else {
    await choice.click();
    await picker.locator('.picker-apply').click();
  }
}
async function confirmExpression(dialog) {
  await dialog.locator('[data-confirm]').click();
  await dialog.waitFor({ state: 'detached' });
}

try {
  await mkdir('test-results/cross-composition-vector-reference', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(project));

  const pointX =
      'ref(componentComposition, "source-comp", "source-art", "source-curve", "anchor.position", "x")',
    pointY =
      'ref(componentComposition, "source-comp", "source-art", "source-curve", "anchor.position", "y")',
    handleLength =
      'ref(componentComposition, "source-comp", "source-art", "source-curve", "anchor.handleA", "length")',
    offsetX =
      'ref(componentComposition, "source-comp", "source-art", "source-transform", "offset", "x")';
  let dialog = await openExpression('target-circle', 'radius');
  await insertReference(dialog, 'source-curve', 'anchor.position', 'Point 1 · X');
  assert.equal(await dialog.locator('[aria-label="Parameter expression"]').inputValue(), pointX);
  await confirmExpression(dialog);
  assert.equal(await result('target-circle', 'radius'), 30);

  dialog = await openExpression('target-circle', 'center');
  await insertReference(dialog, 'source-curve', 'anchor.position', 'Point 1 · X');
  const draft = dialog.locator('[aria-label="Parameter expression"]');
  await draft.fill(`${pointX}; `);
  await insertReference(dialog, 'source-curve', 'anchor.position', 'Point 1 · Y', true);
  assert.equal(await draft.inputValue(), `${pointX}; ${pointY}`);
  await confirmExpression(dialog);
  assert.deepEqual(await result('target-circle', 'center'), [30, 40]);

  dialog = await openExpression('handle-length', 'value');
  await insertReference(dialog, 'source-curve', 'anchor.handleA', 'Point 1 · Incoming · Length');
  assert.equal(
    await dialog.locator('[aria-label="Parameter expression"]').inputValue(),
    handleLength,
  );
  await confirmExpression(dialog);
  assert.equal(await result('handle-length', 'value'), 5);

  dialog = await openExpression('vector-component', 'value');
  await insertReference(dialog, 'source-transform', 'offset', 'offset.x');
  assert.equal(await dialog.locator('[aria-label="Parameter expression"]').inputValue(), offsetX);
  await confirmExpression(dialog);
  assert.equal(await result('vector-component', 'value'), 100);

  dialog = await openExpression('target-fill', 'color');
  const colorDraft = dialog.locator('[aria-label="Parameter expression"]'),
    colorPrefix = 'alpha(value, ';
  await colorDraft.fill(`${colorPrefix} / 10)`);
  await colorDraft.evaluate(
    (input, position) => input.setSelectionRange(position, position),
    colorPrefix.length,
  );
  await insertReference(dialog, 'source-curve', 'anchor.handleA', 'Point 1 · Incoming · Length');
  assert.equal(await colorDraft.inputValue(), `${colorPrefix}${handleLength} / 10)`);
  await confirmExpression(dialog);
  assert.equal((await result('target-fill', 'color')).alpha, 0.5);
  assert.equal((await snapshot()).schemaVersion, 65);
  const svgBefore = await download('export', 'before.svg'),
    pngBefore = await download('export-png', 'before.png');
  assert.match(svgBefore.toString('utf8'), /<svg/);
  assert.deepEqual([...pngBefore.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);

  // Values are the source Curve's local properties, not the downstream transformed position.
  await switchComposition('source-comp');
  await page.locator('.graph [data-point-panel-mode="source-curve"]').click();
  const pointField = (key) =>
    page.locator(`.graph .field[data-node="source-curve"][data-key="anchor.${key}"]`);
  await pointField('position').locator('[data-value]').first().fill('35');
  await pointField('position').locator('[data-value]').first().press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'source-curve').points[0].position
        .value[0] === 35,
  );
  await pointField('handleA').locator('[data-value]').first().fill('6');
  await pointField('handleA').locator('[data-value]').first().press('Tab');
  await pointField('handleA').locator('[data-value]').nth(1).fill('8');
  await pointField('handleA').locator('[data-value]').nth(1).press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'source-curve').points[0].handleA
        .value[1] === 8,
  );
  await selectNode('source-transform');
  await field('source-transform', 'offset').locator('[data-value]').first().fill('115');
  await field('source-transform', 'offset').locator('[data-value]').first().press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'source-transform').params.offset
        .value[0] === 115,
  );
  await switchComposition('target-comp');
  assert.equal(await result('target-circle', 'radius'), 35);
  assert.deepEqual(await result('target-circle', 'center'), [35, 40]);
  assert.equal(await result('handle-length', 'value'), 10);
  assert.equal(await result('vector-component', 'value'), 115);
  assert.equal((await result('target-fill', 'color')).alpha, 1);
  const svgAfter = await download('export', 'after.svg'),
    pngAfter = await download('export-png', 'after.png');
  assert.notDeepEqual(svgAfter, svgBefore);
  assert.notDeepEqual(pngAfter, pngBefore);
  await activateCommand(page, 'undo');
  assert.equal(await result('vector-component', 'value'), 100);
  await activateCommand(page, 'undo');
  assert.ok(Math.abs((await result('handle-length', 'value')) - Math.hypot(6, 4)) < 1e-12);
  assert.ok(Math.abs((await result('target-fill', 'color')).alpha - Math.hypot(6, 4) / 10) < 1e-12);
  await activateCommand(page, 'redo');
  await activateCommand(page, 'redo');
  assert.equal(await result('handle-length', 'value'), 10);
  assert.equal(await result('vector-component', 'value'), 115);
  assert.equal((await result('target-fill', 'color')).alpha, 1);

  const beforeSave = await snapshot(),
    pending = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const path = resolve('test-results/cross-composition-vector-reference/project.json');
  await (await pending).saveAs(path);
  assert.deepEqual(JSON.parse(await readFile(path, 'utf8')), beforeSave);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page.locator('#open-file').setInputFiles(path);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(beforeSave),
  );
  assert.equal(await result('target-circle', 'radius'), 35);
  assert.deepEqual(await result('target-circle', 'center'), [35, 40]);
  assert.equal(await result('handle-length', 'value'), 10);
  assert.equal(await result('vector-component', 'value'), 115);
  assert.equal((await result('target-fill', 'color')).alpha, 1);
  await page.setViewportSize({ width: 1000, height: 650 });
  await selectNode('target-circle');
  await field('target-circle', 'radius').locator('output').scrollIntoViewIfNeeded();
  await page.waitForFunction(() =>
    [...document.querySelectorAll('.workspace-drawer:not([hidden])')].every(
      (drawer) => Number(getComputedStyle(drawer).opacity) === 1,
    ),
  );
  await page.screenshot({
    path: resolve('test-results/cross-composition-vector-reference/reopened.png'),
  });
  assert.deepEqual(errors, []);
  console.log('PASS cross-Composition local components, editing, history, SVG/PNG and JSON reopen');
} finally {
  await browser.close();
}
