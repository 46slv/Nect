import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { blankDocument, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const doc = blankDocument(),
  layer = doc.layers[0],
  curve = node('Curve'),
  transform = node('Transform');
curve.id = 'handle-curve';
curve.name = 'Outline';
curve.points[0].id = 'anchor';
curve.points[0].handleA.value = [3, 4];
curve.points[0].handleB.value = [-12, 5];
transform.id = 'handle-target';
layer.nodes.push(curve, transform);
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
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await ensureWorkspacePanel(page, 'inspector');
  await page.evaluate((source) => window.nect.load(source), serialize(doc));
  await page.locator('.node[data-id="handle-target"] .node-head strong').click();
  const field = page.locator(
    '.inspector-body .field[data-node="handle-target"][data-key="rotation"]',
  );
  for (const [name, expression, expected] of [
    ['Incoming · Length', 'ref("handle-curve", "anchor.handleA", "length")', 5],
    ['Outgoing · X', 'ref("handle-curve", "anchor.handleB", "x")', -12],
  ]) {
    await field.locator('[data-fx]').click();
    const dialog = page
      .locator('dialog[open]')
      .filter({ has: page.locator('[data-insert-reference]') });
    await dialog.locator('[aria-label="Parameter expression"]').fill('');
    await dialog.locator('[data-insert-reference]').click();
    const picker = page.getByRole('dialog', { name: 'Parameter references' });
    await picker.getByRole('combobox').fill(`Point 1 ${name.replace(' · ', ' ')}`);
    await picker.getByRole('option', { name: `Outline · Point 1 · ${name}`, exact: true }).click();
    await picker.locator('.picker-apply').click();
    assert.equal(
      await dialog.locator('[aria-label="Parameter expression"]').inputValue(),
      expression,
    );
    await dialog.locator('[data-confirm]').click();
    await dialog.waitFor({ state: 'detached' });
    const result = await page.evaluate(async () => {
      const { evaluate } = await import('/src/core.js');
      const snapshot = window.nect.snapshot();
      return {
        schema: snapshot.schemaVersion,
        expression: snapshot.layers[0].nodes.find((n) => n.id === 'handle-target').params.rotation
          .expression,
        value: evaluate(snapshot.layers[0]).parameterValue('handle-target', 'rotation'),
      };
    });
    assert.deepEqual(result, { schema: 61, expression, value: expected });
  }
  assert.deepEqual(errors, []);
  console.log('PASS browser incoming/outgoing handle reference picker and Apply');
} finally {
  await browser.close();
}
