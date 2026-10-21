import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

const output = resolve('test-results/selected-point-bounds/native');
await mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_BOUNDS_CDP_URL || 'http://127.0.0.1:9446',
);
const errors = [],
  results = [];
let page;
try {
  const pages = browser
    .contexts()
    .flatMap((c) => c.pages())
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1, 'one task-owned native WebView2');
  [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  const recovery = page.locator('[data-recovery-dialog]');
  try {
    await recovery.waitFor({ state: 'visible', timeout: 1000 });
    await recovery.getByRole('button', { name: 'Later', exact: true }).click();
  } catch (e) {
    if (e.name !== 'TimeoutError') throw e;
  }
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const state = () => page.evaluate(() => window.nect.commandState);
  const panel = () => page.locator('.inspector-body [data-point-delta-panel]');
  const input = (axis) => panel().locator(`[data-point-delta-axis="${axis}"]`);
  const pick = (id, shift = false) =>
    page
      .locator(`.canvas-stage .point-hit[data-point="${id}"]:not([data-handle])`)
      .click({ modifiers: shift ? ['Shift'] : [] });
  const make = () => {
    const f = compoundFixture([8, 5, 7, 4]);
    f.doc.schemaVersion = 42;
    for (const p of f.path.points) {
      p.handleA.value = [-3, 2];
      p.handleB.value = [5, -4];
    }
    f.ids = [f.path.contours[0].points[1], f.path.contours[3].points[2]];
    return f;
  };
  const load = async (f = make()) => {
    await page.evaluate((text) => window.nect.load(text), serialize(f.doc));
    await page.locator(`.graph .node[data-id="${f.path.id}"] .node-head strong`).click();
    await pick(f.ids[0]);
    await pick(f.ids[1], true);
    await panel().waitFor();
    return f;
  };

  const bounds = () => page.locator('.inspector-body [data-point-bounds]');
  const values = () =>
    bounds()
      .locator('[data-point-bound]')
      .evaluateAll((els) =>
        Object.fromEntries(els.map((el) => [el.dataset.pointBound, Number(el.title)])),
      );
  const expected = (f) => {
    const positions = f.path.points
      .filter((p) => f.ids.includes(p.id))
      .map((p) => p.position.value);
    const x = positions.map((p) => p[0]),
      y = positions.map((p) => p[1]);
    return {
      minX: Math.min(...x),
      maxX: Math.max(...x),
      width: Math.max(...x) - Math.min(...x),
      minY: Math.min(...y),
      maxY: Math.max(...y),
      height: Math.max(...y) - Math.min(...y),
    };
  };
  const f = await load(),
    before = await saved(),
    originalState = await state();
  const original = await values();
  assert.deepEqual(original, expected(f));
  assert.match(await bounds().innerText(), /2 anchors.*Node-local.*document units/s);
  assert.equal(await bounds().locator('input,button,select').count(), 0);
  await page.locator('.inspector-body [data-point-panel-contour]').selectOption('scope-hole');
  assert.deepEqual(await values(), original);
  assert.equal(await saved(), before);
  assert.deepEqual(await state(), originalState);
  await bounds().scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'selected-bounds.png') });
  await input(0).fill('12');
  await input(1).fill('-8');
  await input(1).press('Enter');
  const moved = await values();
  assert.deepEqual(moved, {
    ...original,
    minX: original.minX + 12,
    maxX: original.maxX + 12,
    minY: original.minY - 8,
    maxY: original.maxY - 8,
  });
  assert.equal((await state()).expectedRevision, originalState.expectedRevision + 1);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await values(), original);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await values(), moved);
  // Reveal selected rows again, independently from the current browse filter.
  await page.locator('.inspector-body [data-point-panel-contour]').selectOption('');
  const field = page.locator(
    `.inspector-body [data-key="${f.ids[0]}.position"] [data-value][data-axis="0"]`,
  );
  await field.fill('-30');
  await field.press('Tab');
  await page.waitForFunction(
    () => document.querySelector('.inspector-body [data-point-bound="minX"]')?.title === '-30',
  );
  const edited = await values();
  assert.equal(edited.minX, -30);
  assert.equal(edited.width, edited.maxX + 30);
  await page.locator('[data-action="undo"]').click();
  const afterUndoState = await bounds().getAttribute('data-state');
  assert.ok(['empty', 'ready'].includes(afterUndoState));
  if (afterUndoState === 'ready') assert.deepEqual(await values(), moved);
  else assert.equal(await bounds().locator('[data-point-bound]').count(), 0);
  await page.locator('[data-action="redo"]').click();
  if ((await bounds().getAttribute('data-state')) === 'ready')
    assert.deepEqual(await values(), edited);
  else assert.equal(await bounds().locator('[data-point-bound]').count(), 0);
  const text = await saved(),
    svg = await page.evaluate(() => window.nect.export());
  await writeFile(resolve(output, 'bounds.json'), text);
  await page.evaluate(
    (text) => window.nect.load(text),
    await readFile(resolve(output, 'bounds.json'), 'utf8'),
  );
  assert.deepEqual(JSON.parse(await saved()), JSON.parse(text));
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  await page.locator(`.graph .node[data-id="${f.path.id}"] .node-head strong`).click();
  assert.equal(await bounds().getAttribute('data-state'), 'empty');
  assert.equal(await bounds().locator('[data-point-bound]').count(), 0);
  assert.match(await bounds().innerText(), /No points selected/);
  await pick(f.ids[0]);
  assert.equal((await values()).width, 0);
  assert.equal((await values()).height, 0);
  await pick(f.ids[0], true);
  assert.equal(await bounds().getAttribute('data-state'), 'empty');
  results.push({
    case: 'selection and edits',
    exactAnchorBounds: true,
    browseNeutral: true,
    oneUndoDeltaAndFieldUpdates: true,
    singleAndEmpty: true,
    diskJsonReloadSvg: true,
  });
  for (const property of ['locked', 'expression']) {
    const f = make();
    f.path.points.find((p) => p.id === f.ids[1]).position[property] =
      property === 'locked' ? true : 'value; value';
    await load(f);
    const before = await saved(),
      command = await state();
    assert.equal(await bounds().getAttribute('data-state'), 'unavailable');
    assert.equal(await bounds().locator('[data-point-bound]').count(), 0);
    assert.match(
      await bounds().innerText(),
      property === 'locked' ? /Locked position/ : /Driven position/,
    );
    assert.equal(await saved(), before);
    assert.deepEqual(await state(), command);
    results.push({ case: property, clearStateWithoutPartialNumbers: true });
  }
  await bounds().scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'unavailable-bounds.png') });
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(output, 'receipt.json'),
    JSON.stringify(
      {
        renderer: 'Tauri Windows WebView2',
        input: 'Playwright CDP synthetic pointer/keyboard',
        results,
        errors,
        notRun: [
          'OS Save/Open dialogs',
          'cold-process reopen',
          'physical keyboard/IME',
          'disabled native computer-use APIs',
        ],
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify({ pass: results.length, results, errors }));
} catch (error) {
  if (page) await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
  throw error;
} finally {
  await browser.close();
}
