// Headless production web renderer/download route. No native app, profile or OS dialog.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { keyerMaskExample } from './keyer-mask-example.js';

const output = process.argv[2];
assert.ok(output && path.isAbsolute(output), 'Pass the model output directory');
const config = JSON.parse(await fs.readFile('src-tauri/tauri.conf.json', 'utf8')),
  server = await preview({
    preview: {
      host: '127.0.0.1',
      port: 0,
      headers: { 'Content-Security-Policy': config.app.security.csp },
    },
  });
let browser;
const hash = (bytes) => createHash('sha256').update(Buffer.from(bytes)).digest('hex'),
  noExisting = async (file) => assert.rejects(fs.access(file));
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({
      viewport: { width: 1600, height: 900 },
      deviceScaleFactor: 1,
    }),
    errors = [],
    external = [];
  await page.route('**/*', (route) => {
    const u = route.request().url();
    if (!/^(http:\/\/127\.0\.0\.1:|data:|blob:|about:)/.test(u)) {
      external.push(u);
      return route.abort();
    }
    return route.continue();
  });
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(server.resolvedUrls.local[0]);
  await page.waitForFunction(() => window.nect);
  assert.equal(
    await page.evaluate(() => typeof window.__TAURI_INTERNALS__),
    'undefined',
    'Web fallback only; never open a native dialog',
  );
  const $ = (s) => page.locator(s),
    settle = () =>
      page.evaluate(
        () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
      ),
    state = () =>
      page.evaluate(() => ({
        doc: window.nect.document,
        svg: window.nect.export(),
        command: window.nect.commandState,
      })),
    load = async (prefix) => {
      const text = await fs.readFile(path.join(output, prefix + '.json'), 'utf8');
      await page.evaluate((s) => window.nect.load(s), text);
      await settle();
      assert.deepEqual((await state()).doc, JSON.parse(text));
      return text;
    },
    pixels = async (source) =>
      page.evaluate(async (url) => {
        const img = new Image();
        img.src = url;
        await img.decode();
        const canvas = document.createElement('canvas');
        canvas.width = 80;
        canvas.height = 80;
        canvas.getContext('2d').drawImage(img, 0, 0);
        return [...canvas.getContext('2d').getImageData(0, 0, 80, 80).data];
      }, source),
    pixel = (rgba, x, y) => rgba.slice((y * 80 + x) * 4, (y * 80 + x) * 4 + 4),
    png = async (prefix) => {
      const button = $('[data-action="export-png"]');
      if (!(await button.isVisible()))
        await page
          .locator('.command-menu > summary')
          .filter({ hasText: /^Export$/ })
          .click();
      assert.equal(await $('#png-scale').inputValue(), '1');
      const p = path.join(output, prefix + '.png');
      await noExisting(p);
      const pending = page.waitForEvent('download');
      await button.click();
      await (await pending).saveAs(p);
      const bytes = await fs.readFile(p),
        rgba = await pixels('data:image/png;base64,' + bytes.toString('base64')),
        svg = (await state()).svg;
      assert.equal(bytes.readUInt32BE(16), 80);
      assert.equal(bytes.readUInt32BE(20), 80);
      assert.equal(
        hash(rgba),
        hash(await pixels('data:image/svg+xml;base64,' + Buffer.from(svg).toString('base64'))),
      );
      return { rgba, bytes: bytes.length, SHA256: hash(bytes), FullRGBASHA256: hash(rgba) };
    };
  await load('01-pick-background');
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-keyer"] .node-head strong').click();
  const before = await state();
  await $('.inspector-body [data-keyer-pick="example-keyer"]').click();
  assert.deepEqual(await state(), before);
  const target = await page.evaluate(() => {
    const svg = document.querySelector('[data-keyer-input-preview]'),
      p = new DOMPoint(20.5, 20.5).matrixTransform(svg.getScreenCTM());
    return [p.x, p.y];
  });
  await page.mouse.click(...target);
  await settle();
  const picked = await state();
  assert.deepEqual(picked.doc, keyerMaskExample(true));
  assert.equal(picked.command.expectedRevision, before.command.expectedRevision + 1);
  assert.equal(picked.svg, await fs.readFile(path.join(output, '02-keyed-mask.svg'), 'utf8'));
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, before.doc);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, picked.doc);
  const result = [];
  for (const [prefix, green] of [
    ['01-pick-background', [0, 255, 0, 255]],
    ['02-keyed-mask', [0, 0, 0, 0]],
  ]) {
    await load(prefix);
    assert.equal(
      (await state()).svg,
      await fs.readFile(path.join(output, prefix + '.svg'), 'utf8'),
    );
    const rendered = await png(prefix);
    const expected = [
      { point: [20, 20], RGBA: green },
      { point: [50, 30], RGBA: [255, 0, 0, 128] },
      { point: [50, 60], RGBA: [255, 0, 0, 255] },
      { point: [74, 60], RGBA: [0, 0, 0, 0] },
      { point: [6, 20], RGBA: [0, 0, 0, 0] },
    ];
    for (const p of expected) assert.deepEqual(pixel(rendered.rgba, ...p.point), p.RGBA);
    result.push({
      prefix,
      PNGBytes: rendered.bytes,
      PNG_SHA256: rendered.SHA256,
      PNG_SVG_FullRGBA_SHA256: rendered.FullRGBASHA256,
      ExpectedPixels: expected,
    });
  }
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await $('.inspector-body [data-node="example-mask"][data-key="invert"] [data-value]').check();
  await settle();
  const inverse = await png('03-inverted-check');
  assert.deepEqual(pixel(inverse.rgba, 50, 60), [0, 0, 0, 0]);
  assert.deepEqual(pixel(inverse.rgba, 74, 60), [255, 0, 0, 255]);
  await $('[data-action="undo"]').click();
  await settle();
  assert.equal((await state()).svg, picked.svg);
  if (await $('[data-action="export-png"]').isVisible())
    await page
      .locator('.command-menu > summary')
      .filter({ hasText: /^Export$/ })
      .click();
  await page.screenshot({ path: path.join(output, 'headless-editor.png') });
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  const assets = await fs.readdir('dist/assets'),
    bundle = [];
  for (const name of assets.sort())
    bundle.push({
      File: 'assets/' + name,
      SHA256: hash(await fs.readFile(path.join('dist/assets', name))),
    });
  const receipt = {
    Mode: 'Headless Edge production web bundle/CSP; web Export PNG download, no GUI window/native process/OS dialogs',
    Canvas: [80, 80],
    Nodes: 5,
    Connections: 4,
    InputPickerToFinishedJSONExact: true,
    OnePickUndoRedo: true,
    JSONReloadSVGExact: true,
    SVGPNGFullRGBAExact: true,
    InverseMaskPixelsVerified: true,
    ExternalRequests: external,
    PageErrors: errors,
    Results: result,
    Bundle: bundle,
    NotRun: [
      'Native GUI interaction for this example',
      'OS Open/Save/PNG dialogs',
      'Physical input',
      'Normal OS cold reopen',
    ],
    PendingNativeProcessTouched: false,
  };
  const receiptPath = path.join(output, 'render-receipt.json');
  await fs.writeFile(receiptPath, JSON.stringify(receipt, null, 2), { flag: 'wx' });
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser?.close();
  await new Promise((resolve, reject) =>
    server.httpServer.close((error) => (error ? reject(error) : resolve())),
  );
}
