import assert from 'node:assert/strict';
import { mkdir, mkdtemp, writeFile, readFile } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { resolve, join } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize, exportSVG } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0];
const a = node('Rectangle', 40, 40),
  b = node('Rectangle', 40, 300),
  fa = node('Fill', 240, 40),
  fb = node('Fill', 240, 300),
  blend = node('Blend', 440, 100);
a.params.center.value = [120, 160];
b.params.center.value = [420, 160];
fa.params.color.value = { hex: '#000000', alpha: 1 };
fb.params.color.value = { hex: '#ffffff', alpha: 1 };
blend.params.steps.value = 1;
layer.nodes.push(a, b, fa, fb, blend);
connect(layer, a.id, fa.id, 'path');
connect(layer, b.id, fb.id, 'path');
connect(layer, fa.id, blend.id, 'a');
connect(layer, fb.id, blend.id, 'b');
connect(layer, blend.id, layer.output, 'path');
await mkdir('test-results', { recursive: true });
const directory = await mkdtemp(resolve('test-results/blend-export-')),
  svgPath = join(directory, 'blend.svg'),
  pngPath = join(directory, 'blend.png'),
  svg = exportSVG(doc),
  chrome =
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium');
await writeFile(svgPath, svg);
await new Promise((resolveRun, reject) => {
  const child = spawn(
    process.execPath,
    ['cli/svg-to-png.js', svgPath, pngPath, '--chromium-path', chrome],
    { windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] },
  );
  let errors = '';
  child.stderr.on('data', (value) => {
    errors += value;
  });
  child.stdout.resume();
  const timer = setTimeout(() => child.kill(), 45000);
  child.on('error', (error) => {
    clearTimeout(timer);
    reject(error);
  });
  child.on('close', (code) => {
    clearTimeout(timer);
    code === 0 ? resolveRun() : reject(Error(errors || 'Blend PNG export failed'));
  });
});
assert.equal(await readFile(svgPath, 'utf8'), svg);
const png = await readFile(pngPath);
assert.deepEqual([...png.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  const pixels = await page.evaluate(async (base64) => {
    const image = new Image();
    image.src = 'data:image/png;base64,' + base64;
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    const context = canvas.getContext('2d');
    context.drawImage(image, 0, 0);
    return [50, 270, 420].map((x) => [...context.getImageData(x, 160, 1, 1).data]);
  }, png.toString('base64'));
  assert.deepEqual(pixels, [
    [0, 0, 0, 255],
    [188, 188, 188, 255],
    [255, 255, 255, 255],
  ]);
  await page.evaluate((value) => window.nect.load(value), serialize(doc));
  await ensureWorkspacePanel(page, 'inspector');
  await page.locator(`.graph .node[data-id="${fa.id}"] .node-head strong`).click();
  await page
    .locator(`.graph .node[data-id="${fb.id}"] .node-head strong`)
    .click({ modifiers: ['Shift'] });
  const beforeSelectedBlend = await page.evaluate(() => JSON.stringify(window.nect.document));
  if (!(await page.locator('[data-action="blend-selected"]').isVisible()))
    await page.getByRole('button', { name: 'Toggle node graph controls', exact: true }).click();
  await page.locator('[data-action="blend-selected"]').click();
  const withSelectedBlend = await page.evaluate(() => window.nect.document);
  assert.equal(withSelectedBlend.layers[0].nodes.filter((n) => n.type === 'Blend').length, 2);
  assert.deepEqual(
    withSelectedBlend.layers[0].nodes.find((n) => n.id === layer.output),
    layer.nodes.find((n) => n.id === layer.output),
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.document)),
    beforeSelectedBlend,
  );
  await page.locator(`.graph .node[data-id="${blend.id}"] .node-head strong`).click();
  const result = () => page.evaluate((id) => window.nect.evaluate().run(id), blend.id);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  const initial = await result();
  assert.equal(initial.children.length, 3);
  assert.equal(initial.children[1].fill.hex, '#bcbcbc');
  const input = page.locator(
    `.inspector-body .field[data-node="${blend.id}"][data-key="steps"] [data-value]`,
  );
  await input.fill('4');
  await input.press('Tab');
  const after = await page.evaluate(() => JSON.stringify(window.nect.document));
  const changed = await result();
  assert.equal(changed.children.length, 6);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  assert.deepEqual(await result(), initial);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  assert.deepEqual(await result(), changed);
  await page.evaluate((value) => window.nect.load(value), after);
  assert.deepEqual(await result(), changed);
  await page.locator(`.graph .node[data-id="${blend.id}"] .node-head strong`).click();
  const beforeExpand = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.locator(`.inspector-body [data-expand-blend="${blend.id}"]`).click();
  const expanded = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.equal(
    JSON.parse(expanded).layers[0].nodes.find((n) => n.id === blend.id).type,
    'Join Geometry',
  );
  assert.equal(
    JSON.parse(expanded).layers[0].nodes.filter((n) => n.type === 'Multi Path').length,
    6,
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), beforeExpand);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), expanded);
  await page.evaluate((value) => window.nect.load(value), expanded);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), expanded);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/blend-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });
  assert.deepEqual(errors, []);
  console.log('PASS Blend inspector/history/reload and real SVG-to-PNG pixels (headless Chrome)');
} finally {
  await browser.close();
}
