import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { loadDemoFixture } from './helpers/browser-fixture-setup.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
// Real browser controls with deterministic byte-only discovery. Normal selection
// must not copy provider font files; licensed fixture import is explicit Advanced UI.
// Actual Windows discovery/preparation and packaged WebView2 remain separate checks.
await page.route('**/src/storage.js', (route) =>
  route.fulfill({
    contentType: 'text/javascript',
    body: `export * from '/src/storage.js?system-font-original';
export const desktop = true;
export const beginRecovery = async () => 'system-font-test';
export const writeRecovery = async () => Date.now(), listRecovery = async () => [];
export const readRecovery = async () => '', acknowledgeRecovery = async () => {};
export const listSystemFonts = async () => [
  ['system-0', 'Japanese fixture', null, {localizedNames:['日本語テスト', 'ＭＳ テスト']}], ['system-1', 'DejaVu Sans', null],
  ['system-2', 'Variable font', 'Variable fonts unsupported']
];
export async function readSystemFont(id) {
  window.installedFontReads = (window.installedFontReads || 0) + 1;
  window.installedFontReadId = id;
  if (id !== 'system-0') throw Error('Wrong installed face');
  return (await fetch('/tests/fixtures/fonts/NectJPFixture-Regular.otf')).arrayBuffer();
}`,
  }),
);
const picker = () => page.locator('.inspector-body [data-change-font]');
const state = () =>
  page.evaluate(() => ({
    document: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
    path: JSON.stringify(window.nect.evaluate().path),
    canvas: document.querySelector(
      `.artboard .shape[data-layer="${window.nect.document.layers.at(-1).id}"]`,
    )?.outerHTML,
    font: window.nect.document.layers.at(-1).nodes.find((n) => n.type === 'Text')?.font,
  }));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await loadDemoFixture(page);
  await page.waitForSelector('.shape');
  await page.locator('[data-tool="Text"]').click();
  await page.locator('.artboard').click();
  await page
    .locator('.graph .node')
    .filter({ has: page.locator('[data-text]') })
    .locator('.node-head strong')
    .click();
  await ensureWorkspacePanel(page, 'inspector');
  const text = page.locator('.inspector-body [data-text]');
  await text.fill('Abg 123');
  await text.dispatchEvent('change');
  assert.equal(await picker().count(), 1);
  assert.equal(await page.locator('[data-system-font]').count(), 0);
  const beforeSearch = (await state()).document;
  await picker().click();
  await page.locator('.search-panel input[type="search"]').fill('日本語テスト');
  await page.waitForFunction(
    () => document.querySelectorAll('.search-panel .picker-option').length === 1,
  );
  const result = page.locator('.search-panel .picker-option');
  assert.match(await result.innerText(), /Japanese fixture/);
  assert.equal(await result.getAttribute('aria-disabled'), 'false');
  assert.match(await result.innerText(), /Included in saved document/);
  // A real pointer selects the row without reading or mutating the document.
  const disabledBounds = await result.boundingBox();
  assert.ok(disabledBounds);
  await page.mouse.click(
    disabledBounds.x + disabledBounds.width / 2,
    disabledBounds.y + disabledBounds.height / 2,
  );
  assert.equal((await state()).document, beforeSearch);
  await page.locator('.search-panel input[type="search"]').fill('MS テスト');
  await page.waitForFunction(
    () => document.querySelectorAll('.search-panel .picker-option').length === 1,
  );
  assert.equal(
    await page.locator('.search-panel .picker-option').getAttribute('aria-disabled'),
    'false',
  );
  await page.locator('.search-panel .picker-cancel').click();
  assert.equal((await state()).document, beforeSearch);
  assert.equal(await page.evaluate(() => window.installedFontReads || 0), 0);

  const importFont = async (file) => {
    await page.locator('.inspector-body .text-font-advanced').evaluate((el) => {
      el.open = true;
    });
    const chooser = page.waitForEvent('filechooser');
    await page.locator('.inspector-body [data-load-font]').click();
    await (await chooser).setFiles(file);
  };
  await picker().click();
  await page.locator('.search-panel input[type="search"]').fill('Japanese fixture');
  await page.locator('.search-panel .picker-option').click();
  assert.equal(await page.evaluate(() => window.installedFontReads || 0), 0);
  await page.locator('.search-panel .picker-apply').click();
  await page.waitForFunction(() => !document.querySelector('dialog[aria-label="Font"]'));
  assert.equal(await page.evaluate(() => window.installedFontReadId), 'system-0');
  const first = await state();
  assert.match(await page.locator('.inspector-body .text-font-choice').innerText(), /Nect/);
  await importFont(fileURLToPath(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)));
  await page.waitForFunction(() =>
    document.querySelector('.message').textContent.includes('Loaded DejaVu'),
  );
  const second = await state();
  assert.match(await page.locator('.inspector-body .text-font-choice').innerText(), /DejaVu/);
  assert.notEqual(second.path, first.path);
  assert.notEqual(second.svg, first.svg);
  assert.notEqual(second.canvas, first.canvas);
  await page.locator('[data-action="undo"]').click();
  assert.equal((await state()).svg, first.svg);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await state()).svg, second.svg);
  await page.evaluate((json) => window.nect.load(json), second.document);
  assert.equal((await state()).svg, second.svg);
  // Reload opens the preserved first layer; explicitly select the Text layer/node.
  const textLayer = await page.evaluate(() => window.nect.document.layers.at(-1).id);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${textLayer}"]`).click();
  await ensureWorkspacePanel(page, 'inspector');
  await page
    .locator('.node-head strong')
    .filter({ hasText: /^Text$/ })
    .click();
  assert.match(await page.locator('.inspector-body .text-font-choice').innerText(), /DejaVu/);
  await importFont({ name: 'broken.otf', mimeType: 'font/otf', buffer: Buffer.alloc(12) });
  await page.waitForFunction(() =>
    document.querySelector('.message').textContent.includes('Font rejected'),
  );
  assert.equal((await state()).document, second.document);
  assert.equal(await picker().isDisabled(), false);
  assert.equal(await page.evaluate(() => window.installedFontReads || 0), 1);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/font-single-picker.png' });
  assert.deepEqual(errors, []);
  console.log(
    'PASS: installed discovery loads the exact face only on ordinary Apply; imports change canvas/SVG with Undo, redo, JSON reload and failure preservation.',
  );
} finally {
  await browser.close();
}
