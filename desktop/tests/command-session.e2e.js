import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
const errors = [];
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
  assert.deepEqual(errors, []);
  console.log(
    'PASS GUI operation control equals headless command result; shared one-step Undo, monotonic revision, stale refusal and API Redo.',
  );
} finally {
  await browser.close();
}
