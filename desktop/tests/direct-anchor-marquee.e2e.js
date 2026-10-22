// Native Tauri/WebView2 via CDP synthetic input, not OS input.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';

const output = path.resolve(
  process.env.NECT_ANCHOR_OUTPUT || 'test-results/direct-anchor-marquee/native',
);
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_ANCHOR_CDP || 'http://127.0.0.1:9475',
);
let page;
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  [page] = pages;
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  const errors = [],
    results = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const $ = (s) => page.locator(s),
    settle = () =>
      page.evaluate(
        () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
      ),
    state = () =>
      page.evaluate(() => ({
        doc: window.nect.document,
        svg: window.nect.export(),
        file: window.nect.fileState,
        command: window.nect.commandState,
        gesture: window.nect.gestureState,
        undo: document.querySelector('[data-action="undo"]').disabled,
        redo: document.querySelector('[data-action="redo"]').disabled,
      })),
    selection = () =>
      page.evaluate(() => ({
        anchors: [...document.querySelectorAll('.vertex.point-marker.active')].map(
          (p) => p.dataset.point,
        ),
        rows: [...document.querySelectorAll('.inspector-body [data-point-row].point-selected')].map(
          (r) => r.dataset.pointRow,
        ),
        focus:
          document.querySelector('.inspector-body [data-point-row].point-active')?.dataset
            .pointRow || null,
        nodes: [...document.querySelectorAll('.node.selected')].map((n) => n.dataset.id),
      })),
    xy = (p) =>
      page.evaluate(([x, y]) => {
        const q = new DOMPoint(x, y).matrixTransform(
          document.querySelector('.artboard').getScreenCTM(),
        );
        return [q.x, q.y];
      }, p),
    move = async (p) => {
      const q = await xy(p);
      await page.mouse.move(...q);
    },
    rectangle = async (a, b, shift = false) => {
      if (shift) await page.keyboard.down('Shift');
      await move(a);
      await page.mouse.down();
      await move(b);
      await page.mouse.up();
      if (shift) await page.keyboard.up('Shift');
      await settle();
    };
  for (const c of [
    { zoom: 100, pan: [20, -12], additive: false },
    { zoom: 120, pan: [-24, 16], additive: true },
  ]) {
    const f = directMarqueeFixture({ transformed: true });
    f.doc.name = `NECT_DIRECT_ANCHOR_${c.zoom}`;
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
    await $('[data-tool="direct"]').click();
    await $('[data-action="actual-size"]').click();
    if (c.zoom === 120) await $('[data-action="zoom-in"]').click();
    assert.equal((await $('[data-zoom="canvas"]').innerText()).trim(), `${c.zoom}%`);
    const box = await $('.canvas-stage').boundingBox();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.wheel(...c.pan);
    await settle();
    const positions = await page.evaluate(() =>
      Object.fromEntries(
        [...document.querySelectorAll('.vertex.point-marker')].map((el) => [
          el.dataset.point,
          [Number(el.getAttribute('cx')), Number(el.getAttribute('cy'))],
        ]),
      ),
    );
    assert.equal(Object.keys(positions).length, 4);
    assert.notDeepEqual(positions['anchor-0'], [100, 100]);
    if (c.additive) {
      await move(positions['anchor-3']);
      await page.mouse.down();
      await page.mouse.up();
      await settle();
    }
    const before = await state(),
      startingSelection = await selection(),
      a = [0, 1].map((i) => Math.min(positions['anchor-0'][i], positions['anchor-1'][i]) - 14),
      b = [0, 1].map((i) => Math.max(positions['anchor-0'][i], positions['anchor-1'][i]) + 14);
    if (c.additive) await page.keyboard.down('Shift');
    await move(a);
    await page.mouse.down();
    // First pass includes all anchors; final smaller rectangle must not accumulate hits.
    const allEnd = [
      Math.max(...Object.values(positions).map((p) => p[0])) + 20,
      Math.max(...Object.values(positions).map((p) => p[1])) + 20,
    ];
    await move(allEnd);
    await move(b);
    await settle();
    assert.equal(await $('.direct-anchor-marquee:visible').count(), 1);
    assert.deepEqual(await selection(), startingSelection);
    assert.equal(
      await $('.direct-anchor-marquee').evaluate((el) => getComputedStyle(el).pointerEvents),
      'none',
    );
    const previewCount = await $('.direct-anchor-marquee').innerText();
    assert.equal(previewCount, `${c.additive ? 3 : 2} anchors`);
    await page.screenshot({ path: path.join(output, `${c.zoom}-preview.png`) });
    await page.mouse.up();
    if (c.additive) await page.keyboard.up('Shift');
    await settle();
    const chosen = await selection(),
      expected = c.additive ? ['anchor-0', 'anchor-1', 'anchor-3'] : ['anchor-0', 'anchor-1'];
    assert.deepEqual(chosen.anchors, expected);
    assert.deepEqual(chosen.rows, expected);
    assert.equal(chosen.focus, c.additive ? 'anchor-3' : 'anchor-0');
    assert.deepEqual(chosen.nodes, [f.curve.id]);
    assert.deepEqual(await state(), before);
    await page.screenshot({ path: path.join(output, `${c.zoom}-selected.png`) });
    // A different full rectangle is cancelled; the mouse's subsequent release is late.
    await move(a);
    await page.mouse.down();
    await move(allEnd);
    if (c.additive)
      await page.evaluate(() =>
        document
          .querySelector('.canvas-stage')
          .dispatchEvent(new PointerEvent('pointercancel', { bubbles: true, pointerId: 1 })),
      );
    else await page.keyboard.press('Escape');
    await page.mouse.up();
    await settle();
    assert.deepEqual(await selection(), chosen);
    assert.deepEqual(await state(), before);
    assert.equal(await $('.direct-anchor-marquee').count(), 0);
    if (!c.additive) {
      const q = await xy(a);
      await page.mouse.move(...q);
      await page.mouse.down();
      await page.mouse.move(q[0] + 2, q[1]);
      await page.mouse.up();
      await settle();
      assert.deepEqual((await selection()).anchors, []);
      assert.deepEqual(await state(), before);
      await rectangle(b, a);
      assert.deepEqual((await selection()).anchors, expected);
    }
    const committed = await state();
    await page.evaluate((s) => window.nect.load(s), serialize(committed.doc));
    await settle();
    assert.deepEqual((await state()).doc, committed.doc);
    assert.equal((await state()).svg, committed.svg);
    await fs.writeFile(path.join(output, `${c.zoom}-fixture.json`), serialize(committed.doc), {
      flag: 'wx',
    });
    await fs.writeFile(path.join(output, `${c.zoom}-artwork.svg`), committed.svg, { flag: 'wx' });
    results.push({
      ...c,
      transform: { rotation: 35, scaleWidth: 1.5, scaleHeight: 0.7 },
      positions,
      expected,
      focus: chosen.focus,
      anchorOnly: true,
      detachedPreview: true,
      cancellation: c.additive ? 'pointercancel plus late mouseup' : 'Escape plus late mouseup',
      exactDocumentHistoryFileSVG: true,
      JSONReloadExact: true,
    });
  }
  assert.deepEqual(errors, []);
  await fs.writeFile(
    path.join(output, 'receipt.json'),
    JSON.stringify(
      {
        input: 'Native Tauri/WebView2 CDP synthetic mouse/keyboard, DOM pointercancel',
        viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
        results,
        pageErrors: errors,
        notRun: ['physical/OS input', 'native OS Save/Open dialogs and normal clean cold reopen'],
      },
      null,
      2,
    ),
    { flag: 'wx' },
  );
  const own = await page.evaluate(() => ({
    name: window.nect.document.name,
    binding: window.nect.fileState.binding,
    project: window.nect.projectSnapshot(),
  }));
  assert.ok(own.name.startsWith('NECT_DIRECT_ANCHOR_'));
  assert.equal(own.binding, null);
  await fs.writeFile(
    path.join(output, 'closed-fixture.json'),
    serializeCompositionProject(own.project),
    { flag: 'wx' },
  );
  try {
    await page.evaluate(() => window.__TAURI_INTERNALS__.invoke('finish_close'));
  } catch (e) {
    if (!/closed/i.test(String(e))) throw e;
  }
  console.log(
    JSON.stringify(
      { pass: results.length, results, pageErrors: errors, ownFixturePreserved: true },
      null,
      2,
    ),
  );
} catch (error) {
  if (page) {
    await page.screenshot({ path: path.join(output, 'failure-renderer.png') }).catch(() => {});
    const own = await page.evaluate(() => window.nect?.document).catch(() => null);
    if (own?.name?.startsWith('NECT_DIRECT_ANCHOR_'))
      await fs.writeFile(path.join(output, 'failure-fixture.json'), serialize(own));
  }
  throw error;
} finally {
  await browser.close();
}
