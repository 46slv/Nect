import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, connect, serialize, exportSVG } from '../src/core.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';

const doc = document(),
  comp = doc.layers[0],
  image = node('Image', 30, 40),
  out = node('Image Output', 730, 40),
  shape = node('Ellipse', 30, 380);
doc.width = 80;
doc.height = 80;
doc.schemaVersion = 35;
const rgba = new Uint8Array(80 * 80 * 4);
for (let y = 0; y < 80; y++)
  for (let x = 0; x < 80; x++) {
    const i = (y * 80 + x) * 4;
    rgba.set(x < 40 ? [0, 255, 0, 255] : [255, 0, 0, y < 40 ? 128 : 255], i);
  }
doc.imageAssets = {
  'image-synthetic': {
    name: 'synthetic.png',
    mime: 'image/png',
    width: 80,
    height: 80,
    data: encodeImage(encodeRgbaPng(80, 80, rgba)),
  },
};
image.asset = 'image-synthetic';
image.params.position.value = [0, 0];
image.params.size.value = [80, 80];
shape.params.center.value = [40, 40];
shape.params.size.value = [50, 50];
comp.nodes = [image, shape, out];
comp.output = out.id;
connect(comp, image.id, out.id, 'path');
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
  await page.waitForFunction(() => window.nect);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  const send = (type, payload = {}) =>
    page.evaluate(
      ({ type, payload }) =>
        window.nect.dispatch({
          ...window.nect.commandState,
          type,
          payload: { layerId: window.nect.document.layers[0].id, ...payload },
        }),
      { type, payload },
    );
  const inspect = async (id) => {
    await activateBoundaryCommand(page, 'graph-fit');
    await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  };
  const add = async (type) => {
    await page.locator('#add-node').click();
    await page.locator('dialog[open] .picker-search input').fill(type);
    await page.locator(`dialog[open] [data-id="${type}"]`).dblclick();
    return await page.evaluate(
      (type) => window.nect.document.layers[0].nodes.find((n) => n.type === type).id,
      type,
    );
  };
  const key = await add('Keyer');
  await send('connect', { sourceId: image.id, nodeId: key, key: 'path' });
  await send('connect', { sourceId: key, nodeId: out.id, key: 'path' });
  await inspect(key);
  const field = (keyName) =>
    page.locator(`.inspector-body [data-node="${key}"][data-key="${keyName}"]`);
  assert.equal(await field('keyColor').locator('[data-color="alpha"]').count(), 0);
  await field('tolerance').locator('[data-value]').fill('0.2');
  await field('tolerance').locator('[data-value]').press('Tab');
  assert.equal(
    await page.evaluate(
      (key) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === key).params.tolerance.value,
      key,
    ),
    0.2,
  );
  await page.locator('.inspector-body [data-bypass]').click();
  assert.equal(await page.evaluate(() => window.nect.evaluate().path.imageEffects), undefined);
  await page.locator('[data-action="undo"]').click();
  const mask = await add('Mask');
  await send('connect', { sourceId: key, nodeId: mask, key: 'path' });
  await send('connect', { sourceId: shape.id, nodeId: mask, key: 'mask' });
  await inspect(mask);
  await page.locator(`.graph [data-preview="${mask}"]`).click();
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers[0].nodes.find(
          (n) => n.id === window.nect.document.layers[0].output,
        ).type,
    ),
    'Output',
  );
  const artwork = await page.evaluate(() => window.nect.export());
  const pixels = async (svg) =>
    page.evaluate(async (svg) => {
      const im = new Image();
      im.src = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
      await im.decode();
      const c = document.createElement('canvas');
      c.width = 80;
      c.height = 80;
      const ctx = c.getContext('2d');
      ctx.drawImage(im, 0, 0, 80, 80);
      URL.revokeObjectURL(im.src);
      return [
        [20, 30],
        [50, 30],
        [50, 50],
        [70, 30],
      ].map(([x, y]) => [...ctx.getImageData(x, y, 1, 1).data]);
    }, svg);
  assert.deepEqual(await pixels(artwork), [
    [0, 0, 0, 0],
    [255, 0, 0, 128],
    [255, 0, 0, 255],
    [0, 0, 0, 0],
  ]);
  await page.locator('[data-assign-viewer="0"]').click();
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.locator('[data-viewer-alpha]').first().click();
  assert.equal(await page.locator('.node-viewer-body').first().getAttribute('data-alpha'), 'true');
  const alphaSvg = await page
    .locator('.node-viewer-body svg')
    .first()
    .evaluate((el) => {
      const copy = el.cloneNode(true);
      copy.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
      copy.setAttribute('viewBox', '0 0 80 80');
      copy.setAttribute('width', '80');
      copy.setAttribute('height', '80');
      return copy.outerHTML;
    });
  const alphaPixels = await pixels(alphaSvg);
  assert.deepEqual(alphaPixels[1], [128, 128, 128, 255]);
  assert.deepEqual(alphaPixels[0], [0, 0, 0, 255]);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.equal(await page.evaluate(() => window.nect.export()), artwork);
  await page.locator('[data-viewer-mode="artwork"]').click();
  await inspect(mask);
  await page.locator(`.inspector-body [data-key="invert"] [data-value]`).check();
  const inverted = await page.evaluate(() => window.nect.export());
  assert.deepEqual((await pixels(inverted))[3], [255, 0, 0, 128]);
  assert.deepEqual((await pixels(inverted))[1], [0, 0, 0, 0]);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => window.nect.export()), artwork);
  const saved = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(await page.evaluate(() => window.nect.export()), artwork);
  const pending = page.waitForEvent('download');
  await page.locator('[data-action="export-png"]').click();
  const download = await pending;
  const path = await download.path(),
    png = await readFile(path);
  assert.ok(png.length > 100);
  assert.equal(png.readUInt32BE(16), 80);
  assert.equal(png.readUInt32BE(20), 80);
  const data = 'data:image/png;base64,' + png.toString('base64');
  const exported = await page.evaluate(async (src) => {
    const im = new Image();
    im.src = src;
    await im.decode();
    const c = document.createElement('canvas');
    c.width = c.height = 80;
    const ctx = c.getContext('2d');
    ctx.drawImage(im, 0, 0);
    return [...ctx.getImageData(50, 30, 1, 1).data];
  }, data);
  assert.deepEqual(exported, [255, 0, 0, 128]);
  assert.equal(artwork, exportSVG(JSON.parse(saved)));
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  await mkdir('test-results', { recursive: true });
  await writeFile('test-results/image-effects-example.json', saved);
  await writeFile('test-results/image-effects-example.svg', artwork);
  await writeFile('test-results/image-effects-example.png', png);
  await page.screenshot({ path: 'test-results/image-effects.png' });
  await writeFile(
    'test-results/image-effects-receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        actualUi: true,
        png: { width: 80, height: 80, alpha128: true },
        keyer: true,
        mask: true,
        invert: true,
        viewerAlphaSRGB: true,
        undo: true,
        reopen: true,
        offline: true,
        errors,
      },
      null,
      2,
    ),
  );
  console.log('Keyer / Mask production UI, alpha pixels, Undo/reopen, SVG and PNG passed');
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
