import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';

const out = resolve('test-results/harmony-wheel-panel');
const evidence = resolve('test-results');
await mkdir(out, { recursive: true });
const fixture = blankDocument(),
  layer = fixture.layers[0];
fixture.width = 320;
fixture.height = 240;
const shape = node('Rectangle', 40, 40),
  fill = node('Fill', 340, 40),
  output = node('Output', 640, 40);
shape.params.center.value = [160, 120];
shape.params.size.value = [140, 100];
layer.output = output.id;
layer.nodes = [shape, fill, output];
connect(layer, shape.id, fill.id, 'path');
connect(layer, fill.id, output.id, 'path');
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1366, height: 768 } });
page.setDefaultTimeout(30000);
const errors = [],
  captures = [],
  layouts = [],
  interactions = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const encoded = async () => JSON.stringify(await snapshot());
const preview = () => page.evaluate(() => window.nect.colorPreviewSnapshot());
const commandState = () => page.evaluate(() => window.nect.commandState);
const rotation = () => page.locator('[data-wheel-setting="hueRotation"]');
const marker = (id) => page.locator(`[data-wheel-marker="${id}"]`);
const markerPositions = () =>
  page
    .locator('[data-wheel-marker]')
    .evaluateAll((nodes) =>
      nodes.map((el) => [el.dataset.wheelMarker, el.getAttribute('transform')]),
    );
const change = async (locator, value) => {
  await locator.fill(String(value));
  await locator.press('Tab');
};
const idle = () => page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
const openAdvanced = async () => {
  const details = page.locator('[data-color-advanced]');
  await details.waitFor();
  if (!(await details.evaluate((el) => el.open))) await details.locator('summary').click();
  assert.equal(await details.evaluate((el) => el.open), true);
};
const painted = (hex) =>
  page.waitForFunction(
    (value) =>
      [...document.querySelectorAll('.shape')].some((el) => el.getAttribute('fill') === value),
    hex,
  );
