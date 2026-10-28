import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, node, connect, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';

// Shared colors and transient saturation preview; no persisted recipe/native-dialog acceptance.
const output = resolve('test-results/shared-colors-panel');
await mkdir(output, { recursive: true });
const fixture = blankDocument();
fixture.name = 'Shared manual colors browser verification';
fixture.width = 320;
fixture.height = 240;
const makeArtwork = (x) => {
  const layer = blankLayer('Same artwork name'),
    shape = node('Rectangle', 40, 40),
    fill = node('Fill', 340, 40),
    unusedColor = node('Color', 340, 360),
    out = node('Output', 640, 40);
  shape.params.center.value = [x, 120];
  shape.params.size.value = [100, 100];
  fill.name = unusedColor.name = 'Same paint name';
  fill.params.color.value = { hex: '#112233', alpha: 0.6 };
  layer.nodes = [shape, fill, unusedColor, out];
  layer.output = out.id;
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  return { layer, fill, unusedColor };
};
const target = makeArtwork(80),
  sibling = makeArtwork(240);
fixture.layers = [target.layer, sibling.layer];
const fixtureText = serialize(fixture);
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const saved = async () => JSON.stringify(await snapshot());
const documentSnapshot = () => page.evaluate(() => window.nect.document);
const inspectTarget = async () => {
  await page.locator(`[data-select-layer="${target.layer.id}"]`).click();
  await page.locator(`.graph .node[data-id="${target.fill.id}"] .node-head strong`).click();
};
const field = () =>
  page.locator(`.inspector-body .field[data-node="${target.fill.id}"][data-key="color"]`);
const binding = () => field().locator('[data-shared-color-binding]');
const row = (id) => page.locator(`[data-shared-color-row="${id}"]`);
const change = async (locator, value) => {
  await locator.fill(String(value));
  await locator.press('Tab');
};
const shapeHasColor = (hex, alpha) =>
  page.waitForFunction(
    ({ hex, alpha }) =>
      [...document.querySelectorAll('.shape')].some(
        (shape) =>
          shape.getAttribute('fill') === hex &&
          Number(shape.getAttribute('fill-opacity')) === alpha,
      ),
    { hex, alpha },
  );
const parameter = async () => {
  const document = await documentSnapshot();
  return document.layers
    .find((l) => l.id === target.layer.id)
    .nodes.find((n) => n.id === target.fill.id).params.color;
};
const coverageEvidence = [];
// Independent oracle: rasterize the complete exported scene, not the coverage
// bridge's isolated passes. The fixture's equal-area rectangles lie wholly in
// separate artboard halves, so summing those halves gives exact attribution.
// Read actual raster alpha: 0.5 can become 128/255, making the rounded share 46%
// rather than the ideal-alpha 45%. The 0.25 case remains 29% / Other 71%.
const rendererShare = () =>
  page.evaluate(async () => {
    const doc = window.nect.document;
    const url = URL.createObjectURL(new Blob([window.nect.export()], { type: 'image/svg+xml' }));
    const image = new Image();
    try {
      image.src = url;
      await image.decode();
      const canvas = document.createElement('canvas');
      canvas.width = doc.width;
      canvas.height = doc.height;
      const context = canvas.getContext('2d', { willReadFrequently: true });
      context.drawImage(image, 0, 0, canvas.width, canvas.height);
      const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
      let left = 0,
        right = 0;
      for (let y = 0; y < canvas.height; y++)
        for (let x = 0; x < canvas.width; x++) {
          const alpha = pixels[(y * canvas.width + x) * 4 + 3];
          if (x < canvas.width / 2) left += alpha;
          else right += alpha;
        }
      return { percent: Math.round((left / (left + right)) * 100), left, right };
    } finally {
      image.src = '';
      URL.revokeObjectURL(url);
    }
  });
