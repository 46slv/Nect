import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import {
  blankDocument,
  connect,
  enableRemapPoint,
  evaluate,
  node,
  serialize,
} from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  output = layer.nodes[0],
  shape = node('Rectangle', 40, 80),
  stroke = node('Stroke', 340, 80),
  fill = node('Fill', 640, 80),
  first = node('Color', 340, 420),
  second = node('Color', 640, 420);
doc.schemaVersion = 54;
doc.colorSlots = ['first', 'second', 'stroke'].map((id) => ({
  id,
  name: id,
  value: { hex: '#123456', alpha: 0.5 },
}));
first.params.value.colorSlotId = 'first';
second.params.value.colorSlotId = 'second';
stroke.params.color.colorSlotId = 'stroke';
layer.nodes = [shape, stroke, fill, first, second, output];
connect(layer, shape.id, stroke.id, 'path');
connect(layer, stroke.id, fill.id, 'path');
connect(layer, first.id, fill.id, 'color');
connect(layer, fill.id, output.id, 'path');

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } }),
  errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate((value) => window.nect.load(value), serialize(doc));
  await ensureWorkspacePanel(page, 'inspector');
  const card = page.locator(`.graph .node[data-id="${fill.id}"]`),
    field = page.locator(`.inspector-body .field[data-node="${fill.id}"][data-key="color"]`),
    saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    scene = () =>
      page.evaluate(async () => {
        const { exportScene } = await import('/src/core.js');
        const result = exportScene(window.nect.document)[0];
        return { value: result.value, paintProvenance: result.paintProvenance };
      });
  await card.locator('.node-head strong').click();
  const before = await saved(),
    painted = await scene();
  assert.deepEqual(painted.paintProvenance, { stroke: 'stroke', fill: 'first' });
  await field.locator('[data-connect-menu]').click();
  await page.locator('dialog select').selectOption(second.id);
  await page.locator('dialog [data-confirm]').click();
  const rewired = await saved(),
    changed = await scene();
  assert.deepEqual(changed.value, painted.value);
  assert.deepEqual(changed.paintProvenance, { stroke: 'stroke', fill: 'second' });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  assert.deepEqual((await scene()).paintProvenance, { stroke: 'stroke', fill: 'first' });
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), rewired);
  assert.deepEqual((await scene()).paintProvenance, { stroke: 'stroke', fill: 'second' });
  await card.locator(`[data-bypass="${fill.id}"]`).click();
  const bypassed = await scene();
  assert.equal(bypassed.value.fill, undefined);
  assert.deepEqual(bypassed.value.stroke, changed.value.stroke);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), rewired);
  assert.deepEqual(await scene(), changed);
  await page.evaluate((value) => window.nect.load(value), rewired);
  assert.equal(await saved(), rewired);
  assert.deepEqual(await scene(), changed);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: `test-results/node-foundation-fill-${process.env.EVIDENCE_LABEL || 'current'}.png`,
  });

  const remapDoc = blankDocument(),
    remapLayer = remapDoc.layers[0],
    remapOutput = remapLayer.nodes[0],
    imported = node('Imported Path', 40, 80),
    upstreamFill = node('Fill', 340, 80),
    convert = node('Convert to Path', 640, 80),
    remap = node('Path Remap', 940, 80),
    repaint = node('Fill', 1240, 80);
  imported.rawPath = 'M0 0 L100 0 L100 100 L0 100 Z';
  for (const [item, id] of [
    [imported, 'shape'],
    [upstreamFill, 'fill'],
    [convert, 'convert'],
    [remap, 'remap'],
    [repaint, 'final'],
  ])
    item.id = id;
  remapLayer.nodes.push(imported, upstreamFill, convert, remap, repaint);
  for (const [from, to] of [
    [imported, upstreamFill],
    [upstreamFill, convert],
    [convert, remap],
    [remap, repaint],
    [repaint, remapOutput],
  ])
    connect(remapLayer, from.id, to.id, 'path');
  const converted = evaluate(remapLayer, {}, remapDoc.imageAssets, remapDoc).run(convert.id);
  enableRemapPoint(remap, converted, converted.contours[0].points[0].id).params.position.value = [
    3, 1,
  ];
  await page.evaluate((value) => window.nect.load(value), serialize(remapDoc));
  const remapState = () =>
      page.evaluate(() => {
        const evaluation = window.nect.evaluate(),
          path = evaluation.run('remap');
        return {
          warnings: evaluation.warnings,
          fingerprint: evaluation.run('convert').fingerprint,
          point: path.contours[0].points[0],
        };
      }),
    fillCard = page.locator('.graph .node[data-id="fill"]');
  await fillCard.locator('.node-head strong').click();
  const beforeRemap = await saved(),
    activeRemap = await remapState(),
    alpha = page.locator(
      '.inspector-body .field[data-node="fill"][data-key="color"] [data-color="alpha"]',
    );
  assert.deepEqual(activeRemap.point.position, [3, 1]);
  assert.deepEqual(activeRemap.warnings, []);
  await alpha.fill('0.5');
  await alpha.press('Tab');
  const recolored = await saved();
  assert.notEqual(recolored, beforeRemap);
  assert.deepEqual(await remapState(), activeRemap);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), beforeRemap);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), recolored);
  assert.deepEqual(await remapState(), activeRemap);
  await page.locator('[data-action="undo"]').click();
  await fillCard.locator('[data-bypass="fill"]').click();
  const bypassedRemap = await saved(),
    suspended = await remapState();
  assert.equal(suspended.point.id, activeRemap.point.id);
  assert.deepEqual(suspended.point.position, [0, 0]);
  assert.match(suspended.warnings.join(' '), /suspended/i);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), beforeRemap);
  assert.deepEqual(await remapState(), activeRemap);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), bypassedRemap);
  assert.match((await remapState()).warnings.join(' '), /suspended/i);
  await page.evaluate((value) => window.nect.load(value), bypassedRemap);
  assert.equal(await saved(), bypassedRemap);
  assert.match((await remapState()).warnings.join(' '), /suspended/i);
  assert.deepEqual(errors, []);
  console.log('PASS Fill color slots, provenance, bypass, Remap, Undo/Redo and reload');
} finally {
  await browser.close();
}
