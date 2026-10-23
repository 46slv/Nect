import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { textWrapCanvasFixture } from './fixtures/text-wrap-canvas.js';
import { inverseAffineDelta, inverseAffinePoint } from '../src/character-geometry.js';

// Task-owned Tauri/WebView2 renderer. CDP input is synthetic, not physical input.
const output = path.resolve(
  process.env.NECT_TEXT_WRAP_OUTPUT || 'test-results/text-wrap-canvas-resize/native',
);
const references = (process.env.NECT_TEXT_WRAP_REFERENCES || 'start,center,end').split(',');
assert.ok(references.length && references.every((r) => ['start', 'center', 'end'].includes(r)));
const cases = process.env.NECT_TEXT_WRAP_CASES
  ? JSON.parse(process.env.NECT_TEXT_WRAP_CASES)
  : references.flatMap((alignment) =>
      [false, true].map((transformed) => ({ alignment, crossAlignment: 'origin', transformed })),
    );
assert.ok(
  cases.length &&
    cases.every(
      (c) =>
        ['start', 'center', 'end'].includes(c.alignment) &&
        ['origin', 'start', 'center', 'end'].includes(c.crossAlignment) &&
        (c.rotation === undefined || Number.isFinite(c.rotation)) &&
        (c.zoom === undefined || [100, 120].includes(c.zoom)) &&
        typeof c.transformed === 'boolean',
    ),
);
const widthDeltas = JSON.parse(process.env.NECT_TEXT_WRAP_WIDTH_DELTAS || '[-100,100]');
assert.ok(
  widthDeltas.length &&
    widthDeltas.every((d) => Number.isFinite(d) && 320 + d > 0 && 320 + d <= 100000),
);
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
  const frameReference = () =>
    $('.text-wrap-resize-overlay polygon')
      .getAttribute('points')
      .then((s) => {
        const worldPoints = s.split(' ').map((p) => p.split(',').map(Number));
        const points = worldPoints.map((p) => inverseAffinePoint(artworkMatrix, p));
        return {
          x:
            points[0][0] +
            (points[1][0] - points[0][0]) * { start: 0, center: 0.5, end: 1 }[reference],
          top: points[0][1],
          height: points[3][1] - points[0][1],
          y:
            points[0][1] +
            (points[3][1] - points[0][1]) *
              { origin: 0, start: 0, center: 0.5, end: 1 }[crossReference],
          points,
          worldPoints,
        };
      });
  const checkReference = async (before) => {
    const now = await frameReference();
    assert.ok(Math.abs(now.x - before.x) < 1e-9, 'frame reference fixed at unchanged Position');
    assert.ok(Math.abs(now.y - before.y) < 1e-9, 'Y frame reference fixed at unchanged Position');
    const fy = { origin: 0, start: 0, center: 0.5, end: 1 }[crossReference];
    assert.ok(Math.abs(now.top - before.top + (now.height - before.height) * fy) < 1e-9);
    if (['center', 'end'].includes(crossReference))
      assert.notEqual(now.height, before.height, 'representative reflow changes auto height');
    assert.ok(now.points[0][0] < now.points[1][0], 'frame must not flip');
  };
  let fixture,
    reference = 'start',
    crossReference = 'origin',
    activeCase = {},
    artworkMatrix = [1, 0, 0, 1, 0, 0];
  const deltaFactor = () => ({ start: 1, center: 2, end: -1 })[reference];
  const load = async (transformed, zoomIn = false) => {
    fixture = textWrapCanvasFixture({
      transformed,
      alignment: reference,
      referenceBounds: transformed ? 'visible' : 'advance',
      crossAlignment: crossReference,
      rotation: activeCase.rotation || 0,
      transformAnchor: activeCase.transformAnchor,
      transformPosition: activeCase.transformPosition,
    });
    await page.evaluate((s) => window.nect.load(s), serialize(fixture.doc));
    if (await $('.alignment-panel').count())
      await $('.canvas-toolbar [data-action="align-panel"]').click();
    await $('[data-action="graph-reset"]').click();
    await $(`.node[data-id="${fixture.textId}"] .type-tag`).click();
    if (transformed) await $(`[data-select-layer="${fixture.layerId}"]`).click();
    artworkMatrix = transformed
      ? await page.evaluate(
          (id) => window.nect.evaluate().transformValues(id).matrix,
          fixture.transformId,
        )
      : [1, 0, 0, 1, 0, 0];
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
  const begin = async (widthDelta) => {
    const h = await handle().boundingBox();
    const x = h.x + h.width / 2,
      y = h.y + h.height / 2;
    const ctm = await page.evaluate(() => {
      const m = document.querySelector('.artboard').getScreenCTM();
      return [m.a, m.b, m.c, m.d, m.e, m.f];
    });
    const scale = ctm[0];
    const worldAxis = [artworkMatrix[0], artworkMatrix[1]];
    const screenAxis = [
      ctm[0] * worldAxis[0] + ctm[2] * worldAxis[1],
      ctm[1] * worldAxis[0] + ctm[3] * worldAxis[1],
    ];
    const screenAxisLength = Math.hypot(...screenAxis);
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
    await page.mouse.move(
      x + (widthDelta / deltaFactor()) * screenAxis[0],
      y + (widthDelta / deltaFactor()) * screenAxis[1],
      {
        steps: 6,
      },
    );
    await settle();
    return { scale, x, y, ctm, screenAxisLength };
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

  for (const fixtureCase of cases) {
    activeCase = fixtureCase;
    reference = fixtureCase.alignment;
    crossReference = fixtureCase.crossAlignment;
    const transformed = fixtureCase.transformed;
    const zoomIn = (fixtureCase.zoom ?? (transformed ? 120 : 100)) === 120,
      zoomLabel = zoomIn ? 120 : 100;
    for (const widthDelta of widthDeltas) {
      await load(transformed, zoomIn);
      const before = await checkpoint(),
        identity = await selection(),
        origin = await frameReference();
      const canvas = await $('.artboard').innerHTML();
      const { scale, ctm, screenAxisLength } = await begin(widthDelta);
      assert.deepEqual(
        await checkpoint(),
        before,
        'preview preserves committed JSON/SVG/history/file state',
      );
      assert.notEqual(await $('.artboard').innerHTML(), canvas);
      await checkReference(origin);
      await graphUntouched();
      await page.screenshot({
        path: path.join(
          output,
          `preview-${reference}-${crossReference}-${zoomLabel}-${widthDelta < 0 ? 'shrink' : 'grow'}.png`,
        ),
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
      const localDelta = inverseAffineDelta(
        artworkMatrix,
        input[1].world.map((v, i) => v - input[0].world[i]),
      );
      const inputWidth = 320 + localDelta[0] * deltaFactor();
      assert.ok(
        Math.abs(width - inputWidth) < 1e-9,
        'exact inverse of received pointer coordinates',
      );
      // Native/CDP client coordinates have subpixel quantization. Keep that input
      // difference separate from the exact source-coordinate conversion above.
      assert.ok(
        (Math.abs(width - (320 + widthDelta)) / Math.abs(deltaFactor())) * screenAxisLength <
          1 / 1024,
      );
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
        path.join(
          output,
          `committed-${reference}-${crossReference}-${zoomLabel}-${widthDelta < 0 ? 'shrink' : 'grow'}.json`,
        ),
        committed.json,
      );
      await fs.writeFile(
        path.join(
          output,
          `committed-${reference}-${crossReference}-${zoomLabel}-${widthDelta < 0 ? 'shrink' : 'grow'}.svg`,
        ),
        committed.svg,
      );
      await page.evaluate((s) => window.nect.load(s), before.json);
      await $(`.node[data-id="${fixture.textId}"] .type-tag`).click();
      await settle();
      const numeric = $('.inspector-body [data-text-wrap-width]');
      await numeric.fill(String(width));
      await numeric.press('Tab');
      await settle();
      const numericState = await checkpoint();
      assert.equal(
        numericState.json,
        committed.json,
        'native numeric edit matches Canvas document exactly',
      );
      assert.equal(
        numericState.svg,
        committed.svg,
        'native numeric edit matches Canvas export exactly',
      );
      results.push({
        case: 'commit/Undo/Redo/export/JSON reload/numeric equivalence',
        reference,
        crossReference,
        referenceBounds: transformed ? 'visible' : 'advance',
        zoom: zoomLabel,
        rotation: activeCase.rotation || 0,
        artworkMatrix: [...artworkMatrix],
        screenAxisLength,
        widthDelta,
        target: transformed ? 'whole artwork static scale 1.5' : 'Text source',
        canvasScale: scale,
        canvasCTM: ctm,
        width,
        inputWidth,
        input,
        fixedFrameReference: { x: origin.x, y: origin.y, space: 'node-local' },
        frameStart: { top: origin.top, height: origin.height },
        widthOnlyDelta: true,
        graphPreviewMutations: 0,
      });
    }
    for (const route of ['Escape', 'pointercancel', 'blur']) {
      await load(transformed, zoomIn);
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
        reference,
        crossReference,
        zoom: zoomLabel,
        target: transformed ? 'whole artwork' : 'Text source',
      });
    }
    if (transformed && fixtureCase.clamp !== false) {
      await load(true, zoomIn);
      const before = await checkpoint(),
        origin = await frameReference();
      await begin(-330);
      await checkReference(origin);
      const frame = await frameReference(),
        h = await handle().boundingBox();
      assert.equal(
        await page.evaluate(() => window.nect.document.layers[0].nodes[0].textWrap.width),
        320,
        'clamp stays a detached preview',
      );
      // The active handle stays on the original side of the fixed reference.
      const handleWorld = await handle().evaluate((e) =>
        e.getAttribute('transform').slice(10, -1).split(' ').map(Number),
      );
      const handleX = inverseAffinePoint(artworkMatrix, handleWorld)[0];
      assert.ok(Math.abs(handleX - frame.points[reference === 'end' ? 0 : 1][0]) < 1e-9);
      assert.ok(h.width > 0);
      await page.mouse.up();
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
      await $('[data-action="undo"]').click();
      await settle();
      assert.equal((await checkpoint()).json, before.json);
      assert.equal((await checkpoint()).svg, before.svg);
      results.push({
        case: 'crossing Position clamps at minimum without flip; overflow/Undo',
        reference,
        crossReference,
        zoom: zoomLabel,
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
  // Skew remains outside the rotated Text width capsule.
  fixture = textWrapCanvasFixture({
    transformed: true,
    alignment: reference,
    crossAlignment: crossReference,
    rotation: activeCase.rotation || 0,
    skew: 10,
  });
  await page.evaluate((s) => window.nect.load(s), serialize(fixture.doc));
  await $(`[data-select-layer="${fixture.layerId}"]`).click();
  await settle();
  assert.equal(await handle().count(), 0);
  results.push({ case: 'skewed Text whole artwork hides width handle without mutation' });
  assert.deepEqual(errors, []);
  const receipt = {
    input: 'CDP synthetic mouse/keyboard and renderer cancel events in native Tauri/WebView2',
    viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
    results,
    pageErrors: errors,
    notRun: [
      'physical mouse/touch/pen',
      'actual OS focus loss',
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
