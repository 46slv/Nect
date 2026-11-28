import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({
  viewport: { width: 1600, height: 1000 },
  acceptDownloads: true,
});
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
const click = (action) => activateBoundaryCommand(page, action);
const choose = (choice) => page.locator(`dialog button[value="${choice}"]`).click();
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
const project = () => page.evaluate(() => window.nect.projectSnapshot());
const create = async (width, height) => {
  await click('new');
  const dialog = page.locator('.new-project-dialog[open]');
  await dialog.locator('[name="width"]').fill(String(width));
  await dialog.locator('[name="height"]').fill(String(height));
  await dialog.locator('button[type="submit"]').click();
};
try {
  await fs.mkdir('test-results', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForSelector('[data-no-document]');
  assert.equal(await project(), null);
  const unexpectedSaves = [];
  const unexpectedDownload = (download) => unexpectedSaves.push(download.suggestedFilename());
  page.on('download', unexpectedDownload);
  for (const action of ['save', 'save-as', 'undo', 'redo', 'export', 'export-png'])
    assert.equal(await page.locator(`[data-action="${action}"]`).isDisabled(), true);
  for (const shortcut of ['Control+s', 'Control+Shift+s', 'Control+z', 'Control+Shift+z', 'r'])
    await page.keyboard.press(shortcut);
  const invalid = page.waitForEvent('filechooser');
  await click('open');
  await (
    await invalid
  ).setFiles({ name: 'bad.json', mimeType: 'application/json', buffer: Buffer.from('{') });
  await page.waitForFunction(() =>
    document.querySelector('.message')?.textContent.includes('Open rejected'),
  );
  assert.equal(await project(), null);
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  assert.deepEqual(unexpectedSaves, []);
  page.off('download', unexpectedDownload);

  await page.evaluate(async () => {
    const { document, serialize } = await import('/src/core.js');
    window.nect.load(serialize(document()));
  });
  await page.waitForSelector('.shape');
  const demo = await snapshot();
  await create(625.5, 400.25);
  await choose('cancel');
  assert.equal(await snapshot(), demo);
  await create(625.5, 400.25);
  await choose('discard');
  await page.waitForFunction(() => window.nect.document?.width === 625.5);
  await page.locator('[data-tool="Rectangle"]').click();
  const saved = await snapshot();
  assert.equal(JSON.parse(saved).layers.length, 2);
  const downloading = page.waitForEvent('download');
  await click('save');
  await (await downloading).saveAs('test-results/lifecycle-project.json');
  const persisted = JSON.parse(await fs.readFile('test-results/lifecycle-project.json', 'utf8'));
  assert.deepEqual(persisted, await project());
  assert.deepEqual(
    persisted.compositions.map(({ width, height }) => [width, height]),
    [[625.5, 400.25]],
  );
  await page.waitForFunction(() => !window.nect.fileState.dirty);
  await page.locator('[data-tool="Circle"]').click();
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), true);
  await click('undo');
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  await click('redo');
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), true);
  await click('open');
  await choose('cancel');
  assert.equal(JSON.parse(await snapshot()).layers.length, 3);
  await click('open');
  await choose('discard');
  await page.locator('#open-file').setInputFiles('test-results/lifecycle-project.json');
  await page.waitForFunction(() => window.nect.document.layers.length === 2);
  assert.equal(await snapshot(), saved);
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), false);
  await page.locator('[data-tool="Circle"]').click();
  const exportDownload = page.waitForEvent('download');
  await click('export');
  await (await exportDownload).saveAs('test-results/lifecycle-reedited.svg');
  assert.match(await fs.readFile('test-results/lifecycle-reedited.svg', 'utf8'), /<svg/);
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), true);
  await page.screenshot({ path: 'test-results/lifecycle-browser.png' });
  assert.deepEqual(errors, []);
  console.log(
    'PASS: browser UI draw/save/reopen/re-edit/export; Cancel, Undo saved-state, history boundary. Native dialogs/Close are separate acceptance.',
  );
} finally {
  await browser.close();
}
