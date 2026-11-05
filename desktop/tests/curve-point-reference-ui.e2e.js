import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
page.setDefaultTimeout(8000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    curve = node('Curve', 50, 80),
    transform = node('Transform', 450, 80),
    fill = node('Fill', 800, 80);
  curve.id = 'fixture-curve';
  curve.points[0].id = 'fixture-anchor';
  transform.id = 'fixture-transform';
  fill.id = 'fixture-fill';
  layer.nodes.push(curve, transform, fill);
  connect(layer, curve.id, transform.id, 'path');
  connect(layer, transform.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return {
    doc,
    layerId: layer.id,
    curveId: curve.id,
    pointId: curve.points[0].id,
    transformId: transform.id,
  };
}

const inspectorField = (key) =>
  page.locator(`.inspector-body .field[data-node="fixture-transform"][data-key="${key}"]`);
const graphField = (key) =>
  page.locator(`.graph .field[data-node="fixture-transform"][data-key="${key}"]`);
const snapshot = () => page.evaluate(() => window.nect.snapshot());
const transform = async () =>
  (await snapshot()).layers[0].nodes.find((n) => n.id === 'fixture-transform');

async function applyExpression(key, source, field = inspectorField) {
  await field(key).locator('[data-fx]').click();
  const dialog = page.locator('dialog[open]');
  await dialog.locator('[aria-label="Parameter expression"]').fill(source);
  await dialog.locator('[data-confirm]').click();
  await dialog.waitFor({ state: 'detached' });
}

async function removePoint(f) {
  // Use the same Session command as the point-row action inside a real browser.
  return page.evaluate(({ layerId, curveId, pointId }) => {
    try {
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'delete-point',
        payload: { layerId, nodeId: curveId, pointId },
      });
      return null;
    } catch (error) {
      return error.message;
    }
  }, f);
}

try {
  const f = fixture(),
    source = 'ref("fixture-curve", "fixture-anchor.position", "x")';
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5189');
  await page.waitForFunction(() => window.nect);
  await ensureWorkspacePanel(page, 'inspector');
  await page.evaluate((text) => window.nect.load(text), serialize(f.doc));
  await page.locator('.node[data-id="fixture-transform"] .node-head strong').click();
  await applyExpression('rotation', source);
  assert.equal((await transform()).params.rotation.expression, source);
  await page.locator('.graph [data-point-panel-mode="fixture-curve"]').click();
  await page
    .locator('.graph [data-node="fixture-curve"][data-delete-point="fixture-anchor"]')
    .click();
  await page.waitForFunction(
    () =>
      !window.nect
        .snapshot()
        .layers[0].nodes.find((n) => n.id === 'fixture-curve')
        .points.some((p) => p.id === 'fixture-anchor'),
  );
  await page.locator('.node[data-id="fixture-transform"] .node-head strong').click();
  assert.match(await inspectorField('rotation').locator('output').innerText(), /Cannot evaluate/);
  assert.match(await graphField('rotation').locator('output').innerText(), /Cannot evaluate/);
  assert.match(
    await inspectorField('rotation').locator('output').getAttribute('title'),
    /point is missing: fixture-anchor/,
  );
  assert.match(
    await graphField('rotation').locator('output').getAttribute('aria-description'),
    /point is missing: fixture-anchor/,
  );
  assert.equal((await transform()).params.rotation.expression, source);

  const position = inspectorField('position').locator('[data-value]').first();
  assert.equal(await position.isEnabled(), true);
  await position.fill('20');
  await position.press('Tab');
  assert.deepEqual((await transform()).params.position.value, [20, 0]);
  const uniform = inspectorField('uniformScale').locator('[data-value]');
  await uniform.uncheck();
  const width = inspectorField('scaleWidth').locator('[data-value]');
  await page.waitForFunction(() => {
    const input = document.querySelector(
      '.inspector-body .field[data-node="fixture-transform"][data-key="scaleWidth"] [data-value]',
    );
    return input && !input.disabled;
  });
  assert.equal(await width.isEnabled(), true);
  await width.fill('200');
  await width.press('Tab');
  assert.equal((await transform()).params.scaleWidth.value, 2);
  await applyExpression('rotation', '');
  assert.equal((await transform()).params.rotation.expression, '');
  assert.equal(await page.evaluate(() => window.nect.evaluate().error), null);
  await page.locator('[data-action="undo"]').click();
  assert.equal((await transform()).params.rotation.expression, source);
  assert.match(await inspectorField('rotation').locator('output').innerText(), /Cannot evaluate/);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await transform()).params.rotation.expression, '');

  // An unresolved virtual Position cannot show or edit a fabricated fallback.
  await page.evaluate((text) => window.nect.load(text), serialize(f.doc));
  await page.locator('.node[data-id="fixture-transform"] .node-head strong').click();
  await applyExpression('anchor', `${source}; 0`);
  assert.equal(await removePoint(f), null);
  const virtualPosition = inspectorField('position').locator('[data-value]');
  assert.equal(await virtualPosition.first().isDisabled(), true);
  assert.equal(await virtualPosition.first().inputValue(), '');
  assert.equal(await graphField('position').locator('[data-value]').first().isDisabled(), true);
  assert.match(await inspectorField('anchor').locator('output').innerText(), /Cannot evaluate/);
  await applyExpression('anchor', '', graphField);
  assert.equal(await inspectorField('position').locator('[data-value]').first().isEnabled(), true);

  await page.evaluate((text) => window.nect.load(text), serialize(f.doc));
  await page.locator('.node[data-id="fixture-transform"] .node-head strong').click();
  await applyExpression('scale', `${source} / 100`);
  assert.equal(await removePoint(f), null);
  const virtualWidth = inspectorField('scaleWidth').locator('[data-value]');
  assert.equal(await virtualWidth.isDisabled(), true);
  assert.equal(await virtualWidth.inputValue(), '');
  assert.match(await inspectorField('scale').locator('output').innerText(), /Cannot evaluate/);
  const rotation = inspectorField('rotation').locator('[data-value]');
  await rotation.fill('12');
  await rotation.press('Tab');
  assert.equal((await transform()).params.rotation.value, 12);
  await applyExpression('scale', '');
  assert.equal((await transform()).params.scale.expression, '');
  assert.deepEqual(errors, []);
  console.log('PASS browser Curve point missing, Transform controls and expression recovery');
} finally {
  await browser.close();
}
