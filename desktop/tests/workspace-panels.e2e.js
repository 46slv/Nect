import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const out = resolve('test-results/workspace-panels');
await mkdir(out + '/parts', { recursive: true });
await writeFile(out + '/parts/blue.svg', '<svg width="40" height="30"><rect width="40" height="30" fill="#2468ab"/></svg>');
for (let i = 0; i < 30; i++) await writeFile(`${out}/parts/blue-${String(i).padStart(2, '0')}.svg`, '<svg width="10" height="10"><rect width="10" height="10"/></svg>');
const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium', headless: true, args: ['--no-sandbox', '--disable-dev-shm-usage'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const state = () => page.evaluate(() => {
  const rect = (selector) => { const r = document.querySelector(selector).getBoundingClientRect(); return [r.x, r.y, r.width, r.height]; };
  const svg = document.querySelector('.artboard');
  return { stage: rect('.canvas-stage'), graph: rect('.graph-panel'), svg: rect('.artboard'), zoom: document.querySelector('[data-zoom="canvas"]').textContent, graphScroll: [document.querySelector('.graph-scroll').scrollLeft, document.querySelector('.graph-scroll').scrollTop], document: JSON.stringify(window.nect.document), dirty: window.nect.fileState.dirty, undo: document.querySelector('[data-action="undo"]').disabled, redo: document.querySelector('[data-action="redo"]').disabled };
});
const same = (a, b, label) => assert.deepEqual(a, b, label);
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(() => document.fonts.ready);
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
  const beforeViewer = await state();
  await page.getByRole('button', { name: 'Viewers 1 / 2', exact: true }).click();
  assert.equal(await panel.isVisible(), false, 'Assets portal is not interactive over Viewers');
  assert.equal(await panel.evaluate((el) => el.inert), true);
  assert.equal(await page.locator('.workspace-panel-bar-left').isVisible(), false);
  assert.deepEqual((await state()).graph, beforeViewer.graph);
  await page.locator('.graph .node .node-head strong').first().click();
  await ensureWorkspacePanel(page, 'inspector');
  const assign = page.locator('.viewer-assignment [data-assign-viewer="0"]');
  await assign.click();
  assert.notEqual(await page.locator('.node-viewer [data-viewer-status]').first().innerText(), 'Unassigned');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), beforeViewer.document);
  // The global Assets command has a visible effect even while its saved
  // left-panel state is suspended in Viewer mode.
  await activateCommand(page, 'asset-library');
  assert.equal(await page.getByRole('button', { name: 'Artwork', exact: true }).getAttribute('aria-pressed'), 'true');
  assert.equal(await panel.isVisible(), true);
  assert.equal(await page.evaluate((node) => node === document.querySelector('[data-asset-library]'), identity), true);
  assert.equal(await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(), 'blue');
  same(await state(), beforeViewer, 'viewer round trip keeps viewport and document');
  await ensureWorkspacePanel(page, 'align');
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
  assert.equal(await page.locator('#workspace-modal-probe').evaluate((dialog) => dialog.open), false);
  await page.evaluate(() => document.querySelector('#workspace-modal-probe').remove());
  await page.getByRole('searchbox', { name: 'Search assets', exact: true }).focus();
  await page.keyboard.press('Escape');
  await panel.waitFor({ state: 'detached' });
  const assetsTrigger = page.getByRole('button', { name: 'Toggle Assets panel', exact: true });
  assert.equal(await assetsTrigger.getAttribute('aria-expanded'), 'false');
  assert.equal(await assetsTrigger.evaluate((button) => button === document.activeElement), true);
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
  // An editing control owns Escape even with a Grid popover still open.
  await ensureWorkspacePanel(page, 'inspector');
  await page.getByRole('button', { name: 'Grid', exact: true }).click();
  const nameDraft = page.locator('.inspector-body [data-layer-name]');
  const oldName = await nameDraft.inputValue();
  await nameDraft.fill('未確定の名前');
  await nameDraft.press('Escape');
  assert.equal(await nameDraft.inputValue(), '未確定の名前');
  assert.equal(await nameDraft.evaluate((el) => el.isConnected && el === document.activeElement), true);
  assert.equal(await page.getByRole('button', { name: 'Grid', exact: true }).getAttribute('aria-expanded'), 'true');
  await nameDraft.fill(oldName);
  const gridPanel = page.locator('.guides-panel[aria-label="Grid"]');
  await gridPanel.evaluate((el) => { el.tabIndex = -1; el.focus(); });
  // Synthetic key events exercise the explicit guards; they do not simulate a
  // real OS input-method composition session.
  for (const init of [{ isComposing: true }, { keyCode: 229 }]) {
    const prevented = await gridPanel.evaluate((el, init) => {
      const event = new KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true, ...init });
      el.dispatchEvent(event);
      return event.defaultPrevented;
    }, init);
    assert.equal(prevented, false);
    assert.equal(await page.getByRole('button', { name: 'Grid', exact: true }).getAttribute('aria-expanded'), 'true');
  }
  await page.keyboard.press('Escape');
  assert.equal(await page.getByRole('button', { name: 'Grid', exact: true }).getAttribute('aria-expanded'), 'false');

  // Placement rerenders the editor, but the live library search/scroll and its
  // native chooser/async owner stay attached; insertion is one Undo.
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();
  await panel.waitFor({ state: 'visible' });
  const restoredIdentity = await panel.elementHandle();
  await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).fill('blue');
  await panel.getByRole('button', { name: 'blue.svg', exact: true }).click();
  await panel.locator('[data-asset-place-layer]:enabled').waitFor();
  const grid = panel.locator('.asset-library-grid');
  await grid.evaluate((el) => { el.scrollTop = 45; });
  const scrollBefore = await grid.evaluate((el) => el.scrollTop);
  assert.ok(scrollBefore > 0, 'fixture exercises a real scrolled asset list');
  const priorPlacement = await page.evaluate(() => JSON.stringify(window.nect.document));
  await panel.locator('[data-asset-place-layer]').click();
  await page.waitForFunction(() => window.nect.document.layers.some((layer) => layer.nodes.some((node) => node.type === 'SVG')));
  assert.equal(await page.evaluate((node) => node === document.querySelector('[data-asset-library]'), restoredIdentity), true);
  assert.equal(await panel.getByRole('searchbox', { name: 'Search assets', exact: true }).inputValue(), 'blue');
  assert.equal(await grid.evaluate((el) => el.scrollTop), scrollBefore);
  await activateCommand(page, 'undo');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), priorPlacement);
  await page.getByRole('button', { name: 'Toggle Assets panel', exact: true }).click();

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

  // The upper creation rail remains reachable when the graph is made tall.
  const splitter = page.getByRole('separator', { name: 'Resize node graph' });
  await splitter.focus();
  for (let i = 0; i < 7; i++) await splitter.press('ArrowUp');
  const lastTool = page.locator('.tools button').last();
  await lastTool.scrollIntoViewIfNeeded();
  const rail = await page.locator('.tools').boundingBox(), last = await lastTool.boundingBox();
  assert.ok(last.y >= rail.y && last.y + last.height <= rail.y + rail.height + 1);
  await page.setViewportSize({ width: 1000, height: 600 });
  for (const button of await page.locator('.workspace-panel-bar button').all()) {
    const b = await button.boundingBox(), bar = await button.locator('..').boundingBox();
    assert.ok(b && b.width >= 30 && b.height >= 30 && b.y >= bar.y && b.y + b.height <= bar.y + bar.height + 1, 'short split retains each usable edge target');
  }
  assert.equal(await page.evaluate(() => [...document.querySelectorAll('.workspace-panel-bar')].every((bar) => {
    const path = bar.querySelector('path'), inverse = path.getScreenCTM().inverse();
    return [...bar.querySelectorAll('button')].every((button) => {
      const r = button.getBoundingClientRect();
      return [[r.left + 1, r.top + 1], [r.right - 1, r.top + 1], [r.left + 1, r.bottom - 1], [r.right - 1, r.bottom - 1]].every(([x,y]) => path.isPointInFill(new DOMPoint(x,y).matrixTransform(inverse)));
    });
  })), true, 'short split keeps the same silhouette around all hit targets');

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
