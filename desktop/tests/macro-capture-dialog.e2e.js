import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 420, height: 800 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.locator('[data-tool="Circle"]').click();
  const before = await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  await page.evaluate(async () => {
    const css = document.createElement('link');
    css.rel = 'stylesheet';
    css.href = '/src/macro-capture-dialog.css';
    document.head.append(css);
    const { openMacroCaptureDialog } = await import('/src/macro-capture-dialog.js');
    const anchor = document.querySelector('[data-tool="Circle"]');
    anchor.focus();
    window.captureDialog = openMacroCaptureDialog({
      document,
      anchor,
      candidates: [
        { id: 'first-scale', nodeId: 'first', key: 'scale', label: 'Transform 1 · Scale' },
        { id: 'first-rotation', nodeId: 'first', key: 'rotation', label: 'Transform 1 · Rotation' },
        { id: 'second-scale', nodeId: 'second', key: 'scale', label: 'Transform 2 · Scale' },
        {
          id: 'second-rotation',
          nodeId: 'second',
          key: 'rotation',
          label: 'Transform 2 · Rotation',
        },
      ],
    });
    window.captureResult = undefined;
    window.captureDialog.result.then((value) => (window.captureResult = value));
  });
  const dialog = page.getByRole('dialog', { name: 'Create Macro' }),
    name = dialog.getByLabel('Name', { exact: true }),
    create = dialog.getByRole('button', { name: 'Create', exact: true });
  assert.equal(await create.isDisabled(), true);
  await name.focus();
  await page.keyboard.press('Control+z');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())), before);
  await name.fill('Poster effect');
  await dialog.getByRole('checkbox', { name: 'Publish Transform 1 · Scale' }).check();
  await dialog.getByRole('checkbox', { name: 'Publish Transform 2 · Rotation' }).check();
  await dialog.getByLabel('Name for Transform 1 · Scale').fill('Size');
  assert.equal(await create.isEnabled(), true);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/macro-capture-dialog.png', animations: 'disabled' });
  const metrics = await dialog.evaluate((el) => ({
    width: el.getBoundingClientRect().width,
    client: el.clientWidth,
    scroll: el.scrollWidth,
  }));
  assert.ok(metrics.width <= 420 && metrics.scroll <= metrics.client);
  await page.setViewportSize({ width: 320, height: 600 });
  await dialog.getByLabel('Name for Transform 1 · Scale').fill('A'.repeat(100));
  const narrow = await dialog.evaluate((el) => ({
    width: el.getBoundingClientRect().width,
    client: el.clientWidth,
    scroll: el.scrollWidth,
  }));
  assert.ok(narrow.width <= 320 && narrow.scroll <= narrow.client);
  await dialog.getByLabel('Name for Transform 1 · Scale').fill('Size');
  await page.setViewportSize({ width: 420, height: 800 });
  const cdp = await page.context().newCDPSession(page);
  await name.focus();
  await cdp.send('Input.imeSetComposition', { text: '色', selectionStart: 0, selectionEnd: 1 });
  assert.equal(await create.isDisabled(), true);
  await page.keyboard.press('Enter');
  assert.equal(await dialog.isVisible(), true);
  await cdp.send('Input.insertText', { text: '色' });
  assert.equal(await create.isEnabled(), true);
  const actualName = await name.inputValue();
  await name.press('Enter');
  await page.waitForFunction(() => window.captureResult !== undefined);
  assert.deepEqual(await page.evaluate(() => window.captureResult), {
    name: actualName,
    parameters: [
      { id: 'first-scale', nodeId: 'first', key: 'scale', label: 'Size' },
      { id: 'second-rotation', nodeId: 'second', key: 'rotation', label: 'Transform 2 · Rotation' },
    ],
  });
  assert.equal(
    await page.evaluate(() => document.activeElement?.matches('[data-tool="Circle"]')),
    true,
  );
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())), before);
  assert.deepEqual(errors, []);
  console.log(
    'PASS Macro capture draft keyboard, IME, selection, narrow layout and no Project edit',
  );
} finally {
  await browser.close();
}
