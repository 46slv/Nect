import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const metrics = await page.evaluate(async () => {
    const sample = '日本語の長いレイヤー名 0123456789';
    const light = await document.fonts.load('300 14px "IBM Plex Sans JP"', sample);
    const regular = await document.fonts.load('400 12px "IBM Plex Sans JP"', sample);
    await document.fonts.ready;
    const context = document.createElement('canvas').getContext('2d');
    context.font = '300 14px "IBM Plex Sans JP"';
    const actual = context.measureText(sample);
    context.font = '300 14px monospace';
    const fallback = context.measureText(sample);
    return {
      light: light.map((face) => face.status), regular: regular.map((face) => face.status),
      width: actual.width, ink: actual.actualBoundingBoxAscent + actual.actualBoundingBoxDescent,
      fallbackWidth: fallback.width,
      uiFamily: getComputedStyle(document.documentElement).fontFamily,
      uiWeight: getComputedStyle(document.documentElement).fontWeight,
    };
  });
  assert.ok(metrics.light.length && metrics.light.every((status) => status === 'loaded'));
  assert.ok(metrics.regular.length && metrics.regular.every((status) => status === 'loaded'));
  assert.ok(metrics.width > 0 && metrics.ink > 0);
  assert.notEqual(metrics.width, metrics.fallbackWidth, 'mixed sample metrics differ from monospace fallback');
  assert.match(metrics.uiFamily, /IBM Plex Sans JP/);
  assert.equal(metrics.uiWeight, '300');
  await page.evaluate(async () => {
    const { blankDocument } = await import('/src/core.js');
    const doc = blankDocument();
    doc.layers[0].name = '日本語の長いレイヤー名・数字0123456789・確認用';
    window.nect.load(JSON.stringify(doc));
  });
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  const name = page.locator('.layers .layer .name').first();
  assert.match(await name.innerText(), /日本語の長いレイヤー名/);
  assert.match(await name.getAttribute('title'), /数字0123456789/);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-ui-font-japanese-1440.png' });
  await page.setViewportSize({ width: 1000, height: 900 });
  await page.screenshot({ path: 'test-results/workflow-ui-font-japanese-1000.png' });
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  console.log('UI font PASS: offline Light/Regular browser load, mixed-sample metrics and long-label capture');
} finally { await browser.close(); }
