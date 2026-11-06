import assert from 'node:assert/strict';
import { readFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, node, point, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  target = doc.layers[0],
  source = blankLayer('Source'),
  curve = node('Curve');
target.id = 'target';
source.id = 'source';
curve.id = 'curve';
curve.points = Array.from({ length: 10000 }, (_, i) => {
  const p = point(i, 0);
  p.id = `point-${i}`;
  return p;
});
source.nodes.push(curve);
doc.layers.push(source);
doc.schemaVersion = 62;
for (let i = 0; i < 300; i++) {
  const sink = node('Value');
  sink.id = `sink-${i}`;
  sink.params.value.expression = `ref(artwork, "source", "curve", "point-${9700 + i}.position", "x")`;
  target.nodes.push(sink);
}

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(60000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.snapshot());
const value = () =>
  page.evaluate(async () => {
    const { evaluate } = await import('/src/core.js');
    const current = window.nect.snapshot();
    return evaluate(current.layers[0], {}, current.imageAssets, current).parameterValue(
      'sink-299',
      'value',
    );
  });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((input) => window.nect.load(input), serialize(doc));
  await ensureWorkspacePanel(page, 'layers');
  assert.equal(await value(), 9999);
  const before = await snapshot(),
    start = performance.now();
  await page.locator('[data-visible="source"]').click();
  await page.waitForFunction(
    () => window.nect.document.layers.find((layer) => layer.id === 'source').visible === false,
  );
  const clickMs = Math.round(performance.now() - start),
    hidden = await snapshot();
  assert.equal(hidden.layers[1].visible, false);
  assert.equal(await value(), 9999);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), hidden);
  await page.locator('[data-layer-lock="source"]').click();
  assert.equal((await snapshot()).layers[1].locked, true);
  assert.equal(await value(), 9999);
  await page.locator('[data-layer-lock="source"]').click();
  await page.locator('[data-layer-guide="source"]').click();
  await page.locator('[data-layer-solo="source"]').click();
  const final = await snapshot();
  assert.equal(final.layers[1].guide, true);
  assert.equal(final.layers[1].solo, true);
  assert.equal(await value(), 9999);
  await mkdir('test-results/artwork-reference-layer-flags', { recursive: true });
  const download = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve('test-results/artwork-reference-layer-flags/project.json');
  await (await download).saveAs(file);
  const bytes = await readFile(file);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page.locator('#open-file').setInputFiles({ name: 'project.json', buffer: bytes });
  await page.waitForFunction(() =>
    window.nect.document.layers.some((layer) => layer.id === 'source'),
  );
  assert.deepEqual(await snapshot(), final);
  assert.equal(await value(), 9999);
  assert.deepEqual(errors, []);
  console.log(JSON.stringify({ points: 10000, references: 300, clickMs }));
  console.log('PASS qualified references with layer flags, Undo/Redo and JSON reopen');
} finally {
  await browser.close();
}
