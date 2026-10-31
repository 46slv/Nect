import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { angledGridFixture } from './fixtures/angled-grid.js';
const output = path.resolve('test-results/angled-grid/png');
await fs.mkdir(output, { recursive: true });
const config = JSON.parse(await fs.readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = await preview({
  preview: {
    host: '127.0.0.1',
    port: 0,
    headers: { 'Content-Security-Policy': config.app.security.csp },
  },
});
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
try {
  const page = await browser.newPage({ viewport: { width: 1469, height: 827 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.httpServer.address().port}`);
  await page.waitForFunction(() => window.nect);
  const f = angledGridFixture(45, 'center');
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  const svg = await page.evaluate(() => window.nect.export()),
    results = [];
  for (const angle of [0, 45]) {
    if (angle) {
      await page.keyboard.press('Escape');
      await activateBoundaryCommand(page, 'grid-panel');
      const field = page.locator('[data-guide-setting="gridAngle"]');
      await field.fill('45');
      await field.press('Tab');
      assert.equal(await page.evaluate(() => window.nect.document.editor.gridAngle), 45);
      assert.equal(await page.locator('[data-grid-overlay]').getAttribute('data-grid-angle'), '45');
      await page.keyboard.press('Escape');
      assert.equal(await page.evaluate(() => window.nect.export()), svg);
      assert.deepEqual(await page.evaluate(() => window.nect.document.layers), f.doc.layers);
    }
    const button = page.locator('[data-action="export-png"]');
    if (!(await button.isVisible()))
      await page
        .locator('.command-menu > summary')
        .filter({ hasText: /^Export$/ })
        .click();
    assert.equal(await page.locator('#png-scale').inputValue(), '1');
    const downloaded = page.waitForEvent('download');
    await button.click();
    const file = path.join(output, `grid-angle-${angle}.png`);
    await (await downloaded).saveAs(file);
    const png = await fs.readFile(file);
    const pixels = await page.evaluate(
      async ({ png, svg }) => {
        const read = async (url) => {
          const image = new Image();
          image.src = url;
          await image.decode();
          const c = document.createElement('canvas');
          c.width = image.width;
          c.height = image.height;
          const context = c.getContext('2d');
          context.drawImage(image, 0, 0);
          return {
            size: [c.width, c.height],
            RGBA: [
              ...new Uint8Array(
                await crypto.subtle.digest(
                  'SHA-256',
                  context.getImageData(0, 0, c.width, c.height).data,
                ),
              ),
            ]
              .map((v) => v.toString(16).padStart(2, '0'))
              .join(''),
          };
        };
        return {
          png: await read(`data:image/png;base64,${png}`),
          svg: await read(`data:image/svg+xml;base64,${svg}`),
        };
      },
      { png: png.toString('base64'), svg: Buffer.from(svg).toString('base64') },
    );
    assert.deepEqual(pixels.png, pixels.svg);
    assert.deepEqual(pixels.png.size, [600, 600]);
    results.push({ angle, pixels });
  }
  assert.deepEqual(results[0].pixels, results[1].pixels);
  assert.deepEqual(errors, []);
  await fs.writeFile(
    path.join(output, 'receipt.json'),
    JSON.stringify(
      {
        route: 'Existing PNG export/download UI in production-CSP headless Chrome',
        results,
        pageErrors: errors,
        notRun: ['native OS PNG save dialog', 'physical input', 'OS Save/Open/clean cold reopen'],
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify(results, null, 2));
} finally {
  await browser.close();
  await new Promise((r) => server.httpServer.close(r));
}
