import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { textWrapCanvasFixture } from './fixtures/text-wrap-canvas.js';

// Task-owned Tauri/WebView2 renderer. CDP input is synthetic, not physical input.
const output = path.resolve('test-results/text-wrap-canvas-resize/native');
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_TEXT_WRAP_CDP_URL || 'http://127.0.0.1:9460',
);
const results = [],
  errors = [];
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  const [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  await page.waitForTimeout(2000);
  await page.evaluate(() => {
    for (const type of ['pointerdown', 'pointermove'])
      if (window.wrapPointerReader)
        document.removeEventListener(type, window.wrapPointerReader, true);
    window.wrapPointerReader = (e) => {
      const m = document.querySelector('.artboard').getScreenCTM();
      const p = new DOMPoint(e.clientX, e.clientY).matrixTransform(m.inverse());
      window.wrapInputTrace?.push({
        type: e.type,
        client: [e.clientX, e.clientY],
        world: [p.x, p.y],
      });
    };
    for (const type of ['pointerdown', 'pointermove'])
      document.addEventListener(type, window.wrapPointerReader, true);
  });
  const settle = () => page.waitForTimeout(160);
  const $ = (s) => page.locator(s);
  const handle = () => $('[data-text-wrap-resize]');
  const checkpoint = () =>
    page.evaluate(() => ({
      json: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      file: window.nect.fileState,
      command: window.nect.commandState,
      undoDisabled: document.querySelector('[data-action="undo"]').disabled,
      redoDisabled: document.querySelector('[data-action="redo"]').disabled,
    }));
  const selection = () =>
    page.evaluate(() => ({
      graph: [...document.querySelectorAll('.node.selected')].map((e) => e.dataset.id),
      artwork: [...document.querySelectorAll('[data-artwork-selection]')].map(
        (e) => e.dataset.artworkSelection,
      ),
    }));
  const leftTop = () =>
    $('.text-wrap-resize-overlay polygon')
      .getAttribute('points')
      .then((s) => s.split(' ')[0]);
  let fixture;
  const load = async (transformed, zoomIn = false) => {
    fixture = textWrapCanvasFixture({ transformed });
    await page.evaluate((s) => window.nect.load(s), serialize(fixture.doc));
    if (await $('.alignment-panel').count())
      await $('.canvas-toolbar [data-action="align-panel"]').click();
    await $('[data-action="graph-reset"]').click();
    await $(`.node[data-id="${fixture.textId}"] .type-tag`).click();
    if (transformed) await $(`[data-select-layer="${fixture.layerId}"]`).click();
    await $('[data-action="actual-size"]').click();
    if (zoomIn) await $('[data-action="zoom-in"]').click();
    await settle();
    assert.equal(await handle().count(), 1);
    // Pan with the existing Canvas gesture if the handle needs bringing into view.
    const h = await handle().boundingBox(),
      stage = await $('.canvas-stage').boundingBox();
    const hx = h.x + h.width / 2,
      hy = h.y + h.height / 2;
    const dx = stage.x + stage.width / 2 - hx,
      dy = stage.y + stage.height / 2 - hy;
    await $('.canvas-stage').focus();
    await page.keyboard.down('Space');
    await page.mouse.move(stage.x + stage.width / 2, stage.y + stage.height / 2);
    await page.mouse.down();
    await page.mouse.move(stage.x + stage.width / 2 + dx, stage.y + stage.height / 2 + dy, {
      steps: 3,
    });
    await page.mouse.up();
    await page.keyboard.up('Space');
    await settle();
  };
  const begin = async (deltaLocal) => {
    const h = await handle().boundingBox();
    const x = h.x + h.width / 2,
      y = h.y + h.height / 2;
    const scale = await page.evaluate(() => document.querySelector('.artboard').getScreenCTM().a);
    await page.mouse.move(x, y);
    await page.evaluate(() => (window.wrapInputTrace = []));
    await page.mouse.down();
    await page.evaluate(() => {
      window.wrapGraph = document.querySelector('.graph');
      window.wrapInspector = document.querySelector('.inspector');
      window.wrapMutations = 0;
      window.wrapObserver?.disconnect();
      window.wrapObserver = new MutationObserver((rs) => (window.wrapMutations += rs.length));
      window.wrapObserver.observe(window.wrapGraph, {
        subtree: true,
        attributes: true,
        childList: true,
      });
    });
    await page.mouse.move(x + deltaLocal * scale * (fixture.transformId ? 1.5 : 1), y, {
      steps: 6,
    });
    await settle();
    return { scale, x, y };
  };
  const graphUntouched = async () => {
    assert.deepEqual(
      await page.evaluate(() => ({
        sameGraph: window.wrapGraph === document.querySelector('.graph'),
        sameInspector: window.wrapInspector === document.querySelector('.inspector'),
        mutations: window.wrapMutations,
      })),
      { sameGraph: true, sameInspector: true, mutations: 0 },
    );
    await page.evaluate(() => window.wrapObserver.disconnect());
  };

  for (const transformed of [false, true]) {
    await load(transformed, transformed);
    const before = await checkpoint(),
      identity = await selection(),
      origin = await leftTop();
    const canvas = await $('.artboard').innerHTML();
    const { scale } = await begin(-100);
    assert.deepEqual(
      await checkpoint(),
      before,
      'preview preserves committed JSON/SVG/history/file state',
    );
    assert.notEqual(await $('.artboard').innerHTML(), canvas);
    assert.equal(await leftTop(), origin);
    await graphUntouched();
    await page.screenshot({
      path: path.join(output, `preview-${transformed ? '120' : '100'}.png`),
    });
    await page.mouse.up();
    await settle();
    const committed = await checkpoint(),
      expected = JSON.parse(before.json);
    const width = JSON.parse(committed.json).layers[0].nodes[0].textWrap.width;
    const input = await page.evaluate(() => [
      window.wrapInputTrace[0],
      window.wrapInputTrace.at(-1),
    ]);
    const inputWidth = 320 + (input[1].world[0] - input[0].world[0]) / (transformed ? 1.5 : 1);
    assert.ok(Math.abs(width - inputWidth) < 1e-9, 'exact inverse of received pointer coordinates');
    // Native/CDP client coordinates have subpixel quantization. Keep that input
    // difference separate from the exact source-coordinate conversion above.
    assert.ok(Math.abs(width - 220) * scale * (transformed ? 1.5 : 1) < 1 / 1024);
    expected.layers[0].nodes[0].textWrap.width = width;
    assert.deepEqual(JSON.parse(committed.json), expected, 'width is the sole authored delta');
    assert.notEqual(committed.svg, before.svg);
    assert.deepEqual(await selection(), identity);
    assert.equal(committed.undoDisabled, false);
    await $('[data-action="undo"]').click();
    await settle();
    let restored = await checkpoint();
    assert.equal(restored.json, before.json);
    assert.equal(restored.svg, before.svg);
    assert.equal(restored.undoDisabled, true, 'one drag is one Undo');
    await $('[data-action="redo"]').click();
    await settle();
    restored = await checkpoint();
    assert.equal(restored.json, committed.json);
    assert.equal(restored.svg, committed.svg);
    // Save JSON/reload through the supported document API; no OS dialog/cold-reopen claim.
    await page.evaluate((s) => window.nect.load(s), committed.json);
    await settle();
    assert.equal((await checkpoint()).svg, committed.svg);
    await fs.writeFile(
      path.join(output, `committed-${transformed ? '120' : '100'}.json`),
      committed.json,
    );
    await fs.writeFile(
      path.join(output, `committed-${transformed ? '120' : '100'}.svg`),
      committed.svg,
    );
    results.push({
      case: 'commit/Undo/Redo/export/JSON reload',
      target: transformed ? 'whole artwork static scale 1.5' : 'Text source',
      canvasScale: scale,
      width,
      inputWidth,
      input,
      fixedLeftTop: origin,
      widthOnlyDelta: true,
      graphPreviewMutations: 0,
    });

    for (const route of ['Escape', 'pointercancel', 'blur']) {
      await load(transformed, transformed);
      const start = await checkpoint(),
        selected = await selection(),
        shown = await $('.artboard').innerHTML();
      await begin(-120);
      assert.notEqual(await $('.artboard').innerHTML(), shown);
      if (route === 'Escape') await page.keyboard.press('Escape');
      else if (route === 'pointercancel')
        await $('.canvas-stage').evaluate((e) =>
          e.dispatchEvent(new PointerEvent('pointercancel', { bubbles: true, pointerId: 1 })),
        );
      else await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      await page.mouse.up();
      await settle();
      assert.deepEqual(await checkpoint(), start);
      assert.deepEqual(await selection(), selected);
      assert.equal(await $('.artboard').innerHTML(), shown);
      assert.equal(await page.evaluate(() => window.nect.gestureState), null);
      results.push({
        case: route + ' restores exact committed/visible state',
        target: transformed ? 'whole artwork' : 'Text source',
      });
    }
  }
  // Existing numeric width and overflow route coexist with the new Canvas handle.
  await load(false);
  const widthField = $('.inspector-body [data-text-wrap-width]');
  await widthField.fill('0.01');
  await widthField.press('Tab');
  await settle();
  assert.equal(
    await page.evaluate(() => window.nect.document.layers[0].nodes[0].textWrap.width),
    0.01,
  );
  assert.ok(
    (await page.evaluate(() => window.nect.evaluate().warnings)).some((w) =>
      /Wrap overflow/.test(w),
    ),
  );
  assert.ok((await page.evaluate(() => window.nect.export())).includes('<path'));
  results.push({ case: 'numeric coexistence; overflow warning and outlined SVG retained' });
  await page.screenshot({ path: path.join(output, 'overflow.png') });
  // Whole artwork with unsupported rotation has no width handle.
  fixture = textWrapCanvasFixture({ transformed: true });
  fixture.doc.layers[0].nodes[1].params.rotation.value = 25;
  await page.evaluate((s) => window.nect.load(s), serialize(fixture.doc));
  await $(`[data-select-layer="${fixture.layerId}"]`).click();
  await settle();
  assert.equal(await handle().count(), 0);
  results.push({ case: 'rotated whole artwork hides width handle without mutation' });
  assert.deepEqual(errors, []);
  const receipt = {
    input: 'CDP synthetic mouse/keyboard and renderer cancel events in native Tauri/WebView2',
    viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
    results,
    pageErrors: errors,
    notRun: [
      'physical mouse/touch/pen',
      'OS file dialogs/cold reopen',
      'IME',
      'unsupported transforms/vertical or multiple-Text resize',
    ],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser.close();
}
