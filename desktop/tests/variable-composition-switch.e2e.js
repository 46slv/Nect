import assert from 'node:assert/strict';
import fs from 'node:fs';
import { chromium } from 'playwright-core';
import { blankDocument, layer, node } from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

const bytes = fs.readFileSync(
  new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
);
const asset = makeVariableFontAsset(
  bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  'Roboto.ttf',
);
const first = blankDocument(),
  second = blankDocument(),
  firstText = node('Text'),
  firstColor = node('Color'),
  text = node('Text'),
  color = node('Color'),
  other = layer('Other layer');
firstText.position = [400, 120];
firstColor.position = [800, 120];
first.layers[0].nodes.push(firstText, firstColor);
Object.assign(text, {
  font: asset.id,
  text: 'AV',
  variation: { version: 1, fontId: asset.id, axes: { wght: 550 } },
  position: [300, 120],
});
color.position = [700, 120];
second.layers[0].nodes.push(text, color);
second.layers[0].nodes[0].params.path.source = text.id;
second.layers.push(other);
first.fontAssets = { [asset.id]: asset.asset };
first.schemaVersion = 54;
first.colorSlots = [{ id: 'brand', name: 'Brand', value: { hex: '#112233', alpha: 1 } }];
const project = migrateLegacyDocumentToCompositionProject(first);
project.compositions[0].id = 'variable-first';
const target = migrateLegacyDocumentToCompositionProject(second).compositions[0];
target.id = 'variable-second';
target.layers[0].nodes.find((entry) => entry.id === color.id).params.value.colorSlotId = 'brand';
project.compositions.push(target);
const json = serializeCompositionProject(project);
const base = process.env.BASE_URL || 'http://127.0.0.1:5173/';

// Ask Vite to discover and finish pre-bundling the worker-only fontkit import
// before opening the test page. Otherwise its first discovery can reload an
// already-running test. An outdated-dependency response asks for a fresh URL.
async function warmVariableWorkerDependency() {
  for (let attempt = 0; attempt < 3; attempt++) {
    const module = await fetch(new URL('/src/variable-font/backend.js', base));
    if (!module.ok) throw Error(`Variable worker module unavailable: ${module.status}`);
    const source = await module.text();
    const match = source.match(/\bfrom\s+["']([^"']*\/fontkit\.js(?:\?[^"']*)?)["']/);
    if (!match) throw Error('Vite did not expose the optimized variable worker dependency');
    const dependency = await fetch(new URL(match[1], base));
    if (dependency.status === 504 && dependency.statusText === 'Outdated Optimize Dep') continue;
    if (!dependency.ok) throw Error(`Variable worker dependency unavailable: ${dependency.status}`);
    await dependency.arrayBuffer();
    return;
  }
  throw Error('Vite kept replacing the optimized variable worker dependency');
}
await warmVariableWorkerDependency();
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
let mainNavigations = 0;
page.on('framenavigated', (frame) => {
  if (frame === page.mainFrame()) mainNavigations++;
});
page.on('pageerror', (error) => errors.push(error.message));
const currentView = () =>
  page.evaluate(() => ({
    selected: document.querySelector('.graph .node.selected')?.dataset.id ?? null,
    canvasZoom: document.querySelector('[data-zoom="canvas"]')?.textContent,
    graphZoom: document.querySelector('[data-zoom="graph"]')?.textContent,
    graphScroll: [
      document.querySelector('.graph-scroll').scrollLeft,
      document.querySelector('.graph-scroll').scrollTop,
    ],
  }));
const projectState = () =>
  page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    revision: window.nect.projectRevision,
    file: window.nect.fileState,
  }));
