import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

const output = resolve('test-results/selected-point-delta/native');
await mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_DELTA_CDP_URL || 'http://127.0.0.1:9445',
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

  const f = await load(),
    before = await saved(),
    originalState = await state();
  await page.locator('.inspector-body [data-point-panel-contour]').selectOption('scope-hole');
  assert.match(await panel().innerText(), /2 selected points.*Node-local.*document units/s);
  await input(0).fill('12.5');
  await input(1).fill('-7.25');
  assert.equal(await saved(), before, 'typing and blur are draft-only');
  assert.equal(await page.locator('.graph [data-point-delta-axis="0"]').inputValue(), '12.5');
  await page.screenshot({ path: resolve(output, 'draft.png') });
  await input(1).press('Enter');
  assert.equal((await state()).expectedRevision, originalState.expectedRevision + 1);
  const after = await saved(),
    live = JSON.parse(after).layers[0].nodes.find((n) => n.id === f.path.id);
  assert.deepEqual(live.contours, f.path.contours);
  assert.deepEqual(live.units, f.path.units);
  for (const p of f.path.points) {
    const expected = structuredClone(p);
    if (f.ids.includes(p.id))
      expected.position.value = p.position.value.map((v, i) => v + [12.5, -7.25][i]);
    assert.deepEqual(
      live.points.find((q) => q.id === p.id),
      expected,
    );
  }
  assert.equal(await input(0).inputValue(), '0');
  await page.screenshot({ path: resolve(output, 'moved.png') });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), after);
  const file = resolve(output, 'translated.json'),
    svg = await page.evaluate(() => window.nect.export());
  await writeFile(file, after);
  await page.evaluate((text) => window.nect.load(text), await readFile(file, 'utf8'));
  assert.deepEqual(JSON.parse(await saved()), JSON.parse(after));
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  results.push({
    case: 'compound two-contour move',
    bothAxesOneUndoRedo: true,
    outOfFilterSelectionIncluded: true,
    idsClosureHoleGroupsHandlesUnselectedPreserved: true,
    diskJsonReloadAndSvg: true,
  });

  await load();
  const cancelBefore = await saved(),
    cancelState = await state();
  await input(0).fill('6');
  await input(1).fill('-4');
  await input(1).press('Escape');
  assert.equal(await input(0).inputValue(), '0');
  assert.equal(await input(1).inputValue(), '0');
  await input(0).fill('9');
  await panel().getByRole('button', { name: 'Cancel', exact: true }).click();
  assert.equal(await input(0).inputValue(), '0');
  await input(0).fill('0');
  await panel().getByRole('button', { name: 'Move points', exact: true }).click();
  assert.equal(await saved(), cancelBefore);
  assert.deepEqual(await state(), cancelState);
  await input(0).fill('');
  await panel().getByRole('button', { name: 'Move points', exact: true }).click();
  assert.equal(await saved(), cancelBefore);
  assert.deepEqual(await state(), cancelState);
  results.push({
    case: 'cancel and no-op',
    escapeBothAxes: true,
    explicitCancel: true,
    zeroAndInvalidNeutral: true,
  });

  const stale = await load();
  await input(0).fill('8');
  await page.evaluate(
    ({ layerId, nodeId, pointId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'translate-points',
        payload: { layerId, nodeId, pointIds: [pointId], delta: [1, 0] },
      }),
    { layerId: stale.layer.id, nodeId: stale.path.id, pointId: stale.path.contours[2].points[0] },
  );
  const staleBefore = await saved(),
    staleState = await state();
  await panel().getByRole('button', { name: 'Move points', exact: true }).click();
  assert.equal(await saved(), staleBefore);
  assert.deepEqual(await state(), staleState);
  assert.match(await page.locator('body').innerText(), /stale|revision/i);
  results.push({ case: 'stale revision', documentAndHistoryNeutral: true });

  for (const property of ['locked', 'expression']) {
    const f = make();
    f.path.points.find((p) => p.id === f.ids[1]).position[property] =
      property === 'locked' ? true : 'value; value';
    await load(f);
    const before = await saved(),
      command = await state();
    assert.equal(await input(0).isDisabled(), true);
    assert.equal(await input(1).isDisabled(), true);
    assert.equal(
      await panel().getByRole('button', { name: 'Move points', exact: true }).isDisabled(),
      true,
    );
    assert.equal(await saved(), before);
    assert.deepEqual(await state(), command);
    results.push({ case: property, fullSelectionDisabled: true });
  }
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
