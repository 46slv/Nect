// Dedicated native Tauri/WebView2 instance. CDP input is synthetic, not OS input.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { document, guideDefaults, serialize, snapTranslation } from '../src/core.js';

const output = path.resolve(process.env.NECT_GRID_OUTPUT || 'test-results/grid-origin/native');
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(process.env.NECT_GRID_CDP || 'http://127.0.0.1:9472');
const results = [],
  errors = [];
let page;
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  const $ = (selector) => page.locator(selector);
  const settle = () =>
    page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
  const state = () =>
    page.evaluate(() => ({
      doc: window.nect.document,
      svg: window.nect.export(),
      selected: [...document.querySelectorAll('.node.selected')].map((n) => n.dataset.id),
    }));
  const point = (x, y) =>
    $('.artboard').evaluate(
      (el, p) => {
        const q = new DOMPoint(...p).matrixTransform(el.getScreenCTM());
        return [q.x, q.y];
      },
      [x, y],
    );
  const openGrid = async () => {
    if (!(await $('[data-guide-setting="gridAnchor"]').count()))
      await $('[data-action="grid-panel"]').click();
  };
  const configs = [
    { anchor: 'bottom-right', factor: [1, 1], zoomIn: false, pan: [24, 18] },
    { anchor: 'top-center', factor: [0.5, 0], zoomIn: true, pan: [-20, 26] },
    { anchor: 'center', factor: [0.5, 0.5], zoomIn: true, pan: [16, -12] },
  ];
  for (const config of configs) {
    const doc = document();
    doc.name = `NECT_GRID_ACCEPTANCE_${config.anchor}`;
    Object.assign(doc, { width: 961, height: 541 });
    doc.editor = {
      ...guideDefaults(),
      gridVisible: true,
      gridSize: 40,
      snap: true,
      snapGuides: false,
      snapArtboard: false,
    };
    doc.editor.guides.push({ id: 'grid-test-guide', axis: 'x', position: 120, locked: false });
    await page.evaluate((s) => window.nect.load(s), serialize(doc));
    if (await $('[data-guide-setting="gridAnchor"]').count()) await page.keyboard.press('Escape');
    await $('[data-action="actual-size"]').click();
    if (config.zoomIn) await $('[data-action="zoom-in"]').click();
    const stage = await $('.canvas-stage').boundingBox();
    await page.mouse.move(stage.x + stage.width / 2, stage.y + stage.height / 2);
    await page.mouse.wheel(...config.pan);
    await settle();
    const before = await state();
    await openGrid();
    assert.equal(await $('[data-guide-setting="gridAnchor"] option').count(), 9);
    await $('[data-guide-setting="gridAnchor"]').selectOption(config.anchor);
    await settle();
    assert.equal(await $('[data-guide-setting="gridAnchor"]').inputValue(), config.anchor);
    const changed = await state(),
      origin = [961 * config.factor[0], 541 * config.factor[1]];
    assert.equal(changed.doc.schemaVersion, config.anchor === 'center' ? 27 : 48);
    assert.equal(changed.doc.editor.gridAnchor, config.anchor);
    assert.deepEqual(changed.doc.layers, before.doc.layers);
    assert.deepEqual(changed.doc.editor.guides, before.doc.editor.guides);
    assert.deepEqual(changed.selected, before.selected);
    assert.equal(changed.svg, before.svg);
    assert.equal(
      await $('#editing-grid').getAttribute('patternTransform'),
      `translate(${origin[0]} ${origin[1]})`,
    );
    await $('[data-action="undo"]').click();
    assert.deepEqual(
      (await state()).doc,
      before.doc,
      'one Undo restores the whole pre-origin document',
    );
    assert.equal(await $('[data-action="undo"]').isDisabled(), true);
    await $('[data-action="redo"]').click();
    assert.deepEqual(await state(), changed);
    await page.evaluate((s) => window.nect.load(s), JSON.stringify(changed.doc));
    assert.deepEqual((await state()).doc, changed.doc);
    assert.equal((await state()).svg, before.svg);
    // Debug JSON load deliberately resets the view to Fit; restore this case's view.
    await $('[data-action="actual-size"]').click();
    if (config.zoomIn) await $('[data-action="zoom-in"]').click();
    await page.mouse.move(stage.x + stage.width / 2, stage.y + stage.height / 2);
    await page.mouse.wheel(...config.pan);
    await settle();
    // Drag the existing guide close to a visible grid line in source coordinates.
    const targetX = origin[0] + Math.round((200 - origin[0]) / 40) * 40;
    const screenScale = await $('.artboard').evaluate((el) => el.getScreenCTM().a);
    const expectedSnap = snapTranslation(
      changed.doc,
      [[120], []],
      [targetX - 120 - 2, 0],
      screenScale,
      false,
      'grid-test-guide',
    );
    assert.equal(expectedSnap.matches[0].position, targetX);
    await page.mouse.move(...(await point(120, 85)));
    await page.mouse.down();
    await page.mouse.move(...(await point(targetX - 2, 85)), { steps: 3 });
    await page.mouse.up();
    await settle();
    const snapped = await state();
    assert.equal(snapped.doc.editor.guides[0].position, targetX);
    assert.equal(snapped.svg, before.svg);
    assert.deepEqual(snapped.doc.layers, before.doc.layers);
    await $('[data-action="undo"]').click();
    assert.deepEqual((await state()).doc, changed.doc);
    // A cancelled guide gesture does not author an origin or geometry change.
    await page.mouse.move(...(await point(120, 85)));
    await page.mouse.down();
    await page.mouse.move(...(await point(targetX - 2, 85)));
    await page.keyboard.press('Escape');
    await page.mouse.up();
    await settle();
    assert.deepEqual((await state()).doc, changed.doc);
    assert.equal((await state()).svg, changed.svg);
    await openGrid();
    assert.equal(await $('[data-guide-setting="gridAnchor"]').inputValue(), config.anchor);
    const ctm = await $('.artboard').evaluate((el) => {
      const m = el.getScreenCTM();
      return { a: m.a, b: m.b, c: m.c, d: m.d, e: m.e, f: m.f };
    });
    const expectedZoom = config.zoomIn ? 1.2 : 1;
    assert.equal(await $('[data-zoom="canvas"]').textContent(), `${expectedZoom * 100}%`);
    // WebView2 quantizes the SVG's CSS width to 1/64 px; bound that layout error.
    assert.ok(Math.abs(ctm.a - expectedZoom) * doc.width <= 1 / 64);
    await page.screenshot({ path: path.join(output, `${config.anchor}-renderer.png`) });
    await fs.writeFile(
      path.join(output, `${config.anchor}.json`),
      JSON.stringify(changed.doc, null, 2),
    );
    await fs.writeFile(path.join(output, `${config.anchor}.svg`), changed.svg);
    results.push({
      anchor: config.anchor,
      origin,
      zoom: ctm.a,
      panWheel: config.pan,
      ctm,
      snappedGuideX: targetX,
      nativeSelectValue: true,
      oneUndoRedo: true,
      exactJSONReload: true,
      svgAndArtworkUnchanged: true,
      cancelledGuideUnchanged: true,
    });
  }
  assert.deepEqual(errors, []);
  const receipt = {
    input:
      'Native Tauri/WebView2 CDP synthetic select, mouse drag, wheel and Escape; no OS input/dialogs',
    viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
    results,
    pageErrors: errors,
    notRun: ['physical/OS input', 'OS Save/Open/clean cold reopen', 'unrelated full audit'],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt, null, 2));
} catch (error) {
  if (page) {
    const own = await page.evaluate(() => window.nect?.document).catch(() => null);
    if (own?.name?.startsWith('NECT_GRID_ACCEPTANCE_'))
      await fs.writeFile(path.join(output, 'failure-fixture.json'), JSON.stringify(own, null, 2));
    await page.screenshot({ path: path.join(output, 'failure-renderer.png') }).catch(() => {});
  }
  throw error;
} finally {
  await browser.close();
}
