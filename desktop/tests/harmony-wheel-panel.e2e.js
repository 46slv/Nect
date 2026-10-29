import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';

const out = resolve('test-results/harmony-wheel-panel');
await mkdir(out, { recursive: true });
const fixture = blankDocument(),
  layer = fixture.layers[0];
fixture.width = 320;
fixture.height = 240;
const shape = node('Rectangle', 40, 40),
  fill = node('Fill', 340, 40),
  output = node('Output', 640, 40);
layer.output = output.id;
shape.params.center.value = [160, 120];
shape.params.size.value = [140, 100];
layer.nodes = [shape, fill, output];
connect(layer, shape.id, fill.id, 'path');
connect(layer, fill.id, output.id, 'path');
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1000, height: 960 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
const snap = () => page.evaluate(() => window.nect.projectSnapshot());
const encoded = async () => JSON.stringify(await snap());
const change = async (locator, value) => {
  await locator.fill(String(value));
  await locator.press('Tab');
};
const marker = (id) => page.locator(`[data-wheel-marker="${id}"]`);
const markerPositions = () =>
  page
    .locator('[data-wheel-marker]')
    .evaluateAll((nodes) => nodes.map((n) => [n.dataset.wheelMarker, n.getAttribute('transform')]));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), serialize(fixture));
  await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  const before = await encoded();
  await page.locator('[data-harmony-create]').click();
  const created = await snap();
  assert.equal(created.schemaVersion, 56);
  assert.equal(created.colorDesign.version, 2);
  assert.equal(created.colorSlots.length, 5);
  assert.equal(created.colorDesign.palette.length, 5);
  assert.equal(await page.locator('.harmony-wheel-grid path').count(), 84);
  assert.equal(await page.locator('[data-wheel-marker]').count(), 3);
  const afterCreate = await encoded();
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), before);
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), afterCreate);
  const primary = created.colorDesign.palette[0].id,
    secondary = created.colorDesign.palette[1].id;
  await page.locator(`[data-select-layer="${layer.id}"]`).click();
  await page.locator(`.graph .node[data-id="${fill.id}"] .node-head strong`).click();
  await page
    .locator(`.inspector-body [data-shared-color-binding][data-shared-color-node="${fill.id}"]`)
    .selectOption(primary);
  await page.waitForFunction(
    (id) => document.querySelector(`[data-color-coverage-id="${id}"]`)?.textContent === '100%',
    primary,
  );
  const original = await encoded(),
    positions = await markerPositions();
  const initialGrid = await page.locator('.harmony-wheel-grid path').first().getAttribute('fill');
  await change(page.locator('[data-wheel-setting="hueRotation"]'), 75);
  assert.deepEqual(await markerPositions(), positions, 'hue rotation never rotates spokes');
  assert.notEqual(
    await page.locator('.harmony-wheel-grid path').first().getAttribute('fill'),
    initialGrid,
  );
  assert.equal(await encoded(), original);
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-cancel]').click();
  assert.equal(await encoded(), original);
  // A normal palette Apply reaches actual rendered artwork through its stable binding.
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  const liveColor = (await snap()).colorSlots.find((slot) => slot.id === primary).value.hex;
  assert.notEqual(liveColor, created.colorSlots.find((slot) => slot.id === primary).value.hex);
  await page.waitForFunction(
    (hex) => [...document.querySelectorAll('.shape')].some((el) => el.getAttribute('fill') === hex),
    liveColor,
  );
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), original);
  await change(page.locator('[data-wheel-setting="hueRotation"]'), 75);
  // Keep protects the exact original literal even when the wheel's sampled hue changes.
  await page.locator(`[data-chroma-lock="${primary}"]`).check();
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  const rotated = await snap();
  assert.equal(rotated.colorDesign.wheel.hueRotation, 75);
  assert.deepEqual(
    rotated.colorSlots.find((x) => x.id === primary).value,
    created.colorSlots.find((x) => x.id === primary).value,
  );
  assert.notDeepEqual(
    rotated.colorSlots.find((x) => x.id === secondary).value,
    created.colorSlots.find((x) => x.id === secondary).value,
  );
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), original);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snap(), rotated);
  // Native pointer capture on the persistent app root must survive structural renders.
  await page.locator('.shared-colors-panel').evaluate((el) => {
    el.scrollTop = 50;
  });
  // The panel is clipped: a boundingBox can exist below the visible scrollport.
  // Bring the actual handle into view before sending physical mouse input.
  await marker(secondary).scrollIntoViewIfNeeded();
  const scrollBefore = await page.locator('.shared-colors-panel').evaluate((el) => el.scrollTop);
  assert.ok(scrollBefore > 0);
  assert.equal(
    await marker(secondary).evaluate((el) => {
      const r = el.getBoundingClientRect();
      return document
        .elementFromPoint(r.x + r.width / 2, r.y + r.height / 2)
        ?.closest('[data-wheel-marker]')
        ?.getAttribute('data-wheel-marker');
    }),
    secondary,
    'physical drag starts on the visible marker, not the adjacent Inspector',
  );
  const b = await marker(secondary).boundingBox(),
    svg = await page.locator('.harmony-wheel').boundingBox();
  assert.ok(b && svg);
  await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
  await page.mouse.down();
  assert.equal(
    await page.locator('.shared-colors-panel').evaluate((el) => el.scrollTop),
    scrollBefore,
    'pointerdown retains panel scroll',
  );
  const r = (Math.min(svg.width, svg.height) / 230) * 100 * 0.9;
  await page.mouse.move(svg.x + svg.width / 2 + r, svg.y + svg.height / 2, { steps: 5 });
  await page.mouse.up();
  assert.equal(Number(await page.locator('[data-wheel-ring]').inputValue()), 7);
  assert.equal(await encoded(), JSON.stringify(rotated), 'radial drag is a draft');
  const changedTransform = await marker(secondary).getAttribute('transform');
  assert.notEqual(changedTransform, positions.find((x) => x[0] === secondary)[1]);
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  const dragged = await snap();
  assert.equal(dragged.colorDesign.palette.find((x) => x.id === secondary).ring, 6);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snap(), rotated);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snap(), dragged);
  // Explicit additions keep existing identities and the saved design rather than replacing it.
  await page.locator('[data-wheel-add-kind]').selectOption('neutral');
  await page.locator('[data-harmony-add]').click();
  const added = await snap();
  assert.equal(added.colorSlots.length, 6);
  assert.equal(added.colorDesign.palette.at(-1).kind, 'neutral');
  assert.deepEqual(added.colorSlots.slice(0, 5), dragged.colorSlots);
  const download = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(out, 'harmony-schema56.json');
  await (await download).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), added);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  const old = await page.evaluate(() => window.nect.commandState.sessionId);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction((id) => window.nect.commandState.sessionId !== id, old);
  assert.deepEqual(await snap(), added);
  assert.equal(await page.locator('[data-wheel-setting="harmony"]').inputValue(), 'triadic');
  assert.equal(Number(await page.locator('[data-wheel-setting="hueRotation"]').inputValue()), 75);
  await page.setViewportSize({ width: 900, height: 960 });
  await page.locator('.shared-colors-panel').evaluate((el) => {
    el.scrollTop = 0;
  });
  await page.screenshot({ path: resolve(out, 'harmony-narrow.png'), fullPage: true });
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(out, 'result.json'),
    JSON.stringify(
      {
        ok: true,
        createAtomicHistory: true,
        rotationKeepsGeometry: true,
        paletteChangesRenderedArtwork: true,
        keepLiteral: true,
        radialDraftAndScroll: true,
        addPreservesStableIds: true,
        browserSaveReopen: true,
        schemaVersion: 56,
        viewportWidth: 900,
        nativeDialogs: 'NOT_RUN',
        errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'PASS harmony wheel: real UI, geometry, radial draft, history, stable IDs, browser save/reopen',
  );
} catch (error) {
  await page.screenshot({ path: resolve(out, 'failure.png'), fullPage: true });
  await writeFile(
    resolve(out, 'failure.json'),
    JSON.stringify({ error: error.message, errors, project: await snap() }, null, 2),
  );
  throw error;
} finally {
  await browser.close();
}
