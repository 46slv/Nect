import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { document, node, point, connect, serialize, guideDefaults } from '../src/core.js';
import { handlePolar } from '../src/handle-polar.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROME_PATH || 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
const errors = [],
  checks = [];
page.on('pageerror', (e) => errors.push(e.message));
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));
let curve, pt;
const read = () =>
  page.evaluate(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).points[0],
    curve.id,
  );
const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
// Wait through the scheduled structural render, not just the old input's new value.
const settled = () =>
  page.evaluate(
    () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
  );
const input = (key, component) =>
  page.locator(
    `.inspector [data-point="${pt.id}"][data-handle="${key}"][data-handle-polar="${component}"]`,
  );
const show = async () => {
  await page.locator(`.node[data-id="${curve.id}"] .node-head`).click();
  if (!(await input('handleB', 'length').count()))
    await page.locator(`.inspector [data-point-panel-mode="${curve.id}"]`).click();
};
const seed = async (type = 'Curve', mode = 'corner', guard) => {
  const doc = document(),
    layer = doc.layers[0],
    stroke = node('Stroke', 400, 40),
    out = node('Output', 750, 40);
  curve = node(type);
  pt = point(200, 180);
  pt.handleA.value = [-30, 0];
  pt.handleB.value = [50, 0];
  pt.handleType = mode;
  curve.points = [pt, point(450, 200), point(400, 400)];
  if (type === 'Multi Path') {
    curve.contours = [{ id: 'contour-test', points: curve.points.map((p) => p.id), closed: false }];
    curve.units = [{ id: 'unit-test', contours: ['contour-test'] }];
  }
  doc.schemaVersion = 26;
  layer.nodes = [curve, stroke, out];
  layer.output = out.id;
  connect(layer, curve.id, stroke.id, 'path');
  connect(layer, stroke.id, out.id, 'path');
  doc.editor = { ...guideDefaults(), snap: false };
  guard?.({ doc, layer, curve, pt });
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  await show();
};
const edit = async (key, component, value) => {
  await input(key, component).fill(value);
  await input(key, component).press('Enter');
  await settled();
  await page.waitForFunction(
    ({ id, pointId, key, component }) => {
      const p = window.nect.document.layers[0].nodes
        .find((n) => n.id === id)
        .points.find((p) => p.id === pointId);
      const v =
        component === 'angle'
          ? (Math.atan2(p[key].value[1], p[key].value[0]) * 180) / Math.PI
          : Math.hypot(...p[key].value);
      const el = document.querySelector(
        `.inspector [data-point="${pointId}"][data-handle="${key}"][data-handle-polar="${component}"]`,
      );
      return Math.abs(Number(el.defaultValue) - v) < 0.001;
    },
    { id: curve.id, pointId: pt.id, key, component },
  );
  assert.equal(
    await page
      .locator(
        `.graph [data-point="${pt.id}"][data-handle="${key}"][data-handle-polar="${component}"]`,
      )
      .inputValue(),
    await input(key, component).inputValue(),
  );
};
const undo = async () => {
  await page.locator('[data-action="undo"]').click();
  await show();
};
const noUndo = async () =>
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
const choose = async () => {
  const button = page.locator(`.inspector [data-select-panel-point="${pt.id}"]`);
  await button.click();
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5205');
  await page.waitForFunction(() => window.nect);
  for (const type of ['Curve', 'Multi Path'])
    for (const mode of ['corner', 'smooth', 'symmetric']) {
      await seed(type, mode);
      for (const key of ['handleA', 'handleB']) {
        const other = key === 'handleA' ? 'handleB' : 'handleA';
        const before = await saved();
        await edit(key, 'angle', '90');
        near((await read())[key].value, [0, key === 'handleA' ? 30 : 50]);
        if (mode !== 'corner')
          near((await read())[other].value, [
            0,
            -(mode === 'symmetric' ? (key === 'handleA' ? 30 : 50) : key === 'handleA' ? 50 : 30),
          ]);
        const after = await saved();
        await undo();
        assert.equal(await saved(), before);
        await noUndo();
        await page.locator('[data-action="redo"]').click();
        assert.equal(await saved(), after);
        await undo();
        await edit(key, 'length', '25');
        assert.ok(Math.abs(handlePolar((await read())[key].value).length - 25) < 1e-8);
        await undo();
        await noUndo();
      }
      checks.push(`${type} ${mode}: both angle/length, coupling, single Undo/Redo`);
      console.log(checks.at(-1));
    }

  await seed();
  await choose();
  const before = await saved();
  await input('handleB', 'length').fill('999');
  await input('handleB', 'length').press('Escape');
  await input('handleB', 'length').press('Tab');
  assert.equal(await saved(), before);
  await noUndo();
  for (const value of ['-1', '', '1e309']) {
    await input('handleB', 'length').fill(value);
    await input('handleB', 'length').press('Enter');
    assert.equal(await saved(), before);
    await noUndo();
  }
  await input('handleB', 'length').evaluate((el) => {
    el.type = 'text';
    el.value = 'NaN';
    el.dispatchEvent(new Event('change', { bubbles: true }));
  });
  assert.equal(await saved(), before);
  await noUndo();
  await input('handleB', 'length').fill('10');
  await input('handleB', 'length').press('+');
  await input('handleB', 'length').press('5');
  await input('handleB', 'length').press('Enter');
  await settled();
  near((await read()).handleB.value, [15, 0]);
  await undo();
  await noUndo();
  checks.push('Escape, negative/empty/nonfinite rejection, shared arithmetic evaluator');

  await edit('handleB', 'length', '0');
  assert.equal(await input('handleB', 'angle').inputValue(), '0');
  assert.equal(await input('handleB', 'angle').isDisabled(), true);
  await edit('handleB', 'length', '40');
  near((await read()).handleB.value, [40, 0]);
  const reloaded = await saved();
  await page.evaluate((s) => window.nect.load(s), reloaded);
  await show();
  assert.equal(await saved(), reloaded);
  await noUndo();
  checks.push('zero length / +X regrowth / JSON reload');

  await seed('Curve', 'corner', ({ pt }) => {
    pt.handleB.value = [3, 7];
  });
  const precise = await saved();
  for (const component of ['angle', 'length']) {
    await input('handleB', component).press('Enter');
    await settled();
    assert.equal(await saved(), precise);
    await noUndo();
  }
  const fileRevision = await page.evaluate(() => window.nect.fileState.revision);
  await edit('handleB', 'length', '23');
  const file = await page.evaluate(() => window.nect.fileState);
  assert.equal(file.dirty, true);
  assert.equal(file.revision, fileRevision + 1);
  await undo();
  assert.equal(await saved(), precise);
  await noUndo();
  checks.push(
    'untouched rounded fields preserve full precision and History; edit advances dirty revision once',
  );

  for (const type of ['Curve', 'Multi Path'])
    for (const mode of ['corner', 'smooth', 'symmetric']) {
      for (const key of ['handleA', 'handleB']) {
        await seed(type, mode);
        await choose();
        const original = await read(),
          other = key === 'handleA' ? 'handleB' : 'handleA';
        await edit(key, 'angle', '90');
        await page.locator('.canvas-stage').focus();
        const marker = page.locator(`.point-marker[data-point="${pt.id}"][data-handle="${key}"]`);
        assert.equal(Number(await marker.getAttribute('cx')), 200);
        assert.equal(
          Number(await marker.getAttribute('cy')),
          180 + Math.hypot(...original[key].value),
        );
        const box = await marker.boundingBox();
        assert.ok(box);
        await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
        await page.mouse.down();
        await page.mouse.move(box.x + box.width / 2 + 25, box.y + box.height / 2 + 15, {
          steps: 5,
        });
        await page.mouse.up();
        const moved = await read();
        assert.notDeepEqual(moved[key].value, [0, Math.hypot(...original[key].value)]);
        const polar = handlePolar(moved[key].value);
        assert.ok(Math.abs(Number(await input(key, 'angle').inputValue()) - polar.angle) <= 0.001);
        assert.ok(
          Math.abs(Number(await input(key, 'length').inputValue()) - polar.length) <= 0.001,
        );
        if (mode === 'symmetric')
          near(
            moved[other].value,
            moved[key].value.map((v) => -v),
          );
        if (mode === 'smooth')
          assert.ok(
            Math.abs(Math.hypot(...moved[other].value) - Math.hypot(...original[other].value)) <
              1e-8,
          );
        if (mode === 'corner') near(moved[other].value, original[other].value);
        await undo();
        near((await read())[key].value, [0, Math.hypot(...original[key].value)]);
        await undo();
        await noUndo();
      }
      checks.push(
        `${type} ${mode}: numeric → SVG handle; both Canvas drags → polar values; one Undo per operation`,
      );
      console.log(checks.at(-1));
    }

  for (const guard of [
    ({ layer }) => (layer.locked = true),
    ({ curve }) => (curve.locked = true),
    ({ pt }) => (pt.handleB.locked = true),
    ({ pt }) => (pt.handleB.expression = 'value; value'),
    ({ layer, pt }) => {
      const v = node('Vector', 1100, 40);
      v.params.value.value = [3, 4];
      layer.nodes.push(v);
      pt.handleB.source = v.id;
    },
  ]) {
    await seed('Curve', 'corner', guard);
    assert.equal(await input('handleB', 'length').isDisabled(), true);
    assert.equal(await input('handleB', 'angle').isDisabled(), true);
    const snapshot = await saved();
    await input('handleB', 'length').evaluate((el) => {
      el.value = '123';
      el.dispatchEvent(new Event('change', { bubbles: true }));
    });
    assert.equal(await saved(), snapshot);
    await noUndo();
  }
  checks.push(
    'layer/node/parameter lock and expression/connection: disabled and forged change rejected',
  );

  await seed();
  await choose();
  await input('handleB', 'length').focus();
  const scroll = await input('handleB', 'length').evaluate((el) => {
    const inspector = el.closest('.inspector'),
      rows = el.closest('.point-panel-rows');
    rows.scrollTop = 80;
    inspector.scrollTop = 60;
    return [rows.scrollTop, inspector.scrollTop];
  });
  await input('handleB', 'length').fill('61');
  await input('handleB', 'length').press('Enter');
  await settled();
  await page.waitForFunction(
    () =>
      document.querySelector('.inspector [data-handle="handleB"][data-handle-polar="length"]')
        .defaultValue === '61',
  );
  assert.equal(
    await input('handleB', 'length').evaluate((el) => el === document.activeElement),
    true,
  );
  assert.deepEqual(
    await input('handleB', 'length').evaluate((el) => [
      el.closest('.point-panel-rows').scrollTop,
      el.closest('.inspector').scrollTop,
    ]),
    scroll,
  );
  const xy = page.locator(
    `.inspector .field[data-key="${pt.id}.handleB"] [data-value][data-axis="0"]`,
  );
  await xy.fill('70');
  await xy.press('Tab');
  near((await read()).handleB.value, [70, 0]);
  assert.equal(await input('handleB', 'length').inputValue(), '70');
  checks.push(
    'focused Enter commit preserves focus and nested scroll; existing XY edit remains synchronized',
  );
  await mkdir('test-results/handle-polar', { recursive: true });
  await page.screenshot({ path: 'test-results/handle-polar/inspector.png' });
  assert.deepEqual(errors, []);
  await writeFile(
    'test-results/handle-polar/receipt.json',
    JSON.stringify(
      {
        status: 'PASS',
        browser: await browser.version(),
        checks,
        pageErrors: errors,
        native: 'unverified',
        physicalIME: 'unverified',
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify({ status: 'PASS', browser: await browser.version(), checks }));
} finally {
  await browser.close();
}
