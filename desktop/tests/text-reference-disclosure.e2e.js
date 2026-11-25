import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  text = node('Text', 40, 40),
  transform = node('Transform', 370, 40),
  driver = node('Vector', 680, 40);
text.width = 220;
text.text = 'AgW';
text.params.position.value = [120, 180];
driver.params.value.value = [240, 320];
layer.nodes[0].position = [940, 40];
layer.nodes.push(text, transform, driver);
connect(layer, driver.id, text.id, 'position');
connect(layer, text.id, transform.id, 'path');
connect(layer, transform.id, layer.output, 'path');

const base = new URL(process.env.BASE_URL || 'http://127.0.0.1:5173');
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
});
try {
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  await page.route('**/*', (route) =>
    new URL(route.request().url()).origin === base.origin ? route.continue() : route.abort(),
  );
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto(base.href);
  await page.waitForFunction(() => window.nect && !window.nect.evaluate().error);
  await ensureWorkspacePanel(page, 'inspector');
  const card = page.locator(`.graph .node[data-id="${text.id}"]`);
  const inspector = page.locator(`.inspector-body[data-inspector-node="${text.id}"]`);
  const fold = card.locator('details[data-layout-disclosure="text-reference"]');
  const summary = fold.getByText('More position references', { exact: true });
  const bounds = card.getByRole('combobox', { name: 'Text reference bounds', exact: true });
  const cross = card.getByRole('combobox', { name: 'Text cross-axis reference', exact: true });
  const positionRow = card.locator('.field[data-key="position"]');
  const undo = page.locator('[data-action="undo"]');
  const redo = page.locator('[data-action="redo"]');
  const state = () =>
    page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      file: window.nect.fileState,
      revision: window.nect.projectRevision,
    }));
  const selectNode = (id) =>
    page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  const load = async (fixture = doc) => {
    await page.evaluate((json) => window.nect.load(json), serialize(fixture));
    await selectNode(text.id);
    await inspector.waitFor({ state: 'visible' });
  };
  await load();
  const before = await state();
  assert.equal(await fold.getAttribute('open'), null);
  for (const attr of [
    'data-text',
    'data-change-font',
    'data-writing-mode',
    'data-text-alignment',
    'data-text-wrap',
    'data-text-leading',
  ])
    assert.ok(await card.locator(`[${attr}]`).isVisible(), attr);
  assert.ok(await card.locator('.field[data-key="size"] [data-value]').isVisible());
  assert.ok(await positionRow.locator('.socket.input').isVisible());
  assert.match(await positionRow.locator('output.parameter-result').textContent(), /240, 320/);
  assert.ok(await card.getByRole('button', { name: 'Text Path output', exact: true }).isVisible());
  assert.ok(
    await card.getByRole('button', { name: 'Text Position output', exact: true }).isVisible(),
  );
  assert.equal(await bounds.isVisible(), false);
  assert.equal(await cross.isVisible(), false);

  // Keyboard navigation, independent Inspector state, and view-only rerenders.
  await summary.press('Enter');
  assert.ok(await bounds.isVisible());
  await summary.press('Tab');
  assert.equal(await bounds.evaluate((el) => document.activeElement === el), true);
  await selectNode(transform.id);
  await selectNode(text.id);
  assert.ok(await bounds.isVisible());
  const typography = inspector.locator('[data-text-inspector-section="typography"]');
  assert.equal(await typography.getAttribute('open'), null);
  await summary.press('Enter');
  assert.equal(await bounds.isVisible(), false);
  assert.equal(await typography.getAttribute('open'), null);
  await typography.locator(':scope > summary').click();
  await inspector.getByRole('group', { name: 'Text reference bounds', exact: true }).waitFor();
  assert.equal(await bounds.isVisible(), false);
  assert.deepEqual(await state(), before);
  assert.equal(await undo.isDisabled(), true);
  assert.equal(await redo.isDisabled(), true);

  // An authored reference remains visible even if this surface was closed.
  await inspector
    .getByRole('button', { name: 'Text cross-axis reference: Center', exact: true })
    .click();
  await cross.waitFor({ state: 'visible' });
  assert.equal(await fold.count(), 0);
  assert.equal(await cross.inputValue(), 'center');
  const changed = await state();
  assert.notEqual(changed.svg, before.svg);
  const readTransform = ({ doc }) =>
    JSON.parse(doc).layers[0].nodes.find((n) => n.id === transform.id);
  assert.deepEqual(readTransform(changed), readTransform(before));
  await undo.click();
  await fold.waitFor();
  assert.equal(await fold.getAttribute('open'), null);
  assert.equal((await state()).doc, before.doc);
  assert.equal((await state()).svg, before.svg);
  await redo.click();
  await cross.waitFor({ state: 'visible' });
  assert.equal((await state()).doc, changed.doc);
  assert.equal((await state()).svg, changed.svg);
  await undo.click();
  await summary.press('Enter');
  await bounds.selectOption('advance');
  await fold.waitFor({ state: 'detached' });
  assert.equal(await bounds.inputValue(), 'advance');
  await undo.click();
  await bounds.waitFor();
  assert.equal(await bounds.inputValue(), 'visible');
  await summary.press('Enter');
  assert.deepEqual((await state()).doc, before.doc);

  // The closed disclosure does not obstruct the graph's connected value or reconnect menu.
  for (const [value, expected] of [
    ['', [120, 180]],
    [driver.id, [240, 320]],
  ]) {
    await positionRow.locator('[data-connect-menu]').click();
    await page.locator('dialog select').selectOption(value);
    await page.locator('dialog [data-confirm]').click();
    assert.deepEqual(
      await page.evaluate((id) => window.nect.evaluate().run(id, 'position'), text.id),
      expected,
    );
    assert.equal(await fold.getAttribute('open'), null);
  }
  assert.equal((await state()).doc, before.doc);

  // An existing suspension warning stays visible even with both graph disclosures closed.
  const gaps = card.locator('[data-layout-disclosure="text-gaps"]');
  await gaps.locator('summary').click();
  await gaps.getByRole('button', { name: 'Apply', exact: true }).click();
  await card.getByRole('button', { name: 'Disable maintained gaps', exact: true }).waitFor();
  await gaps.locator('summary').click();
  const content = card.getByRole('textbox', { name: 'Text content', exact: true });
  await content.fill('Ag W');
  await content.press('Tab');
  const warning = card.locator('[data-text-gaps-warning]');
  await warning.waitFor({ state: 'visible' });
  assert.match(await warning.textContent(), /Suspended:/);
  assert.equal(await gaps.getAttribute('open'), null);
  assert.equal(await fold.getAttribute('open'), null);
  assert.deepEqual(errors, []);
  console.log(
    'Text reference disclosure: focus, rerender, references, warning, graph reconnect and Undo/Redo passed',
  );
} finally {
  await browser.close();
}
