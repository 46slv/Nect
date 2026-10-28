import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';
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
const out = resolve('test-results/asset-library');
await mkdir(out + '/fixtures/marks', { recursive: true });
await writeFile(
  out + '/fixtures/marks/mark.svg',
  '<svg width="120" height="80" viewBox="0 0 120 80"><rect x="10" y="10" width="80" height="50" fill="#e00000" fill-opacity="0.5"/><path d="M10 20H100" fill="none" stroke="#0044ff" stroke-width="3"/></svg>',
);
await writeFile(out + '/fixtures/unsafe.svg', '<svg><script>alert(1)</script></svg>');
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1440, height: 960 } }),
    errors = [],
    remote = [];
  page.setDefaultTimeout(15000);
  console.log('Browser ready');
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('dialog', (d) => d.accept());
  page.on('request', (r) => {
    if (!/^(http:\/\/127\.0\.0\.1:|data:|blob:)/.test(r.url())) remote.push(r.url());
  });
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  const png = await page.evaluate(() => {
    const c = document.createElement('canvas');
    c.width = 30;
    c.height = 20;
    const x = c.getContext('2d');
    x.fillStyle = '#00a040';
    x.fillRect(0, 0, 30, 20);
    return c.toDataURL('image/png').split(',')[1];
  });
  await writeFile(out + '/fixtures/photo.png', Buffer.from(png, 'base64'));
  const jpeg = await page.evaluate(() => {
    const c = document.createElement('canvas');
    c.width = 24;
    c.height = 16;
    const x = c.getContext('2d');
    x.fillStyle = '#e0a010';
    x.fillRect(0, 0, 24, 16);
    return c.toDataURL('image/jpeg').split(',')[1];
  });
  await writeFile(out + '/fixtures/photo.jpeg', Buffer.from(jpeg, 'base64'));
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const load = (text) => page.evaluate((text) => window.nect.load(text), text);
  await load(serialize(blankDocument()));
  const original = await saved();
  await activateCommand(page, 'asset-library');
  const folderChooser = page.waitForEvent('filechooser');
  await page
    .locator('[data-asset-library]')
    .getByRole('button', { name: 'Add folder', exact: true })
    .click();
  await (await folderChooser).setFiles(out + '/fixtures');
  await page.getByRole('button', { name: 'marks/mark.svg', exact: true }).waitFor();
  await page.getByRole('button', { name: 'marks/mark.svg', exact: true }).click();
  await page.locator('[data-asset-place-layer]:enabled').waitFor();
  await page.getByLabel('Asset license', { exact: true }).fill('Self-authored fixture');
  await page.screenshot({ path: out + '/browse.png' });
  await page.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(
    () => document.querySelector('[data-asset-status]').textContent !== 'Reading asset…',
  );
  assert.equal(await page.locator('[data-asset-status]').textContent(), 'Asset added');
  await page.waitForFunction(() =>
    window.nect.document.layers.some((l) => l.nodes.some((n) => n.type === 'SVG')),
  );
  const after = JSON.parse(await saved()),
    visible = after.layers.filter((l) => l.visible),
    svgLayer = visible.find((l) => l.nodes.some((n) => n.type === 'SVG')),
    svg = svgLayer.nodes.find((n) => n.type === 'SVG');
  console.log('SVG layer inserted');
  assert.equal(visible.length, 2);
  assert.equal(svg.svgAsset.license, 'Self-authored fixture');
  assert.equal(svg.svgAsset.relativePath, 'marks/mark.svg');
  const pixel = await page.evaluate(async () => {
    const image = new Image();
    image.src = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(window.nect.export());
    await image.decode();
    const c = document.createElement('canvas');
    c.width = 960;
    c.height = 540;
    const x = c.getContext('2d');
    x.drawImage(image, 0, 0);
    return [...x.getImageData(20, 30, 1, 1).data];
  });
  assert.deepEqual(pixel, [223, 0, 0, 128]);
  assert.equal(
    svgLayer.nodes.find((n) => n.id === svgLayer.output).params.path.source,
    svgLayer.nodes.find((n) => n.type === 'Join Geometry').id,
  );
  assert.ok(
    (await page.locator(`[data-node="${svg.id}"]`).count()) ||
      (await page.getByText('mark.svg', { exact: true }).count()),
  );
  const send = (type) => activateCommand(page, type);
  await send('undo');
  assert.equal(await saved(), original);
  await send('redo');
  assert.equal(await saved(), JSON.stringify(after));
  console.log('Undo/Redo passed');
  // Actual native JSON adapter download/reopen, with source paths unavailable to the reopened document.
  await page.getByRole('button', { name: 'Close assets', exact: true }).click();
  const downloadPromise = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const download = await downloadPromise;
  await download.saveAs(out + '/placed.json');
  console.log('Native JSON saved');
  const native = await readFile(out + '/placed.json', 'utf8');
  await load(serialize(blankDocument()));
  await load(native);
  assert.equal(
    JSON.parse(await saved())
      .layers.find((l) => l.id === svgLayer.id)
      .nodes.find((n) => n.id === svg.id).svgAsset.sha256,
    svg.svgAsset.sha256,
  );
  await activateCommand(page, 'asset-library');
  await page.getByRole('button', { name: 'marks/mark.svg', exact: true }).click();
  await page.locator('[data-asset-place-graph]:enabled').waitFor();
  const prior = JSON.parse(await saved());
  await page.locator('[data-asset-place-graph]').click();
  await page.waitForFunction(
    (count) =>
      window.nect.document.layers.flatMap((l) => l.nodes).filter((n) => n.type === 'SVG').length >
      count,
    prior.layers.flatMap((l) => l.nodes).filter((n) => n.type === 'SVG').length,
  );
  const disconnected = JSON.parse(await saved()),
    current = disconnected.layers.find((l) => l.id === svgLayer.id);
  console.log('Disconnected graph insertion passed');
  assert.deepEqual(
    current.nodes.slice(0, -1),
    prior.layers.find((l) => l.id === svgLayer.id).nodes,
  );
  // Opt-in join reuses the exclusive terminal Join.
  await page.getByLabel('Join before Out', { exact: true }).check();
  await page.locator('[data-asset-place-graph]:enabled').waitFor();
  await page.locator('[data-asset-place-graph]').click();
  await page.waitForFunction(
    () => document.querySelector('[data-asset-status]').textContent === 'Asset added',
  );
  const joined = JSON.parse(await saved()),
    currentJoined = joined.layers.find((l) => l.id === svgLayer.id);
  assert.equal(currentJoined.nodes.filter((n) => n.type === 'Join Geometry').length, 1);
  // Unsupported input is reported and never inserted or fetched.
  await page.getByRole('button', { name: 'unsafe.svg', exact: true }).click();
  await page.waitForFunction(
    () => document.querySelector('[data-asset-status]').dataset.error === 'true',
  );
  const unsupported = await saved();
  assert.equal(await page.locator('[data-asset-place-layer]').isDisabled(), true);
  assert.equal(await saved(), unsupported);
  // Raster shares the visual library and persists source provenance on canonical PNG.
  await page.getByLabel('Search assets', { exact: true }).fill('photo');
  await page.getByRole('button', { name: 'photo.png', exact: true }).click();
  await page.locator('[data-asset-place-layer]:enabled').waitFor();
  assert.equal(await page.locator('[data-asset-place-graph]').isDisabled(), true);
  await page.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(
    () => document.querySelector('[data-asset-status]').textContent !== 'Reading asset…',
  );
  assert.equal(await page.locator('[data-asset-status]').textContent(), 'Asset added');
  await page.waitForFunction(() =>
    Object.values(window.nect.document.imageAssets || {}).some((a) => a.provenance),
  );
  const withImage = JSON.parse(await saved()),
    asset = Object.values(withImage.imageAssets).find((a) => a.provenance);
  assert.equal(asset.mime, 'image/png');
  assert.equal(asset.provenance.relativePath, 'photo.png');
  assert.equal(asset.width, 30);
  assert.equal(asset.height, 20);
  await page.getByRole('button', { name: 'photo.jpeg', exact: true }).click();
  await page.locator('[data-asset-place-layer]:enabled').waitFor();
  await page.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(() =>
    Object.values(window.nect.document.imageAssets || {}).some(
      (a) => a.provenance?.relativePath === 'photo.jpeg',
    ),
  );
  const jpegAsset = await page.evaluate(() =>
    Object.values(window.nect.document.imageAssets).find(
      (a) => a.provenance?.relativePath === 'photo.jpeg',
    ),
  );
  assert.equal(jpegAsset.mime, 'image/png');
  assert.equal(jpegAsset.width, 24);
  await page.screenshot({ path: out + '/placed.png' });
  // Slow file read cannot write over a newer document; close cancels pending placement too.
  await page.getByLabel('Search assets', { exact: true }).fill('mark');
  await page.getByRole('button', { name: 'marks/mark.svg', exact: true }).click();
  await page.locator('[data-asset-place-layer]:enabled').waitFor();
  await page.evaluate(() => {
    const original = File.prototype.arrayBuffer;
    File.prototype.arrayBuffer = function () {
      return new Promise((resolve, reject) => {
        window.releaseAssetRead = () => original.call(this).then(resolve, reject);
      });
    };
    window.restoreAssetRead = () => {
      File.prototype.arrayBuffer = original;
    };
  });
  await page.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(() => window.releaseAssetRead);
  await load(serialize(blankDocument()));
  const changed = await saved();
  await page.evaluate(() => window.releaseAssetRead());
  await page.waitForFunction(() =>
    document.querySelector('[data-asset-status]').textContent.includes('Document changed'),
  );
  assert.equal(await saved(), changed);
  await page.evaluate(() => window.restoreAssetRead());
  await page.evaluate(() => {
    const original = File.prototype.arrayBuffer;
    File.prototype.arrayBuffer = function () {
      return new Promise((resolve, reject) => {
        window.releaseClosedAssetRead = () => original.call(this).then(resolve, reject);
      });
    };
    window.restoreClosedAssetRead = () => {
      File.prototype.arrayBuffer = original;
    };
  });
  await page.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(() => window.releaseClosedAssetRead);
  await page.getByRole('button', { name: 'Close assets', exact: true }).click();
  await page.evaluate(() => window.releaseClosedAssetRead());
  await page.evaluate(() => window.restoreClosedAssetRead());
  assert.equal(await saved(), changed);
  await activateCommand(page, 'asset-library');
  // Cancel directory selection does not change the document or library.
  await page.locator('[data-asset-folder-input]').dispatchEvent('cancel');
  assert.equal(await saved(), changed);
  assert.deepEqual(errors, []);
  assert.deepEqual(remote, []);
  await writeFile(
    out + '/results.json',
    JSON.stringify(
      {
        passed: true,
        svgRoutes: ['new-layer', 'disconnected-graph', 'safe-join'],
        raster: true,
        undoRedo: true,
        nativeSaveReopen: true,
        unsupported: true,
        staleDocument: true,
        closedPendingPlacement: true,
        cancel: true,
        errors,
        remote,
      },
      null,
      2,
    ),
  );
  console.log(
    'PASS asset library: real browser browse/search/preview SVG + PNG, both routes, safe Join, Undo/Redo, save/reopen, unsupported, Cancel and stale-document race',
  );
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
