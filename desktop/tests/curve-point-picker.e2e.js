import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, connect, node, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/curve-point-picker');
await mkdir(output, { recursive: true });
const pointCount = Number(process.env.POINT_COUNT || 10000);
const doc = blankDocument(),
  layer = doc.layers[0],
  curve = node('Curve', 30, 70),
  rectangle = node('Rectangle', 30, 330),
  transform = node('Transform', 380, 70),
  fill = node('Fill', 730, 70),
  out = layer.nodes[0];
curve.id = 'picker-curve';
curve.name = `Contour ${pointCount}`;
curve.points = Array.from({ length: pointCount }, (_, i) => {
  const point = structuredClone(curve.points[0]);
  point.id = `point-${i + 1}`;
  point.position.value = [i % 500, (i * 7) % 500];
  return point;
});
transform.id = 'picker-transform';
fill.id = 'picker-fill';
layer.nodes.push(curve, rectangle, transform, fill);
connect(layer, rectangle.id, transform.id, 'path');
connect(layer, transform.id, fill.id, 'path');
connect(layer, fill.id, out.id, 'path');
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
page.setDefaultTimeout(30000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const loadStart = Date.now();
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  console.log(`Loaded ${pointCount} points in ${Date.now() - loadStart} ms`);
  await page.locator('.node[data-id="picker-transform"] .node-head strong').click();
  await ensureWorkspacePanel(page, 'inspector');
  const field = page.locator(
    '.inspector-body .field[data-node="picker-transform"][data-key="rotation"]',
  );
  await field.locator('[data-fx]').click();
  const expressionDialog = page
    .locator('dialog[open]')
    .filter({ has: page.locator('[data-insert-reference]') });
  const start = Date.now();
  await expressionDialog.locator('[data-insert-reference]').click();
  const picker = page.getByRole('dialog', { name: 'Parameter references' });
  await picker.locator('.picker-option').first().waitFor();
  const openMs = Date.now() - start;
  const initialCount = await picker.locator('.picker-option').count();
  assert.equal(initialCount, Math.min(100, pointCount * 2 + 4));
  if (pointCount * 2 > 100)
    assert.match(await picker.locator('.picker-status').textContent(), /^1–100 of /);
  await page.screenshot({ path: resolve(output, 'large-picker.png') });
  await page.setViewportSize({ width: 900, height: 640 });
  await page.screenshot({ path: resolve(output, 'large-picker-narrow.png') });
  assert.equal(await picker.locator('.picker-apply').isVisible(), true);
  const search = picker.getByRole('combobox');
  await search.fill(`point-${pointCount}.position`);
  assert.equal(await picker.locator('.picker-option').count(), 2);
  const searchStart = Date.now();
  await picker
    .getByRole('combobox', { name: 'Parameter references search' })
    .fill(`Point ${pointCount}`);
  await picker.locator('.picker-option').first().waitFor();
  const searchMs = Date.now() - searchStart;
  const labels = await picker.locator('.picker-option').allTextContents();
  console.log(
    JSON.stringify({
      points: pointCount,
      openMs,
      initialCount,
      searchMs,
      first: labels.slice(0, 2),
    }),
  );
  assert.deepEqual(labels.slice(0, 2), [
    `Contour ${pointCount} · Point ${pointCount} · X`,
    `Contour ${pointCount} · Point ${pointCount} · Y`,
  ]);
  await search.press('ArrowDown');
  await search.press('ArrowUp');
  await search.press('Enter');
  const expression = `ref("picker-curve", "point-${pointCount}.position", "x")`;
  assert.equal(
    await expressionDialog.locator('[aria-label="Parameter expression"]').inputValue(),
    expression,
  );
  await expressionDialog.locator('[data-confirm]').click();
  const result = field.locator('output');
  const beforeMove = await result.textContent();
  await page.evaluate(
    ({ layerId, pointId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'translate-points',
        payload: { layerId, nodeId: 'picker-curve', pointIds: [pointId], delta: [1, 0] },
      }),
    { layerId: layer.id, pointId: `point-${pointCount}` },
  );
  const afterMove = await result.textContent();
  assert.notEqual(afterMove, beforeMove);
  await activateCommand(page, 'undo');
  assert.equal(await result.textContent(), beforeMove);
  await activateCommand(page, 'redo');
  assert.equal(await result.textContent(), afterMove);
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(output, 'curve-picker-project.json');
  await (await downloading).saveAs(file);
  const saved = JSON.parse(await readFile(file, 'utf8'));
  const savedRotation = saved.compositions[0].layers[0].nodes.find(
    (item) => item.id === 'picker-transform',
  ).params.rotation;
  assert.equal(savedRotation.expression, expression);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (source) =>
      window.nect.snapshot().layers[0].nodes.find((item) => item.id === 'picker-transform').params
        .rotation.expression === source,
    expression,
  );
  await page.locator('.node[data-id="picker-transform"] .node-head strong').click();
  assert.equal(await result.textContent(), afterMove);

  // Equal Curve names still show which owner a point belongs to. Completion
  // also accepts a readable point ordinal rather than requiring a UUID.
  const small = structuredClone(doc),
    firstCurve = small.layers[0].nodes.find((item) => item.id === 'picker-curve');
  firstCurve.name = 'Contour';
  firstCurve.points = firstCurve.points.slice(0, 2);
  const secondCurve = structuredClone(firstCurve);
  secondCurve.id = 'picker-curve-b';
  secondCurve.points.forEach((point, index) => (point.id = `other-point-${index + 1}`));
  small.layers[0].nodes.push(secondCurve);
  await page.evaluate((text) => window.nect.load(text), serialize(small));
  await page.locator('.node[data-id="picker-transform"] .node-head strong').click();
  await field.locator('[data-fx]').click();
  const expressionInput = expressionDialog.locator('[aria-label="Parameter expression"]');
  await expressionInput.fill('point2');
  const suggestion = page.getByRole('option', { name: /Contour · Point 2 · X.*picker-curve-b/ });
  await suggestion.click();
  assert.equal(
    await expressionInput.inputValue(),
    'ref("picker-curve-b", "other-point-2.position", "x")',
  );
  await expressionInput.fill('');
  await expressionDialog.locator('[data-insert-reference]').click();
  const duplicateSearch = picker.getByRole('combobox', { name: 'Parameter references search' });
  await duplicateSearch.fill('Point 1');
  const sameNames = await picker.locator('.picker-option').allTextContents();
  assert.equal(sameNames.length, 4);
  assert.ok(sameNames.some((text) => text.includes('picker-curve-b')));
  await page.screenshot({ path: resolve(output, 'duplicate-curves.png') });
  await picker.getByRole('option', { name: 'Contour · Point 1 · Y · picker-curve-b' }).click();
  await duplicateSearch.press('Enter');
  assert.equal(
    await expressionInput.inputValue(),
    'ref("picker-curve-b", "other-point-1.position", "y")',
  );
  await expressionDialog.locator('[data-confirm]').click();
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.snapshot().layers[0].nodes.find((item) => item.id === 'picker-transform').params
          .rotation.expression,
    ),
    'ref("picker-curve-b", "other-point-1.position", "y")',
  );

  // Every matching result remains reachable, even when IDs and names are
  // substring-colliding and the desired owner is beyond the first 100 rows.
  const many = structuredClone(small),
    manyLayer = many.layers[0];
  manyLayer.nodes = manyLayer.nodes.filter(
    (item) => !['picker-curve', 'picker-curve-b'].includes(item.id),
  );
  for (let index = 1; index <= 102; index++) {
    const copy = structuredClone(firstCurve);
    copy.id = index === 102 ? 'curve' : `curve-${index}`;
    copy.points = [copy.points[0]];
    copy.points[0].id = index === 102 ? 'anchor-target' : `anchor-${index}`;
    manyLayer.nodes.push(copy);
  }
  await page.evaluate((text) => window.nect.load(text), serialize(many));
  await page.locator('.node[data-id="picker-transform"] .node-head strong').click();
  await field.locator('[data-fx]').click();
  await expressionDialog.locator('[data-insert-reference]').click();
  const manySearch = picker.getByRole('combobox', { name: 'Parameter references search' }),
    previous = picker.locator('[aria-label="Previous results"]'),
    next = picker.locator('[aria-label="Next results"]');
  await manySearch.fill('Contour Point 1 curve');
  assert.equal(await picker.locator('.picker-option').count(), 100);
  assert.match(await picker.locator('.picker-status').textContent(), /^1–100 of 204$/);
  for (let i = 0; i < 3 && !(await next.evaluate((el) => el === document.activeElement)); i++)
    await page.keyboard.press('Tab');
  assert.equal(await next.evaluate((el) => el === document.activeElement), true);
  await page.keyboard.press('Enter');
  assert.match(await picker.locator('.picker-status').textContent(), /^101–200 of 204$/);
  assert.equal(await next.evaluate((el) => el === document.activeElement), true);
  await manySearch.fill('Contour Point 1 curve-1');
  assert.equal(await previous.isDisabled(), true);
  assert.equal(await previous.isHidden(), true);
  await manySearch.fill('Contour Point 1 curve');
  assert.match(await picker.locator('.picker-status').textContent(), /^1–100 of 204$/);
  for (let i = 0; i < 3 && !(await next.evaluate((el) => el === document.activeElement)); i++)
    await page.keyboard.press('Tab');
  assert.equal(await next.evaluate((el) => el === document.activeElement), true);
  await page.keyboard.press('Enter');
  await page.keyboard.press('Enter');
  assert.match(await picker.locator('.picker-status').textContent(), /^201–204 of 204$/);
  assert.equal(await previous.evaluate((el) => el === document.activeElement), true);
  assert.equal(await next.isDisabled(), true);
  await page.screenshot({ path: resolve(output, 'paged-curves.png') });
  await page.keyboard.press('Enter');
  await page.keyboard.press('Enter');
  assert.match(await picker.locator('.picker-status').textContent(), /^1–100 of 204$/);
  assert.equal(await next.evaluate((el) => el === document.activeElement), true);
  await page.keyboard.press('Enter');
  await page.keyboard.press('Enter');
  const target = picker.getByRole('option', { name: 'Contour · Point 1 · X · curve', exact: true });
  assert.equal(await target.count(), 1);
  const targetIndex = await target.evaluate((el) => [...el.parentElement.children].indexOf(el));
  await manySearch.focus();
  for (let index = 0; index < targetIndex; index++) await page.keyboard.press('ArrowDown');
  await page.keyboard.press('Enter');
  assert.equal(await expressionInput.inputValue(), 'ref("curve", "anchor-target.position", "x")');
  await expressionDialog.locator('[data-confirm]').click();

  // Compare against the connected large-Curve route that previously spent
  // most of its load time repeatedly searching every point parameter.
  const connected = structuredClone(doc),
    connectedLayer = connected.layers[0];
  connect(connectedLayer, 'picker-curve', 'picker-transform', 'path');
  const connectedStart = Date.now();
  await page.evaluate((text) => window.nect.load(text), serialize(connected));
  console.log(`Loaded connected ${pointCount}-point Curve in ${Date.now() - connectedStart} ms`);
  assert.equal(await page.evaluate(() => window.nect.evaluate().error), null);
  assert.equal(
    await page.evaluate(() => window.nect.evaluate().path.contours[0].points.length),
    pointCount,
  );
  assert.deepEqual(errors, []);
} finally {
  await browser.close();
}