async function assertCoverage(slotId, unusedSlotId, percent, label) {
  const before = await saved();
  const commandState = await page.evaluate(() => window.nect.commandState);
  const dirty = await page.evaluate(() => window.nect.fileState.dirty);
  const other = 100 - percent;
  await page.waitForFunction(
    ({ slotId, unusedSlotId, percent, other }) => {
      const outputs = [...document.querySelectorAll('[data-color-coverage-id]')];
      const value = (id) => outputs.find((el) => el.dataset.colorCoverageId === id)?.textContent;
      return (
        value(slotId) === `${percent}%` &&
        value(unusedSlotId) === '0%' &&
        document.querySelector('[data-color-coverage-status]')?.textContent ===
          `Visible area · approx. 256 × 192 sample · Other ${other}%`
      );
    },
    { slotId, unusedSlotId, percent, other },
  );
  assert.equal(await row(slotId).locator('[data-color-coverage-id]').textContent(), `${percent}%`);
  assert.equal(await saved(), before, `${label}: measurement must not author document changes`);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), commandState);
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), dirty);
  coverageEvidence.push({ label, slotPercent: percent, unusedSlotPercent: 0, otherPercent: other });
}
let libraryIdentity;
const assertAssetRetained = async () => {
  assert.equal(
    await page.evaluate(
      (element) => element === document.querySelector('[data-asset-library]'),
      libraryIdentity,
    ),
    true,
    'color edits and rendering retain the mounted Assets panel',
  );
  assert.equal(
    await page.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(),
    'retained-search',
  );
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), fixtureText);
  const initial = await documentSnapshot();
  const beforePanels = await saved();
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  libraryIdentity = await page.locator('[data-asset-library]').elementHandle();
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('retained-search');
  const launcher = page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true });
  await launcher.click();
  await page.locator('.shared-colors-panel').waitFor();
  assert.equal(await launcher.getAttribute('aria-pressed'), 'true');
  assert.equal(await saved(), beforePanels, 'opening workspace panels is not an authored edit');

  await page.getByRole('button', { name: 'Add shared color', exact: true }).click();
  let slots = (await documentSnapshot()).colorSlots;
  assert.equal(slots.length, 1);
  const slotId = slots[0].id;
  await change(row(slotId).locator('[data-shared-color-edit="name"]'), 'Brand <&>');
  await row(slotId)
    .locator('[data-shared-color-edit="hex"]')
    .evaluate((el) => {
      el.value = '#2468ab';
      el.dispatchEvent(new Event('change', { bubbles: true }));
    });
  await change(row(slotId).locator('[data-shared-color-edit="alpha"]'), '0.5');
  await page.getByRole('button', { name: 'Add shared color', exact: true }).click();
  slots = (await documentSnapshot()).colorSlots;
  const otherSlotId = slots[1].id;
  assert.notEqual(slotId, otherSlotId);
  await change(row(otherSlotId).locator('[data-shared-color-edit="name"]'), 'Brand <&>');
  assert.deepEqual((await documentSnapshot()).colorSlots[0], {
    id: slotId,
    name: 'Brand <&>',
    value: { hex: '#2468ab', alpha: 0.5 },
  });
  assert.equal(
    await page.locator('.shared-colors-panel img,.shared-colors-panel script').count(),
    0,
  );
  await inspectTarget();
  assert.equal(await binding().getAttribute('data-shared-color-layer'), target.layer.id);
  assert.equal(await binding().getAttribute('data-shared-color-node'), target.fill.id);
  assert.equal(await binding().getAttribute('data-shared-color-key'), 'color');
  await binding().selectOption(slotId);
  assert.equal((await parameter()).colorSlotId, slotId);
  assert.deepEqual((await parameter()).value, { hex: '#112233', alpha: 0.6 });
  assert.equal(await field().locator('[data-color="hex"]').isDisabled(), true);
  assert.equal(await field().locator('[data-color="alpha"]').isDisabled(), true);
  const boundDocument = await documentSnapshot();
  assert.deepEqual(
    boundDocument.layers.find((l) => l.id === sibling.layer.id),
    initial.layers[1],
  );
  assert.equal(
    boundDocument.layers
      .find((l) => l.id === target.layer.id)
      .nodes.find((n) => n.id === target.unusedColor.id).params.value.colorSlotId,
    undefined,
    'same-name Color node is not accidentally bound',
  );
  await shapeHasColor('#2468ab', 0.5);
  const halfAlphaShare = await rendererShare();
  assert.ok([45, 46].includes(halfAlphaShare.percent), 'equal-area 0.5 / 0.6 raster-alpha fixture');
  await assertCoverage(slotId, otherSlotId, halfAlphaShare.percent, 'bound alpha 0.5');
  await assertAssetRetained();

  // A shared edit updates the bound output and remains exactly one Undo step.
  const beforeSharedEdit = await saved();
  await change(row(slotId).locator('[data-shared-color-edit="alpha"]'), '0.25');
  await shapeHasColor('#2468ab', 0.25);
  const quarterAlphaShare = await rendererShare();
  assert.equal(quarterAlphaShare.percent, 29, 'equal-area 0.25 / 0.6 renderer contribution');
  await assertCoverage(slotId, otherSlotId, 29, 'shared alpha edit');
  const afterSharedEdit = await saved();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), beforeSharedEdit);
  await shapeHasColor('#2468ab', 0.5);
  await assertCoverage(slotId, otherSlotId, halfAlphaShare.percent, 'Undo shared alpha edit');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), afterSharedEdit);
  await shapeHasColor('#2468ab', 0.25);
  await assertCoverage(slotId, otherSlotId, 29, 'Redo shared alpha edit');

  // Unlink retains the effective color rather than jumping to the old local base.
  const beforeUnbind = await saved();
  await binding().selectOption('');
  assert.equal((await parameter()).colorSlotId, undefined);
  assert.deepEqual((await parameter()).value, { hex: '#2468ab', alpha: 0.25 });
  assert.equal(await field().locator('[data-color="hex"]').isDisabled(), false);
  await assertCoverage(slotId, otherSlotId, 0, 'unlink: equal display color stays Other');
  const afterUnbind = await saved();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), beforeUnbind);
  await assertCoverage(slotId, otherSlotId, 29, 'Undo unlink');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), afterUnbind);
  await assertCoverage(slotId, otherSlotId, 0, 'Redo unlink');
  await binding().selectOption(slotId);
  await assertCoverage(slotId, otherSlotId, 29, 'rebind');
  await assertAssetRetained();

  const beforeLayout = await saved();
  for (const width of [1440, 1000, 900]) {
    await page.setViewportSize({ width, height: 960 });
    await page.waitForTimeout(80);
    const geometry = await page.evaluate(() => {
      const rect = (selector) => {
        const r = document.querySelector(selector).getBoundingClientRect();
        return { x: r.x, right: r.right, width: r.width, height: r.height };
      };
      return {
        colors: rect('.shared-colors-panel'),
        canvas: rect('.canvas-stage'),
        inspector: rect('.inspector'),
        overflow: document.documentElement.scrollWidth > innerWidth,
      };
    });
    assert.equal(geometry.overflow, false, `no document overflow at ${width}`);
    assert.ok(geometry.colors.width > 0 && geometry.colors.height >= 76);
    assert.ok(
      geometry.colors.x >= geometry.canvas.right - 1,
      `colors do not cover artwork at ${width}`,
    );
    assert.ok(geometry.canvas.width > 0 && geometry.inspector.height >= 120);
    assert.equal(await launcher.isVisible(), true);
    assert.equal(await binding().isVisible(), true);
  }
  assert.equal(await saved(), beforeLayout);
  await page.screenshot({ path: resolve(output, 'narrow.png'), fullPage: true });
  await page.setViewportSize({ width: 1440, height: 960 });

  // Real browser download + file input reopen; native Save/Open dialogs are not covered.
  const expectedProject = await snapshot();
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(output, 'shared-colors.json');
  await (await downloading).saveAs(file);
  const contents = await readFile(file, 'utf8');
  assert.deepEqual(JSON.parse(contents), expectedProject);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  const staleBinding = await binding().elementHandle();
  const previousSession = await page.evaluate(() => window.nect.commandState.sessionId);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (sessionId) => window.nect.commandState.sessionId !== sessionId,
    previousSession,
  );
  assert.deepEqual(await snapshot(), expectedProject);
  await inspectTarget();
  assert.equal(await binding().inputValue(), slotId);
  await shapeHasColor('#2468ab', 0.25);
  await assertCoverage(slotId, otherSlotId, 29, 'browser reopen');

  // A retained old control is explicitly reattached to test the actual delegated stale-session guard.
  const reopened = await saved();
  await page.evaluate((el) => {
    document.querySelector('#app').append(el);
    el.value = '';
    el.dispatchEvent(new Event('change', { bubbles: true }));
  }, staleBinding);
  assert.equal(await saved(), reopened, 'an old document control must not unlink the new document');
  await page.waitForFunction(() =>
    document.querySelector('.status')?.textContent.includes('stale'),
  );
  await staleBinding.dispose();
  await assertCoverage(slotId, otherSlotId, 29, 'stale unlink rejected');
  await assertAssetRetained();
  // Real preview uses the current shared literals and commits one atomic batch.
  const beforeChroma = await saved();
  const beforeChromaState = await page.evaluate(() => window.nect.commandState);
  await page.locator('[data-chroma-profile]').selectOption('soft');
  assert.equal(await page.locator('[data-chroma-profile]').inputValue(), 'soft');
  await page.locator(`[data-chroma-lock="${otherSlotId}"]`).check();
  await page.locator('[data-chroma-preview]').click();
  assert.match(
    await page.locator('[data-chroma-area-status]').textContent(),
    /current visible area/,
  );
  const proposed = await page.locator('.chroma-preview output').allTextContents();
  assert.equal(await saved(), beforeChroma, 'preview is not an authored edit');
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeChromaState);
  assert.equal(proposed.length, 2);
  await page.setViewportSize({ width: 900, height: 960 });
  await page.locator('[data-chroma-apply]').scrollIntoViewIfNeeded();
  const previewBounds = await page.locator('[data-chroma-apply]').evaluate((el) => {
    const button = el.getBoundingClientRect();
    const panel = el.closest('.shared-colors-panel').getBoundingClientRect();
    return {
      visible: button.top >= panel.top && button.bottom <= panel.bottom,
      viewportWidth: window.innerWidth,
    };
  });
  assert.equal(previewBounds.visible, true, 'preview Apply is inside the scrollable panel');
  assert.equal(previewBounds.viewportWidth, 900);
  await page.screenshot({ path: resolve(output, 'chroma-preview-narrow.png'), fullPage: true });
  await page.locator('[data-chroma-cancel]').click();
  assert.equal(await saved(), beforeChroma);
  assert.equal(await page.locator('[data-chroma-apply]').count(), 0);
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  const chromaDoc = await documentSnapshot();
  const changedColor = chromaDoc.colorSlots.find((slot) => slot.id === slotId).value;
  assert.notEqual(changedColor.hex, '#2468ab');
  assert.equal(changedColor.alpha, 0.25);
  assert.equal(chromaDoc.colorSlots.find((slot) => slot.id === otherSlotId).value.hex, '#808080');
  await shapeHasColor(changedColor.hex, 0.25);
  const afterChroma = await saved();
  assert.equal(await page.locator('[data-chroma-apply]').count(), 0);
  await activateCommand(page, 'undo');
  assert.equal(await saved(), beforeChroma, 'one Undo restores the full palette');
  await activateCommand(page, 'redo');
  assert.equal(await saved(), afterChroma);
  await activateCommand(page, 'undo');
  await assertAssetRetained();
  await page.locator('[data-chroma-preview]').click();
  await page.getByRole('button', { name: 'Close Shared colors', exact: true }).click();
  assert.equal(await page.locator('.shared-colors-panel').count(), 0);
  assert.equal(await launcher.getAttribute('aria-pressed'), 'false');
  assert.equal(await launcher.evaluate((el) => el === document.activeElement), true);
  assert.equal(await saved(), reopened);
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(output, 'result.json'),
    JSON.stringify(
      {
        ok: true,
        scope: 'shared colors and transient saturation preview; recipe and contrast guards pending',
        chromaPreviewCancelApplyAtomicHistory: true,
        currentVisibleAreaWeightsPreview: true,
        actualPanelAndInspector: true,
        exactBindingAndUnlinkMaterialization: true,
        sharedEditAndUnlinkUndoRedo: true,
        browserDownloadAndReopen: true,
        staleSessionRejected: true,
        assetDomAndSearchRetained: true,
        visibleAreaCoverage: coverageEvidence,
        rendererAlphaOracle: { halfAlphaShare, quarterAlphaShare },
        coverageDoesNotAuthorState: true,
        viewportWidths: [1440, 1000, 900],
        nativeDialogs: 'NOT_RUN',
        errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'PASS shared manual colors: panel/Inspector, exact bind/unlink, live color, history, browser save/reopen, stale controls, retained Assets and narrow layout',
  );
} catch (error) {
  await page.screenshot({ path: resolve(output, 'failure.png'), fullPage: true });
  await writeFile(
    resolve(output, 'failure.json'),
    JSON.stringify({ error: error.message, errors, project: await snapshot() }, null, 2),
  );
  throw error;
} finally {
  await browser.close();
}
