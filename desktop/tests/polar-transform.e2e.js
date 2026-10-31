import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { activateCommand } from './command-menu-actions.js';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } }),
  errors = [];
page.on('pageerror', (e) => errors.push(e.message));
page.on('dialog', (d) => d.accept());
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await fs.mkdir('test-results', { recursive: true });
  const source = await fs.readFile('examples/polar-transform-ring.json', 'utf8');
  const id = await page.evaluate((s) => {
    window.nect.load(s);
    return window.nect.document.layers[0].nodes.find((n) => n.type === 'Polar Transform').id;
  }, source);
  await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  const radius = page.locator(
    `.inspector-body .field[data-node="${id}"][data-key="radius"] [data-value]`,
  );
  await radius.fill('300');
  await radius.press('Tab');
  assert.equal(await radius.inputValue(), '300');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await radius.inputValue(), '270');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await radius.inputValue(), '300');
  await page.locator('[data-action="undo"]').click();
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  await radius.fill('0');
  await radius.press('Tab');
  // Rejected edits restore the field on the app's next scheduled render.
  await page.waitForFunction(
    (id) =>
      document.querySelector(
        `.inspector-body .field[data-node="${id}"][data-key="radius"] [data-value]`,
      )?.value === '270',
    id,
  );
  assert.equal(await radius.inputValue(), '270');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  // Enter keeps focus on this control: a rejected value must not be restored
  // as though it were an uncommitted draft during the scheduled render.
  await radius.fill('0');
  await radius.press('Enter');
  assert.equal(await radius.inputValue(), '270');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const download = page.waitForEvent('download');
    await activateCommand(page, action);
    await (await download).saveAs(`test-results/workflow-polar-transform.${ext}`);
  }
  const saved = await fs.readFile('test-results/workflow-polar-transform.json', 'utf8');
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.radius.value,
      id,
    ),
    270,
  );
  const svg = await fs.readFile('test-results/workflow-polar-transform.svg', 'utf8');
  const png = await fs.readFile('test-results/workflow-polar-transform.png');
  const pixels = await page.evaluate(
    async ({ svg, png }) => {
      async function sample(url) {
        const image = new Image();
        image.src = url;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = image.width;
        canvas.height = image.height;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(image, 0, 0);
        return [
          [480, 270],
          [640, 270],
          [480, 430],
          [320, 270],
          [481, 110],
          [700, 270],
        ].map(([x, y]) => [
          ...ctx.getImageData(
            Math.floor((x * image.width) / 960),
            Math.floor((y * image.height) / 540),
            1,
            1,
          ).data,
        ]);
      }
      return {
        svg: await sample('data:image/svg+xml;charset=utf-8,' + encodeURIComponent(svg)),
        png: await sample('data:image/png;base64,' + png),
      };
    },
    { svg, png: png.toString('base64') },
  );
  for (const kind of ['svg', 'png']) {
    assert.equal(pixels[kind][0][3], 0, 'ring center stays transparent');
    assert.equal(pixels[kind][5][3], 0, 'outside ring stays transparent');
    for (const rgba of pixels[kind].slice(1, 5))
      [80, 110, 130, 255].forEach((v, i) =>
        assert.ok(Math.abs(rgba[i] - v) <= 2, `${kind}: ${rgba}`),
      );
  }
  assert.deepEqual(errors, []);
  await page.screenshot({
    path: 'test-results/workflow-polar-transform-editor.png',
    fullPage: true,
  });
  console.log(
    'PASS Polar Transform: radius edit/rejection/history, JSON reopen, SVG/PNG ring pixels',
  );
} finally {
  await browser.close();
}
