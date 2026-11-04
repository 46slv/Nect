import { chromium } from 'playwright-core';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import { alignmentFixture } from './fixtures/point-alignment.js';
import { serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(15000);
await page.route(/\.(ttf|otf|ttc|otc|woff2?)(\?|$)/i, (route) => route.abort());
try {
  const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);
  for (const remapped of [false, true]) {
    const f = alignmentFixture(remapped);
    if (!remapped) f.curve.collapsed = false;
    await page.evaluate((json) => window.nect.load(json), serialize(f.doc));
    await page.locator('[data-tool="direct"]').click();
    await activateBoundaryCommand(page, 'actual-size');
    for (const [i, id] of f.ids.entries()) {
      const marker = page.locator(`.vertex.point-marker[data-point="${id}"]`);
      await marker.waitFor({ state: 'visible' });
      if (i) await page.keyboard.down('Shift');
      await marker.click({ force: true });
      if (i) await page.keyboard.up('Shift');
    }
    await ensureWorkspacePanel(page, 'inspector');
    const input = page.locator(`${f.region} [data-point-spacing="${f.owner.id}"]`);
    await input.waitFor({ state: 'visible' });
    assert.equal(await input.isEnabled(), true);
    await input.scrollIntoViewIfNeeded();
    const snap = () =>
      page.evaluate(() => ({
        doc: JSON.stringify(window.nect.document),
        revision: window.nect.commandState.expectedRevision,
        undo: document.querySelector('[data-action="undo"]').disabled,
        redo: document.querySelector('[data-action="redo"]').disabled,
      }));
    const before = await snap(),
      value = await input.inputValue(),
      attr = await input.getAttribute('data-scrub-step');
    const b = await input.boundingBox();
    await page.mouse.move(b.x + 12, b.y + b.height / 2);
    await page.mouse.down();
    await page.mouse.move(b.x + 36, b.y + b.height / 2, { steps: 5 });
    await page.mouse.up();
    const after = await snap(),
      changed = (await input.count()) ? await input.inputValue() : null;
    console.log(
      JSON.stringify({
        browser: await browser.version(),
        fixture: 'three selected anchors',
        field: 'Point spacing',
        before: value,
        after: changed,
        scrubStep: attr,
        box: b,
        dragPixels: 24,
        documentAndHistoryUnchanged: JSON.stringify(after) === JSON.stringify(before),
      }),
    );
    assert.equal(JSON.stringify(after), JSON.stringify(before));
    assert.equal(changed, '30', 'drag updates the staged value');
    const peer = remapped ? null : page.locator('.graph [data-point-spacing]');
    if (peer) assert.equal(await peer.inputValue(), '30');
    const rect = await input.boundingBox();
    await page.mouse.move(rect.x + 12, rect.y + rect.height / 2);
    await page.mouse.down();
    await page.mouse.move(rect.x + 40, rect.y + rect.height / 2, { steps: 5 });
    await page.keyboard.press('Escape');
    await page.mouse.up();
    assert.equal(await input.inputValue(), '30');
    if (peer) assert.equal(await peer.inputValue(), '30');
    const r = await input.boundingBox();
    await page.mouse.move(r.x + 12, r.y + r.height / 2);
    await page.mouse.down();
    await page.mouse.move(r.x + 36, r.y + r.height / 2, { steps: 5 });
    await page.mouse.move(r.x + 12, r.y + r.height / 2, { steps: 5 });
    await page.mouse.up();
    assert.equal(await input.inputValue(), '30');
    if (peer) assert.equal(await peer.inputValue(), '30');
    assert.equal(JSON.stringify(await snap()), JSON.stringify(before));
    const interrupted = await input.boundingBox();
    await page.mouse.move(interrupted.x + 12, interrupted.y + interrupted.height / 2);
    await page.mouse.down();
    await page.mouse.move(interrupted.x + 36, interrupted.y + interrupted.height / 2, { steps: 5 });
    await input.fill('47');
    await page.mouse.move(interrupted.x + 48, interrupted.y + interrupted.height / 2, { steps: 5 });
    await page.mouse.up();
    assert.equal(await input.inputValue(), '47', 'new input wins over the old drag');
    if (peer) assert.equal(await peer.inputValue(), '47');
    assert.deepEqual(await snap(), before);
    await input.fill('30');
    const composing = await input.boundingBox();
    await page.mouse.move(composing.x + 12, composing.y + composing.height / 2);
    await page.mouse.down();
    await page.mouse.move(composing.x + 36, composing.y + composing.height / 2, { steps: 5 });
    await input.evaluate((el) =>
      el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
    );
    assert.equal(await input.inputValue(), '30');
    if (peer) assert.equal(await peer.inputValue(), '30');
    await input.fill('47');
    await page.mouse.move(composing.x + 48, composing.y + composing.height / 2, { steps: 5 });
    await page.mouse.up();
    assert.equal(await input.inputValue(), '47', 'composition input survives the old release');
    await input.evaluate((el) =>
      el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
    );
    assert.deepEqual(await snap(), before);
    await input.evaluate((el) =>
      el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
    );
    const blocked = await input.boundingBox();
    await page.mouse.move(blocked.x + 12, blocked.y + blocked.height / 2);
    await page.mouse.down();
    await page.mouse.move(blocked.x + 36, blocked.y + blocked.height / 2, { steps: 5 });
    await page.mouse.up();
    assert.equal(await input.inputValue(), '47', 'a composing input cannot start a scrub');
    await input.evaluate((el) =>
      el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true })),
    );
    assert.deepEqual(await snap(), before);
    await input.fill('12.50');
    const exact = await input.boundingBox();
    await page.mouse.move(exact.x + 12, exact.y + exact.height / 2);
    await page.mouse.down();
    await page.mouse.move(exact.x + 36, exact.y + exact.height / 2, { steps: 5 });
    await page.keyboard.press('Escape');
    await page.mouse.up();
    assert.equal(await input.inputValue(), '12.50');
    if (peer) assert.equal(await peer.inputValue(), '12.50');
    await input.fill('0');
    const lower = await input.boundingBox();
    await page.mouse.move(lower.x + 40, lower.y + lower.height / 2);
    await page.mouse.down();
    await page.mouse.move(lower.x + 12, lower.y + lower.height / 2, { steps: 5 });
    await page.mouse.up();
    assert.equal(await input.inputValue(), '0');
    if (peer) assert.equal(await peer.inputValue(), '0');
    assert.deepEqual(await snap(), before);
    await input.fill('');
    const invalid = await input.boundingBox();
    await page.mouse.move(invalid.x + 12, invalid.y + invalid.height / 2);
    await page.mouse.down();
    await page.mouse.move(invalid.x + 36, invalid.y + invalid.height / 2, { steps: 5 });
    await page.mouse.up();
    assert.equal(await input.inputValue(), '', 'an empty draft is not converted to zero');
    if (peer) assert.equal(await peer.inputValue(), '');
    assert.deepEqual(await snap(), before);
    await input.fill('30');
    await page.locator(f.region + ' [data-point-set-spacing="x"]').click();
    const applied = await snap();
    assert.notEqual(applied.doc, before.doc);
    assert.equal(applied.revision, before.revision + 1);
    await page.getByTitle('Ctrl+Z', { exact: true }).click();
    assert.equal((await snap()).doc, before.doc);
    await ensureWorkspacePanel(page, 'inspector');
    await input.scrollIntoViewIfNeeded();
    const priorNoop = await snap();
    const small = await input.boundingBox();
    await page.mouse.move(small.x + 12, small.y + small.height / 2);
    await page.mouse.down();
    await page.mouse.move(small.x + 14, small.y + small.height / 2);
    await page.mouse.up();
    assert.deepEqual(await snap(), priorNoop);
    await page.getByTitle('Ctrl+Shift+Z', { exact: true }).click();
    assert.equal((await snap()).doc, applied.doc);
    await ensureWorkspacePanel(page, 'inspector');
    await input.scrollIntoViewIfNeeded();
    const beforeStale = await snap(),
      stale = await input.boundingBox();
    await page.mouse.move(stale.x + 12, stale.y + stale.height / 2);
    await page.mouse.down();
    await page.mouse.move(stale.x + 36, stale.y + stale.height / 2, { steps: 5 });
    assert.deepEqual(await snap(), beforeStale);
    await page.evaluate((json) => window.nect.load(json), serialize(f.doc));
    const external = await snap();
    await page.mouse.up();
    assert.deepEqual(await snap(), external, 'late release cannot commit a stale draft');
  }
  console.log('PASS Point spacing staged scrub, cancellation, Apply and no-op Redo');
} finally {
  await browser.close();
}