try {
  await page.goto(base);
  await page.waitForFunction(() => !!window.nect);
  await page.locator('#open-file').setInputFiles({
    name: 'two-compositions.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(json),
  });
  await page.waitForFunction(
    () =>
      window.nect.activeCompositionId === 'variable-first' && window.nect.compositions.length === 2,
  );
  const before = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    revision: window.nect.projectRevision,
    file: window.nect.fileState,
  }));
  assert.equal(before.file.dirty, false);
  await ensureWorkspacePanel(page, 'colors');
  const initialAdvanced = page.locator('[data-color-advanced]');
  if (!(await initialAdvanced.evaluate((element) => element.open)))
    await initialAdvanced.locator('summary').click();
  const colorName = page.locator('[data-shared-color-row="brand"] [data-shared-color-edit="name"]');
  await colorName.focus();
  await colorName.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await colorName.fill('編集中');
  await colorName.evaluate((element) => element.setSelectionRange(2, 2));
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'variable-first');
  assert.equal(await colorName.inputValue(), '編集中');
  assert.equal(
    await colorName.evaluate(
      (element) => document.activeElement === element && element.selectionStart === 2,
    ),
    true,
  );
  assert.deepEqual(
    await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      revision: window.nect.projectRevision,
      file: window.nect.fileState,
    })),
    before,
  );
  await colorName.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  await colorName.fill('Brand');
  await colorName.press('Tab');
  await page.locator(`.node[data-id="${firstColor.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const hslButton = page.locator('.inspector [data-hsl-picker-open]');
  await hslButton.click();
  const hslHex = page.locator('[data-hsl-hex]');
  const originalHex = await hslHex.inputValue();
  await hslHex.fill('#445566');
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'variable-first');
  assert.equal(await hslHex.inputValue(), '#445566');
  assert.equal(await hslHex.evaluate((element) => document.activeElement === element), true);
  await hslHex.fill(originalHex.toLowerCase());
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  assert.deepEqual(
    await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      revision: window.nect.projectRevision,
      file: window.nect.fileState,
    })),
    before,
  );
  await page.locator('#open-file').setInputFiles({
    name: 'two-compositions.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(json),
  });
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  await page.locator(`.node[data-id="${firstText.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const numeric = page.locator('.inspector [aria-label="Text size"]');
  const numericBefore = await numeric.inputValue();
  await numeric.fill('137');
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'variable-first');
  assert.equal(await numeric.inputValue(), '137');
  assert.equal(await numeric.evaluate((element) => document.activeElement === element), true);
  assert.deepEqual(
    await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      revision: window.nect.projectRevision,
      file: window.nect.fileState,
    })),
    before,
  );
  await numeric.fill(numericBefore);
  await activateBoundaryCommand(page, 'actual-size');
  await activateBoundaryCommand(page, 'zoom-in');
  await activateBoundaryCommand(page, 'graph-zoom-in');
  await page.locator('.graph-scroll').evaluate((element) => {
    element.scrollLeft = 120;
    element.scrollTop = 70;
  });
  const firstView = await currentView();
  assert.equal(firstView.selected, firstText.id);
  assert.ok(firstView.graphScroll[0] > 0 && firstView.graphScroll[1] > 0);
  const cleanSelect = page.locator(`.inspector [data-text-wrap="${firstText.id}"]`);
  await cleanSelect.focus();
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  const after = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    revision: window.nect.projectRevision,
    file: window.nect.fileState,
    svg: window.nect.export(),
  }));
  assert.deepEqual(after.project, before.project);
  assert.equal(after.revision, before.revision);
  assert.deepEqual(after.file, before.file);
  assert.match(after.svg, /<path/);
  await page.locator(`.node[data-id="${text.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const input = page.locator(`.inspector-body [data-text="${text.id}"]`);
  await input.fill('AW');
  await input.dispatchEvent('change');
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AW',
    text.id,
  );
  const edited = await page.evaluate(() => window.nect.export());
  assert.notEqual(edited, after.svg);
  await page.locator('[data-action="undo"]').click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AV',
    text.id,
  );
  assert.equal(await page.evaluate(() => window.nect.export()), after.svg);
  await page.locator('[data-action="redo"]').click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AW',
    text.id,
  );
  assert.equal(await page.evaluate(() => window.nect.export()), edited);
  await page.locator('[data-action="undo"]').click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AV',
    text.id,
  );
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await activateBoundaryCommand(page, 'actual-size');
  await activateBoundaryCommand(page, 'zoom-out');
  await activateBoundaryCommand(page, 'graph-zoom-out');
  await page.locator('.graph-scroll').evaluate((element) => {
    element.scrollLeft = 55;
    element.scrollTop = 140;
  });
  const secondView = await currentView(),
    beforeTabs = await projectState();
  assert.equal(secondView.selected, text.id);
  assert.notEqual(secondView.canvasZoom, firstView.canvasZoom);
  assert.notEqual(secondView.graphZoom, firstView.graphZoom);
  const cleanReadySelect = page.locator(`.inspector [data-text-path="${text.id}"]`);
  await cleanReadySelect.focus();
  await page.locator('[role="tab"][data-active-composition="variable-first"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  assert.deepEqual(await currentView(), firstView);
  assert.deepEqual(await projectState(), beforeTabs);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  assert.deepEqual(await currentView(), secondView);
  assert.deepEqual(await projectState(), beforeTabs);
  const secondTab = page.locator('[role="tab"][data-active-composition="variable-second"]');
  await secondTab.focus();
  await page.keyboard.press('ArrowLeft');
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'variable-second');
  assert.equal(
    await page.evaluate(() => document.activeElement?.dataset.activeComposition),
    'variable-first',
  );
  await page.keyboard.press('Enter');
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  assert.deepEqual(await currentView(), firstView);
  await page.locator('[role="tab"][data-active-composition="variable-first"]').focus();
  await page.keyboard.press('ArrowRight');
  await page.keyboard.press('Space');
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  assert.deepEqual(await currentView(), secondView);
  assert.deepEqual(await projectState(), beforeTabs);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-action="redo"]').click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AW',
    text.id,
  );
  assert.equal(await page.evaluate(() => window.nect.export()), edited);

  await page.locator('#open-file').setInputFiles({
    name: 'two-compositions.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(json),
  });
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  const beforeUse = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    revision: window.nect.projectRevision,
    file: window.nect.fileState,
  }));
  await ensureWorkspacePanel(page, 'colors');
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((element) => element.open)))
    await advanced.locator('summary').click();
  const slot = page.locator('[data-shared-color-row="brand"]');
  const toggle = slot.locator('[data-shared-color-copy-toggle]');
  if ((await toggle.getAttribute('aria-expanded')) !== 'true') await toggle.click();
  await slot.locator('[data-shared-color-uses]').click();
  const dialog = page.locator('.shared-color-uses-dialog[open]');
  await dialog.locator('[data-color-uses-list] button').click();
  await dialog.waitFor({ state: 'detached' });
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  const binding = page.locator(
    `.inspector .field[data-node="${color.id}"][data-key="value"] [data-shared-color-binding]`,
  );
  await binding.waitFor({ state: 'visible' });
  await page.waitForFunction(
    (id) => document.activeElement?.dataset.sharedColorNode === id,
    color.id,
  );
  assert.equal(await binding.inputValue(), 'brand');
  assert.deepEqual(
    await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      revision: window.nect.projectRevision,
      file: window.nect.fileState,
    })),
    beforeUse,
  );

  await page.locator('#open-file').setInputFiles({
    name: 'two-compositions.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(json),
  });
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  const beforeName = await page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    revision: window.nect.projectRevision,
    file: window.nect.fileState,
  }));
  await page.evaluate(() => {
    const NativeWorker = window.Worker;
    window.holdVariableWorker = true;
    window.Worker = class extends NativeWorker {
      postMessage(...args) {
        if (!window.holdVariableWorker) return super.postMessage(...args);
        window.holdVariableWorker = false;
        window.releaseVariableWorker = () => super.postMessage(...args);
      }
    };
  });
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => !!window.releaseVariableWorker);
  const title = page.locator(`.node[data-id="${first.layers[0].nodes[0].id}"] .node-head strong`);
  await title.focus();
  await page.keyboard.press('F2');
  const nameInput = page.locator('[data-rename-name]');
  await nameInput.fill('AVX');
  await nameInput.evaluate((element) => element.setSelectionRange(2, 2));
  await page.evaluate(() => window.releaseVariableWorker());
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('changed'),
  );
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'variable-first');
  assert.equal(await nameInput.inputValue(), 'AVX');
  assert.equal(
    await nameInput.evaluate(
      (element) => document.activeElement === element && element.selectionStart === 2,
    ),
    true,
  );
  assert.deepEqual(
    await page.evaluate(() => ({
      project: window.nect.projectSnapshot(),
      revision: window.nect.projectRevision,
      file: window.nect.fileState,
    })),
    beforeName,
  );
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'variable-first');
  assert.equal(await nameInput.inputValue(), 'AVX');
  assert.equal(
    await nameInput.evaluate(
      (element) => document.activeElement === element && element.selectionStart === 2,
    ),
    true,
  );
  await nameInput.press('Escape');
  // The variable preview is real Worker-backed pending work even though it has
  // not made the saved project dirty. A cancelled browser reload keeps it.
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  await page.locator(`.node[data-id="${text.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const typography = page.locator('.inspector-body [data-text-inspector-section="typography"]');
  if (!(await typography.evaluate((element) => element.open)))
    await typography.locator('summary').click();
  const axis = page.locator('.inspector-body [data-variable-axis="wght"]');
  await axis.fill('650');
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('axis preview'),
  );
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  let warning;
  page.once('dialog', async (dialog) => {
    warning = dialog.type();
    await dialog.dismiss();
  });
  let reloadError;
  try {
    await page.reload({ timeout: 3000 });
  } catch (error) {
    reloadError = error;
  }
  assert.equal(warning, 'beforeunload');
  if (reloadError) assert.match(reloadError.message, /Timeout/);
  assert.equal(await axis.inputValue(), '650');
  // Browser focusout may commit the value after the warning is dismissed.
  // Either it is still a clean preview or it is an authored, dirty edit.
  const afterWarning = await page.evaluate(
    (id) => ({
      weight: window.nect.document.layers[0].nodes.find((entry) => entry.id === id).variation.axes
        .wght,
      dirty: window.nect.fileState.dirty,
    }),
    text.id,
  );
  assert.ok([550, 650].includes(afterWarning.weight));
  assert.equal(afterWarning.dirty, afterWarning.weight === 650);
  // Hold a genuine Worker request while the user leaves the axis field and
  // clicks another Layer's Delete. The prepared edit must still be able to finish.
  await page.evaluate(() => {
    const post = window.Worker.prototype.postMessage;
    window.holdAxisMessages = true;
    window.heldAxisMessages = [];
    window.Worker.prototype.postMessage = function (...args) {
      if (window.holdAxisMessages) {
        window.heldAxisMessages.push(() => post.apply(this, args));
        return;
      }
      return post.apply(this, args);
    };
  });
  await axis.fill('700');
  await page.waitForFunction(() => window.heldAxisMessages.length > 0);
  const beforeDelete = await page.evaluate(() => window.nect.projectSnapshot());
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-delete-layer="${other.id}"]`).click();
  assert.match(
    await page.locator('.status .message').innerText(),
    /Finish or cancel variable font preparation/,
  );
  assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), beforeDelete);
  await page.evaluate(() => {
    window.holdAxisMessages = false;
    for (const send of window.heldAxisMessages) send();
  });
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((entry) => entry.id === id).variation.axes.wght ===
      700,
    text.id,
  );
  await page.locator('[data-action="undo"]').click();
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === JSON.stringify(expected),
    beforeDelete,
  );

  // A captured variable Text run is prepared together with its edited live run.
  await page.locator('#open-file').setInputFiles({
    name: 'two-compositions.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(json),
  });
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${second.layers[0].id}"]`).focus();
  await page.keyboard.press('Control+c');
  await page.locator(`.node[data-id="${text.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const variableText = page.locator(`.inspector-body [data-text="${text.id}"]`);
  await variableText.fill('AWZ');
  await variableText.dispatchEvent('change');
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AWZ',
    text.id,
  );
  const beforeSnapshotPaste = await page.evaluate(() =>
    JSON.stringify(window.nect.projectSnapshot()),
  );
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${second.layers[0].id}"]`).focus();
  await page.keyboard.press('Control+v');
  await page.waitForFunction(() => window.nect.document.layers.length === 3);
  const pastedVariable = await page.evaluate(() => window.nect.document.layers[1]);
  assert.equal(pastedVariable.nodes.find((entry) => entry.type === 'Text').text, 'AV');
  assert.notEqual(pastedVariable.id, second.layers[0].id);
  await page.waitForFunction(
    (id) => document.querySelector('.layer.selected')?.dataset.layer === id,
    pastedVariable.id,
  );
  const afterSnapshotPaste = await page.evaluate(() =>
    JSON.stringify(window.nect.projectSnapshot()),
  );
  const beforeDraftUndo = await page.evaluate(() => window.nect.projectRevision);
  await page.evaluate((id) => {
    document.querySelector('[data-action="undo"]').click();
    const row = document.querySelector(`[data-select-layer="${id}"]`);
    row.focus();
    row.dispatchEvent(new KeyboardEvent('keydown', { key: 'F2', bubbles: true }));
    const name = document.querySelector('[data-rename-name]');
    name.value = 'Uncommitted name';
    name.dispatchEvent(new Event('input', { bubbles: true }));
  }, pastedVariable.id);
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('operation cancelled'),
  );
  assert.equal(await page.locator('[data-rename-name]').inputValue(), 'Uncommitted name');
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    afterSnapshotPaste,
  );
  assert.equal(await page.evaluate(() => window.nect.projectRevision), beforeDraftUndo);
  await page.locator('[data-rename-name]').press('Escape');
  await page.locator('[data-action="undo"]').click();
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    beforeSnapshotPaste,
  );
  await page.locator('[data-action="redo"]').click();
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    afterSnapshotPaste,
  );
  await page.waitForFunction(
    (id) => document.querySelector('.layer.selected')?.dataset.layer === id,
    pastedVariable.id,
  );

  // A newer multi-selection must retain its surviving member after an async Undo.
  await page.evaluate((id) => {
    document.querySelector('[data-action="undo"]').click();
    document
      .querySelector(`[data-select-layer="${id}"]`)
      .dispatchEvent(new MouseEvent('click', { bubbles: true, shiftKey: true }));
  }, other.id);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    beforeSnapshotPaste,
  );
  assert.deepEqual(
    await page
      .locator('.layer.selected')
      .evaluateAll((rows) => rows.map((row) => row.dataset.layer)),
    [other.id],
  );
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('Delete');
  await page.waitForFunction(
    (id) => !window.nect.document.layers.some((layer) => layer.id === id),
    other.id,
  );

  // A pending real Worker-backed text edit must survive both Layer shortcuts.
  await page.locator('#open-file').setInputFiles({
    name: 'two-compositions.nect',
    mimeType: 'application/json',
    buffer: Buffer.from(json),
  });
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
  const folderId = await page.evaluate(
    () =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'create-layer-folder',
        payload: { name: 'Target', layerIds: [] },
      }).folderId,
  );
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${other.id}"]`).focus();
  await page.keyboard.press('Control+c');
  const beforeCancelledPaste = await page.evaluate(() =>
    JSON.stringify(window.nect.projectSnapshot()),
  );
  const disabledWhilePreparing = await page.evaluate(
    ({ layerId, folderId }) => {
      const row = document.querySelector(`[data-select-layer="${layerId}"]`);
      row.dispatchEvent(new KeyboardEvent('keydown', { key: 'v', ctrlKey: true, bubbles: true }));
      row.click();
      return document.querySelector(`[data-folder-add="${folderId}"]`).disabled;
    },
    { layerId: other.id, folderId },
  );
  assert.equal(disabledWhilePreparing, true);
  await page.waitForFunction(() =>
    document.querySelector('.status .message')?.textContent.includes('operation cancelled'),
  );
  assert.equal(await page.locator(`[data-folder-add="${folderId}"]`).isEnabled(), true);
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    beforeCancelledPaste,
  );
  await page.locator(`[data-select-layer="${second.layers[0].id}"]`).click();
  await page.locator(`[data-select-layer="${other.id}"]`).focus();
  await page.keyboard.press('Control+c');
  await page.locator(`.node[data-id="${text.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  await page.evaluate(() => {
    window.holdAxisMessages = true;
    window.heldAxisMessages = [];
  });
  const pendingText = page.locator(`.inspector-body [data-text="${text.id}"]`);
  await pendingText.fill('AWZ');
  await pendingText.dispatchEvent('change');
  await page.waitForFunction(() => window.heldAxisMessages.length > 0);
  const beforeShortcuts = await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${other.id}"]`).focus();
  for (const shortcut of ['Control+v', 'Control+d']) {
    await page.keyboard.press(shortcut);
    assert.match(
      await page.locator('.status .message').innerText(),
      /Finish or cancel variable font preparation/,
    );
    assert.equal(
      await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
      beforeShortcuts,
    );
    assert.equal(await pendingText.inputValue(), 'AWZ');
  }
  await page.evaluate(() => {
    window.holdAxisMessages = false;
    for (const send of window.heldAxisMessages) send();
  });
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id)?.text === 'AWZ',
    text.id,
  );
  assert.deepEqual(errors, []);
  assert.equal(mainNavigations, 1, 'Vite must not reload the active test page');
  console.log(
    'PASS: sandboxed Chrome variable Composition view state, keyboard, History/Redo, Color uses, inline name race, pending preview reload and Layer Delete guards, and SVG',
  );
} finally {
  await browser.close();
}
