import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { alignmentFixture } from './fixtures/point-alignment.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(15000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
// Isolated geometry only: do not load font binaries or user documents.
await page.route(/\.(ttf|otf|ttc|otc|woff2?)(\?|$)/i, (route) => route.abort());
try {
  const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);
  const fixture = alignmentFixture(true);
  await page.evaluate((json) => window.nect.load(json), serialize(fixture.doc));
  const node = page.locator(`.graph .node[data-id="${fixture.owner.id}"]`);
  await node.getByRole('button', { name: 'Browse all points', exact: true }).click();
  for (const id of fixture.ids.slice(0, 2))
    await node.locator(`[data-batch-point="${id}"]`).check();
  const input = node.getByRole('spinbutton', { name: 'Batch delta X', exact: true });
  await input.waitFor({ state: 'visible' });
  await page.evaluate(
    () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
  );
  await input.scrollIntoViewIfNeeded();
  const snapshot = () =>
    page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      revision: window.nect.commandState.expectedRevision,
      undo: document.querySelector('[data-action="undo"]').disabled,
      redo: document.querySelector('[data-action="redo"]').disabled,
    }));
  const start = async () => {
    await input.scrollIntoViewIfNeeded();
    const box = await input.boundingBox();
    const x = box.x + 12,
      y = box.y + box.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    return { x, y };
  };
  const move = (p, dx) => page.mouse.move(p.x + dx, p.y, { steps: 5 });
  const before = await snapshot();
  let p = await start();
  await move(p, 24);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '20');
  assert.deepEqual(await snapshot(), before);

  p = await start();
  await move(p, 24);
  await input.fill('47');
  await move(p, 36);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '47', 'new typing survives the old drag');
  assert.deepEqual(await snapshot(), before);

  p = await start();
  await move(p, 24);
  assert.equal(await input.inputValue(), '67');
  await input.fill('67');
  await move(p, 36);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '67', 'an input event matching the preview still wins');
  assert.deepEqual(await snapshot(), before);
  await input.fill('47');
  await input.press('Tab');

  p = await start();
  await move(p, 24);
  await node.locator(`[data-batch-point="${fixture.ids[0]}"]`).evaluate((el) => {
    el.checked = false;
    el.dispatchEvent(new Event('change', { bubbles: true }));
  });
  await page.evaluate(
    () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
  );
  await move(p, 36);
  await page.mouse.up();
  await input.waitFor({ state: 'visible' });
  assert.equal(await input.inputValue(), '47', 'selection changes cannot retain a stale preview');
  assert.deepEqual(await snapshot(), before);
  await node.locator(`[data-batch-point="${fixture.ids[0]}"]`).check();
  await page.evaluate(
    () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
  );

  await input.fill('12.50');
  p = await start();
  await move(p, 24);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.equal(await input.inputValue(), '12.50', 'Escape restores the exact typed draft');
  assert.deepEqual(await snapshot(), before);

  await input.fill('20');
  p = await start();
  await move(p, 24);
  await move(p, 0);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '20', 'return to origin preserves the draft');
  assert.deepEqual(await snapshot(), before);

  p = await start();
  await move(p, 24);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.mouse.up();
  assert.equal(await input.inputValue(), '20', 'window blur restores the draft');
  assert.deepEqual(await snapshot(), before);

  p = await start();
  await move(p, 24);
  await input.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  assert.equal(await input.inputValue(), '20');
  await input.fill('55');
  await move(p, 36);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '55', 'composition input survives the old release');
  await input.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  assert.deepEqual(await snapshot(), before);

  await input.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  p = await start();
  await move(p, 24);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '55', 'composition prevents starting another scrub');
  await input.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
  );
  await input.fill('');
  p = await start();
  await move(p, 24);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '', 'an empty draft is not converted to zero by a drag');
  assert.deepEqual(await snapshot(), before);

  await input.fill('20');
  await input.press('Tab');
  await node.getByRole('button', { name: 'Move selected points', exact: true }).click();
  const applied = await snapshot();
  assert.notEqual(applied.doc, before.doc);
  assert.equal(applied.revision, before.revision + 1);
  await page.getByTitle('Ctrl+Z', { exact: true }).click();
  assert.equal((await snapshot()).doc, before.doc);
  if (!(await node.locator('[data-batch-point]').count()))
    await node.getByRole('button', { name: 'Browse all points', exact: true }).click();
  for (const id of fixture.ids.slice(0, 2))
    await node.locator(`[data-batch-point="${id}"]`).check();
  await input.waitFor({ state: 'visible' });
  await page.evaluate(
    () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
  );
  const beforeNoop = await snapshot();
  p = await start();
  await move(p, 2);
  await page.mouse.up();
  assert.deepEqual(await snapshot(), beforeNoop, 'no-op preserves pending Redo');
  await page.getByTitle('Ctrl+Shift+Z', { exact: true }).click();
  assert.equal((await snapshot()).doc, applied.doc);
  if (!(await node.locator('[data-batch-point]').count()))
    await node.getByRole('button', { name: 'Browse all points', exact: true }).click();
  for (const id of fixture.ids.slice(0, 2))
    await node.locator(`[data-batch-point="${id}"]`).check();
  await input.waitFor({ state: 'visible' });
  await page.evaluate(
    () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
  );
  p = await start();
  await move(p, 24);
  const beforeReplacement = await snapshot();
  await page.evaluate((json) => window.nect.load(json), serialize(fixture.doc));
  const replacement = await snapshot();
  assert.notEqual(replacement.doc, beforeReplacement.doc);
  await page.mouse.up();
  assert.deepEqual(
    await snapshot(),
    replacement,
    'late release cannot edit a replacement document',
  );
  assert.deepEqual(errors, []);
  console.log('PASS Remap batch staged scrub, new typing, cancellation and Undo/Redo');
} finally {
  await browser.close();
}
