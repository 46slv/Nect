import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';
import { loadDemoFixture } from './helpers/browser-fixture-setup.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } }),
  errors = [];
page.on('pageerror', (e) => errors.push(e.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await loadDemoFixture(page);
  await page.waitForSelector('.shape');
  await fs.mkdir('test-results', { recursive: true });
  await page.getByRole('button', { name: 'Add Polygon', exact: true }).click();
  assert.equal(
    await page.evaluate(() =>
      window.nect.document.layers.at(-1).nodes.some((n) => n.type === 'Polygon'),
    ),
    true,
  );
  await page.getByRole('button', { name: 'Add Star', exact: true }).click();
  assert.equal(
    await page.evaluate(() =>
      window.nect.document.layers.at(-1).nodes.some((n) => n.type === 'Star'),
    ),
    true,
  );
  const ids = await page.evaluate(async () => {
    const { document, node, connect } = await import('/src/core.js');
    const d = document(),
      l = d.layers[0],
      p = node('Polygon', 40, 40),
      s = node('Star', 40, 400),
      j = node('Join Geometry', 360, 40),
      f = node('Fill', 680, 40),
      o = node('Output', 990, 40);
    d.width = 400;
    d.height = 240;
    d.name = 'Polygon and star';
    p.params.center.value = [100, 120];
    p.params.radius.value = 70;
    s.params.center.value = [290, 120];
    s.params.outerRadius.value = 90;
    s.params.innerRadius.value = 40;
    l.nodes = [p, s, j, f, o];
    l.output = o.id;
    connect(l, p.id, j.id, 'a');
    connect(l, s.id, j.id, 'b');
    connect(l, j.id, f.id, 'path');
    connect(l, f.id, o.id, 'path');
    window.nect.load(JSON.stringify(d));
    return { p: p.id, s: s.id, l: l.id };
  });
  await page.locator(`.node[data-id="${ids.s}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  const field = (key) =>
    page.locator(`.inspector-body .field[data-node="${ids.s}"][data-key="${key}"]`);
  const count = () => field('points').locator('[data-value]');
  await count().fill('7');
  await count().press('Tab');
  assert.equal(
    await page.evaluate(() => window.nect.document.layers[0].nodes[1].params.points.value),
    7,
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await count().inputValue(), '5');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await count().inputValue(), '7');
  await page.evaluate(() => {
    const original = window.setTimeout;
    const tasks = [];
    window.setTimeout = (fn, delay, ...args) => {
      if (delay === 0 && new Error().stack.includes('scheduleRender')) {
        tasks.push(() => fn(...args));
        return 987654;
      }
      return original(fn, delay, ...args);
    };
    window.deliverStarRender = () => {
      window.setTimeout = original;
      const total = tasks.length;
      for (const task of tasks.splice(0)) task();
      return total;
    };
  });
  await count().fill('2.5');
  const editedCount = await count().elementHandle(),
    oldMarker = await page.locator('.node-stage-move rect').elementHandle();
  await count().press('Tab');
  // A rejected edit resets the existing input before the pending structural
  // render replaces the canvas marker; the input value alone is not a barrier.
  assert.equal(await editedCount.inputValue(), '7');
  assert.equal(await oldMarker.evaluate((el) => el.isConnected), true);
  assert.ok((await page.evaluate(() => window.deliverStarRender())) > 0);
  assert.equal(await oldMarker.boundingBox(), null);
  await page.waitForFunction(
    (old) =>
      !old.isConnected &&
      window.nect.document.layers[0].nodes[1].params.points.value === 7 &&
      document.querySelector('.inspector-body [data-key="points"] [data-value]')?.value === '7',
    oldMarker,
  );
  await editedCount.dispose();
  await oldMarker.dispose();
  const center = () =>
    page.evaluate(() => window.nect.document.layers[0].nodes[1].params.center.value);
  const drag = async () => {
    const markerHandle = await page.waitForFunction(() => {
      const el = document.querySelector('.node-stage-move rect'),
        r = el?.getBoundingClientRect();
      return r && r.width > 0 && r.height > 0 && getComputedStyle(el).visibility === 'visible'
        ? { x: r.x, y: r.y, width: r.width, height: r.height }
        : false;
    });
    const marker = await markerHandle.jsonValue();
    await markerHandle.dispose();
    const pt = { x: marker.x + marker.width / 2, y: marker.y + marker.height / 2 };
    await page.mouse.move(pt.x, pt.y);
    await page.mouse.down();
    await page.mouse.move(pt.x + 25, pt.y + 10, { steps: 4 });
    await page.mouse.up();
  };
  const before = await center();
  await drag();
  assert.notDeepEqual(await center(), before);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await center(), before);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-layer-lock="${ids.l}"]`).click();
  await drag();
  assert.deepEqual(await center(), before);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-layer-lock="${ids.l}"]`).click();
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const pending = page.waitForEvent('download');
    await activateCommand(page, action);
    await (await pending).saveAs(`test-results/workflow-polygon-star.${ext}`);
  }
  const svg = await fs.readFile('test-results/workflow-polygon-star.svg', 'utf8');
  assert.match(svg, /<path/);
  assert.doesNotMatch(svg, /NaN|Infinity/);
  const png = await fs.readFile('test-results/workflow-polygon-star.png');
  const alpha = await page.evaluate(async (data) => {
    const im = new Image();
    im.src = 'data:image/png;base64,' + data;
    await im.decode();
    const c = document.createElement('canvas');
    c.width = im.width;
    c.height = im.height;
    const ctx = c.getContext('2d');
    ctx.drawImage(im, 0, 0);
    return [
      [100, 120],
      [290, 120],
      [5, 5],
    ].map(([x, y]) => ctx.getImageData(x, y, 1, 1).data[3]);
  }, png.toString('base64'));
  assert.deepEqual(alpha, [255, 255, 0]);
  await page.evaluate(
    (s) => window.nect.load(s),
    await fs.readFile('test-results/workflow-polygon-star.json', 'utf8'),
  );
  assert.equal(
    await page.evaluate(() => window.nect.document.layers[0].nodes[1].params.points.value),
    7,
  );
  await page.screenshot({ path: 'test-results/workflow-polygon-star-editor.png', fullPage: true });
  assert.deepEqual(errors, []);
  console.log(
    'PASS Polygon/Star add, count, invalid preservation, drag/lock/history, JSON/SVG/PNG pixels',
  );
} finally {
  await browser.close();
}
