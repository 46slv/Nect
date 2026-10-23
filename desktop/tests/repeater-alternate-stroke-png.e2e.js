import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { strokeFixture } from './fixtures/repeater-alternate-stroke.js';
const output = path.resolve('test-results/repeater-alternate-stroke/browser-png');
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
  await page.waitForTimeout(1000);
  const f = strokeFixture('radial');
  f.repeat.params.alternateStroke.value = true;
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  const svg = await page.evaluate(() => window.nect.export());
  await page
    .locator('.command-menu > summary')
    .filter({ hasText: /^Export$/ })
    .click();
  const event = page.waitForEvent('download');
  await page.locator('[data-action="export-png"]').click();
  const pngPath = path.join(output, 'radial-three-variations.png');
  await (await event).saveAs(pngPath);
  const png = await fs.readFile(pngPath);
  const comparison = await page.evaluate(
    async ({ svg, png }) => {
      const pixels = async (url) => {
        const image = new Image();
        image.src = url;
        await image.decode();
        const c = document.createElement('canvas');
        c.width = image.width;
        c.height = image.height;
        const ctx = c.getContext('2d');
        ctx.drawImage(image, 0, 0);
        return {
          size: [c.width, c.height],
          digest: [
            ...new Uint8Array(
              await crypto.subtle.digest('SHA-256', ctx.getImageData(0, 0, c.width, c.height).data),
            ),
          ]
            .map((v) => v.toString(16).padStart(2, '0'))
            .join(''),
        };
      };
      const shapes = [...document.querySelectorAll('.artboard .shape')]
        .map((el) => {
          const c = el.cloneNode(true);
          c.removeAttribute('class');
          return c.outerHTML;
        })
        .join('');
      const url = URL.createObjectURL(
        new Blob(
          [
            `<svg xmlns="http://www.w3.org/2000/svg" width="960" height="540" viewBox="0 0 960 540">${shapes}</svg>`,
          ],
          { type: 'image/svg+xml' },
        ),
      );
      try {
        return {
          svg: await pixels(`data:image/svg+xml;base64,${svg}`),
          png: await pixels(`data:image/png;base64,${png}`),
          canvas: await pixels(url),
        };
      } finally {
        URL.revokeObjectURL(url);
      }
    },
    { svg: Buffer.from(svg).toString('base64'), png: png.toString('base64') },
  );
  assert.deepEqual(comparison.png.size, [960, 540]);
  assert.deepEqual(comparison.svg, comparison.png);
  assert.deepEqual(comparison.canvas, comparison.svg);
  assert.deepEqual(errors, []);
  await fs.writeFile(path.join(output, 'radial-three-variations.svg'), svg);
  await fs.writeFile(path.join(output, 'radial-three-variations.json'), serialize(f.doc));
  const receipt = {
    input: 'existing PNG export/download UI in headless Chrome production-CSP',
    scale: 60,
    alternateFill: true,
    alternateStroke: true,
    copies: 24,
    comparison,
    pageErrors: errors,
    notRun: ['native OS PNG save dialog', 'physical input', 'OS cold reopen'],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser.close();
  await new Promise((resolve) => server.httpServer.close(resolve));
}
