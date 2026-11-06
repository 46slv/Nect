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
  text = node('Text');
Object.assign(text, {
  font: asset.id,
  text: 'AV',
  variation: { version: 1, fontId: asset.id, axes: { wght: 550 } },
});
second.layers[0].nodes.push(text);
second.layers[0].nodes[0].params.path.source = text.id;
first.fontAssets = { [asset.id]: asset.asset };
const project = migrateLegacyDocumentToCompositionProject(first);
project.compositions[0].id = 'variable-first';
const target = migrateLegacyDocumentToCompositionProject(second).compositions[0];
target.id = 'variable-second';
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
  assert.deepEqual(errors, []);
  console.log(
    'PASS: sandboxed Chrome Open, variable Composition tabs, SVG, text edit and Undo/Redo',
  );
} finally {
  await browser.close();
}
