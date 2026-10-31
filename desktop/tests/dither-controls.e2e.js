import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize, exportSVG } from '../src/core.js';
const doc = JSON.parse(await readFile('test-results/dither-controls/comparison.json', 'utf8'));
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const p = resolve(
    root,
    '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
  );
  if (!p.startsWith(root + sep)) return res.writeHead(403).end();
  try {
    res.setHeader('Content-Security-Policy', config.app.security.csp);
    res.setHeader(
      'Content-Type',
      { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.ttf': 'font/ttf' }[
        extname(p)
      ] || 'application/octet-stream',
    );
    res.end(await readFile(p));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } }),
    errors = [],
    external = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('dialog', (d) => d.accept());
  page.on('request', (r) => {
    if (!/^(http:\/\/127\.0\.0\.1:|data:|blob:)/.test(r.url())) external.push(r.url());
  });
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((s) => window.nect.load(s), serialize(doc));

  const effect = doc.layers[0].nodes.find((n) => n.type === 'Halftone');
  await activateBoundaryCommand(page, 'graph-fit');
  await page.locator('.graph .node[data-id="' + effect.id + '"] .node-head strong').click();
  const field = (key) =>
    page.locator(
      '.inspector-body [data-node="' + effect.id + '"][data-key="' + key + '"] [data-value]',
    );
  const shape = page.locator('.inspector-body [data-operation="' + effect.id + '"]');
  assert.deepEqual(await shape.locator('option').allTextContents(), [
    'Legacy calibrated',
    'Square',
    'Circle',
    'Polygon',
    'Star',
  ]);
  for (const [key, min, max] of [
    ['dotSize', '1', '1024'],
    ['pitch', '1', '1024'],
    ['dotSides', '3', '32'],
    ['dotRotation', '-360', '360'],
    ['starInnerRatio', '0.05', '0.95'],
  ]) {
    assert.equal(await field(key).getAttribute('min'), min);
    assert.equal(await field(key).getAttribute('max'), max);
  }
  await shape.selectOption('polygon');
  await field('dotSides').fill('6');
  await field('dotSides').press('Tab');
  await field('dotRotation').fill('30');
  await field('dotRotation').press('Tab');
  await field('dotSize').fill('24');
  await field('dotSize').press('Tab');
  await field('pitch').fill('12');
  await field('pitch').press('Tab');
  let n = await page.evaluate(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id),
    effect.id,
  );
  assert.equal(n.operation, 'polygon');
  assert.equal(n.params.dotSize.value, 24);
  assert.equal(n.params.pitch.value, 12);
  assert.equal(n.params.dotSides.value, 6);
  assert.equal(n.params.dotRotation.value, 30);
  const saved = await page.evaluate(() => JSON.stringify(window.nect.document));
  const svg = await page.evaluate(() => window.nect.export());
  assert.equal(svg, exportSVG(JSON.parse(saved)));
  const equality = await page.evaluate(() => {
    const exported = new DOMParser().parseFromString(window.nect.export(), 'image/svg+xml');
    const preview = [
      ...document.querySelectorAll('.artboard path.shape, .artboard .shape path'),
    ].map((p) => p.getAttribute('d'));
    return {
      preview,
      output: [...exported.querySelectorAll('path')].map((p) => p.getAttribute('d')),
    };
  });
  assert.deepEqual(equality.preview, equality.output);
  await page.locator('[data-action="undo"]').click();
  n = await page.evaluate(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id),
    effect.id,
  );
  assert.equal(n.params.pitch.value, 16);
  assert.equal(n.params.dotSize.value, 24);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  const pending = page.waitForEvent('download');
  await page.locator('[data-action="export-png"]').click();
  const downloaded = await pending,
    png = await readFile(await downloaded.path());
  assert.equal(png.readUInt32BE(16), 304);
  assert.equal(png.readUInt32BE(20), 288);
  const matching = await page.evaluate(
    async ({ svg, png }) => {
      const pixels = async (src) => {
        const image = new Image();
        image.src = src;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = 304;
        canvas.height = 288;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(image, 0, 0);
        return ctx.getImageData(0, 0, 304, 288).data;
      };
      const a = await pixels('data:image/svg+xml;base64,' + btoa(svg)),
        b = await pixels('data:image/png;base64,' + png);
      return a.length === b.length && a.every((v, i) => v === b[i]);
    },
    { svg, png: png.toString('base64') },
  );
  assert.ok(matching, 'PNG pixels match SVG rasterization');
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  await activateBoundaryCommand(page, 'fit');
  await mkdir('test-results/dither-controls', { recursive: true });
  await page.screenshot({ path: 'test-results/dither-controls/editor.png' });
  const art = await browser.newPage({ viewport: { width: 608, height: 576 } });
  await art.setContent(
    '<style>body{margin:0;background:white}img{width:608px;height:576px}</style><img src="data:image/svg+xml;base64,' +
      Buffer.from(exportSVG(doc)).toString('base64') +
      '">',
  );
  await art.locator('img').screenshot({ path: 'test-results/dither-controls/comparison.png' });
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  await writeFile(
    'test-results/dither-controls/browser-receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        shapeControls: true,
        previewSvgGeometry: true,
        svgPngPixels: true,
        saveUndoRedo: true,
        pageErrors: errors,
        externalRequests: external,
      },
      null,
      2,
    ),
  );
  console.log(
    'Dither production UI, independent controls, preview/SVG/PNG equality, save and undo verified.',
  );
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
