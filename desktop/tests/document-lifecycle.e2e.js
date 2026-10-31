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
try {
  await fs.mkdir('test-results', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForSelector('.shape');
  const demo = await snapshot();
  await click('new');
  await choose('cancel');
  assert.equal(await snapshot(), demo);
  await click('new');
  await choose('discard');
  await page.locator('[data-tool="Rectangle"]').click();
  const saved = await snapshot();
  assert.equal(JSON.parse(saved).layers.length, 2);
  const downloading = page.waitForEvent('download');
  await click('save');
  await (await downloading).saveAs('test-results/lifecycle-project.json');
  assert.deepEqual(
    JSON.parse(await fs.readFile('test-results/lifecycle-project.json', 'utf8')),
    JSON.parse(saved),
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
