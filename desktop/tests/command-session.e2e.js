import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
const errors = [];
const clickUndo = () => page.locator('[data-action="undo"]').click();
page.on('pageerror', (e) => errors.push(e.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForSelector('.shape');
  await page.locator('#add-node').selectOption('Math');
  const setup = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      layer = before.layers[0],
      math = layer.nodes.find((n) => n.type === 'Math');
    const headless = new CommandSession(before);
    headless.dispatch({
      ...headless.state,
      type: 'set-operation',
      payload: { layerId: layer.id, nodeId: math.id, operation: 'multiply' },
    });
    return {
      before,
      expected: headless.snapshot(),
      math: math.id,
      state: window.nect.commandState,
    };
  });
  await page.locator(`.graph [data-operation="${setup.math}"]`).selectOption('multiply');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), setup.expected);
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    setup.state.expectedRevision + 1,
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), setup.before);
  const refusal = await page.evaluate((state) => {
    try {
      window.nect.dispatch({ ...state, type: 'redo' });
      return null;
    } catch (e) {
      return e.message;
    }
  }, setup.state);
  assert.match(refusal, /STALE_REVISION/);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), setup.expected);
  const property = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      layer = before.layers[0],
      circle = layer.nodes.find((n) => n.type === 'Circle');
    const headless = new CommandSession(before);
    headless.dispatch({
      ...headless.state,
      type: 'set-parameter',
      payload: { layerId: layer.id, nodeId: circle.id, key: 'radius', value: 175 },
    });
    return {
      before,
      expected: headless.snapshot(),
      circle: circle.id,
      revision: window.nect.commandState.expectedRevision,
    };
  });
  const field = () =>
    page.locator(`.graph .field[data-node="${property.circle}"][data-key="radius"]`);
  await field().locator('[data-value]').fill('175');
  await field().locator('[data-value]').dispatchEvent('change');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), property.expected);
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    property.revision + 1,
  );
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), property.before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  page.once('dialog', (dialog) => dialog.accept('20 * 9'));
  await field().locator('[data-fx]').click();
  assert.equal(await field().locator('[data-expression]').inputValue(), '20 * 9');
  const expressed = await page.evaluate(() => window.nect.snapshot());
  await field().locator('[data-expression]').fill('1 / 0');
  await field().locator('[data-expression]').dispatchEvent('change');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), expressed);
  await field().locator('[data-lock]').click();
  assert.equal(await field().locator('[data-expression]').isDisabled(), true);
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), expressed);
  assert.deepEqual(errors, []);
  console.log(
    'PASS GUI operation/property controls equal headless commands; shared one-step Undo/API Redo, monotonic revision, stale refusal, expression validation and locks.',
  );
} finally {
  await browser.close();
}
