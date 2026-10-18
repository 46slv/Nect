import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, connect, serialize } from '../src/core.js';
const doc = document(),
  l = doc.layers[0],
  curve = node('Curve', 40, 40),
  tr = node('Transform', 350, 40),
  fill = node('Fill', 660, 40),
  out = node('Output', 970, 40);
curve.params.closed.value = true;
for (const [i, p] of curve.points.entries()) {
  p.position.value = [
    [20, 20],
    [130, 30],
    [140, 110],
    [30, 130],
  ][i];
  p.handleA.value = [0, 0];
  p.handleB.value = [0, 0];
  p.handleType = 'corner';
}
curve.points[0].handleA.value = [-25, -10];
curve.points[0].handleB.value = [25, 10];
curve.points[0].handleType = 'symmetric';
tr.params.scale.value = 1.5;
tr.params.rotation.value = 25;
tr.params.offset.value = [150, 120];
l.nodes = [curve, tr, fill, out];
l.output = out.id;
connect(l, curve.id, tr.id, 'path');
connect(l, tr.id, fill.id, 'path');
connect(l, fill.id, out.id, 'path');
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
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
  browser = await chromium.launch({
    executablePath:
      process.env.CHROME_PATH ||
      'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1050 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  const send = (type, payload = {}) =>
    page.evaluate(
      ({ type, payload }) => window.nect.dispatch({ ...window.nect.commandState, type, payload }),
      { type, payload: { layerId: l.id, nodeId: curve.id, ...payload } },
    );
  await send('set-guide-setting', { key: 'snap', value: false });
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    undo = () => page.locator('[data-action="undo"]').click(),
    stage = () => page.locator('.canvas-stage');
  const points = () =>
    page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).points,
      curve.id,
    );
  const world = async (i) =>
    page.evaluate(
      ({ id, tr, i }) => {
        const n = window.nect.document.layers[0].nodes.find((n) => n.id === id),
          p = n.points[i].position.value,
          m = window.nect.evaluate().transformValues(tr).matrix;
        return [m[0] * p[0] + m[2] * p[1] + m[4], m[1] * p[0] + m[3] * p[1] + m[5]];
      },
      { id: curve.id, tr: tr.id, i },
    );
  const screen = (p) =>
    page.locator('.artboard').evaluate((svg, p) => {
      const q = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
      return [q.x, q.y];
    }, p);
  const near = (a, b) =>
    a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 0.001, JSON.stringify({ a, b })));
  const selectPoint = async (i, shift = false) => {
    if (shift) await page.keyboard.down('Shift');
    await page.mouse.click(...(await screen(await world(i))));
    if (shift) await page.keyboard.up('Shift');
  };
  await stage().focus();
  await page.keyboard.press('a');
  await page.waitForSelector('[data-tool="direct"].active');
  assert.equal(await page.locator(`.node[data-id="${curve.id}"].selected`).count(), 1);
  const before = await saved();
  await selectPoint(0);
  const oldWorld = await world(0),
    oldPoints = await points();
  await page.keyboard.press('ArrowRight');
  near(await world(0), [oldWorld[0] + 1, oldWorld[1]]);
  assert.deepEqual((await points())[0].handleA, oldPoints[0].handleA);
  await undo();
  assert.equal(await saved(), before);
  await stage().focus();
  await page.keyboard.press('Shift+ArrowUp');
  near(await world(0), [oldWorld[0], oldWorld[1] - 10]);
  await undo();
  assert.equal(await saved(), before);
  await selectPoint(0);
  await selectPoint(1, true);
  const groupBefore = await saved(),
    world1 = await world(1);
  await page.keyboard.press('ArrowLeft');
  near(await world(0), [oldWorld[0] - 1, oldWorld[1]]);
  near(await world(1), [world1[0] - 1, world1[1]]);
  await undo();
  assert.equal(await saved(), groupBefore);
  await selectPoint(1, true);
  // Point drag maps screen movement back through the visible Transform.
  await selectPoint(0);
  const start = await screen(await world(0)),
    end = await screen([oldWorld[0] + 17, oldWorld[1] - 9]);
  await page.mouse.move(...start);
  await page.mouse.down();
  await page.mouse.move(...end, { steps: 5 });
  await page.mouse.up();
  near(await world(0), [oldWorld[0] + 17, oldWorld[1] - 9]);
  await undo();
  assert.equal(await saved(), before);
  // Alt breaks only the dragged handle, without changing the opposite vector.
  await selectPoint(0);
  const h = page.locator(
      `.point-marker[data-point="${curve.points[0].id}"][data-handle="handleB"]`,
    ),
    box = await h.boundingBox();
  assert.ok(box);
  await page.keyboard.down('Alt');
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 18, box.y + box.height / 2 + 8, { steps: 5 });
  await page.mouse.up();
  await page.keyboard.up('Alt');
  const independent = (await points())[0];
  assert.equal(independent.handleType, 'corner');
  assert.deepEqual(independent.handleA, oldPoints[0].handleA);
  assert.notDeepEqual(independent.handleB, oldPoints[0].handleB);
  await undo();
  assert.equal(await saved(), before);
  // Selected points own deletion and arrow keys; locks and IME refuse safely.
  await selectPoint(0);
  await send('set-parameter-lock', { key: `${curve.points[0].id}.position`, locked: true });
  const locked = await saved();
  await stage().focus();
  await page.keyboard.press('ArrowRight');
  assert.equal(await saved(), locked);
  await page.keyboard.press('Delete');
  assert.equal(await saved(), locked);
  await undo();
  const toolbarSafe = await saved();
  await page.locator('[data-action="undo"]').focus();
  await page.keyboard.press('Delete');
  assert.equal(await saved(), toolbarSafe);
  await selectPoint(0);
  const pointInput = page.locator(
    `.inspector-body [data-key="${curve.points[0].id}.position"] input[data-axis="0"]`,
  );
  await pointInput.focus();
  await pointInput.press('ArrowLeft');
  assert.equal(await saved(), before);
  await stage().focus();
  await stage().evaluate((el) =>
    el.dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true, isComposing: true }),
    ),
  );
  assert.equal(await saved(), before);
  await page.keyboard.press('Delete');
  assert.equal((await points()).length, oldPoints.length - 1);
  await undo();
  assert.equal(await saved(), before);
  const generated = document(),
    g = generated.layers[0],
    circle = node('Circle'),
    paint = node('Fill'),
    output = node('Output');
  g.nodes = [circle, paint, output];
  g.output = output.id;
  connect(g, circle.id, paint.id, 'path');
  connect(g, paint.id, output.id, 'path');
  await page.evaluate((s) => window.nect.load(s), serialize(generated));
  await stage().focus();
  await page.keyboard.press('a');
  const protectedDoc = await saved();
  await page.keyboard.press('Delete');
  await page.keyboard.press('ArrowRight');
  assert.equal(await saved(), protectedDoc);
  await mkdir('test-results/direct-selection', { recursive: true });
  await page.screenshot({ path: 'test-results/direct-selection/editor.png' });
  assert.deepEqual(errors, []);
  const receipt = {
    productionCsp: true,
    directShortcut: true,
    transformedPointDrag: true,
    worldPointNudge: true,
    multiPointNudge: true,
    shiftStep10: true,
    altIndependentHandle: true,
    oppositePreserved: true,
    oneUndo: true,
    lockedDeleteAndNudge: true,
    imeGuard: true,
    inputFocusGuard: true,
    toolbarDeleteSafe: true,
    generatedSourceProtected: true,
    pageErrors: errors,
  };
  await writeFile('test-results/direct-selection/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
