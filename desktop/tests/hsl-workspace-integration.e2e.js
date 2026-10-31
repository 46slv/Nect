import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

// Browser evidence for the actual drawer surfaces and existing Session owner.
// The native OS color dialog itself is outside headless Chrome acceptance.
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const documentText = () => page.evaluate(() => JSON.stringify(window.nect.document));
const channel = (name) => page.locator(`.inspector-body [data-color="hsl-${name}"]`);
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
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
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
  for (const name of ['h', 's', 'l']) assert.equal(await channel(name).isVisible(), true);
  assert.equal(await field.locator('[data-color="hex"]').getAttribute('type'), 'color');
  assert.equal(await field.locator('[data-color="hex"]').inputValue(), '#ff0000');
  assert.equal(await field.locator('[data-color="alpha"]').inputValue(), '0.5');
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
  // The existing command owner defers a full render to the next task so a
  // numeric Tab transition keeps its focus and draft selection intact.
  const colorSelector = '.inspector-body [data-color="hex"]';
  await page.waitForFunction((s) => document.querySelector(s)?.value === '#00ff00', colorSelector);
  const edited = await documentText();
  assert.equal(await field.locator('[data-color="hex"]').inputValue(), '#00ff00');
  assert.equal(await field.locator('[data-color="alpha"]').inputValue(), '0.5');
  await activateCommand(page, 'undo');
  assert.equal(await documentText(), original, 'one Undo restores the color edit');
  await activateCommand(page, 'redo');
  assert.equal(await documentText(), edited, 'Redo restores the color edit');

  // Connected parameters retain their separately editable Base in a labelled
  // group, with keyboard order H → S → L rather than nested labels.
  await page.locator('#add-node').selectOption('Color');
  const sourceId = await page.evaluate(() => window.nect.document.layers[0].nodes.at(-1).id);
  const graphField = page.locator(`.graph .field[data-node="${sourceId}"][data-key="value"]`);
  for (const name of ['h', 's', 'l'])
    await canHit(graphField.locator(`[data-color="hsl-${name}"]`), `graph ${name}`, 40);
  await page.screenshot({ path: 'test-results/workflow-hsl-graph-1600.png' });
  await selectColor(id);
  await field.locator('[data-connect-menu]').click();
  await page.locator('dialog select').selectOption(sourceId);
  await page.locator('dialog [data-confirm]').click();
  const base = field.locator('.base-component[role="group"]');
  assert.match(await base.getAttribute('aria-label'), /base/i);
  for (const name of ['h', 's', 'l']) {
    assert.equal(
      await base.locator(`[data-color="hsl-${name}"]`).getAttribute('data-color-base'),
      '',
    );
  }
  const baseH = base.locator('[data-color="hsl-h"]');
  await baseH.focus();
  await page.keyboard.press('Tab');
  assert.equal(
    await base.locator('[data-color="hsl-s"]').evaluate((el) => document.activeElement === el),
    true,
  );
  await page.keyboard.press('Tab');
  assert.equal(
    await base.locator('[data-color="hsl-l"]').evaluate((el) => document.activeElement === el),
    true,
  );
  await page.screenshot({ path: 'test-results/workflow-hsl-inspector-driven-1600.png' });

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
  const sharedBefore = await documentText();
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
  assert.deepEqual(errors, []);
  await writeFile(
    'test-results/workflow-hsl-workspace.json',
    JSON.stringify(
      {
        ok: true,
        viewportWidths: [1600, 900],
        limits: ['Headless Chrome; native color dialog and OS IME are not tested.'],
      },
      null,
      2,
    ),
  );
  console.log(
    'HSL workspace: inspector and shared live/Undo, invalid input, narrow hit testing PASS',
  );
} finally {
  await browser.close();
}
