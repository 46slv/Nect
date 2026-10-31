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
page.on('dialog', (dialog) => dialog.accept());
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await fs.mkdir('test-results', { recursive: true });
  const id = await page.evaluate(async () => {
    const { blankDocument, node, connect } = await import('/src/core.js');
    const d = blankDocument(),
      l = d.layers[0],
      c = node('Circle'),
      g = node('Angular Gradient Fill'),
      out = l.nodes[0];
    c.params.center.value = [480, 270];
    c.params.radius.value = 200;
    c.position = [40, 40];
    g.position = [340, 40];
    out.position = [640, 40];
    l.nodes.unshift(c, g);
    connect(l, c.id, g.id, 'path');
    connect(l, g.id, out.id, 'path');
    window.nect.load(JSON.stringify(d));
    return g.id;
  });
  await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  const position = page.locator(
    `.inspector-body .field[data-node="${id}"][data-key="position1"] [data-value]`,
  );
  await position.fill('0.9');
  await position.press('Tab');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((n) => n.id === id).params.position1.value ===
        1 / 3 &&
      Math.abs(
        Number(
          document.querySelector(
            `.inspector-body .field[data-node="${id}"][data-key="position1"] [data-value]`,
          )?.value,
        ) -
          1 / 3,
      ) < 1e-12,
    id,
  );
  assert.ok(Math.abs(Number(await position.inputValue()) - 1 / 3) < 1e-12);
  const angle = page.locator(
    `.inspector-body .field[data-node="${id}"][data-key="angle"] [data-value]`,
  );
  await angle.fill('0');
  await angle.press('Tab');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await angle.inputValue(), '-90');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await angle.inputValue(), '0');
  await page.locator('[data-action="undo"]').click();
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const download = page.waitForEvent('download');
    await activateCommand(page, action);
    await (await download).saveAs(`test-results/workflow-angular-gradient.${ext}`);
  }
  const saved = await fs.readFile('test-results/workflow-angular-gradient.json', 'utf8');
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.angle.value,
      id,
    ),
    -90,
  );
  const svg = await fs.readFile('test-results/workflow-angular-gradient.svg', 'utf8'),
    png = await fs.readFile('test-results/workflow-angular-gradient.png');
  assert.match(svg, /<pattern /);
  const pixels = await page.evaluate(
    async ({ svg, png }) => {
      async function sample(url) {
        const img = new Image();
        img.src = url;
        await img.decode();
        const canvas = document.createElement('canvas');
        canvas.width = img.width;
        canvas.height = img.height;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0);
        return [
          [480, 90],
          [660, 270],
          [480, 450],
          [300, 270],
        ].map(([x, y]) => [
          ...ctx.getImageData(
            Math.floor((x * img.width) / 960),
            Math.floor((y * img.height) / 540),
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
  const expected = [
    [255, 0, 0, 255],
    [64, 191, 0, 255],
    [0, 128, 128, 255],
    [64, 0, 191, 255],
  ];
  for (const kind of ['svg', 'png'])
    pixels[kind].forEach((rgba, i) =>
      rgba.forEach((v, c) =>
        assert.ok(Math.abs(v - expected[i][c]) <= 5, `${kind} sample ${i}: ${rgba}`),
      ),
    );
  // Check actual exported alpha along shared sector boundaries, not only
  // midpoint colors. The pixel centers on these diagonals lie on 45° seams.
  await page.evaluate((id) => {
    const doc = structuredClone(window.nect.document);
    const g = doc.layers[0].nodes.find((n) => n.id === id);
    for (const key of ['startColor', 'color1', 'color2', 'endColor'])
      g.params[key].value.alpha = 0.5;
    window.nect.load(JSON.stringify(doc));
  }, id);
  const alphaDownload = page.waitForEvent('download');
  await activateCommand(page, 'export-png');
  await (await alphaDownload).saveAs('test-results/workflow-angular-gradient-alpha.png');
  const alphaPNG = await fs.readFile('test-results/workflow-angular-gradient-alpha.png');
  const alphaPixels = await page.evaluate(async (data) => {
    const img = new Image();
    img.src = 'data:image/png;base64,' + data;
    await img.decode();
    const canvas = document.createElement('canvas');
    canvas.width = img.width;
    canvas.height = img.height;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(img, 0, 0);
    const samples = [];
    for (let i = 0; i < 120; i++)
      for (const [dx, dy] of [
        [i, i],
        [-i, -i],
        [i, -i],
        [-i, i],
      ]) {
        samples.push(
          ctx.getImageData(
            Math.floor(((480 + dx) * img.width) / 960),
            Math.floor(((270 + dy) * img.height) / 540),
            1,
            1,
          ).data[3],
        );
      }
    return samples;
  }, alphaPNG.toString('base64'));
  assert.ok(
    alphaPixels.every((a) => Math.abs(a - 128) <= 1),
    'transparent sectors have no alpha gaps or double-painted seams',
  );
  await page.screenshot({
    path: 'test-results/workflow-angular-gradient-editor.png',
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  console.log(
    'PASS angular gradient: UI validation/history, JSON reopen, SVG/PNG cardinal colors and no page errors',
  );
} finally {
  await browser.close();
}
