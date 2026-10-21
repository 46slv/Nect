// Nect's production renderer and existing PNG export; the comparison only tiles PNGs.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { preview } from 'vite';
import { chromium } from 'playwright-core';

const output = process.argv[2];
assert.ok(output && path.isAbsolute(output), 'Pass the new model output directory');
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
const results = [],
  errors = [];
try {
  const page = await browser.newPage({ viewport: { width: 1469, height: 827 } });
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.httpServer.address().port}`);
  await page.waitForFunction(() => window.nect);
  const exports = [];
  for (const name of ['baseline', 'variant']) {
    const prefix = `fireworks-ring-${name}`,
      input = await fs.readFile(path.join(output, `${prefix}.json`), 'utf8');
    await page.evaluate((s) => window.nect.load(s), input);
    const loaded = await page.evaluate(() => ({
      doc: window.nect.document,
      svg: window.nect.export(),
    }));
    assert.deepEqual(loaded.doc, JSON.parse(input));
    assert.equal(loaded.svg, await fs.readFile(path.join(output, `${prefix}.svg`), 'utf8'));
    const button = page.locator('[data-action="export-png"]');
    if (!(await button.isVisible()))
      await page
        .locator('.command-menu > summary')
        .filter({ hasText: /^Export$/ })
        .click();
    assert.equal(await page.locator('#png-scale').inputValue(), '1');
    const pngPath = path.join(output, `${prefix}.png`);
    await assert.rejects(fs.access(pngPath));
    const download = page.waitForEvent('download');
    await button.click();
    await (await download).saveAs(pngPath);
    const png = await fs.readFile(pngPath);
    const comparison = await page.evaluate(
      async ({ svg, png }) => {
        const pixels = async (url) => {
          const img = new Image();
          img.src = url;
          await img.decode();
          const c = document.createElement('canvas');
          c.width = img.width;
          c.height = img.height;
          const ctx = c.getContext('2d');
          ctx.drawImage(img, 0, 0);
          const data = ctx.getImageData(0, 0, c.width, c.height).data;
          const digest = [...new Uint8Array(await crypto.subtle.digest('SHA-256', data))]
            .map((v) => v.toString(16).padStart(2, '0'))
            .join('');
          return { size: [c.width, c.height], rgbaSHA256: digest };
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
              `<svg xmlns="http://www.w3.org/2000/svg" width="600" height="600" viewBox="0 0 600 600">${shapes}</svg>`,
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
      { svg: Buffer.from(loaded.svg).toString('base64'), png: png.toString('base64') },
    );
    assert.deepEqual(comparison.png.size, [600, 600]);
    assert.deepEqual(comparison.png, comparison.svg);
    assert.deepEqual(comparison.canvas, comparison.svg);
    results.push({ name, exactJSONReload: true, exactSVGExport: true, comparison });
    exports.push(png.toString('base64'));
    if (name === 'variant') {
      await page.keyboard.press('Escape');
      await page.locator('[data-action="fit"]').click();
      await page.screenshot({ path: path.join(output, 'fireworks-ring-editor.png') });
    }
  }
  const composite = await page.evaluate(async (images) => {
    const c = document.createElement('canvas');
    c.width = 1280;
    c.height = 680;
    const ctx = c.getContext('2d');
    ctx.fillStyle = '#FFFFFF';
    ctx.fillRect(0, 0, c.width, c.height);
    ctx.fillStyle = '#242029';
    ctx.font = '20px sans-serif';
    ctx.fillText('Baseline', 28, 32);
    ctx.fillText('Alternate Scale / Fill / Stroke', 652, 32);
    const panels = [];
    for (let i = 0; i < 2; i++) {
      const img = new Image();
      img.src = `data:image/png;base64,${images[i]}`;
      await img.decode();
      const x = 28 + i * 624,
        y = 56;
      ctx.drawImage(img, x, y);
      const expected = document.createElement('canvas');
      expected.width = expected.height = 600;
      const ex = expected.getContext('2d');
      ex.fillStyle = '#FFFFFF';
      ex.fillRect(0, 0, 600, 600);
      ex.drawImage(img, 0, 0);
      const digest = async (data) =>
        [...new Uint8Array(await crypto.subtle.digest('SHA-256', data))]
          .map((v) => v.toString(16).padStart(2, '0'))
          .join('');
      panels.push({
        position: [x, y],
        size: [600, 600],
        comparisonPanelRGBA: await digest(ctx.getImageData(x, y, 600, 600).data),
        originalPNGOnWhiteRGBA: await digest(ex.getImageData(0, 0, 600, 600).data),
      });
    }
    return {
      size: [c.width, c.height],
      background: '#FFFFFF',
      panels,
      png: c.toDataURL('image/png').split(',')[1],
    };
  }, exports);
  for (const panel of composite.panels)
    assert.equal(panel.comparisonPanelRGBA, panel.originalPNGOnWhiteRGBA);
  const comparisonPath = path.join(output, 'fireworks-ring-comparison.png');
  await fs.writeFile(comparisonPath, Buffer.from(composite.png, 'base64'), { flag: 'wx' });
  delete composite.png;
  assert.deepEqual(errors, []);
  const receipt = {
    route:
      'Existing Nect PNG export/download UI in headless Chrome, current production bundle and native CSP',
    results,
    composite,
    pageErrors: errors,
    notRun: ['OS PNG save dialog', 'physical input', 'native Save/Open/clean cold reopen'],
    composition:
      'Two unmodified 600x600 Nect PNG exports on identical white panels; only outside-panel labels/background added',
  };
  await fs.writeFile(path.join(output, 'render-receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser.close();
  await new Promise((r) => server.httpServer.close(r));
}
