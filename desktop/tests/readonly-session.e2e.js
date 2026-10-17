import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForSelector('.shape');
  const proof = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      svg = window.nect.export(),
      state = window.nect.commandState;
    const copy = window.nect.document;
    copy.layers[0].nodes[0].params.radius.value = 900;
    copy.layers.length = 0;
    const evaluated = window.nect.evaluate();
    evaluated.path.contours[0].points[0].position[0] = 99999;
    const safe =
      JSON.stringify(window.nect.snapshot()) === JSON.stringify(before) &&
      window.nect.export() === svg;
    const layer = before.layers[0],
      node = layer.nodes[0];
    const command = {
      type: 'set-parameter',
      payload: { layerId: layer.id, nodeId: node.id, key: 'radius', value: 180 },
    };
    const headless = new CommandSession(before);
    headless.dispatch({ ...headless.state, ...command });
    window.nect.dispatch({ ...state, ...command });
    const actual = window.nect.snapshot();
    let stale;
    try {
      window.nect.dispatch({ ...state, type: 'undo' });
    } catch (error) {
      stale = error.message;
    }
    return { before, expected: headless.snapshot(), actual, safe, stale };
  });
  assert.equal(proof.safe, true, 'debug outputs cannot alter authored state or renderer cache');
  assert.deepEqual(proof.actual, proof.expected);
  assert.match(proof.stale, /STALE_REVISION/);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), proof.before);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), proof.expected);
  assert.deepEqual(errors, []);
  console.log(
    'PASS read-only debug outputs, evaluation cache isolation, GUI/headless command parity and shared Undo/Redo/stale refusal',
  );
} finally {
  await browser.close();
}
