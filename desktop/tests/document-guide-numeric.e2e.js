import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { guideDefaults } from '../src/guides.js';
import { compoundFixture } from './fixtures/contour-navigation.js';
import { displayLengthToDocumentUnits } from '../src/artboard-units.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } }),
  errors = [],
  results = [];
page.on('pageerror', (e) => errors.push(e.message));
const output = 'test-results/document-guide-numeric';
await fs.mkdir(output, { recursive: true });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await ensureWorkspacePanel(page, 'layers');
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const state = () => page.evaluate(() => window.nect.commandState);
  const width = () => page.locator('[data-dimension="width"]');
  const undo = () => page.locator('[data-action="undo"]').click();
  const load = async () => {
    await page.locator('#app > header').click({ position: { x: 20, y: 10 } });
    const f = compoundFixture([8, 5, 7, 4]);
    f.doc.width = 123.45678901234567;
    f.doc.editor = {
      ...guideDefaults(),
      gridVisible: true,
      guides: [{ id: 'p2-guide', axis: 'x', position: 100, locked: false }],
    };
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  };
  const start = async (input) => {
    await input.scrollIntoViewIfNeeded();
    const b = await input.boundingBox(),
      x = Math.round(b.x + b.width * 0.4),
      y = Math.round(b.y + b.height / 2);
    assert.equal(
      await input.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, { x, y }),
      true,
    );
    await page.mouse.move(x, y);
    await page.mouse.down();
    return { x, y, handle: await input.elementHandle() };
  };
  const move = (p, dx) => page.mouse.move(p.x + dx, p.y, { steps: 5 });
  await load();
  await activateBoundaryCommand(page, 'grid-panel');
  const opacity = page.locator('input[type="number"][data-grid-opacity]'),
    range = page.locator('input[type="range"][data-grid-opacity]'),
    opacityBefore = await saved(),
    opacityState = await state(),
    opacityPointer = await start(opacity);
  await move(opacityPointer, 24);
  await page.screenshot({ path: `${output}/grid-opacity-preview.png` });
  assert.equal(Number(await opacity.inputValue()), 45);
  assert.equal(Number(await range.inputValue()), 45);
  assert.equal(Number(await page.locator('[data-grid-overlay]').getAttribute('opacity')), 0.45);
  assert.equal(await saved(), opacityBefore);
  await page.mouse.up();
  assert.equal(JSON.parse(await saved()).editor.gridOpacity, 0.45);
  assert.equal((await state()).expectedRevision, opacityState.expectedRevision + 1);
  await undo();
  assert.equal(await saved(), opacityBefore);
  results.push({ case: 'Grid opacity', percentBoundary: true, peerSync: true, oneUndo: true });
  const openGrid = async () => {
    if (!(await opacity.isVisible())) await activateBoundaryCommand(page, 'grid-panel');
  };
  const redoState = await state();
  for (const ending of ['jitter', 'origin', 'Escape', 'blur', 'lostcapture']) {
    await openGrid();
    const p = await start(opacity);
    await move(p, ending === 'jitter' ? 3 : 24);
    if (ending === 'origin') await move(p, 0);
    if (ending === 'Escape') await page.keyboard.press('Escape');
    if (ending === 'blur') await page.evaluate(() => window.dispatchEvent(new Event('blur')));
    if (ending === 'lostcapture')
      await p.handle.evaluate((el) => {
        for (let id = 1; id < 8; id++) if (el.hasPointerCapture(id)) el.releasePointerCapture(id);
      });
    await page.mouse.up();
    assert.equal(await saved(), opacityBefore);
    assert.deepEqual(await state(), redoState);
    await openGrid();
    assert.equal(Number(await opacity.inputValue()), 25);
    assert.equal(Number(await range.inputValue()), 25);
  }
  await openGrid();
  await start(opacity);
  await page.mouse.up();
  assert.deepEqual(await state(), redoState);
  await opacity.fill('40');
  await opacity.press('ArrowUp');
  assert.equal(Number(await opacity.inputValue()), 41);
  await opacity.press('Tab');
  assert.equal(JSON.parse(await saved()).editor.gridOpacity, 0.41);
  await undo();
  assert.equal(await saved(), opacityBefore);
  await openGrid();
  await opacity.fill('60');
  const typed = await start(opacity);
  await move(typed, 24);
  assert.equal(
    await typed.handle.evaluate((el) => el.isConnected && el === document.activeElement),
    true,
  );
  assert.equal(JSON.parse(await saved()).editor.gridOpacity, 0.6);
  assert.equal(Number(await range.inputValue()), 80);
  await page.mouse.up();
  assert.equal(JSON.parse(await saved()).editor.gridOpacity, 0.8);
  await undo();
  assert.equal(JSON.parse(await saved()).editor.gridOpacity, 0.6);
  await undo();
  assert.equal(await saved(), opacityBefore);
  await openGrid();
  const rangeBox = await range.boundingBox();
  await page.mouse.move(rangeBox.x + rangeBox.width * 0.5, rangeBox.y + rangeBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(rangeBox.x + rangeBox.width * 0.7, rangeBox.y + rangeBox.height / 2, {
    steps: 5,
  });
  assert.equal(await saved(), opacityBefore);
  const rangeValue = Number(await range.inputValue());
  assert.equal(Number(await opacity.inputValue()), rangeValue);
  await page.mouse.up();
  assert.equal(JSON.parse(await saved()).editor.gridOpacity, rangeValue / 100);
  await undo();
  assert.equal(await saved(), opacityBefore);
  results.push({
    case: 'Grid opacity lifecycle',
    nativeTypingArrows: true,
    rangeSlider: true,
    pendingTypingUndo: true,
    cancelNoopRedo: true,
  });
  await openGrid();
  const staleOpacityState = await state(),
    staleOpacity = await start(opacity);
  await move(staleOpacity, 24);
  await opacity.evaluate((el) => {
    el.min = '10';
  });
  await page.mouse.up();
  await opacity.press('Tab');
  assert.equal(await saved(), opacityBefore);
  assert.deepEqual(await state(), staleOpacityState);
  await openGrid();
  const pendingOpacity = await start(opacity);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-guide-setting',
      payload: { key: 'gridOpacity', value: 0.6 },
    }),
  );
  const revisedOpacity = await saved(),
    revisedOpacityState = await state();
  await move(pendingOpacity, 24);
  await page.mouse.up();
  assert.equal(await saved(), revisedOpacity);
  assert.deepEqual(await state(), revisedOpacityState);
  results.push({ case: 'Grid opacity stale input/revision', lateCommit: false });
  for (const unit of ['px', 'mm', 'cm']) {
    await load();
    await page.locator('[data-artboard-unit]').selectOption(unit);
    const before = await saved(),
      original = JSON.parse(before).width,
      history = await state(),
      p = await start(width());
    await move(p, 24);
    assert.equal(
      await p.handle.evaluate((el) => el.isConnected && el === document.activeElement),
      true,
    );
    assert.equal(await saved(), before);
    assert.ok(
      Math.abs(
        (await page
          .locator('.artboard')
          .evaluate((el) => Number(el.getAttribute('viewBox').split(' ')[2]))) -
          (original + displayLengthToDocumentUnits(20, unit)),
      ) < 1e-10,
      'Document preview changes the actual artboard viewBox',
    );
    await page.mouse.up();
    assert.ok(
      Math.abs(
        JSON.parse(await saved()).width - (original + displayLengthToDocumentUnits(20, unit)),
      ) < 1e-10,
    );
    assert.equal((await state()).expectedRevision, history.expectedRevision + 1);
    await undo();
    assert.equal(await saved(), before);
    const redo = await state();
    for (const ending of ['jitter', 'origin', 'Escape', 'blur', 'lostcapture']) {
      const p = await start(width());
      await move(p, ending === 'jitter' ? 3 : 24);
      if (ending === 'origin') await move(p, 0);
      if (ending === 'Escape') await page.keyboard.press('Escape');
      if (ending === 'blur') await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      if (ending === 'lostcapture')
        await p.handle.evaluate((el) => {
          for (let id = 1; id < 8; id++) if (el.hasPointerCapture(id)) el.releasePointerCapture(id);
        });
      await page.mouse.up();
      assert.equal(
        (await saved()) === before,
        true,
        JSON.stringify({
          unit,
          ending,
          expected: JSON.parse(before).width,
          actual: JSON.parse(await saved()).width,
        }),
      );
      assert.deepEqual(await state(), redo);
    }
    const stale = await start(width());
    await move(stale, 24);
    await page.locator('[data-artboard-unit]').selectOption(unit === 'cm' ? 'px' : 'cm');
    await page.mouse.up();
    assert.equal(await saved(), before);
    assert.deepEqual(await state(), redo);
    results.push({
      case: `Document ${unit}`,
      oneUndo: true,
      canonicalSnapshot: true,
      cancelNoopRedo: true,
      staleUnit: true,
    });
  }
  await load();
  await page.locator('[data-artboard-unit]').selectOption('mm');
  const original = await saved();
  await width().fill('100');
  const p = await start(width());
  await move(p, 24);
  await page.mouse.up();
  await undo();
  assert.equal(JSON.parse(await saved()).width, displayLengthToDocumentUnits(100, 'mm'));
  await undo();
  assert.equal(await saved(), original);
  results.push({ case: 'Document pending typing', separateUndo: true });
  await load();
  await page.locator('[data-artboard-unit]').selectOption('cm');
  const height = page.locator('[data-dimension="height"]'),
    heightBefore = await saved();
  await height.fill('10');
  await height.press('ArrowUp');
  assert.equal(Number(await height.inputValue()), 11);
  await height.press('Tab');
  assert.equal(JSON.parse(await saved()).height, displayLengthToDocumentUnits(11, 'cm'));
  await undo();
  assert.equal(await saved(), heightBefore);
  results.push({ case: 'Document height', nativeArrowAndAbsoluteTyping: true });
  for (const [selector, action] of [
    ['[data-guide-position]', 'guides-panel'],
    ['[data-guide-setting="gridSize"]', 'grid-panel'],
    ['[data-guide-setting="gridAngle"]', 'grid-panel'],
  ]) {
    await load();
    if (!(await page.locator(selector).isVisible())) await activateBoundaryCommand(page, action);
    const input = page.locator(selector);
    await input.fill('100');
    await input.press('ArrowUp');
    assert.equal(Number(await input.inputValue()), 101);
    await input.press('ArrowDown');
    assert.equal(Number(await input.inputValue()), 100);
    await input.press('Tab');
    const before = await saved(),
      p = await start(input);
    await move(p, 24);
    assert.equal(await saved(), before);
    if (selector.includes('gridAngle'))
      assert.equal(
        await page.locator('[data-grid-overlay]').getAttribute('data-grid-angle'),
        '120',
      );
    await page.screenshot({ path: `${output}/guide-preview.png` });
    await page.mouse.up();
    assert.equal(Number(await input.inputValue()), 120);
    await undo();
    assert.equal(await saved(), before);
    if (!(await input.isVisible())) await activateBoundaryCommand(page, action);
    const h = await state(),
      cancel = await start(input);
    await move(cancel, 24);
    await page.keyboard.press('Escape');
    await page.mouse.up();
    assert.equal(await saved(), before);
    assert.deepEqual(await state(), h);
    results.push({ case: selector, oneUndo: true, cancelRedo: true });
  }
  await load();
  if (!(await page.locator('[data-guide-setting="gridSize"]').isVisible()))
    await activateBoundaryCommand(page, 'grid-panel');
  const spacing = page.locator('[data-guide-setting="gridSize"]'),
    guideBefore = await saved();
  await spacing.fill('50');
  const staged = await start(spacing);
  await move(staged, 24);
  await page.mouse.up();
  await undo();
  assert.equal(JSON.parse(await saved()).editor.gridSize, 50);
  await undo();
  assert.equal(await saved(), guideBefore);
  results.push({ case: 'Guide pending typing', separateUndo: true });
  assert.deepEqual(errors, []);
  await fs.writeFile(
    `${output}/results.json`,
    JSON.stringify({ browser: await browser.version(), results, pageErrors: errors }, null, 2),
  );
  console.log(
    'Document/Guide numeric Chrome PASS: units, pending typing, canonical state, focus, stale unit, one Undo and cancel/Redo',
  );
} finally {
  await browser.close();
}