const capture = async (name) => {
  const file = `workflow-color-live-${name}.png`;
  const bytes = await page.screenshot({ path: resolve(evidence, file), fullPage: false });
  captures.push({ file, width: bytes.readUInt32BE(16), height: bytes.readUInt32BE(20) });
};
const undoTo = async (value, description) => {
  await activateCommand(page, 'undo');
  assert.equal(await encoded(), value, description);
  await idle();
};
const pressNumber = async (locator) => {
  await locator.scrollIntoViewIfNeeded();
  const box = await locator.boundingBox();
  assert.ok(box && box.width > 24 && box.height > 0);
  const point = { x: box.x + Math.min(14, box.width / 3), y: box.y + box.height / 2 };
  assert.equal(
    await locator.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, point),
    true,
    `numeric control is reachable at its drag start: ${JSON.stringify({ box, point })}`,
  );
  await page.mouse.move(point.x, point.y);
  await page.mouse.down();
  return point;
};
// Geometry includes every clipping ancestor and a hit test, not merely boundingBox().
const pinnedLayout = async (label) => {
  const result = await page.locator('.shared-colors-panel').evaluate(async (panel) => {
    await document.fonts.ready;
    await new Promise(requestAnimationFrame);
    const bounds = (el) => {
      const { left, top, right, bottom, width, height } = el.getBoundingClientRect();
      return { left, top, right, bottom, width, height };
    };
    const port = (el) => {
      const r = bounds(el),
        result = {
          left: Math.max(0, r.left + el.clientLeft),
          top: Math.max(0, r.top + el.clientTop),
          right: Math.min(innerWidth, r.left + el.clientLeft + el.clientWidth),
          bottom: Math.min(innerHeight, r.top + el.clientTop + el.clientHeight),
        };
      for (let parent = el.parentElement; parent; parent = parent.parentElement) {
        const style = getComputedStyle(parent),
          r = bounds(parent);
        if (/(auto|scroll|hidden|clip)/.test(style.overflowX)) {
          result.left = Math.max(result.left, r.left + parent.clientLeft);
          result.right = Math.min(result.right, r.left + parent.clientLeft + parent.clientWidth);
        }
        if (/(auto|scroll|hidden|clip)/.test(style.overflowY)) {
          result.top = Math.max(result.top, r.top + parent.clientTop);
          result.bottom = Math.min(result.bottom, r.top + parent.clientTop + parent.clientHeight);
        }
      }
      return result;
    };
    const wheel = panel.querySelector('.harmony-wheel'),
      swatches = panel.querySelector('.harmony-palette-swatches');
    const test = (el) => {
      const r = bounds(el),
        hit = document.elementFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);
      return { bounds: r, unobscured: !!hit && (el === hit || el.contains(hit)) };
    };
    return {
      viewport: { width: innerWidth, height: innerHeight },
      panel: bounds(panel),
      port: port(panel),
      wheel: test(wheel),
      swatches: test(swatches),
      neutral: test(panel.querySelector('.harmony-neutral-track')),
      status: bounds(panel.querySelector('.chroma-draft-bar')),
      parameters: bounds(panel.querySelector('.chroma-settings-scroll')),
      parameterScrollTop: panel.querySelector('.chroma-settings-scroll').scrollTop,
      panelScrollTop: panel.scrollTop,
      clientWidth: panel.clientWidth,
      scrollWidth: panel.scrollWidth,
      swatchIds: [...swatches.querySelectorAll('[data-wheel-select]')].map(
        (el) => el.dataset.wheelSelect,
      ),
      inspectorPort: port(document.querySelector('.inspector')),
      markers: [...panel.querySelectorAll('[data-wheel-marker]')].map((el) => ({
        id: el.dataset.wheelMarker,
        ...bounds(el),
      })),
    };
  });
  const inside = (b, p) =>
    b.left >= p.left - 0.5 &&
    b.right <= p.right + 0.5 &&
    b.top >= p.top - 0.5 &&
    b.bottom <= p.bottom + 0.5;
  for (const [name, item] of [
    ['wheel', result.wheel],
    ['swatches', result.swatches],
    ['neutral track', result.neutral],
  ]) {
    assert.ok(item.bounds.width > 0 && item.bounds.height > 0);
    assert.ok(
      inside(item.bounds, result.port),
      `${label}: ${name} is fully inside the visible panel`,
    );
    assert.equal(item.unobscured, true, `${label}: ${name} is not overlaid by status or Inspector`);
  }
  assert.ok(
    result.wheel.bounds.top >= result.status.bottom - 0.5,
    `${label}: status does not cover wheel`,
  );
  assert.ok(
    result.swatches.bounds.left >= result.wheel.bounds.right - 0.5,
    `${label}: vertical colors sit to the right of the wheel`,
  );
  assert.ok(
    result.neutral.bounds.top >= result.wheel.bounds.bottom - 0.5,
    `${label}: neutral track follows wheel`,
  );
  assert.ok(
    result.parameters.top >= result.neutral.bounds.bottom - 0.5,
    `${label}: settings follow colors`,
  );
  assert.ok(result.parameters.height >= 60, `${label}: parameters retain a usable scrollport`);
  assert.equal(result.scrollWidth, result.clientWidth, `${label}: no panel horizontal overflow`);
  assert.equal(result.panelScrollTop, 0, `${label}: only the parameter region scrolls`);
  assert.ok(
    result.inspectorPort.bottom - result.inspectorPort.top >= 100,
    `${label}: Inspector remains accessible`,
  );
  for (const item of result.markers)
    assert.ok(inside(item, result.port), `${label}: marker ${item.id} is visible`);
  layouts.push({ label, ...result });
  return result;
};
// A report may exceed the inner scrollport. Scroll and verify each painted text line,
// without shrinking its font, dropping authored pairs, or claiming the entire report fits.
const readableContrast = async (mode, label, expectedPairs) => {
  const report = page.locator(`[data-palette-contrast="${mode}"]`);
  assert.equal(await report.count(), 1);
  const summary = await report.evaluate((el) => ({
    label: el.getAttribute('aria-label'),
    text: el.innerText,
    status: el.dataset.contrastStatus,
    pairs: [...el.querySelectorAll('[data-contrast-text-id]')].map((row) => ({
      textId: row.dataset.contrastTextId,
      backgroundId: row.dataset.contrastBackgroundId,
    })),
    blocks: [...el.querySelectorAll('strong,p')].map((block) => ({
      text: block.textContent,
      lines: (() => {
        const r = document.createRange();
        r.selectNodeContents(block);
        return r.getClientRects().length;
      })(),
    })),
  }));
  assert.equal(summary.label, `${label} palette contrast`);
  assert.match(summary.text, new RegExp(`${label} normal-text contrast:`));
  assert.match(summary.text, /Text × Background candidates, 4\.5:1/);
  assert.match(summary.text, /Not an artwork or whole-WCAG audit\. No colors are changed\./);
  assert.match(summary.text, /\d+ pass, \d+ fail, \d+ unknown/);
  const pairKeys = (rows) => rows.map((row) => `${row.textId}/${row.backgroundId}`).sort();
  assert.deepEqual(pairKeys(summary.pairs), pairKeys(expectedPairs));
  assert.equal(summary.blocks.length, 3 + expectedPairs.length);
  const lines = [];
  for (let block = 0; block < summary.blocks.length; block++) {
    assert.ok(summary.blocks[block].text.trim());
    assert.ok(summary.blocks[block].lines > 0);
    for (let line = 0; line < summary.blocks[block].lines; line++) {
      const geometry = await report.evaluate(
        async (el, { block, line }) => {
          const text = el.querySelectorAll('strong,p')[block],
            scroller = el.closest('.chroma-settings-scroll');
          const range = document.createRange();
          range.selectNodeContents(text);
          const initial = range.getClientRects()[line],
            viewport = scroller.getBoundingClientRect();
          scroller.scrollTop += initial.top - viewport.top - 4;
          await new Promise(requestAnimationFrame);
          const b = range.getClientRects()[line],
            style = getComputedStyle(text);
          const port = { left: 0, top: 0, right: innerWidth, bottom: innerHeight };
          for (let parent = text.parentElement; parent; parent = parent.parentElement) {
            const s = getComputedStyle(parent),
              r = parent.getBoundingClientRect();
            if (/(auto|scroll|hidden|clip)/.test(s.overflowX)) {
              port.left = Math.max(port.left, r.left + parent.clientLeft);
              port.right = Math.min(port.right, r.left + parent.clientLeft + parent.clientWidth);
            }
            if (/(auto|scroll|hidden|clip)/.test(s.overflowY)) {
              port.top = Math.max(port.top, r.top + parent.clientTop);
              port.bottom = Math.min(port.bottom, r.top + parent.clientTop + parent.clientHeight);
            }
          }
          return {
            block,
            line,
            text: text.textContent,
            fontSize: parseFloat(style.fontSize),
            lineHeight: parseFloat(style.lineHeight),
            display: style.display,
            visibility: style.visibility,
            opacity: Number(style.opacity),
            bounds: { left: b.left, top: b.top, right: b.right, bottom: b.bottom },
            port,
            unobscured: el.contains(
              document.elementFromPoint(b.left + b.width / 2, b.top + b.height / 2),
            ),
            parameterScrollTop: scroller.scrollTop,
            clientWidth: el.clientWidth,
            scrollWidth: el.scrollWidth,
          };
        },
        { block, line },
      );
      assert.ok(geometry.fontSize >= 10 && geometry.lineHeight >= geometry.fontSize);
      assert.notEqual(geometry.display, 'none');
      assert.equal(geometry.visibility, 'visible');
      assert.equal(geometry.opacity, 1);
      const b = geometry.bounds,
        p = geometry.port;
      assert.ok(
        b.left >= p.left - 0.5 &&
          b.right <= p.right + 0.5 &&
          b.top >= p.top - 0.5 &&
          b.bottom <= p.bottom + 0.5,
        `${label}: readable line ${block}/${line} is visible: ${JSON.stringify(geometry)}`,
      );
      assert.equal(geometry.unobscured, true);
      assert.equal(geometry.scrollWidth, geometry.clientWidth);
      lines.push(geometry);
    }
    await capture(`contrast-${mode}-${block}`);
  }
  return { ...summary, lines };
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect?.colorPreviewSnapshot);
  await page.evaluate((text) => window.nect.load(text), serialize(fixture));
  await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  const beforeCreate = await encoded();
  await page.locator('[data-harmony-create]').click();
  const created = await snapshot(),
    afterCreate = await encoded();
  assert.equal(await page.locator('[data-color-advanced]').evaluate((el) => el.open), false);
  assert.equal(await page.locator('[data-chroma-lock]').first().isVisible(), false);
  assert.equal(Number(await page.locator('[data-chroma-spread]').inputValue()), 0.62);
  assert.equal(await page.locator('[data-wheel-range]').count(), 4);
  assert.equal(await page.locator('[data-chroma-strength-range]').count(), 1);
  assert.equal(await page.locator('[data-chroma-spread-range]').count(), 1);
  assert.equal(await page.locator('[data-wheel-neutral]').count(), 2);
  assert.equal(created.colorDesign.version, 2);
  assert.equal(created.colorSlots.length, 5);
  assert.deepEqual(
    created.colorDesign.palette.map((item) => item.role),
    ['Primary', 'Secondary', 'Accent', 'Background', 'Text'],
  );
  assert.equal(await page.locator('.harmony-wheel-grid path').count(), 84);
  assert.equal(await page.locator('[data-wheel-marker]').count(), 3);
  assert.equal(
    await page.locator('[data-chroma-preview],[data-chroma-apply],[data-chroma-cancel]').count(),
    0,
  );
  await undoTo(beforeCreate, 'Create palette is exactly one edit');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), afterCreate);
  const primary = created.colorDesign.palette[0].id,
    secondary = created.colorDesign.palette[1].id;
  const baseRotation = created.colorDesign.wheel.hueRotation;
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${layer.id}"]`).click();
  await page.locator(`.graph .node[data-id="${fill.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  await page
    .locator(`.inspector-body [data-shared-color-binding][data-shared-color-node="${fill.id}"]`)
    .selectOption(primary);
  const original = await encoded(),
    originalState = await commandState(),
    positions = await markerPositions();
  const gridBefore = await page.locator('.harmony-wheel-grid path').first().getAttribute('fill');
  await rotation().fill('75'); // No Enter, Tab or Apply before inspecting actual live output.
  await page.waitForFunction(
    () => window.nect.colorPreviewSnapshot()?.colorDesign.wheel.hueRotation === 75,
  );
  assert.equal(await encoded(), original, 'valid typing projects without committing on each input');
  assert.deepEqual(await commandState(), originalState);
  assert.equal(
    await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
    'editing',
  );
  assert.deepEqual(await markerPositions(), positions, 'hue rotation never rotates spokes');
  assert.notEqual(
    await page.locator('.harmony-wheel-grid path').first().getAttribute('fill'),
    gridBefore,
  );
  const typedPreview = await preview(),
    typedHex = typedPreview.colorSlots.find((slot) => slot.id === primary).value.hex;
  assert.notEqual(typedHex, created.colorSlots.find((slot) => slot.id === primary).value.hex);
  await painted(typedHex);
  await capture('typed-without-enter');
  await rotation().press('Tab');
  await idle();
  const typed = await encoded();
  assert.equal((await snapshot()).colorDesign.wheel.hueRotation, 75);
  assert.equal((await commandState()).expectedRevision, originalState.expectedRevision + 1);
  await undoTo(original, 'one Undo restores the entire typed live edit');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), typed);
  await undoTo(original, 'Redo/Undo retains exact saved palette');

  // Returning typed input to its original value settles without a stranded gesture/history edit.
  const noOpState = await commandState();
  await rotation().fill('75');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  await rotation().fill(String(baseRotation));
  await rotation().press('Tab');
  await idle();
  assert.equal(await encoded(), original);
  assert.deepEqual(await commandState(), noOpState);
  assert.equal(
    await page.locator('[data-chroma-state]').getAttribute('data-chroma-state'),
    'saved',
  );

  // A press on another number settles the typed field first without detaching that new press.
  await rotation().fill('75');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  const strength = page.locator('[data-chroma-strength]');
  const secondPoint = await pressNumber(strength);
  const visibleWheel = await page.locator('.shared-colors-panel').evaluate((panel) => {
    const wheel = panel.querySelector('.harmony-wheel').getBoundingClientRect();
    const port = panel.getBoundingClientRect();
    return (
      wheel.width > 0 && wheel.height > 0 && wheel.top >= port.top && wheel.bottom <= port.bottom
    );
  });
  assert.equal(visibleWheel, true, 'wheel remains visible while adjusting Colorfulness');
  const rotationOnly = await encoded();
  assert.equal((await snapshot()).colorDesign.wheel.hueRotation, 75);
  assert.equal((await snapshot()).colorDesign.colorfulness, created.colorDesign.colorfulness);
  await page.mouse.move(secondPoint.x - 14, secondPoint.y, { steps: 6 });
  await page.waitForFunction(
    () => window.nect.colorPreviewSnapshot()?.colorDesign.colorfulness === 0.62,
  );
  assert.equal(await encoded(), rotationOnly, 'the second press owns a new detached edit');
  await page.mouse.up();
  await idle();
  assert.equal((await snapshot()).colorDesign.colorfulness, 0.62);
  await undoTo(rotationOnly, 'one Undo restores only the second numeric drag');
  await undoTo(original, 'the previous typed field is a separate edit');

  // Invalid/incomplete and IME input must not become authored colors.
  const invalidState = await commandState();
  await rotation().fill('');
  await rotation().press('-');
  assert.equal(await encoded(), original);
  assert.deepEqual(await commandState(), invalidState);
  await page.keyboard.press('Escape');
  await idle();
  assert.equal(
    await page.locator('.shared-colors-panel').count(),
    1,
    'Escape cancels the field edit without closing its panel',
  );
  assert.equal(await encoded(), original);
  const beforeComposition = await encoded();
  await rotation().focus();
  await rotation().evaluate((el) => {
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
    el.value = '123';
    el.dispatchEvent(new InputEvent('input', { bubbles: true, isComposing: true, data: '123' }));
  });
  assert.equal(await encoded(), beforeComposition, 'IME intermediate input never commits');
  await rotation().evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: '' })),
  );
  await page.keyboard.press('Escape');
  assert.equal(
    await page.locator('.shared-colors-panel').count(),
    1,
    'Escape cancels the field edit without closing its panel',
  );
  assert.equal(await encoded(), original);

  // A continuous real mouse scrub keeps controls/Inspector mounted and has one commit.
  const beforeDrag = await encoded(),
    stateBeforeDrag = await commandState(),
    metricsBeforeDrag = await page.evaluate(() => window.nect.colorPreviewMetrics());
  const inputIdentity = await rotation().elementHandle(),
    inspectorIdentity = await page.locator('.inspector').elementHandle();
  await page.evaluate(() => {
    window.__colorLivePointerMoves = 0;
    document
      .querySelector('#app')
      .addEventListener('pointermove', () => window.__colorLivePointerMoves++, true);
    document.querySelector('#app').addEventListener(
      'pointerdown',
      (event) => {
        window.__colorLivePointerId = event.pointerId;
      },
      true,
    );
  });
  const point = await pressNumber(rotation());
  await page.mouse.move(point.x + 2, point.y);
  assert.equal(await preview(), null, '4px dead zone leaves click/type intact');
  await page.mouse.move(point.x + 44, point.y, { steps: 24 });
  await page.waitForFunction(
    (value) => window.nect.colorPreviewSnapshot()?.colorDesign.wheel.hueRotation === value,
    baseRotation + 40,
  );
  assert.equal(await encoded(), beforeDrag);
  assert.deepEqual(await commandState(), stateBeforeDrag);
  assert.equal(
    await page.evaluate(
      (el) => el === document.querySelector('[data-wheel-setting="hueRotation"]'),
      inputIdentity,
    ),
    true,
    'no full control replacement per move',
  );
  assert.equal(
    await page.evaluate((el) => el === document.querySelector('.inspector'), inspectorIdentity),
    true,
    'no full Inspector render per move',
  );
  await page.mouse.up();
  await idle();
  const dragged = await encoded();
  assert.equal((await snapshot()).colorDesign.wheel.hueRotation, baseRotation + 40);
  assert.equal((await commandState()).expectedRevision, stateBeforeDrag.expectedRevision + 1);
  const metricsAfterDrag = await page.evaluate(() => window.nect.colorPreviewMetrics()),
    deltaRequested = metricsAfterDrag.requested - metricsBeforeDrag.requested,
    deltaEvaluated = metricsAfterDrag.evaluated - metricsBeforeDrag.evaluated;
  assert.ok(deltaEvaluated > 0, 'continuous drag evaluated actual preview frames');
  assert.ok(deltaRequested >= deltaEvaluated);
  interactions.push({
    kind: 'continuous-real-mouse',
    pointerMoves: await page.evaluate(() => window.__colorLivePointerMoves),
    metrics: {
      scope: 'continuous drag only',
      requested: deltaRequested,
      evaluated: deltaEvaluated,
      coalescedRequests: deltaRequested - deltaEvaluated,
      samples: metricsAfterDrag.samples.slice(-deltaEvaluated),
    },
    singleCommit: true,
    stableControlAndInspector: true,
  });
  await inputIdentity.dispose();
  await inspectorIdentity.dispose();
  await undoTo(beforeDrag, 'one Undo restores the full continuous drag');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), dragged);
  await undoTo(original, 'repeated drag baseline remains exact');
  for (const interruption of ['Escape', 'pointercancel', 'window-blur']) {
    const baseline = await encoded(),
      state = await commandState(),
      point = await pressNumber(rotation());
    await page.mouse.move(point.x + 24, point.y, { steps: 5 });
    await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
    if (interruption === 'Escape') await page.keyboard.press('Escape');
    else if (interruption === 'pointercancel')
      await page.evaluate(() =>
        document.querySelector('#app').dispatchEvent(
          new PointerEvent('pointercancel', {
            bubbles: true,
            pointerId: window.__colorLivePointerId,
          }),
        ),
      );
    else await page.evaluate(() => window.dispatchEvent(new Event('blur')));
    await page.mouse.up();
    await idle();
    assert.equal(await encoded(), baseline, `${interruption} rolls back exact document`);
    assert.deepEqual(await commandState(), state, `${interruption} adds no history`);
    assert.equal(Number(await rotation().inputValue()), baseRotation);
    interactions.push({
      kind: interruption,
      rollback: true,
      eventSource:
        interruption === 'Escape'
          ? 'real keyboard'
          : 'dispatched interruption during real mouse drag',
    });
  }
  // Click, native arrows and native spinner remain usable alongside scrubbing.
  await rotation().click();
  await rotation().press('ArrowUp');
  await rotation().press('Tab');
  await idle();
  assert.equal((await snapshot()).colorDesign.wheel.hueRotation, baseRotation + 1);
  await undoTo(original, 'native ArrowUp is one edit');
  await rotation().scrollIntoViewIfNeeded();
  const spinner = await rotation().evaluate((el) => {
    const box = el.getBoundingClientRect(),
      style = getComputedStyle(el),
      paddingTop = parseFloat(style.paddingTop),
      paddingBottom = parseFloat(style.paddingBottom),
      x = box.right - parseFloat(style.borderRightWidth) - parseFloat(style.paddingRight) - 6,
      y =
        box.top +
        parseFloat(style.borderTopWidth) +
        paddingTop +
        (el.clientHeight - paddingTop - paddingBottom) * 0.25;
    return { x, y, hitsInput: document.elementFromPoint(x, y) === el };
  });
  assert.equal(spinner.hitsInput, true, 'the physical spinner point reaches the rotation input');
  await page.mouse.move(spinner.x, spinner.y);
  await capture('native-spinner-before');
  await page.mouse.click(spinner.x, spinner.y);
  await rotation().press('Tab');
  await idle();
  assert.equal(
    (await snapshot()).colorDesign.wheel.hueRotation,
    baseRotation + 1,
    'native upper spinner is not swallowed by scrub',
  );
  await undoTo(original, 'native spinner is one edit');

  // Native platform range input is live, coalesced and committed as one pointer edit.
  const spreadRange = () => page.locator('[data-chroma-spread-range]');
  const startRange = async (fraction) => {
    await spreadRange().scrollIntoViewIfNeeded();
    const box = await spreadRange().boundingBox();
    assert.ok(box && box.width > 24);
    const value = Number(await spreadRange().inputValue());
    await page.mouse.move(box.x + 8 + (box.width - 16) * value, box.y + box.height / 2);
    await page.mouse.down();
    await page.mouse.move(box.x + box.width * fraction, box.y + box.height / 2, { steps: 10 });
  };
  const rangeBefore = await encoded(),
    rangeState = await commandState();
  await startRange(0.8);
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  const nativeSpread = Number(await spreadRange().inputValue()),
    rangePreview = await preview();
  assert.notEqual(nativeSpread, 0.62);
  assert.equal(rangePreview.colorDesign.spread, nativeSpread);
  assert.notDeepEqual(rangePreview.colorSlots, (await snapshot()).colorSlots);
  assert.equal(await encoded(), rangeBefore);
  assert.deepEqual(await commandState(), rangeState);
  await page.mouse.up();
  await idle();
  const rangeAfter = await encoded();
  assert.equal((await snapshot()).colorDesign.spread, nativeSpread);
  assert.equal((await commandState()).expectedRevision, rangeState.expectedRevision + 1);
  await undoTo(rangeBefore, 'one Undo restores the native range gesture including absent Spread');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), rangeAfter);
  await undoTo(rangeBefore, 'native range Redo/Undo returns to the exact baseline');
  for (const interruption of ['pointercancel', 'Escape']) {
    const state = await commandState();
    await startRange(0.2);
    await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
    if (interruption === 'Escape') await page.keyboard.press('Escape');
    else
      await page.evaluate(() =>
        document.querySelector('#app').dispatchEvent(
          new PointerEvent('pointercancel', {
            bubbles: true,
            pointerId: window.__colorLivePointerId,
          }),
        ),
      );
    await page.mouse.up();
    await idle();
    assert.equal(await encoded(), rangeBefore, `native range ${interruption} rolls back`);
    assert.deepEqual(await commandState(), state);
    assert.equal(Number(await spreadRange().inputValue()), 0.62);
  }

  // Neutral lightness has its own physical lane, not an overlapping center marker.
  const neutralId = created.colorDesign.palette.find((item) => item.kind === 'neutral').id,
    neutral = () => page.locator(`[data-wheel-neutral="${neutralId}"]`),
    neutralBefore = await encoded(),
    neutralState = await commandState(),
    lane = await page.locator('.harmony-neutral-track').boundingBox(),
    handle = await neutral().boundingBox();
  assert.ok(lane && handle);
  assert.equal(await neutral().getAttribute('role'), 'slider');
  await page.mouse.move(handle.x + handle.width / 2, handle.y + handle.height / 2);
  await page.mouse.down();
  await page.mouse.move(lane.x + lane.width * 0.7, lane.y + lane.height / 2, { steps: 8 });
  await page.waitForFunction((id) => {
    const projected = window.nect.colorPreviewSnapshot(),
      entry = projected?.colorDesign.palette.find((item) => item.id === id);
    return entry?.kind === 'neutral' && entry.L > 0.25 && entry.L < 0.35;
  }, neutralId);
  assert.equal(await encoded(), neutralBefore);
  assert.deepEqual(await commandState(), neutralState);
  await page.mouse.up();
  await idle();
  const neutralAfter = await encoded(),
    neutralEntry = (await snapshot()).colorDesign.palette.find((item) => item.id === neutralId);
  assert.equal(neutralEntry.kind, 'neutral');
  assert.ok(neutralEntry.L > 0.25 && neutralEntry.L < 0.35);
  assert.equal(await neutral().getAttribute('aria-valuenow'), String(neutralEntry.L));
  await undoTo(neutralBefore, 'neutral physical drag is one edit at the exact stable ID');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), neutralAfter);
  await undoTo(neutralBefore, 'neutral lane Redo/Undo retains exact IDs');
  await neutral().focus();
  await page.keyboard.press('ArrowRight');
  await idle();
  const initialNeutral = created.colorDesign.palette.find((item) => item.id === neutralId).L,
    keyboardNeutral = (await snapshot()).colorDesign.palette.find(
      (item) => item.id === neutralId,
    ).L;
  assert.ok(Math.abs(keyboardNeutral - (initialNeutral - 0.01)) < 1e-12);
  await undoTo(neutralBefore, 'neutral keyboard adjustment is one edit');
  await capture('native-ranges-and-neutral-track');

  // Stable-ID swatch selection is view-only; global Delete/tools must not escape.
  const swatchBaseline = await encoded();
  await page.locator(`[data-wheel-select="${secondary}"]`).click();
  assert.equal(await page.locator('[data-wheel-entry]').inputValue(), secondary);
  await page.locator(`[data-wheel-select="${secondary}"]`).focus();
  await page.keyboard.press('Delete');
  await page.keyboard.press('t');
  assert.equal(
    await encoded(),
    swatchBaseline,
    'focused swatch cannot delete selected Fill or invoke authoring shortcuts',
  );
  await page.locator('.chroma-settings-scroll').focus();
  await page.keyboard.press('Delete');
  assert.equal(
    await encoded(),
    swatchBaseline,
    'focused parameter scrollport cannot delete selected Fill',
  );
  const preservedLayer = (await snapshot()).compositions
    .flatMap((composition) => composition.layers)
    .find((item) => item.id === layer.id);
  assert.ok(preservedLayer, 'the original layer survives color-panel keyboard navigation');
  assert.ok(preservedLayer.nodes.some((item) => item.id === fill.id));
  await page.locator(`[data-wheel-select="${primary}"]`).click();
  await openAdvanced();
  await page.locator('[data-wheel-chroma-mode]').selectOption('override');
  const beforeOverrideNumber = await encoded();
  await change(page.locator('[data-wheel-chroma]'), 0.03);
  await idle();
  assert.equal(
    (await snapshot()).colorDesign.palette.find((item) => item.id === primary).overrideC,
    0.03,
  );
  const override = await encoded(),
    overrideHex = (await snapshot()).colorSlots.find((slot) => slot.id === primary).value.hex;
  await painted(overrideHex);
  await undoTo(
    beforeOverrideNumber,
    'one Undo restores numeric Chroma edit, not its previous separate mode change',
  );
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), override);
  await page.locator(`[data-chroma-lock="${primary}"]`).check();
  const kept = await snapshot(),
    keptLiteral = kept.colorSlots.find((slot) => slot.id === primary).value;
  await change(rotation(), 75);
  await idle();
  const rotated = await snapshot();
  assert.deepEqual(
    rotated.colorSlots.find((slot) => slot.id === primary).value,
    keptLiteral,
    'Keep wins over wheel and explicit Chroma',
  );
  assert.notDeepEqual(
    rotated.colorSlots.find((slot) => slot.id === secondary).value,
    kept.colorSlots.find((slot) => slot.id === secondary).value,
  );
  // Radial drag previews immediately and commits only on pointerup.
  const beforeRadial = await encoded(),
    radialState = await commandState();
  const b = await marker(secondary).boundingBox(),
    svg = await page.locator('.harmony-wheel').boundingBox();
  assert.ok(b && svg);
  await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2);
  await page.mouse.down();
  const r = (Math.min(svg.width, svg.height) / 230) * 90;
  await page.mouse.move(svg.x + svg.width / 2 + r, svg.y + svg.height / 2, { steps: 8 });
  await page.waitForFunction(
    (id) =>
      window.nect.colorPreviewSnapshot()?.colorDesign.palette.find((item) => item.id === id)
        ?.ring === 6,
    secondary,
  );
  assert.equal(await encoded(), beforeRadial);
  assert.deepEqual(await commandState(), radialState);
  await page.mouse.up();
  await idle();
  const afterRadial = await encoded();
  assert.equal(
    (await snapshot()).colorDesign.palette.find((item) => item.id === secondary).ring,
    6,
  );
  await undoTo(beforeRadial, 'radial gesture is one Undo');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), afterRadial);
  await marker(secondary).focus();
  await page.keyboard.press('Home');
  await idle();
  assert.equal(
    (await snapshot()).colorDesign.palette.find((item) => item.id === secondary).ring,
    0,
  );
  await undoTo(afterRadial, 'marker keyboard edit is one Undo');

  // Spread is an optional saved rule setting, not a display-only or renamed strength control.
  const beforeSpread = await encoded(),
    beforeSpreadState = await commandState();
  assert.equal(Object.hasOwn((await snapshot()).colorDesign, 'spread'), false);
  await page.locator('[data-chroma-spread]').fill('0.35');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot()?.colorDesign.spread === 0.35);
  assert.equal(await encoded(), beforeSpread);
  assert.deepEqual(await commandState(), beforeSpreadState);
  await page.locator('[data-chroma-spread]').press('Tab');
  await idle();
  const spreadSaved = await encoded();
  assert.equal((await snapshot()).colorDesign.spread, 0.35);
  assert.equal((await commandState()).expectedRevision, beforeSpreadState.expectedRevision + 1);
  assert.equal(await page.locator('[data-color-advanced]').evaluate((el) => el.open), true);
  await undoTo(beforeSpread, 'one Undo removes the optional numeric Spread edit');
  await activateCommand(page, 'redo');
  assert.equal(await encoded(), spreadSaved);

  const beforeAdd = await snapshot();
  await page.locator('[data-wheel-add-kind]').selectOption('neutral');
  await page.locator('[data-harmony-add]').click();
  const added = await snapshot();
  assert.equal(added.colorSlots.length, 6);
  assert.equal(added.colorDesign.palette.at(-1).kind, 'neutral');
  assert.deepEqual(added.colorDesign.palette.slice(0, 5), beforeAdd.colorDesign.palette);
  assert.deepEqual(added.colorSlots.slice(0, 5), beforeAdd.colorSlots);
  const download = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(out, 'harmony-live.json');
  await (await download).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), added);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  const session = (await commandState()).sessionId,
    chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction((id) => window.nect.commandState.sessionId !== id, session);
  assert.deepEqual(await snapshot(), added);
  await openAdvanced();
  await page.locator('[data-wheel-entry]').selectOption(primary);
  assert.equal(Number(await page.locator('[data-wheel-chroma]').inputValue()), 0.03);
  assert.equal(Number(await page.locator('[data-chroma-spread]').inputValue()), 0.35);
  await page.locator('[data-wheel-entry]').selectOption(created.colorDesign.palette[4].id);
  assert.equal(await page.locator('[data-wheel-chroma-mode]').count(), 0);
  assert.equal(await page.locator('[data-wheel-chroma-neutral]').textContent(), 'Chroma 0');

  for (const viewport of [
    { width: 900, height: 720 },
    { width: 1366, height: 768 },
  ]) {
    await page.setViewportSize(viewport);
    const top = await pinnedLayout(`top-${viewport.width}x${viewport.height}`);
    assert.deepEqual(
      top.swatchIds,
      added.colorDesign.palette.map((item) => item.id),
    );
    await page.locator('[data-wheel-setting="lightnessRange"]').scrollIntoViewIfNeeded();
    const scrolled = await pinnedLayout(`parameter-${viewport.width}x${viewport.height}`);
    assert.ok(scrolled.parameterScrollTop > 0, 'actual parameter scrolling occurs');
    assert.deepEqual(
      scrolled.wheel.bounds,
      top.wheel.bounds,
      'wheel remains pinned while reaching lower parameters',
    );
    assert.deepEqual(
      scrolled.swatches.bounds,
      top.swatches.bounds,
      'palette remains immediately below pinned wheel',
    );
    await capture(`parameters-${viewport.width}x${viewport.height}`);
  }
  await page.setViewportSize({ width: 900, height: 720 });
  const recipe = added.colorDesign.palette,
    pairs = recipe
      .filter((item) => item.role === 'Text')
      .flatMap((text) =>
        recipe
          .filter((item) => item.role === 'Background')
          .map((background) => ({ textId: text.id, backgroundId: background.id })),
      );
  assert.equal(pairs.length, 2);
  await openAdvanced();
  const savedContrast = await readableContrast('current', 'Saved', pairs);
  const beforeReportEdit = await encoded();
  await rotation().fill('110');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  const proposedContrast = await readableContrast('preview', 'Proposed', pairs);
  assert.equal(await encoded(), beforeReportEdit, 'reading live contrast does not author the edit');
  await rotation().focus();
  await page.keyboard.press('Escape');
  await idle();
  assert.equal(await encoded(), beforeReportEdit);
  assert.equal(await page.locator('[data-palette-contrast="preview"]').count(), 0);
  assert.deepEqual(errors, []);
  const receipt = {
    ok: true,
    capturedAt: new Date().toISOString(),
    createAtomicHistory: true,
    typedInputReachesWheelAndArtworkWithoutEnter: true,
    validTypingCommitOnBlurOnce: true,
    continuousDragSingleUndo: true,
    interruptedDragRollback: true,
    stableControlIdentityDuringDrag: true,
    nativeArrowsAndSpinner: true,
    nativeRangeLiveCommitAndCancel: true,
    neutralLanePhysicalAndKeyboardStableId: true,
    spreadLiveHistoryAndBrowserReopen: true,
    advancedDefaultCollapsedAndOpenRetained: true,
    typeToOtherNumberDragAndNoOpReturn: true,
    swatchStableIdsAndKeyboardIsolation: true,
    radialLiveAndKeyboardHistory: true,
    perColorChromaAndKeep: true,
    addPreservesStableIds: true,
    browserSaveReopen: true,
    pinnedWheelAndPaletteWhileParametersScroll: true,
    invalidAndImeNeverCommit: true,
    interactions,
    layouts,
    captures,
    contrast: {
      saved: savedContrast,
      proposed: proposedContrast,
      everyPaintedLineScrolledAndHitTested: true,
    },
    limits: [
      'Browser Chrome evidence, not native WebView2 or physical Windows input acceptance.',
      'Pointercancel/window blur are dispatched interruption events during a real mouse gesture.',
      'Timing samples are observations, not a 60fps claim.',
    ],
    errors,
  };
  await writeFile(resolve(out, 'result.json'), JSON.stringify(receipt, null, 2));
  await writeFile(
    resolve(evidence, 'workflow-color-live-harmony.json'),
    JSON.stringify(receipt, null, 2),
  );
  console.log(
    'PASS live harmony color panel: typed/mouse output, single-edit history, rollback, stable IDs, pinned layout and readable contrast',
  );
} catch (error) {
  await capture('failure');
  await writeFile(
    resolve(evidence, 'workflow-color-live-failure.json'),
    JSON.stringify(
      {
        error: error.message,
        errors,
        project: await snapshot(),
        preview: await preview(),
        captures,
        layouts,
        interactions,
      },
      null,
      2,
    ),
  );
  throw error;
} finally {
  await browser.close();
}
