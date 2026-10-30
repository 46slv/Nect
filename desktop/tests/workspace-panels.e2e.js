import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';

const out = resolve('test-results/workspace-panels');
await mkdir(out + '/parts', { recursive: true });
await writeFile(out + '/parts/blue.svg', '<svg width="40" height="30"><rect width="40" height="30" fill="#2468ab"/></svg>');
const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true, args: ['--no-sandbox', '--disable-dev-shm-usage'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const state = () => page.evaluate(() => {
  const rect = (selector) => { const r = document.querySelector(selector).getBoundingClientRect(); return [r.x, r.y, r.width, r.height]; };
  const svg = document.querySelector('.artboard');
  return { stage: rect('.canvas-stage'), graph: rect('.graph-panel'), svg: rect('.artboard'), zoom: document.querySelector('[data-zoom="canvas"]').textContent, graphScroll: [document.querySelector('.graph-scroll').scrollLeft, document.querySelector('.graph-scroll').scrollTop], document: JSON.stringify(window.nect.document), undo: document.querySelector('[data-action="undo"]').disabled, redo: document.querySelector('[data-action="redo"]').disabled };
});
const same = (a, b, label) => assert.deepEqual(a, b, label);
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.screenshot({ path: out + '/01-closed.png' });
  const initial = await state();
  const svgIdentity = await page.locator('.artboard').elementHandle();
  const insideShape = await page.evaluate(() => [...document.querySelectorAll('.workspace-panel-bar')].every((bar) => {
    const path = bar.querySelector('path'), inverse = path.getScreenCTM().inverse();
    return [...bar.querySelectorAll('button')].every((button) => {
      const r = button.getBoundingClientRect();
      return [[r.left + 1, r.top + 1], [r.right - 1, r.top + 1], [r.left + 1, r.bottom - 1], [r.right - 1, r.bottom - 1]].every(([x,y]) => path.isPointInFill(new DOMPoint(x,y).matrixTransform(inverse)));
    });
  }));
  assert.equal(insideShape, true, 'all button hit boxes fit inside the accepted silhouette');
  const stage = page.locator('.canvas-stage'), graph = page.locator('.graph-panel');
  const initialGraph = await graph.boundingBox();
  assert.ok(initialGraph.x <= initial.stage[0], 'Graph extends under the upper creation rail');
  for (const name of ['Focus Layers', 'Focus Inspector', 'Toggle Alignment panel']) {
    await page.getByRole('button', { name, exact: true }).click();
    same(await state(), initial, `${name} cannot resize or recenter either surface`);
  }
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  const panel = page.locator('[data-asset-library]');
  await panel.waitFor({ state: 'visible' });
  const identity = await panel.elementHandle();
  same(await state(), initial, 'opening both sides is view-only');
  const chooser = page.waitForEvent('filechooser');
  await panel.getByRole('button', { name: 'Add folder', exact: true }).click();
  await (await chooser).setFiles(out + '/parts');
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('blue');
  await panel.getByRole('button', { name: 'blue.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  await page.screenshot({ path: out + '/02-assets-and-alignment.png' });
  await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  same(await state(), initial, 'switching left panel leaves both surfaces stable');
  assert.equal(await page.evaluate((node) => node === document.querySelector('.artboard'), svgIdentity), true, 'view-only toggles do not replace or refit the canvas');
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  assert.equal(await page.evaluate((node) => node === document.querySelector('[data-asset-library]'), identity), true);
  assert.equal(await page.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(), 'blue');
  same(await state(), initial, 'restoring Assets retains viewport and document');
  const boundaries = await page.evaluate(() => {
    const rect = (s) => { const r = document.querySelector(s).getBoundingClientRect(); return { top:r.top, bottom:r.bottom, left:r.left, right:r.right }; };
    return { left: rect('[data-drawer-side="left"]'), right: rect('[data-drawer-side="right"]'), stage: rect('.canvas-stage'), graph: rect('.graph-panel') };
  });
  for (const side of ['left','right']) {
    assert.ok(boundaries[side].top >= boundaries.stage.top - 1 && boundaries[side].bottom <= boundaries.stage.bottom + 1, `${side} drawer stays in stage`);
    assert.ok(boundaries[side].bottom <= boundaries.graph.top + 1, `${side} drawer never covers graph`);
  }
  await page.evaluate(() => { const dialog = document.createElement('dialog'); dialog.id = 'workspace-modal-probe'; dialog.innerHTML = '<button>Modal owner</button>'; document.body.append(dialog); dialog.showModal(); });
  await page.keyboard.press('Escape');
  assert.equal(await panel.count(), 1, 'native modal owns Escape');
  await page.evaluate(() => document.querySelector('#workspace-modal-probe').remove());
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).focus();
  await page.keyboard.press('Escape');
  await panel.waitFor({ state: 'detached' });
  await page.getByRole('button', { name: 'Toggle Alignment panel', exact: true }).click();
  same(await state(), initial, 'closing is view-only');
  // Manual zoom and pan must remain stable too, not only fit mode.
  await page.getByRole('button', { name: 'Artboard zoom in', exact: true }).click();
  await stage.hover();
  await page.mouse.wheel(40, 25);
  const manual = await state();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  same(await state(), manual, 'manual zoom and pan survive open');
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  same(await state(), manual, 'manual zoom and pan survive close');
  for (const width of [1000, 900]) {
    await page.setViewportSize({ width, height: 960 });
    const before = await state();
    await page.getByRole('button', { name: 'Focus Inspector', exact: true }).click();
    await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
    same(await state(), before, `both drawers preserve geometry at ${width}`);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
    await page.getByRole('button', { name: 'Focus Inspector', exact: true }).click();
    await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  }
  // A valid native numeric draft follows the existing blur/commit policy even
  // when the next click switches drawers; one Undo restores the authored value.
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  const oldWidth = await page.evaluate(() => window.nect.document.width);
  await page.locator('[data-dimension="width"]').fill(String(oldWidth + 11));
  await page.getByRole('button', { name: 'Focus Inspector', exact: true }).click();
  await page.waitForFunction((value) => window.nect.document.width === value, oldWidth + 11);
  assert.equal(await page.locator('.inspector').isVisible(), true);
  await activateCommand(page, 'undo');
  assert.equal(await page.evaluate(() => window.nect.document.width), oldWidth);

  // The visible Artboard controls address the active Composition, not its selected layer.
  await activateCommand(page, 'add-composition');
  await page.getByRole('button', { name: 'Focus Layers', exact: true }).click();
  const projectBefore = await page.evaluate(() => window.nect.projectSnapshot());
  const activeId = await page.evaluate(() => window.nect.activeCompositionId);
  const target = projectBefore.compositions.find((composition) => composition.id === activeId);
  assert.ok((await page.locator('.artboard-settings').innerText()).includes(target.name));
  const width = page.locator('[data-dimension="width"]');
  await width.fill(String(target.width + 13));
  await width.press('Tab');
  const projectAfter = await page.evaluate(() => window.nect.projectSnapshot());
  assert.equal(projectAfter.compositions.find((composition) => composition.id === activeId).width, target.width + 13);
  assert.deepEqual(projectAfter.compositions.filter((composition) => composition.id !== activeId), projectBefore.compositions.filter((composition) => composition.id !== activeId));
  assert.deepEqual(errors, []);
  await writeFile(out + '/result.json', JSON.stringify({ ok: true, errors }, null, 2));
  console.log('workspace overlay PASS: stable fit/manual geometry, panel state, boundaries and asset identity');
} finally { await browser.close(); }
