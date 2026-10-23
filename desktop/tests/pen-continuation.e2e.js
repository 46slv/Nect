import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, connect, serialize } from '../src/core.js';
function sample(multi = false) {
  const d = document(),
    l = d.layers[0],
    base = node('Curve', 40, 40),
    extra = node('Value', 40, 550),
    tr = node('Transform', 350, 40),
    paint = node('Stroke', 660, 40),
    out = node('Output', 970, 40);
  base.params.closed.value = false;
  for (const [i, p] of base.points.entries()) {
    p.position.value = [
      [30, 30],
      [130, 35],
      [145, 115],
      [40, 125],
    ][i];
    p.handleA.value = [0, 0];
    p.handleB.value = [0, 0];
  }
  let path = base;
  if (multi) {
    path = node('Multi Path', 40, 40);
    path.points = base.points;
    path.contours = [
      { id: 'open-part', closed: false, points: base.points.slice(0, 2).map((p) => p.id) },
      { id: 'closed-part', closed: true, points: base.points.slice(2).map((p) => p.id) },
    ];
    path.units = [{ id: 'parts', contours: ['open-part', 'closed-part'] }];
    d.schemaVersion = 32;
  }
  tr.params.scale.value = 1.3;
  tr.params.rotation.value = 20;
  tr.params.offset.value = [150, 100];
  paint.params.width.value = 3;
  l.nodes = [extra, path, tr, paint, out];
  l.output = out.id;
  connect(l, path.id, tr.id, 'path');
  connect(l, tr.id, paint.id, 'path');
  connect(l, paint.id, out.id, 'path');
  return { d, l, path, tr };
}
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8')),
  server = createServer(async (req, res) => {
    const p = resolve(
      root,
      '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
    );
    if (!p.startsWith(root + sep)) {
      res.writeHead(403).end();
      return;
    }
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
  const edge =
    process.env.CHROME_PATH || 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
  browser = await chromium.launch({ executablePath: edge, headless: true });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1050 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    screen = (p) =>
      page.locator('.artboard').evaluate((svg, p) => {
        const q = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
        return [q.x, q.y];
      }, p),
    undo = () => page.locator('[data-action="undo"]').click();
  let lastCommitted, lastSvg;
  for (const multi of [false, true])
    for (const side of ['head', 'tail']) {
      const f = sample(multi);
      await page.evaluate((s) => window.nect.load(s), serialize(f.d));
      await page.evaluate(
        (layerId) =>
          window.nect.dispatch({
            ...window.nect.commandState,
            type: 'set-guide-setting',
            payload: { layerId, key: 'snap', value: false },
          }),
        f.l.id,
      );
      const before = await saved(),
        ids = multi ? f.path.contours[0].points : f.path.points.map((p) => p.id),
        endpoint = side === 'head' ? ids[0] : ids.at(-1);
      const world = (point) =>
        page.evaluate(
          ({ tr, point }) => {
            const m = window.nect.evaluate().transformValues(tr).matrix;
            return [
              m[0] * point[0] + m[2] * point[1] + m[4],
              m[1] * point[0] + m[3] * point[1] + m[5],
            ];
          },
          { tr: f.tr.id, point },
        );
      await page.locator('[data-tool="pen"]').click();
      await page.mouse.click(
        ...(await screen(await world(f.path.points.find((p) => p.id === endpoint).position.value))),
      );
      assert.equal(await page.evaluate(() => window.nect.gestureState?.resuming), true);
      assert.equal(await saved(), before);
      assert.equal(await page.locator('.artboard > .shape').count(), 1);
      const next = side === 'head' ? [5, 65] : [205, 150],
        handle = side === 'head' ? [-5, 75] : [220, 160],
        a = await screen(await world(next)),
        b = await screen(await world(handle));
      await page.mouse.move(...a);
      await page.mouse.down();
      await page.mouse.move(...b, { steps: 5 });
      await page.mouse.up();
      assert.equal(await saved(), before);
      await page.locator('.canvas-stage').focus();
      await page.keyboard.press('Enter');
      await page.waitForFunction(() => !window.nect.gestureState);
      const after = JSON.parse(await saved()),
        live = after.layers[0].nodes.find((n) => n.id === f.path.id),
        added = live.points.find((p) => !f.path.points.some((q) => q.id === p.id));
      assert.ok(added);
      assert.equal(after.layers.length, 1);
      for (const p of f.path.points)
        assert.deepEqual(
          live.points.find((q) => q.id === p.id),
          p,
        );
      added.position.value.forEach((v, i) => assert.ok(Math.abs(v - next[i]) < 0.001));
      if (multi) assert.deepEqual(live.contours[1], f.path.contours[1]);
      lastCommitted = await saved();
      lastSvg = await page.evaluate(() => window.nect.export());
      await undo();
      assert.equal(await saved(), before);
      // Selecting the same existing endpoint then cancelling must add nothing.
      await page.locator('[data-tool="pen"]').click();
      await page.mouse.click(
        ...(await screen(await world(f.path.points.find((p) => p.id === endpoint).position.value))),
      );
      assert.equal(await page.evaluate(() => window.nect.gestureState?.resuming), true);
      await page.locator('.canvas-stage').focus();
      await page.keyboard.press('Control+z');
      assert.equal(await saved(), before);
      assert.equal(await page.evaluate(() => window.nect.gestureState), null);
    }
  for (const side of ['head', 'tail']) {
    const f = sample(true);
    await page.evaluate((s) => window.nect.load(s), serialize(f.d));
    const before = await saved(),
      ids = f.path.contours[0].points,
      first = side === 'head' ? ids[0] : ids.at(-1),
      other = side === 'head' ? ids.at(-1) : ids[0];
    const xy = async (id) =>
      screen(
        await page.evaluate(
          ({ id, tr }) => {
            const p = window.nect.document.layers[0].nodes
                .find((n) => n.type === 'Multi Path')
                .points.find((p) => p.id === id).position.value,
              m = window.nect.evaluate().transformValues(tr).matrix;
            return [m[0] * p[0] + m[2] * p[1] + m[4], m[1] * p[0] + m[3] * p[1] + m[5]];
          },
          { id, tr: f.tr.id },
        ),
      );
    await page.locator('[data-tool="pen"]').click();
    await page.mouse.click(...(await xy(first)));
    await page.mouse.click(...(await xy(other)));
    assert.equal(
      await page.evaluate(
        () =>
          window.nect.document.layers[0].nodes.find((n) => n.type === 'Multi Path').contours[0]
            .closed,
      ),
      true,
    );
    await undo();
    assert.equal(await saved(), before);
    const redoDisabled = await page.locator('[data-action="redo"]').isDisabled();
    await page.locator('[data-tool="pen"]').click();
    await page.mouse.click(...(await xy(first)));
    await page.locator('.canvas-stage').focus();
    await page.keyboard.press('Enter');
    assert.equal(await saved(), before);
    assert.equal(await page.locator('[data-action="redo"]').isDisabled(), redoDisabled);
  }
  await page.evaluate((s) => window.nect.load(s), lastCommitted);
  assert.equal(await page.evaluate(() => window.nect.export()), lastSvg);
  await mkdir('test-results/pen-continuation', { recursive: true });
  const outputs = {};
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const pending = page.waitForEvent('download');
    await page.locator(`[data-action="${action}"]`).click();
    const file = `test-results/pen-continuation/continued.${ext}`;
    await (await pending).saveAs(file);
    outputs[ext] = await readFile(file);
  }
  assert.deepEqual(JSON.parse(outputs.json.toString()), JSON.parse(lastCommitted));
  assert.equal(outputs.svg.toString(), lastSvg);
  const pixels = await page.evaluate(
    async ({ svg, png }) => {
      const all = [];
      for (const [mime, data] of [
        ['image/svg+xml', svg],
        ['image/png', png],
      ]) {
        const image = new Image();
        image.src =
          mime === 'image/png'
            ? 'data:image/png;base64,' + data
            : URL.createObjectURL(new Blob([data], { type: mime }));
        await image.decode();
        const c = document.createElement('canvas');
        c.width = image.naturalWidth;
        c.height = image.naturalHeight;
        const ctx = c.getContext('2d');
        ctx.drawImage(image, 0, 0);
        all.push(ctx.getImageData(0, 0, c.width, c.height).data);
        if (mime !== 'image/png') URL.revokeObjectURL(image.src);
      }
      return {
        equal: all[0].length === all[1].length && all[0].every((v, i) => v === all[1][i]),
        painted: all[0].filter((v, i) => i % 4 === 3 && v > 0).length,
      };
    },
    { svg: lastSvg, png: outputs.png.toString('base64') },
  );
  assert.equal(pixels.equal, true);
  assert.ok(pixels.painted > 0);
  await mkdir('test-results/pen-continuation', { recursive: true });
  await page.screenshot({ path: 'test-results/pen-continuation/editor.png' });
  assert.deepEqual(errors, []);
  const receipt = {
    productionCsp: true,
    curveHeadTail: true,
    multiPathHeadTail: true,
    transformedPlacement: true,
    oldPointRecordsPreserved: true,
    otherContoursPreserved: true,
    noDuplicatePreviewLayer: true,
    oneUndo: true,
    existingOtherEndpointClose: true,
    unchangedFinishPreservesRedo: true,
    jsonReload: true,
    svgPngPixelsEqual: true,
    cancelPreservesDocument: true,
    pageErrors: errors,
  };
  await writeFile('test-results/pen-continuation/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
