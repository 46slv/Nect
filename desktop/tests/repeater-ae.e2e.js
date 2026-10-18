import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { document, node, connect } from '../src/core.js';

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
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
page.on('dialog', (dialog) => dialog.accept());
const root = 'test-results/repeater-ae';
await fs.mkdir(root, { recursive: true });
try {
  const d = document(),
    l = d.layers[0];
  d.width = 160;
  d.height = 100;
  const shape = node('Rectangle', 40, 40),
    repeat = node('Geometry Repeater', 340, 40),
    fill = node('Fill', 640, 40),
    stroke = node('Stroke', 940, 40),
    out = node('Output', 1240, 40);
  shape.params.center.value = [40, 40];
  shape.params.size.value = [40, 40];
  repeat.params.count.value = 3;
  repeat.params.position.value = [10, 0];
  fill.params.color.value = { hex: '#ff0000', alpha: 0.6 };
  stroke.params.color.value = { hex: '#0000ff', alpha: 0.8 };
  stroke.params.width.value = 6;
  l.nodes = [shape, repeat, fill, stroke, out];
  l.output = out.id;
  for (const [a, b] of [
    [shape, repeat],
    [repeat, fill],
    [fill, stroke],
    [stroke, out],
  ])
    connect(l, a.id, b.id, 'path');
  await page.goto(`http://127.0.0.1:${server.httpServer.address().port}`);
  await page.waitForFunction(() => Boolean(window.nect));
  await page.evaluate((json) => window.nect.load(json), JSON.stringify(d));
  await page.locator(`.graph .node[data-id="${repeat.id}"] .node-head strong`).click();
  const operation = page.locator(`.inspector-body [data-operation="${repeat.id}"]`);
  await operation.selectOption('ae-2d');
  assert.equal(await page.evaluate(() => window.nect.document.schemaVersion), 37);
  const field = (key) =>
    page.locator(`.inspector-body .field[data-node="${repeat.id}"][data-key="${key}"]`);
  assert.equal(await field('step').count(), 0);
  assert.equal(await field('anchor').count(), 1);
  assert.equal(await field('position').count(), 1);
  assert.equal(await field('scale').count(), 1);
  const edit = async (key, value) => {
    const input = field(key).locator('[data-value]').first();
    await input.fill(value);
    await input.press('Tab');
    // Numeric change commits synchronously, then replaces controls on the next
    // frame. Let that replacement finish before filling the following control.
    await page.evaluate(
      () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
    );
  };
  await edit('offset', '-1');
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.offset.value,
      repeat.id,
    ),
    -1,
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await field('offset').locator('[data-value]').inputValue(), '0');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await field('offset').locator('[data-value]').inputValue(), '-1');
  await edit('offset', '0');
  await edit('count', '2.5');
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.count.value,
      repeat.id,
    ),
    2.5,
  );
  await edit('offset', '0.5');
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.offset.value,
      repeat.id,
    ),
    0.5,
  );
  await edit('startOpacity', '20');
  await edit('endOpacity', '80');
  const noteText = await page.locator('.inspector-body').textContent();
  assert.ok(noteText.includes('Fractional Copies materialize'), noteText);
  assert.ok(noteText.includes('fractional Offset with negative Scale is rejected'), noteText);
  assert.ok(await page.locator('.inspector-body .repeater-limits').isVisible());
  const download = async (action, extension) => {
    const event = page.waitForEvent('download');
    await page.locator(`[data-action="${action}"]`).click();
    const file = `${root}/copies-above.${extension}`;
    await (await event).saveAs(file);
    return fs.readFile(file);
  };
  const json = await download('save', 'json'),
    svg = await download('export', 'svg'),
    png = await download('export-png', 'png');
  const saved = await page.evaluate(() => JSON.stringify(window.nect.document));
  const pixels = async (bytes, mime) =>
    page.evaluate(
      async ({ data, mime }) => {
        const image = new Image();
        image.src = `data:${mime};base64,${data}`;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = image.width;
        canvas.height = image.height;
        const context = canvas.getContext('2d');
        context.drawImage(image, 0, 0);
        const rgba = context.getImageData(0, 0, canvas.width, canvas.height).data;
        const digest = [...new Uint8Array(await crypto.subtle.digest('SHA-256', rgba))]
          .map((v) => v.toString(16).padStart(2, '0'))
          .join('');
        return {
          digest,
          size: [canvas.width, canvas.height],
          overlap: [...context.getImageData(31, 40, 1, 1).data],
        };
      },
      { data: bytes.toString('base64'), mime },
    );
  const svgPixels = await pixels(svg, 'image/svg+xml'),
    pngPixels = await pixels(png, 'image/png');
  assert.deepEqual(svgPixels, pngPixels);
  const canvasSVG = await page.evaluate(() => {
    const shapes = [...document.querySelectorAll('.artboard .shape')]
      .map((shape) => {
        const clone = shape.cloneNode(true);
        clone.removeAttribute('class');
        return clone.outerHTML;
      })
      .join('');
    return `<svg xmlns="http://www.w3.org/2000/svg" width="160" height="100" viewBox="0 0 160 100">${shapes}</svg>`;
  });
  const canvasPixels = await pixels(Buffer.from(canvasSVG), 'image/svg+xml');
  assert.deepEqual(canvasPixels, svgPixels);
  assert.ok(svgPixels.overlap[3] > 0, `${svgPixels.overlap} should retain alpha`);
  await field('copiesAbove').locator('input[type="checkbox"]').uncheck();
  const behind = Buffer.from(await page.evaluate(() => window.nect.export()));
  const behindPixels = await pixels(behind, 'image/svg+xml');
  assert.notEqual(behindPixels.digest, svgPixels.digest);
  await fs.writeFile(`${root}/copies-below.svg`, behind);
  await page
    .locator('#open-file')
    .setInputFiles({ name: 'repeater.json', mimeType: 'application/json', buffer: json });
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.document) === expected,
    saved,
  );
  assert.deepEqual(
    await pixels(Buffer.from(await page.evaluate(() => window.nect.export())), 'image/svg+xml'),
    svgPixels,
  );
  await page.locator(`.graph .node[data-id="${repeat.id}"] .node-head strong`).click();
  await page.screenshot({ path: `${root}/inspector.png`, fullPage: true });
  await page.locator('.inspector-body .control-help summary').click();
  await page.locator('.inspector-body .control-help').scrollIntoViewIfNeeded();
  await page.screenshot({ path: `${root}/limits.png`, fullPage: true });
  assert.deepEqual(errors, []);
  const evidence = {
    uiControls: true,
    undoRedo: true,
    fractionalCopies: true,
    fractionalOffset: true,
    jsonRestore: true,
    canvasPixels,
    svgPixels,
    pngPixels,
    behindPixels,
    pageErrors: errors,
    csp: config.app.security.csp,
  };
  await fs.writeFile(`${root}/evidence.json`, JSON.stringify(evidence, null, 2));
  console.log(JSON.stringify(evidence));
} finally {
  await browser.close();
  await new Promise((resolve) => server.httpServer.close(resolve));
}
