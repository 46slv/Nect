import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } }),
  errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  for (const keys of [['Control+d'], ['Control+c', 'Control+v']]) {
    await page.evaluate(async () => {
      const { blankDocument, node, connect } = await import('/src/core.js');
      const doc = blankDocument(),
        layer = doc.layers[0],
        driver = node('Value'),
        circle = node('Circle'),
        fill = node('Fill'),
        output = layer.nodes[0];
      doc.schemaVersion = 44;
      layer.id = 'expression-layer';
      layer.name = 'Expression artwork';
      driver.id = 'driver';
      driver.params.value.value = 42;
      circle.id = 'circle';
      circle.params.radius.expression = 'ref("driver", "value")';
      layer.nodes = [driver, circle, fill, output];
      connect(layer, circle.id, fill.id, 'path');
      connect(layer, fill.id, output.id, 'path');
      window.nect.load(JSON.stringify(doc));
    });
    const before = await saved();
    const row = page.locator('.layer[data-layer="expression-layer"]');
    await ensureWorkspacePanel(page, 'layers');
    await row.focus();
    for (const key of keys) await page.keyboard.press(key);
    await page.waitForFunction(() => window.nect.document.layers.length === 2);
    const { document: doc, values } = await page.evaluate(async () => {
      const { evaluate } = await import('/src/core.js');
      const document = window.nect.document;
      return {
        document,
        values: document.layers.map((layer) =>
          evaluate(layer).parameterValue(layer.nodes[1].id, 'radius'),
        ),
      };
    });
    assert.deepEqual(values, [42, 42]);
    assert.deepEqual(doc.layers[0], JSON.parse(before).compositions[0].layers[0]);
    const copy = doc.layers[1];
    assert.notEqual(copy.nodes[0].id, 'driver');
    assert.equal(copy.nodes[1].params.radius.expression, `ref("${copy.nodes[0].id}", "value")`);
    const after = await saved();
    await page.locator('[data-action="undo"]').click();
    assert.equal(await saved(), before);
    await page.locator('[data-action="redo"]').click();
    assert.equal(await saved(), after);
    await page.evaluate((text) => window.nect.load(text), after);
    assert.equal(await saved(), after, 'editable JSON retains both independent graphs');
    const result = await page.evaluate(
      ({ layerId, driverId, circleId }) => {
        window.nect.dispatch({
          ...window.nect.commandState,
          type: 'set-parameter',
          payload: { layerId, nodeId: driverId, key: 'value', value: 60 },
        });
        return window.nect.document.layers.map((layer) => layer.nodes[0].params.value.value);
      },
      { layerId: copy.id, driverId: copy.nodes[0].id, circleId: copy.nodes[1].id },
    );
    assert.deepEqual(result, [42, 60], 'editing the copy keeps the original Value unchanged');
  }
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({
    path: 'test-results/layer-duplicate-expression-refs.png',
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  console.log(
    'PASS layer ref() duplication: Ctrl+D, Ctrl+C/V, evaluation, Undo/Redo, JSON reopen, independent edit',
  );
} finally {
  await browser.close();
}
