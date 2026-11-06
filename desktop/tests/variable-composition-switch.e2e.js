import assert from 'node:assert/strict';
import fs from 'node:fs';
import { chromium } from 'playwright-core';
import { blankDocument, node } from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
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
  text = node('Text'),
  color = node('Color');
Object.assign(text, {
  font: asset.id,
  text: 'AV',
  variation: { version: 1, fontId: asset.id, axes: { wght: 550 } },
  position: [300, 120],
});
color.position = [700, 120];
second.layers[0].nodes.push(text, color);
second.layers[0].nodes[0].params.path.source = text.id;
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
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5179/');
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
  await page.locator('[role="tab"][data-active-composition="variable-first"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-first');
  await page.locator('[role="tab"][data-active-composition="variable-second"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'variable-second');
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
  assert.deepEqual(errors, []);
  console.log(
    'PASS: sandboxed Chrome Open, variable Composition tabs, Color uses, inline name race, SVG and Undo/Redo',
  );
} finally {
  await browser.close();
}
