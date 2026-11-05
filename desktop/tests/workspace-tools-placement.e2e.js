import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';

const output = resolve('test-results/workspace-tools-placement');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.addInitScript(() => {
  if (!sessionStorage.getItem('tools-placement-seeded')) {
    localStorage.setItem(
      'nect.workspace-drawers.v1',
      JSON.stringify({ left: null, right: 'inspector', lastSide: 'right' }),
    );
    sessionStorage.setItem('tools-placement-seeded', 'yes');
  }
});
const tools = page.locator('.tools');
const inspector = page.locator('[data-drawer-side="right"] .inspector');
const placement = page.locator('[data-workspace-tools-placement]');
const inspectorRail = page.locator('[data-workspace-panel="inspector"]');
const layersRail = page.locator('[data-workspace-panel="layers"]');
const alignRail = page.locator('[data-workspace-panel="align"]');
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () => page.evaluate(() => window.nect.commandState);

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const before = await snapshot();
  const beforeState = await state();
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('canvas-wrap')),
    true,
  );
  assert.equal(await placement.getAttribute('aria-pressed'), 'false');
  await tools.evaluate((element) => (window.originalCreationTools = element));
  await placement.focus();
  await page.keyboard.press('Space');
  assert.equal(await placement.getAttribute('aria-pressed'), 'true');
  assert.equal(await tools.evaluate((element) => element === window.originalCreationTools), true);
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('inspector')),
    true,
  );
  assert.equal(await placement.evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.screenshot({ path: resolve(output, 'wide.png') });

  const select = tools.getByRole('button', { name: 'Select / move', exact: true });
  await select.focus();
  await page.keyboard.press('Enter');
  assert.equal(await select.getAttribute('aria-pressed'), 'true');
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('inspector')),
    true,
  );
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await page.setViewportSize({ width: 1000, height: 650 });
  const textTool = tools.getByRole('button', { name: 'Horizontal Text', exact: true });
  await textTool.focus();
  const box = await textTool.boundingBox();
  assert.ok(box && box.width > 20 && box.height > 20);
  assert.equal(
    await textTool.evaluate(
      (element, point) => {
        const hit = document.elementFromPoint(point.x, point.y);
        return element === hit || element.contains(hit);
      },
      { x: box.x + box.width / 2, y: box.y + box.height / 2 },
    ),
    true,
  );
  await page.screenshot({ path: resolve(output, 'short.png') });
  await page.setViewportSize({ width: 420, height: 520 });
  await textTool.focus();
  await page.waitForFunction(() => {
    const shelf = document.querySelector('.tools');
    const focused = document.activeElement;
    const a = shelf.getBoundingClientRect();
    const b = focused.getBoundingClientRect();
    return (
      focused.getAttribute('aria-label') === 'Horizontal Text' &&
      b.left >= a.left &&
      b.right <= a.right
    );
  });
  const narrowBox = await textTool.boundingBox();
  assert.ok(narrowBox && narrowBox.width > 20 && narrowBox.height > 20);
  await page.screenshot({ path: resolve(output, 'narrow-stress.png') });
  assert.equal(
    await textTool.evaluate(
      (element, point) => {
        const hit = document.elementFromPoint(point.x, point.y);
        return element === hit || element.contains(hit);
      },
      { x: narrowBox.x + narrowBox.width / 2, y: narrowBox.y + narrowBox.height / 2 },
    ),
    true,
  );
  await page.setViewportSize({ width: 1000, height: 650 });

  // A real pointer held on the tool must finish before a placement activation.
  const selected = tools.getByRole('button', { name: 'Select / move', exact: true });
  const selectedBox = await selected.boundingBox();
  await page.mouse.move(
    selectedBox.x + selectedBox.width / 2,
    selectedBox.y + selectedBox.height / 2,
  );
  await page.mouse.down();
  await placement.evaluate((element) => element.click());
  assert.equal(await placement.getAttribute('aria-pressed'), 'true');
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('inspector')),
    true,
  );
  await page.mouse.up();

  const name = inspector.locator('[data-layer-name]');
  await name.scrollIntoViewIfNeeded();
  const nameBox = await name.boundingBox();
  assert.ok(nameBox);
  assert.equal(
    await name.evaluate(
      (element, point) => document.elementFromPoint(point.x, point.y) === element,
      { x: nameBox.x + nameBox.width / 2, y: nameBox.y + nameBox.height / 2 },
    ),
    true,
  );
  await name.focus();
  const originalName = await name.inputValue();
  await name.evaluate((element) => {
    element.value = '編集中';
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
  });
  await placement.click();
  assert.equal(await name.evaluate((element) => element === document.activeElement), true);
  assert.equal(await name.inputValue(), '編集中');
  assert.equal(await placement.getAttribute('aria-pressed'), 'true');
  await name.evaluate((element, value) => {
    element.value = value;
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true }));
  }, originalName);
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  // A normal draft commits once and the first placement click still lands.
  await name.fill('Renamed layer');
  await placement.click();
  assert.equal(await placement.getAttribute('aria-pressed'), 'false');
  assert.equal((await snapshot()).compositions[0].layers[0].name, 'Renamed layer');
  assert.equal((await state()).expectedRevision, beforeState.expectedRevision + 1);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), before);
  await placement.click();
  assert.equal(await placement.getAttribute('aria-pressed'), 'true');

  await tools.getByRole('button', { name: 'Add Rectangle', exact: true }).click();
  const created = await snapshot();
  assert.notDeepEqual(created, before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await snapshot(), created);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), before);

  await tools.evaluate((element) => (window.toolsBeforePanelSwitch = element));
  await alignRail.click();
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('canvas-wrap')),
    true,
  );
  assert.equal(await tools.evaluate((element) => element === window.toolsBeforePanelSwitch), true);
  await inspectorRail.click();
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('inspector')),
    true,
  );
  assert.equal(await tools.evaluate((element) => element === window.toolsBeforePanelSwitch), true);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  assert.equal(await placement.getAttribute('aria-pressed'), 'true');
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('inspector')),
    true,
  );
  await tools.getByRole('button', { name: 'Horizontal Text', exact: true }).focus();
  await page.keyboard.press('Escape');
  assert.equal(await inspectorRail.getAttribute('aria-expanded'), 'false');
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('canvas-wrap')),
    true,
  );
  assert.equal(await inspectorRail.evaluate((element) => element === document.activeElement), true);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  assert.equal(await inspectorRail.getAttribute('aria-expanded'), 'false');
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('canvas-wrap')),
    true,
  );
  await layersRail.click();
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('inspector')),
    true,
  );
  await placement.click();
  assert.equal(await placement.getAttribute('aria-pressed'), 'false');
  assert.equal(
    await tools.evaluate((element) => element.parentElement.classList.contains('canvas-wrap')),
    true,
  );
  assert.equal(
    await page.evaluate(
      () => JSON.parse(localStorage.getItem('nect.workspace-drawers.v1')).toolsAbovePanel,
    ),
    undefined,
  );
  assert.deepEqual(errors, []);
  console.log(
    'Workspace tools: optional placement, keyboard, IME guard, preference and return passed',
  );
} finally {
  await browser.close();
}
