import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(async () => {
    const { blankDocument, node, connect } = await import('/src/core.js');
    const doc = blankDocument(),
      layer = doc.layers[0],
      text = node('Text'),
      fill = node('Fill');
    text.id = 'race-text';
    text.text = 'AB';
    text.params.position.value = [180, 210];
    layer.nodes.unshift(text, fill);
    connect(layer, text.id, fill.id, 'path');
    connect(layer, fill.id, layer.output, 'path');
    window.nect.load(JSON.stringify(doc));
  });
  const field = (key) => page.locator(`[data-character-style="${key}"]`);
  const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const styles = () =>
    page.evaluate(() => window.nect.document.layers[0].nodes[0].characterPositions?.styles || {});
  const select = async () => {
    const text = page.locator('.inspector-body [data-text]');
    await text.focus();
    await text.evaluate((el) => el.setSelectionRange(0, 2));
    await page.locator('.inspector-body [data-style-text-range]').click();
  };
  const edit = async (key, value) => {
    await field(key).fill(String(value));
    await field(key).press('Tab');
  };
  await select();
  const initial = await snapshot();
  // Hold the existing zero-delay structural-render task so its delivery falls
  // inside the next real browser edit. Model/input handlers are not replaced.
  const holdRender = () =>
    page.evaluate(() => {
      const original = window.setTimeout;
      const pending = [];
      window.setTimeout = function (fn, delay, ...args) {
        if (delay === 0 && new Error().stack.includes('scheduleRender')) {
          pending.push(() => fn(...args));
          return 987654;
        }
        return original(fn, delay, ...args);
      };
      window.deliverRender = () => {
        window.setTimeout = original;
        const count = pending.length;
        for (const task of pending.splice(0)) task();
        return count;
      };
    });
  await holdRender();
  await edit('tracking', 12);
  const tracked = await snapshot();
  await field('height').fill('135');
  await field('height').evaluate((el) => {
    window.draftInput = el;
  });
  assert.ok((await page.evaluate(() => window.deliverRender())) > 0);
  assert.equal(await snapshot(), tracked, 'A previous edit render must not commit the next draft');
  assert.equal(await field('height').evaluate((el) => el === window.draftInput), true);
  await field('height').press('Escape');
  await field('height').press('Tab');
  assert.equal(await snapshot(), tracked);
  assert.equal(await field('height').inputValue(), '100');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), initial);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), tracked);
  await select();
  await holdRender();
  await edit('baselineShift', 18);
  const shifted = await snapshot();
  // fill() selects then inserts through separate browser operations. Deliver a
  // pending render between them: number controls cannot expose selectionStart,
  // so replacing the input loses select-all and used to insert 135100 instead.
  await field('height').focus();
  await field('height').press('ControlOrMeta+A');
  assert.ok((await page.evaluate(() => window.deliverRender())) > 0);
  await page.keyboard.insertText('135');
  assert.equal(await field('height').inputValue(), '135');
  assert.equal(await snapshot(), shifted);
  await field('height').press('Tab');
  assert.deepEqual(await styles(), {
    c1: { tracking: 12, baselineShift: 18, height: 1.35 },
    c2: { tracking: 12, baselineShift: 18, height: 1.35 },
  });
  const committed = await snapshot();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), shifted);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), committed);
  await page.evaluate((saved) => window.nect.load(saved), committed);
  assert.equal(await snapshot(), committed);
  await select();
  await holdRender();
  await edit('tracking', 13);
  const beforeExpression = await snapshot();
  await field('height').focus();
  await field('height').press('+');
  await field('height').press('2');
  assert.ok((await page.evaluate(() => window.deliverRender())) > 0);
  assert.equal(await field('height').inputValue(), '135+2');
  assert.equal(await snapshot(), beforeExpression);
  await field('height').press('Escape');
  assert.equal(await field('height').inputValue(), '135');
  assert.equal(await snapshot(), beforeExpression);
  await field('height').press('+');
  await field('height').press('2');
  await field('height').press('Enter');
  assert.equal((await styles()).c1.height, 1.37);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), beforeExpression);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), committed);
  await select();
  await edit('height', 0);
  assert.equal(await snapshot(), committed);
  await page.waitForFunction(
    () =>
      document.querySelector('[data-character-style="height"]')?.getAttribute('value') === '135',
  );
  assert.equal(await field('height').inputValue(), '135');
  await fs.mkdir('test-results', { recursive: true });
  await edit('height', '');
  assert.equal(await field('height').inputValue(), '135');
  assert.equal(await snapshot(), committed);
  // Tab into the adjacent numeric component is navigation, not an unfinished
  // draft. The prior edit must render, and native Tab select-all must survive.
  const position = (axis) =>
    page.locator(`.inspector-body .field[data-key="position"] input[data-axis="${axis}"]`);
  await holdRender();
  await position(0).fill('190');
  await position(0).press('Tab');
  const tabTarget = await position(1).elementHandle();
  assert.equal(await tabTarget.evaluate((el) => el === document.activeElement), true);
  assert.ok((await page.evaluate(() => window.deliverRender())) > 0);
  assert.equal(
    await tabTarget.evaluate((el) => el.isConnected),
    false,
    'Tab must allow the committed edit to render',
  );
  await page.keyboard.insertText('240');
  assert.equal(await position(1).inputValue(), '240', 'Tab select-all survives replacement');
  await position(1).press('Tab');
  assert.deepEqual(
    await page.evaluate(() => window.nect.document.layers[0].nodes[0].params.position.value),
    [190, 240],
  );
  await page.locator('[data-action="undo"]').click();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), committed);
  await tabTarget.dispose();
  await page.screenshot({ path: 'test-results/numeric-render-race.png' });
  assert.deepEqual(errors, []);
  console.log(
    'Numeric draft survives deferred render; Escape, consecutive commit, invalid edit, Undo/Redo and JSON reopen passed',
  );
} finally {
  await browser.close();
}
