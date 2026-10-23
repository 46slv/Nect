import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect, serialize } from '../src/core.js';

const stage = process.env.NECT_NUMERIC_STAGE || 'patched';
assert.ok(['baseline', 'patched'].includes(stage));
const output = path.resolve('test-results/legacy-numeric-wrap-tests/native');
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_NUMERIC_CDP_URL || 'http://127.0.0.1:9463',
);
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  const [page] = pages,
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  await page.waitForTimeout(2000);
  const doc = blankDocument(),
    layer = doc.layers[0],
    rectangle = node('Rectangle'),
    fill = node('Fill'),
    out = layer.nodes[0];
  doc.name = 'Numeric Escape maintenance';
  rectangle.id = 'numeric-escape-rectangle';
  rectangle.params.center.value = [200, 160];
  layer.nodes = [rectangle, fill, out];
  layer.nodes.forEach((n, i) => (n.position = [40 + i * 340, 40]));
  connect(layer, rectangle.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  await page.locator('[data-action="graph-reset"]').click();
  await page.locator(`.node[data-id="${rectangle.id}"] .type-tag`).click();
  const field = page.locator(
    `.inspector-body .field[data-node="${rectangle.id}"][data-key="center"] input[data-axis="0"]:not([data-base])`,
  );
  await field.click();
  await page.waitForTimeout(150);
  const checkpoint = () =>
    page.evaluate(() => ({
      json: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      file: window.nect.fileState,
      command: window.nect.commandState,
      gesture: window.nect.gestureState,
      undoDisabled: document.querySelector('[data-action="undo"]').disabled,
    }));
  const before = await checkpoint();
  assert.equal(before.undoDisabled, true);
  const box = await field.boundingBox(),
    x = box.x + 10,
    y = box.y + box.height / 2;
  await page.mouse.move(x, y);
  await page.mouse.down();
  await page.mouse.move(x + 24, y, { steps: 3 });
  await page.waitForTimeout(100);
  assert.equal(await field.inputValue(), '220');
  assert.equal(await field.evaluate((e) => e === document.activeElement), true);
  assert.equal((await checkpoint()).json, before.json);
  await page.evaluate(() =>
    document.addEventListener(
      'keydown',
      (e) => {
        if (e.key === 'Escape')
          window.numericEscapeKey = {
            targetType: e.target.type,
            active: e.target === document.activeElement,
            isNumber: e.target.matches('input[type="number"]'),
          };
      },
      { capture: true, once: true },
    ),
  );
  await page.keyboard.press('Escape');
  const afterEscape = await checkpoint();
  await page.mouse.up();
  await page.waitForTimeout(150);
  const afterUp = await checkpoint();
  const contractPass =
    afterEscape.gesture === null &&
    afterUp.json === before.json &&
    afterUp.svg === before.svg &&
    afterUp.undoDisabled;
  if (stage === 'patched') {
    assert.equal(contractPass, true);
    assert.deepEqual(
      afterUp,
      before,
      'Escape plus late pointerup preserves complete document/file/history state',
    );
  } else {
    assert.equal(
      contractPass,
      false,
      'baseline diagnostic must reproduce the actual contract violation',
    );
    assert.ok(afterEscape.gesture);
    const expected = JSON.parse(before.json);
    expected.layers[0].nodes[0].params.center.value[0] = 220;
    assert.deepEqual(
      JSON.parse(afterUp.json),
      expected,
      'baseline pointerup wrongly commits the discarded preview',
    );
  }
  assert.deepEqual(errors, []);
  const receipt = {
    stage,
    contractPass,
    input: 'CDP synthetic mouse/keyboard on focused native WebView2 number input',
    key: await page.evaluate(() => window.numericEscapeKey),
    viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
    before,
    afterEscape,
    afterUp,
    pageErrors: errors,
  };
  await fs.writeFile(path.join(output, stage + '.json'), JSON.stringify(receipt, null, 2));
  await page.screenshot({ path: path.join(output, stage + '.png') });
  console.log(
    JSON.stringify({
      stage,
      contractPass,
      focusedKeyTarget: receipt.key,
      gestureSurvivedEscape: !!afterEscape.gesture,
      finalX: JSON.parse(afterUp.json).layers[0].nodes[0].params.center.value[0],
      undoDisabled: afterUp.undoDisabled,
      pageErrors: errors,
    }),
  );
} finally {
  await browser.close();
}
