import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { node, connect, serialize } from '../src/core.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';

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
  const value = () =>
    page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.color.value,
      id,
    );
  const openPicker = async () => {
    if (!(await page.locator('.hsl-color-popover').count()))
      await field.locator('[data-hsl-picker-open]').click();
  };
  const begin = async (dx, name = 'h') => {
    await openPicker();
    const input = channel(name),
      r = await input.boundingBox(),
      x = r.x + 12,
      y = r.y + r.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + dx, y, { steps: 6 });
    return { x, y };
  };
  await begin(24);
  assert.equal(await documentText(), original, 'preview does not author document');
  assert.equal(await channel('h').inputValue(), '20');
  await channel('s').dispatchEvent('pointerdown', {
    pointerId: 9,
    pointerType: 'pen',
    isPrimary: true,
    button: 0,
    bubbles: true,
  });
  await channel('s').dispatchEvent('pointerup', {
    pointerId: 9,
    pointerType: 'pen',
    isPrimary: true,
    button: 0,
    bubbles: true,
  });
  await page.mouse.up();
  assert.deepEqual(await value(), { hex: '#ff5500', alpha: 0.5 });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await begin(3);
  await page.mouse.up();
  assert.equal(await documentText(), original, 'jitter is no-op');
  let point = await begin(24);
  await page.mouse.move(point.x, point.y, { steps: 6 });
  await page.mouse.up();
  assert.equal(await documentText(), original, 'return to origin is no-op');
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await begin(24);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.equal(await documentText(), original, 'Escape cancels preview');
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await begin(24);
  await channel('h').dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await documentText(), original, 'pointer cancellation rolls back preview');
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await value(), { hex: '#ff5500', alpha: 0.5 }, 'no-ops retain the exact Redo');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  await openPicker();
  await channel('h').fill('30');
  await channel('h').click();
  assert.equal(await documentText(), original, 'click leaves pending typing unauthored');
  await begin(3);
  await page.mouse.up();
  assert.equal(await documentText(), original, 'jitter leaves pending typing unauthored');
  await begin(24);
  assert.deepEqual(await value(), { hex: '#ff8000', alpha: 0.5 });
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.deepEqual(await value(), { hex: '#ff8000', alpha: 0.5 }, 'cancel retains prior typing');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  await openPicker();
  await channel('h').fill('30');
  await begin(24);
  assert.deepEqual(
    await value(),
    { hex: '#ff8000', alpha: 0.5 },
    'pending typing commits before gesture',
  );
  await page.mouse.up();
  assert.deepEqual(await value(), { hex: '#ffd400', alpha: 0.5 });
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await value(), { hex: '#ff8000', alpha: 0.5 });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  for (const [typed, typedValue, dragged, dx, expected] of [
    ['h', '30', 's', -24, '80'],
    ['s', '80', 'h', 24, '20'],
  ]) {
    await openPicker();
    await channel(typed).fill(typedValue);
    assert.equal(await documentText(), original, 'cross-channel typing remains a draft');
    await begin(dx, dragged);
    const typedDocument = await documentText();
    assert.notEqual(typedDocument, original, 'blur commits typing before the other channel drag');
    assert.equal(
      await channel(dragged).inputValue(),
      expected,
      'first cross-channel drag previews',
    );
    await page.mouse.up();
    assert.notEqual(await documentText(), typedDocument, 'drag commits separately');
    assert.equal((await value()).alpha, 0.5);
    await page.locator('[data-action="undo"]').click();
    assert.equal(await documentText(), typedDocument, 'first Undo reverses only drag');
    await page.locator('[data-action="undo"]').click();
    assert.equal(await documentText(), original, 'second Undo reverses typing');
  }
  for (const [name, dx, expected] of [
    ['s', -24, '80'],
    ['l', 24, '70'],
  ]) {
    await begin(dx, name);
    assert.equal(await documentText(), original);
    assert.equal(await channel(name).inputValue(), expected);
    await page.mouse.up();
    assert.notEqual(await documentText(), original);
    assert.equal((await value()).alpha, 0.5);
    await page.locator('[data-action="undo"]').click();
    assert.equal(await documentText(), original);
  }
  await begin(24, 's');
  await page.mouse.up();
  assert.equal(await documentText(), original, 'saturation upper bound is a no-op');
  await begin(-104, 's');
  await page.mouse.up();
  assert.deepEqual(await value(), { hex: '#808080', alpha: 0.5 }, 'saturation reaches zero');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  await openPicker();
  await channel('h').dispatchEvent('compositionstart');
  await begin(24);
  await page.mouse.up();
  assert.equal(await documentText(), original, 'composition prevents a scrub');
  await channel('h').dispatchEvent('compositionend');
  await begin(24);
  await channel('h').press('Control+A');
  await page.keyboard.type('30');
  await page.mouse.up();
  assert.equal(await documentText(), original, 'new input cancels stale scrub');
  assert.equal(await channel('h').inputValue(), '30');
  await channel('h').press('Tab');
  assert.deepEqual(await value(), { hex: '#ff8000', alpha: 0.5 });
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  await openPicker();
  await channel('h').focus();
  await channel('h').press('ArrowUp');
  await channel('h').press('Tab');
  assert.notEqual(await documentText(), original, 'native keyboard step still commits');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  await begin(24);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 321 },
    }),
  );
  await page.mouse.up();
  assert.equal((await value()).hex, '#ff0000', 'revision change rejects late color commit');
  assert.equal(await page.locator('.hsl-color-popover').count(), 0, 'stale picker closes');
  assert.equal((await page.evaluate(() => window.nect.document)).width, 321);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), original);
  await begin(24);
  await page.evaluate(() => {
    const doc = structuredClone(window.nect.document);
    doc.width = 321;
    window.nect.load(JSON.stringify(doc));
  });
  await page.mouse.up();
  assert.equal((await value()).hex, '#ff0000', 'document replacement rejects old gesture');
  assert.equal(await page.locator('.hsl-color-popover').count(), 0);
  await page.evaluate(
    ({ id, original }) => {
      const doc = JSON.parse(original);
      doc.layers[0].nodes.find((node) => node.id === id).params.color.value = {
        hex: '#AaAaAa',
        alpha: 0,
      };
      window.nect.load(JSON.stringify(doc));
    },
    { id, original },
  );
  await selectColor(id);
  const gray = await documentText();
  await begin(24);
  await page.mouse.up();
  assert.equal(await documentText(), gray, 'gray hue preserves literal case and alpha zero');
  assert.equal(await channel('h').inputValue(), '20');
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  await begin(24, 's');
  await page.mouse.up();
  assert.notEqual((await value()).hex.toLowerCase(), '#aaaaaa');
  assert.equal((await value()).alpha, 0);
  assert.equal(await channel('h').inputValue(), '20', 'gray hue intent survives saturation edit');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await documentText(), gray);
  assert.deepEqual(errors, []);
  console.log(
    'HSL numeric scrub: preview, one Undo, cancel, origin, Redo, typed baseline and stale target PASS',
  );
} finally {
  await browser.close();
}
