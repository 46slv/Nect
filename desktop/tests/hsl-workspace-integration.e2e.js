import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { node, connect, serialize } from '../src/core.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';
import { updateHexHsl } from '../src/hsl-color-picker.js';

// Browser evidence for the actual drawer surfaces and existing Session owner.
// Physical Windows/WebView2 and OS IME acceptance remain separate.
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (route) => route.abort());
const documentText = () => page.evaluate(() => JSON.stringify(window.nect.document));
const channel = (name) => page.locator(`.hsl-color-popover [data-hsl-number="${name}"]`);
const selectColor = async (id) => {
  await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
};
const openAdvanced = async () => {
  const details = page.locator('[data-color-advanced]');
  await details.waitFor({ state: 'visible' });
  if (!(await details.evaluate((el) => el.open))) await details.locator('summary').click();
};
const canHit = async (locator, name, minWidth = 1) => {
  await locator.scrollIntoViewIfNeeded();
  const hit = await locator.evaluate((el) => {
    const r = el.getBoundingClientRect();
    const top = document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2);
    return { width: r.width, height: r.height, hit: top === el || el.contains(top) };
  });
  assert.ok(hit.width >= minWidth && hit.height > 0 && hit.hit, `${name} is visible and reachable`);
};
try {
  await mkdir('test-results', { recursive: true });
  const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);
  await page.evaluate(() => document.fonts.ready);
  const id = await page.evaluate(async () => {
    const { document, node } = await import('/src/core.js');
    const d = document(),
      rect = node('Rectangle', 40, 40),
      fill = node('Fill', 360, 40),
      out = node('Output', 680, 40);
    d.width = 320;
    d.height = 240;
    rect.params.center.value = [160, 120];
    rect.params.size.value = [160, 120];
    fill.params.path.source = rect.id;
    fill.params.color.value = { hex: '#ff0000', alpha: 0.5 };
    out.params.path.source = fill.id;
    d.layers[0].nodes = [rect, fill, out];
    d.layers[0].output = out.id;
    window.nect.load(JSON.stringify(d));
    return fill.id;
  });
  await selectColor(id);
  const field = page.locator(`.inspector-body .field[data-node="${id}"][data-key="color"]`);
  assert.equal(await field.locator('[data-color^="hsl-"]').count(), 0);
  assert.equal(await field.locator('input[type="color"]').count(), 0);
  assert.equal(
    await field.locator('[data-hsl-picker-open]').getAttribute('style'),
    'background:#ff0000',
  );
  assert.equal(await field.locator('[data-color="alpha"]').inputValue(), '0.5');
  await field.locator('[data-hsl-picker-open]').click();
  for (const name of ['h', 's', 'l']) assert.equal(await channel(name).isVisible(), true);
  const original = await documentText();
  await channel('h').fill('');
  assert.equal(await documentText(), original, 'incomplete HSL stays unauthored');
  await channel('h').press('Tab');
  assert.equal(await documentText(), original, 'incomplete HSL cannot commit');
  await channel('h').fill('400');
  await channel('h').press('Tab');
  assert.equal(await documentText(), original, 'out-of-range HSL cannot commit');
  await channel('h').fill('120');
  await channel('h').press('Tab');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((n) => n.id === id).params.color.value.hex ===
      '#00ff00',
    id,
  );
  const edited = await documentText();
  // The typed command authors synchronously, but the editor intentionally
  // rerenders on the next task so an in-flight field keeps its focus/draft.
  await page.waitForFunction(
    (id) =>
      document
        .querySelector(
          `.inspector-body .field[data-node="${id}"][data-key="color"] [data-hsl-picker-open]`,
        )
        ?.getAttribute('style') === 'background:#00ff00',
    id,
  );
  assert.equal(
    await field.locator('[data-hsl-picker-open]').getAttribute('style'),
    'background:#00ff00',
  );
  assert.equal(await field.locator('[data-color="alpha"]').inputValue(), '0.5');
  await page.locator('[data-hsl-close]').click();
  assert.equal(
    await field.locator('[data-hsl-picker-open]').evaluate((el) => document.activeElement === el),
    true,
  );
  await activateCommand(page, 'undo');
  assert.equal(await documentText(), original, 'one Undo restores the color edit');
  await activateCommand(page, 'redo');
  assert.equal(await documentText(), edited, 'Redo restores the color edit');

  // Graph swatches open the same popup; connected fields keep their Base group.
  await page.locator('#add-node').selectOption('Color');
  const sourceId = await page.evaluate(() => window.nect.document.layers[0].nodes.at(-1).id);
  const graphField = page.locator(`.graph .field[data-node="${sourceId}"][data-key="value"]`);
  await canHit(graphField.locator('[data-hsl-picker-open]'), 'graph swatch', 32);
  await graphField.locator('[data-hsl-picker-open]').click();
  for (const name of ['h', 's', 'l']) await canHit(channel(name), `popup ${name}`, 40);
  await page.screenshot({ path: 'test-results/workflow-hsl-graph-1600.png' });
  await page.locator('[data-hsl-close]').click();
  await selectColor(id);
  await field.locator('[data-connect-menu]').click();
  await page.locator('dialog select').selectOption(sourceId);
  await page.locator('dialog [data-confirm]').click();
  const base = field.locator('.base-component[role="group"]');
  assert.match(await base.getAttribute('aria-label'), /base/i);
  assert.equal(await base.locator('[data-hsl-picker-open]').getAttribute('data-color-base'), '');
  await base.locator('[data-hsl-picker-open]').click();
  await channel('h').focus();
  await page.keyboard.press('Tab');
  assert.equal(await channel('s').evaluate((el) => document.activeElement === el), true);
  await page.keyboard.press('Tab');
  assert.equal(await channel('l').evaluate((el) => document.activeElement === el), true);
  await page.screenshot({ path: 'test-results/workflow-hsl-inspector-driven-1600.png' });
  await page.locator('[data-hsl-close]').click();

  await page.getByRole('button', { name: 'Toggle Shared Colors panel', exact: true }).click();
  await page.getByRole('button', { name: 'Add shared color', exact: true }).click();
  await openAdvanced();
  const slotId = await page.evaluate(() => window.nect.document.colorSlots[0].id);
  const row = () => page.locator(`[data-shared-color-row="${slotId}"]`);
  const shared = (name) => row().locator(`[data-shared-color-edit="hsl-${name}"]`);
  await shared('s').fill('100');
  await shared('s').press('Tab');
  await page.waitForFunction(
    (id) => window.nect.document.colorSlots.find((slot) => slot.id === id).value.hex !== '#808080',
    slotId,
  );
  let sharedBefore = await documentText();
  const scrub = shared('h');
  await scrub.scrollIntoViewIfNeeded();
  const scrubBox = await scrub.boundingBox(),
    scrubX = scrubBox.x + scrubBox.width * 0.4,
    scrubY = scrubBox.y + scrubBox.height / 2;
  assert.equal(
    await scrub.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, {
      x: scrubX,
      y: scrubY,
    }),
    true,
  );
  await page.mouse.move(scrubX, scrubY);
  await page.mouse.down();
  await page.mouse.move(scrubX + 24, scrubY, { steps: 5 });
  await page.mouse.up();
  await page.waitForTimeout(100);
  assert.notEqual(await documentText(), sharedBefore, 'shared HSL drag commits a change');
  await activateCommand(page, 'undo');
  assert.deepEqual(JSON.parse(await documentText()), JSON.parse(sharedBefore));
  sharedBefore = await documentText();
  const beforeState = await page.evaluate(() => window.nect.commandState);
  const gesture = async (name, action, dx = 24) => {
    const input = shared(name);
    await input.evaluate((el) => el.scrollIntoView({ block: 'center', inline: 'center' }));
    const box = await input.boundingBox(),
      x = box.x + box.width * 0.4,
      y = box.y + box.height / 2;
    assert.equal(
      await input.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, { x, y }),
      true,
    );
    await page.mouse.move(x, y);
    await page.mouse.down();
    if (action !== 'click') await page.mouse.move(x + dx, y, { steps: 5 });
    if (action === 'cancel') await page.keyboard.press('Escape');
    if (action === 'origin') await page.mouse.move(x, y, { steps: 5 });
    await page.mouse.up();
    await page.waitForFunction(
      () => !window.nect.gestureState && !window.nect.colorPreviewSnapshot(),
    );
  };
  for (const action of ['click', 'cancel', 'origin']) {
    await gesture('h', action);
    assert.deepEqual(
      JSON.parse(await documentText()),
      JSON.parse(sharedBefore),
      `${action} preserves color`,
    );
    assert.deepEqual(
      await page.evaluate(() => window.nect.commandState),
      beforeState,
      `${action} preserves history`,
    );
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  }
  await shared('h').focus();
  await shared('h').evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await gesture('h', 'commit');
  assert.deepEqual(
    JSON.parse(await documentText()),
    JSON.parse(sharedBefore),
    'composition blocks scrub',
  );
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeState);
  await shared('h').evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  await page.keyboard.press('Escape');
  for (const [typedChannel, typedValue, scrubChannel, expectedValue] of [
    ['h', '30', 's', '80'],
    ['s', '70', 'h', '20'],
  ]) {
    const before = JSON.parse(await documentText());
    const literal = before.colorSlots.find((slot) => slot.id === slotId).value.hex;
    const typedHex = updateHexHsl(literal, typedChannel, typedValue);
    await shared(typedChannel).fill(typedValue);
    await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
    await gesture(scrubChannel, 'commit', scrubChannel === 's' ? -24 : 24);
    const after = JSON.parse(await documentText());
    assert.equal(
      after.colorSlots.find((slot) => slot.id === slotId).value.hex,
      updateHexHsl(typedHex, scrubChannel, expectedValue),
    );
    await activateCommand(page, 'undo');
    assert.equal(
      JSON.parse(await documentText()).colorSlots.find((slot) => slot.id === slotId).value.hex,
      typedHex,
    );
    await activateCommand(page, 'undo');
    assert.deepEqual(JSON.parse(await documentText()), before);
  }
  sharedBefore = await documentText();
  await shared('h').fill('120');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() !== null);
  assert.equal(await documentText(), sharedBefore, 'live HSL preview does not author each input');
  await shared('h').press('Tab');
  await page.waitForFunction(() => window.nect.colorPreviewSnapshot() === null);
  const sharedAfter = await documentText();
  assert.notEqual(sharedAfter, sharedBefore);
  const value = await page.evaluate(
    (id) => window.nect.document.colorSlots.find((slot) => slot.id === id).value,
    slotId,
  );
  assert.equal(value.alpha, 1, 'shared HSL retains alpha');
  assert.ok(
    parseInt(value.hex.slice(3, 5), 16) > parseInt(value.hex.slice(1, 3), 16),
    'hue edit produces green',
  );
  await activateCommand(page, 'undo');
  assert.deepEqual(
    JSON.parse(await documentText()),
    JSON.parse(sharedBefore),
    'one Undo restores shared HSL gesture',
  );
  await activateCommand(page, 'redo');
  assert.deepEqual(JSON.parse(await documentText()), JSON.parse(sharedAfter));
  await openAdvanced();
  await shared('h').focus();
  await page.keyboard.press('Tab');
  assert.equal(await shared('s').evaluate((el) => document.activeElement === el), true);
  await page.keyboard.press('Tab');
  assert.equal(await shared('l').evaluate((el) => document.activeElement === el), true);
  await page.keyboard.press('Tab');
  assert.equal(
    await row()
      .locator('[data-shared-color-edit="hex"]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  for (const width of [1600, 900]) {
    await page.setViewportSize({ width, height: width === 900 ? 720 : 1000 });
    for (const name of ['h', 's', 'l']) await canHit(shared(name), `shared ${name} at ${width}`);
    assert.equal(
      await page.evaluate(() => document.documentElement.scrollWidth > innerWidth),
      false,
    );
    await page.screenshot({ path: `test-results/workflow-hsl-workspace-${width}.png` });
  }
  await page.setViewportSize({ width: 1440, height: 960 });
  const boundaryEvidence = [];
  const project = () => page.evaluate(() => window.nect.projectSnapshot());
  const state = () => page.evaluate(() => window.nect.commandState);
  async function loadBoundary(hex) {
    const d = retainedRecipeFixture(),
      layer = d.layers[0],
      shape = node('Rectangle', 40, 40),
      fill = node('Fill', 360, 40),
      out = node('Output', 680, 40);
    fill.params.color.value = { hex, alpha: 0 };
    layer.nodes = [shape, fill, out];
    layer.output = out.id;
    connect(layer, shape.id, fill.id, 'path');
    connect(layer, fill.id, out.id, 'path');
    await page.evaluate((text) => window.nect.load(text), serialize(d));
    await selectColor(fill.id);
    const swatch = page.locator(
      `.inspector-body .field[data-node="${fill.id}"][data-key="color"] [data-hsl-picker-open]`,
    );
    return { fillId: fill.id, swatch };
  }
  for (const [source, edit, raw, expected, directHex] of [
    ['#000011', 'h', '120', '#001100', false],
    ['#000011', 'l', '50', '#0000ff', false],
    ['#fbfbff', 'h', '120', '#fbfffb', false],
    ['#fbfbff', 'l', '50', '#0000ff', false],
    ['#aBcD09', 'h', '120', '#001100', true],
  ]) {
    const { fillId, swatch } = await loadBoundary(source);
    const original = await project(),
      originalState = await state();
    await swatch.click();
    await page.locator('[data-hsl-close]').click();
    assert.deepEqual(
      await project(),
      original,
      'opening/closing keeps literal spelling and alpha0',
    );
    assert.deepEqual(await state(), originalState);
    await swatch.click();
    if (directHex) {
      await page.locator('[data-hsl-hex]').fill('#000011');
      await channel('h').click();
    }
    const before = await project(),
      beforeState = await state();
    await channel('s').fill('100.00000000000003');
    await channel('s').press('Tab');
    assert.deepEqual(await project(), before, 'user S above 100 stays rejected');
    assert.deepEqual(await state(), beforeState);
    await channel(edit).fill(raw);
    assert.deepEqual(await project(), before, 'popup input remains unauthored before change');
    await channel(edit).press('Tab');
    const after = await project(),
      wanted = structuredClone(before);
    wanted.compositions[0].layers[0].nodes.find((n) => n.id === fillId).params.color.value.hex =
      expected;
    assert.deepEqual(
      after,
      wanted,
      'only the intended literal changes; alpha0, recipe, legacy and references survive',
    );
    assert.equal((await state()).expectedRevision, beforeState.expectedRevision + 1);
    if (boundaryEvidence.length === 0)
      await page
        .locator('.hsl-color-popover')
        .screenshot({ path: 'test-results/workflow-hsl-hex-boundary-popup.png' });
    await page.locator('[data-hsl-close]').click();
    await activateCommand(page, 'undo');
    assert.deepEqual(await project(), before);
    await activateCommand(page, 'redo');
    assert.deepEqual(await project(), after);
    boundaryEvidence.push({ source, edit, raw, expected, directHex, oneUndoRedo: true });
  }
  const { fillId: grayId, swatch: graySwatch } = await loadBoundary('#aAaAaA');
  const gray = await project(),
    grayState = await state();
  await graySwatch.click();
  await channel('h').fill('120');
  await channel('h').press('Tab');
  assert.deepEqual(await project(), gray, 'gray hue intent must not rewrite its source literal');
  assert.deepEqual(await state(), grayState);
  await channel('s').fill('100');
  await channel('s').press('Tab');
  const green = structuredClone(gray);
  green.compositions[0].layers[0].nodes.find((n) => n.id === grayId).params.color.value.hex =
    '#55ff55';
  assert.deepEqual(
    await project(),
    green,
    'adding saturation uses the selected gray hue and preserves alpha0',
  );
  await page.locator('[data-hsl-close]').click();
  await activateCommand(page, 'undo');
  assert.deepEqual(await project(), gray);
  await activateCommand(page, 'redo');
  assert.deepEqual(await project(), green);
  assert.deepEqual(errors, []);
  await writeFile(
    'test-results/workflow-hsl-workspace.json',
    JSON.stringify(
      {
        ok: true,
        viewportWidths: [1600, 900],
        boundaryEvidence,
        grayHueIntent: true,
        limits: ['Headless Chrome; physical Windows/WebView2 and OS IME are not tested.'],
      },
      null,
      2,
    ),
  );
  console.log(
    'HSL workspace: swatch popup, inspector focus/Undo, Shared Colors live HSL and narrow hit testing PASS',
  );
} finally {
  await browser.close();
}
