import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { marqueeFixture } from './fixtures/canvas-artwork-marquee.js';
const output = resolve('test-results/canvas-artwork-marquee/native');
await mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_MARQUEE_CDP_URL || 'http://127.0.0.1:9453',
);
let page;
const errors = [],
  results = [];
try {
  const context = browser.contexts()[0];
  if (!context.pages().length) await context.waitForEvent('page', { timeout: 30000 });
  await Promise.any(
    context.pages().map((p) => p.waitForURL(/tauri\.localhost/, { timeout: 30000 })),
  );
  const pages = context.pages().filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  try {
    await page.locator('[data-recovery-dialog]').waitFor({ state: 'visible', timeout: 1000 });
    await page
      .locator('[data-recovery-dialog]')
      .getByRole('button', { name: 'Later', exact: true })
      .click();
  } catch (e) {
    if (e.name !== 'TimeoutError') throw e;
  }
  const f = marqueeFixture();
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    selected = () =>
      page
        .locator('[data-artwork-selection]')
        .evaluateAll((els) => els.map((e) => e.dataset.artworkSelection)),
    checkpoint = async () => ({
      saved: await saved(),
      command: await page.evaluate(() => window.nect.commandState),
      file: await page.evaluate(() => window.nect.fileState),
      svg: await page.evaluate(() => window.nect.export()),
    }),
    neutral = async (before) => assert.deepEqual(await checkpoint(), before),
    xy = (p) =>
      page.evaluate(([x, y]) => {
        const m = document.querySelector('.artboard').getScreenCTM();
        return { x: m.a * x + m.c * y + m.e, y: m.b * x + m.d * y + m.f };
      }, p),
    move = async (p) => {
      const q = await xy(p);
      await page.mouse.move(q.x, q.y, { steps: 5 });
    },
    start = async (p, shift = false) => {
      if (shift) await page.keyboard.down('Shift');
      await move(p);
      await page.mouse.down();
    },
    finish = async (p, shift = false) => {
      await move(p);
      await page.mouse.up();
      if (shift) await page.keyboard.up('Shift');
    },
    drag = async (a, b, shift = false) => {
      await start(a, shift);
      await finish(b, shift);
    };
  const before = await checkpoint();
  await page.evaluate(() => {
    window.marqueeGraph = document.querySelector('.graph');
    window.marqueeInspector = document.querySelector('.inspector-body');
  });
  await start([20, 20]);
  await move([125, 75]);
  assert.match(await page.locator('.canvas-marquee').innerText(), /Bounds overlap/);
  assert.deepEqual(await selected(), []);
  assert.equal(
    await page.evaluate(
      () =>
        window.marqueeGraph === document.querySelector('.graph') &&
        window.marqueeInspector === document.querySelector('.inspector-body'),
    ),
    true,
  );
  await neutral(before);
  await page.screenshot({ path: resolve(output, 'preview.png') });
  await finish([125, 75]);
  assert.deepEqual(await selected(), [f.a.layer.id, f.b.layer.id]);
  assert.equal(await page.locator('.layers .layer.selected').count(), 2);
  assert.equal(await page.locator('.graph .node.selected').count(), 0);
  assert.equal(await page.locator(`.graph .node[data-id="${f.b.shape.id}"]`).count(), 1);
  await neutral(before);
  results.push({
    case: 'replacement preview/commit',
    detachedPreview: true,
    lockedSelectable: true,
    canvasLayersGraphInspectorSync: true,
    documentFileHistoryNeutral: true,
  });

  await start([145, 20], true);
  await move([280, 180]);
  await finish([185, 75], true);
  assert.deepEqual(await selected(), [f.a.layer.id, f.b.layer.id, f.path.layer.id]);
  await drag([145, 20], [280, 180]);
  assert.deepEqual(await selected(), [f.path.layer.id, f.generated.layer.id, f.reference.id]);
  assert.ok(!(await selected()).includes(f.hidden.layer.id));
  await neutral(before);
  await page.screenshot({ path: resolve(output, 'generated-reference-selection.png') });
  results.push({
    case: 'Shift/source identity',
    fixedStartingUnion: true,
    ownedPathGeneratedReference: true,
    hiddenSourceExcluded: true,
    outerLayerIds: true,
    documentFileHistoryNeutral: true,
  });

  const old = await page
    .locator('.artboard')
    .evaluate((e) => ({ a: e.getScreenCTM().a, e: e.getScreenCTM().e, f: e.getScreenCTM().f }));
  await activateBoundaryCommand(page, 'zoom-out');
  await page.keyboard.down('Space');
  await move([310, 200]);
  await page.mouse.down();
  const pan = await xy([310, 200]);
  await page.mouse.move(pan.x + 35, pan.y - 15, { steps: 4 });
  await page.mouse.up();
  await page.keyboard.up('Space');
  const changed = await page
    .locator('.artboard')
    .evaluate((e) => ({ a: e.getScreenCTM().a, e: e.getScreenCTM().e, f: e.getScreenCTM().f }));
  assert.notDeepEqual(changed, old);
  assert.equal(await page.locator('.canvas-marquee').count(), 0);
  await drag([25, 25], [65, 65]);
  assert.deepEqual(await selected(), [f.a.layer.id]);
  await start([10, 10]);
  await move([280, 180]);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.deepEqual(await selected(), [f.a.layer.id]);
  assert.equal(await page.locator('.canvas-marquee').count(), 0);
  await neutral(before);
  await drag([300, 190], [360, 225]);
  assert.deepEqual(await selected(), []);
  await neutral(before);
  results.push({
    case: 'zoom/pan/cancel/empty',
    existingViewTransform: true,
    panPriority: true,
    escapeRestores: true,
    emptyClears: true,
    documentFileHistoryNeutral: true,
  });

  await move([50, 50]);
  await page.mouse.down();
  await move([55, 55]);
  await page.mouse.up();
  assert.notEqual(await saved(), before.saved);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before.saved);
  await page.locator('[data-tool="direct"]').click();
  const anchor = await xy([160, 40]);
  await page.mouse.click(anchor.x, anchor.y);
  assert.equal(await page.locator('.canvas-marquee').count(), 0);
  assert.ok((await page.locator('.vertex.point-marker.active').count()) > 0);
  assert.equal(await saved(), before.saved);
  results.push({
    case: 'existing edit routes',
    targetDragOneUndo: true,
    directAnchorClick: true,
    noAnchorMarquee: true,
  });
  assert.deepEqual(errors, []);
  await writeFile(resolve(output, 'fixture.json'), serialize(f.doc));
  const receipt = {
    nativeWebView2: true,
    syntheticCdpInput: true,
    results,
    errors,
    notRun: [
      'physical mouse/keyboard/IME',
      'OS dialogs/cold file reopen',
      'broad graph/frame-time benchmarks',
    ],
  };
  await writeFile(resolve(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify({ pass: results.length, ...receipt }));
} catch (e) {
  if (page) await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
  throw e;
} finally {
  await browser.close();
}
