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
// Keep these captures separate from the wheel-at-scrollTop=0 fixtures below.
// Bounds alone do not prove visibility inside the panel's nested scrollports.
const captureContrastReport = async (mode, label, expectedPairs) => {
  const report = page.locator(`[data-palette-contrast="${mode}"]`);
  assert.equal(await report.count(), 1);
  await report.scrollIntoViewIfNeeded();
  const layout = await report.evaluate(async (el) => {
    await document.fonts.ready;
    await new Promise(requestAnimationFrame);
    const panel = el.closest('.shared-colors-panel');
    const rect = (node) => {
      const { left, top, right, bottom, width, height } = node.getBoundingClientRect();
      return { left, top, right, bottom, width, height };
    };
    const port = { left: 0, top: 0, right: innerWidth, bottom: innerHeight };
    for (let parent = el.parentElement; parent; parent = parent.parentElement) {
      const style = getComputedStyle(parent),
        bounds = parent.getBoundingClientRect();
      if (/(auto|scroll|hidden|clip)/.test(style.overflowX)) {
        port.left = Math.max(port.left, bounds.left + parent.clientLeft);
        port.right = Math.min(port.right, bounds.left + parent.clientLeft + parent.clientWidth);
      }
      if (/(auto|scroll|hidden|clip)/.test(style.overflowY)) {
        port.top = Math.max(port.top, bounds.top + parent.clientTop);
        port.bottom = Math.min(port.bottom, bounds.top + parent.clientTop + parent.clientHeight);
      }
    }
    return {
      mode: el.dataset.paletteContrast,
      label: el.getAttribute('aria-label'),
      status: el.dataset.contrastStatus,
      text: el.innerText,
      bounds: rect(el),
      visibleScrollport: port,
      viewport: { width: innerWidth, height: innerHeight },
      panelScrollTop: panel.scrollTop,
      panelClientWidth: panel.clientWidth,
      panelScrollWidth: panel.scrollWidth,
      reportClientWidth: el.clientWidth,
      reportScrollWidth: el.scrollWidth,
      pairCount: el.querySelectorAll('[data-contrast-text-id]').length,
      pairs: [...el.querySelectorAll('[data-contrast-text-id]')].map((row) => ({
        textId: row.dataset.contrastTextId,
        backgroundId: row.dataset.contrastBackgroundId,
      })),
      textBlocks: [...el.querySelectorAll('strong, p')].map((node) => {
        const style = getComputedStyle(node),
          range = document.createRange();
        range.selectNodeContents(node);
        return {
          text: node.textContent,
          fontSize: Number.parseFloat(style.fontSize),
          lineHeight: Number.parseFloat(style.lineHeight),
          display: style.display,
          visibility: style.visibility,
          opacity: Number(style.opacity),
          lines: [...range.getClientRects()].map((line) => ({
            left: line.left,
            top: line.top,
            right: line.right,
            bottom: line.bottom,
            // Catch text obscured by another panel, not only clipping.
            unobscured: el.contains(
              document.elementFromPoint(line.left + line.width / 2, line.top + line.height / 2),
            ),
          })),
        };
      }),
    };
  });
  const screenshot = `harmony-contrast-${mode === 'current' ? 'saved' : 'proposed'}.png`,
    png = await page.screenshot({ path: resolve(out, screenshot), fullPage: false });
  layout.screenshot = {
    file: screenshot,
    width: png.readUInt32BE(16),
    height: png.readUInt32BE(20),
  };
  assert.equal(layout.label, `${label} palette contrast`);
  assert.match(layout.text, new RegExp(`${label} normal-text contrast:`));
  assert.match(layout.text, /Text × Background candidates, 4\.5:1/);
  assert.match(layout.text, /Not an artwork or whole-WCAG audit\. No colors are changed\./);
  assert.match(layout.text, /\d+ pass, \d+ fail, \d+ unknown/);
  assert.match(layout.text, /[\d.]+:1 · (pass|fail)/);
  layout.expectedPairs = expectedPairs;
  assert.equal(layout.pairCount, expectedPairs.length, `${label} retains every authored role pair`);
  const pairKeys = (pairs) =>
    pairs.map(({ textId, backgroundId }) => `${textId}/${backgroundId}`).sort();
  assert.deepEqual(
    pairKeys(layout.pairs),
    pairKeys(expectedPairs),
    `${label} rows retain the exact Text/Background stable IDs independent of status ordering`,
  );
  assert.ok(layout.panelScrollTop > 0, `${label} is reached by scrolling the real panel`);
  assert.equal(layout.panelScrollWidth, layout.panelClientWidth, `${label}: no panel overflow`);
  assert.equal(layout.reportScrollWidth, layout.reportClientWidth, `${label}: no report overflow`);
  assert.deepEqual(layout.viewport, { width: 900, height: 720 });
  assert.equal(layout.screenshot.width, 900);
  assert.equal(layout.screenshot.height, 720);
  const inside = (bounds, port, description) => {
    assert.ok(bounds.right > bounds.left && bounds.bottom > bounds.top, `${description} has area`);
    assert.ok(
      bounds.left >= port.left &&
        bounds.top >= port.top &&
        bounds.right <= port.right &&
        bounds.bottom <= port.bottom,
      `${description} is fully visible: ${JSON.stringify({ bounds, port })}`,
    );
  };
  inside(layout.bounds, layout.visibleScrollport, `${label} report`);
  assert.equal(
    layout.textBlocks.length,
    3 + expectedPairs.length,
    `${label} heading, scope, counts and all authored pairs are present`,
  );
  for (const block of layout.textBlocks) {
    assert.ok(block.text.trim(), `${label} text is not empty`);
    assert.notEqual(block.display, 'none');
    assert.equal(block.visibility, 'visible');
    assert.equal(block.opacity, 1);
    assert.ok(block.fontSize >= 10, `${label} text retains the supported readable font size`);
    assert.ok(block.lineHeight >= block.fontSize, `${label} text lines are not compressed`);
    assert.ok(block.lines.length > 0, `${label} text has painted line geometry`);
    for (const line of block.lines) {
      inside(line, layout.bounds, `${label} text line`);
      assert.equal(line.unobscured, true, `${label} text is not covered by another panel`);
    }
  }
  return layout;
};
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
  assert.deepEqual(
    created.colorDesign.palette.map(({ role }) => role),
    ['Primary', 'Secondary', 'Accent', 'Background', 'Text'],
    'the default recipe has one explicitly authored Text/Background pair',
  );
  const defaultSavedContrastStatus = await page
    .locator('[data-palette-contrast="current"]')
    .getAttribute('data-contrast-status');
  assert.equal(defaultSavedContrastStatus, 'pass');
  assert.equal(
    await page.locator('[data-palette-contrast="current"] [data-contrast-text-id]').count(),
    1,
  );
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
  assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 1);
  await page.locator('[data-chroma-cancel]').click();
  assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 0);
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
  // Explicit per-ID Chroma remains a draft until Apply; Auto is the default.
  await page.locator('[data-wheel-entry]').selectOption(primary);
  assert.equal(await page.locator('[data-wheel-chroma-mode]').inputValue(), 'auto');
  await page.locator('[data-wheel-chroma-mode]').selectOption('override');
  await change(page.locator('[data-wheel-chroma]'), 0.03);
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-cancel]').click();
  assert.equal(await encoded(), original, 'cancelled override preview never authors');
  assert.equal(Number(await page.locator('[data-wheel-chroma]').inputValue()), 0.03);
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  const chromaApplied = await snap();
  assert.equal(chromaApplied.schemaVersion, 57);
  assert.equal(chromaApplied.colorDesign.palette.find(({ id }) => id === primary).overrideC, 0.03);
  const overrideHex = chromaApplied.colorSlots.find(({ id }) => id === primary).value.hex;
  assert.equal(
    await marker(primary).locator('[data-wheel-marker-color]').getAttribute('style'),
    `fill:${overrideHex};stroke:none`,
  );
  await page.waitForFunction(
    (hex) => [...document.querySelectorAll('.shape')].some((el) => el.getAttribute('fill') === hex),
    overrideHex,
  );
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), original);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snap(), chromaApplied);
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), original);
  await change(page.locator('[data-wheel-setting="hueRotation"]'), 75);
  await page.locator('[data-wheel-chroma-mode]').selectOption('override');
  await change(page.locator('[data-wheel-chroma]'), 0.04);
  // Keep protects the exact original literal even against an explicit Chroma override.
  await page.locator(`[data-chroma-lock="${primary}"]`).check();
  await page.locator('[data-chroma-preview]').click();
  await page.locator('[data-chroma-apply]').click();
  const rotated = await snap();
  assert.equal(rotated.colorDesign.wheel.hueRotation, 75);
  assert.equal(rotated.colorDesign.palette.find(({ id }) => id === primary).overrideC, 0.04);
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
  assert.equal(added.colorDesign.palette.at(-1).role, 'Background');
  assert.deepEqual(added.colorDesign.palette.slice(0, 5), dragged.colorDesign.palette);
  assert.deepEqual(added.colorSlots.slice(0, 5), dragged.colorSlots);
  const download = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(out, 'harmony-schema57.json');
  await (await download).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), added);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  const old = await page.evaluate(() => window.nect.commandState.sessionId);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction((id) => window.nect.commandState.sessionId !== id, old);
  assert.deepEqual(await snap(), added);
  await page.locator('[data-wheel-entry]').selectOption(primary);
  assert.equal(await page.locator('[data-wheel-chroma-mode]').inputValue(), 'override');
  assert.equal(Number(await page.locator('[data-wheel-chroma]').inputValue()), 0.04);
  await page.setViewportSize({ width: 900, height: 720 });
  await page.locator('[data-wheel-chroma]').scrollIntoViewIfNeeded();
  const chromaControlLayout = await page.locator('[data-wheel-chroma]').evaluate((el) => {
    const panel = el.closest('.shared-colors-panel');
    const { left, right, top, bottom } = el.getBoundingClientRect();
    const port = panel.getBoundingClientRect();
    return {
      id: el.dataset.wheelId,
      value: el.value,
      left,
      right,
      top,
      bottom,
      visible:
        left >= port.left &&
        right <= port.right &&
        top >= port.top &&
        bottom <= port.bottom &&
        bottom <= innerHeight,
      panelClientWidth: panel.clientWidth,
      panelScrollWidth: panel.scrollWidth,
      screenshot: 'harmony-chroma-override.png',
    };
  });
  assert.equal(chromaControlLayout.id, primary);
  assert.equal(chromaControlLayout.visible, true);
  assert.equal(chromaControlLayout.panelScrollWidth, chromaControlLayout.panelClientWidth);
  await page.screenshot({ path: resolve(out, chromaControlLayout.screenshot), fullPage: false });
  await page.locator('[data-wheel-entry]').selectOption(created.colorDesign.palette[4].id);
  assert.equal(await page.locator('[data-wheel-chroma-mode]').count(), 0);
  assert.equal(await page.locator('[data-wheel-chroma-neutral]').textContent(), 'Chroma 0');
  assert.equal(await page.locator('[data-wheel-setting="harmony"]').inputValue(), 'triadic');
  assert.equal(Number(await page.locator('[data-wheel-setting="hueRotation"]').inputValue()), 75);
  const visibleLayouts = [];
  for (const height of [960, 720]) {
    await page.setViewportSize({ width: 900, height });
    const layout = await page.locator('.shared-colors-panel').evaluate(async (panel) => {
      panel.scrollTop = 0;
      panel.scrollLeft = 0;
      await new Promise(requestAnimationFrame);
      const rect = (el) => {
        const { left, top, right, bottom, width, height } = el.getBoundingClientRect();
        return { left, top, right, bottom, width, height };
      };
      const scrollport = (el) => {
        const r = el.getBoundingClientRect();
        return {
          left: r.left + el.clientLeft,
          top: r.top + el.clientTop,
          right: r.left + el.clientLeft + el.clientWidth,
          bottom: r.top + el.clientTop + el.clientHeight,
        };
      };
      // Intersect with the viewport and every clipping ancestor, not just the
      // element's boundingBox: an offscreen/clipped element still has a box.
      const visibleScrollport = (el) => {
        const visible = scrollport(el);
        visible.left = Math.max(visible.left, 0);
        visible.top = Math.max(visible.top, 0);
        visible.right = Math.min(visible.right, innerWidth);
        visible.bottom = Math.min(visible.bottom, innerHeight);
        for (let parent = el.parentElement; parent; parent = parent.parentElement) {
          const style = getComputedStyle(parent),
            bounds = scrollport(parent);
          if (/(auto|scroll|hidden|clip)/.test(style.overflowX)) {
            visible.left = Math.max(visible.left, bounds.left);
            visible.right = Math.min(visible.right, bounds.right);
          }
          if (/(auto|scroll|hidden|clip)/.test(style.overflowY)) {
            visible.top = Math.max(visible.top, bounds.top);
            visible.bottom = Math.min(visible.bottom, bounds.bottom);
          }
        }
        return visible;
      };
      return {
        viewport: { width: innerWidth, height: innerHeight },
        scrollTop: panel.scrollTop,
        panelClientWidth: panel.clientWidth,
        panelScrollWidth: panel.scrollWidth,
        panelScrollport: visibleScrollport(panel),
        header: rect(panel.querySelector('header')),
        wheel: rect(panel.querySelector('.harmony-wheel')),
        markers: [...panel.querySelectorAll('[data-wheel-marker]')].map((el) => {
          const bounds = rect(el),
            circle = el.querySelector('circle'),
            circleBounds = rect(circle),
            matrix = circle.getScreenCTM(),
            halfStroke = Number.parseFloat(getComputedStyle(circle).strokeWidth) / 2,
            strokeX = halfStroke * Math.hypot(matrix.a, matrix.c),
            strokeY = halfStroke * Math.hypot(matrix.b, matrix.d);
          // SVG getBoundingClientRect excludes the painted stroke. Include it
          // so even a marker's outermost edge must remain visible.
          return {
            id: el.dataset.wheelMarker,
            left: Math.min(bounds.left, circleBounds.left - strokeX),
            top: Math.min(bounds.top, circleBounds.top - strokeY),
            right: Math.max(bounds.right, circleBounds.right + strokeX),
            bottom: Math.max(bounds.bottom, circleBounds.bottom + strokeY),
          };
        }),
        inspectorScrollport: visibleScrollport(document.querySelector('.inspector')),
      };
    });
    const screenshot = height === 960 ? 'harmony-narrow.png' : 'harmony-narrow-short.png';
    // A viewport screenshot preserves the actual 900 px width; fullPage could
    // expand the image and disguise horizontal overflow or offscreen content.
    const png = await page.screenshot({ path: resolve(out, screenshot), fullPage: false });
    layout.screenshot = {
      file: screenshot,
      width: png.readUInt32BE(16),
      height: png.readUInt32BE(20),
    };
    visibleLayouts.push(layout);
    await writeFile(resolve(out, 'visible-layouts.json'), JSON.stringify(visibleLayouts, null, 2));
    const context = `at 900×${height}`,
      port = layout.panelScrollport,
      insidePanel = (bounds, label) => {
        assert.ok(
          bounds.right > bounds.left && bounds.bottom > bounds.top,
          `${label} has area ${context}`,
        );
        assert.ok(
          bounds.left >= port.left &&
            bounds.top >= port.top &&
            bounds.right <= port.right &&
            bounds.bottom <= port.bottom,
          `${label} fits the visible Shared colors scrollport ${context}: ${JSON.stringify({ bounds, port })}`,
        );
      };
    assert.deepEqual(layout.viewport, { width: 900, height });
    assert.equal(layout.screenshot.width, 900, `screenshot keeps viewport width ${context}`);
    assert.equal(layout.screenshot.height, height, `screenshot keeps viewport height ${context}`);
    assert.equal(layout.scrollTop, 0, `wheel is visible without scrolling ${context}`);
    assert.equal(
      layout.panelScrollWidth,
      layout.panelClientWidth,
      `Shared colors has no horizontal overflow ${context}`,
    );
    insidePanel(layout.header, 'Shared colors header');
    insidePanel(layout.wheel, 'Entire harmony wheel');
    assert.equal(layout.markers.length, 3, `all three chromatic markers remain present ${context}`);
    for (const bounds of layout.markers) insidePanel(bounds, `Painted marker ${bounds.id}`);
    const inspector = layout.inspectorScrollport;
    assert.ok(inspector.right > inspector.left, `Inspector retains visible width ${context}`);
    assert.ok(
      inspector.bottom - inspector.top >= 100,
      `Inspector retains at least 100 px of accessible viewport ${context}: ${JSON.stringify(inspector)}`,
    );
  }
  // Preserve the 900×960/720 wheel screenshots above; now scroll to the
  // real saved/proposed reports instead of mistaking the wheel for contrast evidence.
  // Neutral Add is a second Background, so the reopened authored recipe has
  // two candidates. Derive their Cartesian product from roles, never UI rows.
  const contrastRecipe = added.colorDesign.palette,
    expectedContrastPairs = contrastRecipe
      .filter(({ role }) => role === 'Text')
      .flatMap(({ id: textId }) =>
        contrastRecipe
          .filter(({ role }) => role === 'Background')
          .map(({ id: backgroundId }) => ({ textId, backgroundId })),
      );
  assert.deepEqual(expectedContrastPairs, [
    { textId: created.colorDesign.palette[4].id, backgroundId: created.colorDesign.palette[3].id },
    { textId: created.colorDesign.palette[4].id, backgroundId: contrastRecipe.at(-1).id },
  ]);
  const beforeContrastPreview = await encoded(),
    savedContrast = await captureContrastReport('current', 'Saved', expectedContrastPairs);
  await page.locator('[data-chroma-preview]').click();
  assert.equal(
    await encoded(),
    beforeContrastPreview,
    'contrast preview does not edit the project',
  );
  const proposedContrast = await captureContrastReport(
    'preview',
    'Proposed',
    expectedContrastPairs,
  );
  await page.locator('[data-chroma-cancel]').click();
  assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 0);
  assert.equal(await encoded(), beforeContrastPreview, 'Cancel preserves the saved palette');
  assert.equal(
    await page.locator('[data-palette-contrast="current"]').innerText(),
    savedContrast.text,
    'Cancel retains the saved contrast report',
  );
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
        schemaVersion: 57,
        perColorChroma: {
          defaultAuto: true,
          explicitDraftApplyCancel: true,
          stableIdAndRenderedArtwork: true,
          keepTakesPrecedence: true,
          neutralProtected: true,
          undoRedoAndSaveReopen: true,
          controlLayout: chromaControlLayout,
        },
        viewportWidth: 900,
        visibleLayouts,
        contrast: {
          defaultSavedStatus: defaultSavedContrastStatus,
          previewDoesNotEditProject: true,
          cancelPreservesSavedPaletteAndReport: true,
          visibleGeometryAndReadableText: true,
          noHorizontalOverflow: true,
          reports: [savedContrast, proposedContrast],
        },
        nativeDialogs: 'NOT_RUN',
        errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'PASS harmony wheel: real UI, geometry, radial draft, history, stable IDs, browser save/reopen, visible saved/proposed contrast',
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
