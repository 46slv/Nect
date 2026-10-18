import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { blankDocument, serialize } from '../src/core.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 900, height: 620 } });
const result = { steps: [], errors: [] };
page.on('pageerror', (error) => result.errors.push(error.message));
const record = (name, pass, detail = '') => result.steps.push({ name, pass, detail });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:4173');
  await page.waitForSelector('.node');
  const source = blankDocument();
  const sourceText = serialize(source);
  const initial = await page.evaluate(() => window.nect.document);
  await page.getByText('Import', { exact: true }).click();
  await page.getByText('Project composition', { exact: true }).click();
  await page.locator('body > input[type="file"]').last().setInputFiles({
    name: 'source.json',
    mimeType: 'application/json',
    buffer: Buffer.from(sourceText),
  });
  await page.waitForSelector('dialog[open] [data-composition-choice]');
  record('file chooser and composition list', true);
  await page.locator('dialog[open] button[value="import"]').click();
  await page.waitForTimeout(300);
  result.afterImport = await page.evaluate(() => ({
    layers: window.nect.document.layers.length,
    dialogs: document.querySelectorAll('dialog[open]').length,
    status: document.querySelector('.status')?.textContent,
  }));
  await page.waitForFunction(() => window.nect.document.layers.length === 2);
  const first = await page.evaluate(() => window.nect.document);
  record('import', first.layers.length === 2);
  await page.getByText('Import', { exact: true }).click();
  await page.getByText('Project composition', { exact: true }).click();
  await page.locator('body > input[type="file"]').last().setInputFiles({
    name: 'source.json',
    mimeType: 'application/json',
    buffer: Buffer.from(sourceText),
  });
  await page.locator('dialog[open] button[value="import"]').click();
  await page.waitForTimeout(300);
  result.afterSecondImport = await page.evaluate(() => ({
    layers: window.nect.document.layers.length,
    ids: window.nect.document.layers.map((layer) => layer.id),
    status: document.querySelector('.status')?.textContent,
  }));
  await page.waitForFunction(() => window.nect.document.layers.length === 3);
  const second = await page.evaluate(() => window.nect.document);
  const ids = second.layers.map((layer) => layer.id);
  record('repeat import remaps IDs', new Set(ids).size === ids.length);
  await page.locator('[data-action="undo"]').click();
  record('undo', (await page.evaluate(() => window.nect.document.layers.length)) === 2);
  await page.locator('[data-action="redo"]').click();
  record('redo', (await page.evaluate(() => window.nect.document.layers.length)) === 3);
  const saved = await page.evaluate(() => window.nect.export ? null : JSON.stringify(window.nect.document));
  await page.evaluate((text) => window.nect.load(text), saved || JSON.stringify(second));
  record('save/reload model roundtrip', (await page.evaluate(() => window.nect.document.layers.length)) === 3);
  await page.getByText('Import', { exact: true }).click();
  await page.getByText('Project composition', { exact: true }).click();
  await page.locator('body > input[type="file"]').last().setInputFiles({
    name: 'invalid.json',
    mimeType: 'application/json',
    buffer: Buffer.from('{}'),
  });
  await page.waitForFunction(() => document.querySelector('.status')?.textContent.includes('rejected'));
  record('invalid JSON atomic rejection', JSON.stringify(await page.evaluate(() => window.nect.document)) === JSON.stringify(await page.evaluate((x) => x, second)));
  assert.equal(result.errors.length, 0, result.errors.join('; '));
  assert.ok(result.steps.every((step) => step.pass), JSON.stringify(result.steps));
} catch (error) {
  result.failure = error.stack || String(error);
  throw error;
} finally {
  await fs.mkdir('test-results', { recursive: true });
  await fs.writeFile('test-results/project-composition-import-ui.json', JSON.stringify(result, null, 2));
  await browser.close();
}
