import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize, exportSVG } from '../src/core.js';
const doc = JSON.parse(
  await readFile('test-results/dither-controls/points-comparison.json', 'utf8'),
);
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

  const points = doc.layers[0].nodes.find((n) => n.type === 'Luminance to Points');
  const motif = doc.layers[0].nodes.find((n) => n.type === 'Star');
  await activateBoundaryCommand(page, 'graph-fit');
  await page.locator('.graph .node[data-id="' + points.id + '"] .node-head strong').click();
  const field = (key) =>
    page.locator(
      '.inspector-body [data-node="' + points.id + '"][data-key="' + key + '"] [data-value]',
    );
  for (const [key, min, max] of [
    ['pitch', '1', '1024'],
    ['minScale', '0', '100'],
    ['maxScale', '0', '100'],
    ['minRotation', '-360', '360'],
    ['maxRotation', '-360', '360'],
  ]) {
    assert.equal(await field(key).getAttribute('min'), min);
    assert.equal(await field(key).getAttribute('max'), max);
  }
  assert.match(await page.locator('.inspector-body').textContent(), /256 active points/);
  const initial = await page.evaluate(() => window.nect.export());
  await field('maxScale').fill('1.2');
  await field('maxScale').press('Tab');
  await field('pitch').fill('12');
  await field('pitch').press('Tab');
  await field('maxRotation').fill('20');
  await field('maxRotation').press('Tab');
  const get = () =>
    page.evaluate((id) => window.nect.document.layers[0].nodes.find((n) => n.id === id), points.id);
  let pointNode = await get();
  assert.equal(pointNode.params.pitch.value, 12);
  assert.equal(pointNode.params.maxScale.value, 1.2);
  assert.equal(pointNode.params.maxRotation.value, 20);
  const saved = await page.evaluate(() => JSON.stringify(window.nect.document));
  const svg = await page.evaluate(() => window.nect.export());
  assert.notEqual(svg, initial);
  assert.equal(svg, exportSVG(JSON.parse(saved)));
  const paths = await page.evaluate(() => ({
    preview: [...document.querySelectorAll('.artboard path.shape,.artboard .shape path')].map((p) =>
      p.getAttribute('d'),
    ),
    output: [
      ...new DOMParser()
        .parseFromString(window.nect.export(), 'image/svg+xml')
        .querySelectorAll('path'),
    ].map((p) => p.getAttribute('d')),
  }));
  assert.deepEqual(paths.preview, paths.output);
  await page.locator('[data-action="undo"]').click();
  assert.equal((await get()).params.maxRotation.value, 0);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  // Change the motif through the same Session; the point graph stays separate.
  await page.evaluate(
    (id) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-parameter',
        payload: {
          layerId: window.nect.document.layers[0].id,
          nodeId: id,
          key: 'innerRadius',
          value: 6,
        },
      }),
    motif.id,
  );
  assert.equal((await get()).params.pitch.value, 12);
  assert.notEqual(await page.evaluate(() => window.nect.export()), svg);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  const pending = page.waitForEvent('download');
  await page.locator('[data-action="export-png"]').click();
  const downloaded = await pending,
    png = await readFile(await downloaded.path());
  const matching = await page.evaluate(
    async ({ svg, png }) => {
      const pixels = async (src) => {
        const image = new Image();
        image.src = src;
        await image.decode();
        const c = document.createElement('canvas');
        c.width = 304;
        c.height = 288;
        const ctx = c.getContext('2d');
        ctx.drawImage(image, 0, 0);
        return ctx.getImageData(0, 0, 304, 288).data;
      };
      const a = await pixels('data:image/svg+xml;base64,' + btoa(svg)),
        b = await pixels('data:image/png;base64,' + png);
      return a.length === b.length && a.every((v, i) => v === b[i]);
    },
    { svg, png: png.toString('base64') },
  );
  assert.ok(matching);
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  await activateBoundaryCommand(page, 'fit');
  await page.screenshot({ path: 'test-results/dither-controls/points-editor.png' });
  const art = await browser.newPage({ viewport: { width: 608, height: 576 } });
  await art.setContent(
    '<style>body{margin:0;background:white}img{width:608px;height:576px}</style><img src="data:image/svg+xml;base64,' +
      Buffer.from(exportSVG(doc)).toString('base64') +
      '">',
  );
  await art
    .locator('img')
    .screenshot({ path: 'test-results/dither-controls/points-comparison.png' });
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  await writeFile(
    'test-results/dither-controls/points-browser-receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        independentControls: true,
        customMotif: true,
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
    'Luminance to Points production UI, custom motif, save/undo and preview/SVG/PNG equality verified.',
  );
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
