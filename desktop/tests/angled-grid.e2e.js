// Native Tauri/WebView2 renderer; CDP synthetic input, not OS input.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { angledGridFixture } from './fixtures/angled-grid.js';
const output = path.resolve(process.env.NECT_ANGLED_OUTPUT || 'test-results/angled-grid/native');
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_ANGLED_CDP || 'http://127.0.0.1:9474',
);
let page;
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  [page] = pages;
  const errors = [],
    results = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  const $ = (s) => page.locator(s),
    settle = () =>
      page.evaluate(
        () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
      );
  const state = () =>
    page.evaluate(() => ({
      doc: window.nect.document,
      svg: window.nect.export(),
      file: window.nect.fileState,
      command: window.nect.commandState,
      gesture: window.nect.gestureState,
      undo: document.querySelector('[data-action="undo"]').disabled,
      selected: [...document.querySelectorAll('.node.selected')].map((n) => n.dataset.id),
    }));
  const openGrid = async () => {
    if (!(await $('[data-guide-setting="gridAngle"]').count()))
      await $('[data-action="grid-panel"]').click();
  };
  const center = async () =>
    page.evaluate(
      () =>
        window.nect.document.layers[0].nodes.find((n) => n.id === 'angled-grid-rectangle').params
          .center.value,
    );
  const moveStart = () =>
    $('g.node-stage-move[data-node-stage-move]')
      .first()
      .evaluate((el) => {
        const r = (el.querySelector('rect') || el).getBoundingClientRect();
        return [r.left + r.width / 2, r.top + r.height / 2];
      });
  for (const c of [
    { angle: 45, origin: 'top-left', zoom: 100, pan: [20, 12] },
    { angle: -30, origin: 'center', zoom: 120, pan: [-24, -16] },
  ]) {
    const f = angledGridFixture(c.angle, c.origin);
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
    await settle();
    await openGrid();
    assert.equal(await $('[data-guide-setting="gridAnchor"]').inputValue(), c.origin);
    const before = await state(),
      input = $('[data-guide-setting="gridAngle"]');
    await input.fill(String(c.angle));
    await input.press('Tab');
    await settle();
    const angled = await state();
    assert.equal(angled.doc.editor.gridAngle, c.angle);
    assert.equal(angled.doc.schemaVersion, 49);
    assert.deepEqual(angled.doc.layers, before.doc.layers);
    assert.equal(angled.svg, before.svg);
    await $('[data-action="undo"]').click();
    assert.deepEqual((await state()).doc, before.doc);
    await $('[data-action="redo"]').click();
    assert.deepEqual((await state()).doc, angled.doc);
    if (c.angle === 45) {
      await openGrid();
      const box = await $('[data-guide-setting="gridAngle"]').boundingBox(),
        prior = await state();
      await $('[data-guide-setting="gridAngle"]').focus();
      await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
      await page.mouse.down();
      await page.mouse.move(box.x + box.width / 2 + 24, box.y + box.height / 2);
      await settle();
      assert.equal(await $('[data-grid-overlay]').getAttribute('data-grid-angle'), '65');
      assert.equal((await state()).doc.editor.gridAngle, 45);
      await page.keyboard.press('Escape');
      assert.equal((await state()).gesture, null);
      await page.mouse.up();
      const cancelled = await state();
      for (const key of ['doc', 'svg', 'file', 'command', 'undo', 'selected'])
        assert.deepEqual(cancelled[key], prior[key], key);
    }
    await page.keyboard.press('Escape');
    await $('[data-action="actual-size"]').click();
    if (c.zoom === 120) await $('[data-action="zoom-in"]').click();
    const stage = await $('.canvas-stage').boundingBox();
    await page.mouse.move(stage.x + stage.width / 2, stage.y + stage.height / 2);
    await page.mouse.wheel(...c.pan);
    await settle();
    assert.equal(await $('[data-zoom="canvas"]').textContent(), `${c.zoom}%`);
    const overlay = await $('[data-grid-overlay]').evaluate((el) => ({
      angle: el.dataset.gridAngle,
      stride: Number(el.dataset.gridStride),
      paths: el.querySelectorAll('[data-grid-axis]').length,
      finite: ![...el.querySelectorAll('path')].some((p) =>
        /NaN|Infinity/.test(p.getAttribute('d')),
      ),
    }));
    assert.equal(overlay.angle, String(c.angle));
    assert.ok(overlay.paths > 0 && overlay.paths <= 2048);
    assert.equal(overlay.finite, true);
    assert.equal((await state()).doc.editor.gridSize, 40);
    const start = await moveStart(),
      matrix = await $('.artboard').evaluate((el) => {
        const m = el.getScreenCTM();
        return { a: m.a, b: m.b, c: m.c, d: m.d };
      });
    assert.equal(
      await page.evaluate(
        ([x, y]) => !!document.elementFromPoint(x, y)?.closest('[data-node-stage-move]'),
        start,
      ),
      true,
    );
    const end = [start[0] - 5 * matrix.a + 5 * matrix.c, start[1] - 5 * matrix.b + 5 * matrix.d];
    await page.mouse.move(...start);
    await page.mouse.down();
    await page.mouse.move(...end, { steps: 4 });
    await settle();
    const feedback = await $('[data-snap-kind="grid"]').evaluateAll((els) =>
      els.map((el) =>
        el
          .getAttribute('d')
          .match(/-?\d+(?:\.\d+)?(?:e[+-]?\d+)?/gi)
          .map(Number),
      ),
    );
    assert.equal(feedback.length, 2);
    for (const [x1, y1, x2, y2] of feedback)
      assert.ok(
        Math.abs((f.target[0] - x1) * (y2 - y1) - (f.target[1] - y1) * (x2 - x1)) /
          Math.hypot(x2 - x1, y2 - y1) <
          1e-7,
      );
    await page.mouse.up();
    await settle();
    const moved = await state();
    (await center()).forEach((v, i) => assert.ok(Math.abs(v - f.target[i]) < 1e-8));
    assert.equal(moved.doc.editor.gridAngle, c.angle);
    assert.equal(moved.doc.editor.gridSize, 40);
    await $('[data-action="undo"]').click();
    assert.deepEqual((await state()).doc, angled.doc);
    await $('[data-action="redo"]').click();
    assert.deepEqual((await state()).doc, moved.doc);
    await $('[data-action="undo"]').click();
    const priorCancel = await state(),
      cancelStart = await moveStart();
    await page.mouse.move(...cancelStart);
    await page.mouse.down();
    await page.mouse.move(
      cancelStart[0] - 5 * matrix.a + 5 * matrix.c,
      cancelStart[1] - 5 * matrix.b + 5 * matrix.d,
    );
    await settle();
    await page.keyboard.press('Escape');
    await page.mouse.up();
    await settle();
    const cancelled = await state();
    for (const key of ['doc', 'svg', 'file', 'command', 'undo', 'selected'])
      assert.deepEqual(cancelled[key], priorCancel[key], key);
    await page.evaluate((s) => window.nect.load(s), JSON.stringify(cancelled.doc));
    await settle();
    assert.deepEqual((await state()).doc, cancelled.doc);
    assert.equal((await state()).svg, before.svg);
    await openGrid();
    assert.equal(await $('[data-guide-setting="gridAngle"]').inputValue(), String(c.angle));
    const name = `${c.angle}-${c.origin}`;
    await page.screenshot({ path: path.join(output, `${name}-renderer.png`) });
    await fs.writeFile(path.join(output, `${name}.json`), serialize(cancelled.doc), { flag: 'wx' });
    await fs.writeFile(path.join(output, `${name}.svg`), cancelled.svg, { flag: 'wx' });
    results.push({
      ...c,
      overlay,
      target: f.target,
      snappedCenter: moved.doc.layers[0].nodes[0].params.center.value,
      feedback,
      oneUndoRedo: true,
      cancelExact: true,
      JSONReloadExact: true,
      settingsLeaveArtworkSVGUnchanged: true,
    });
  }
  assert.deepEqual(errors, []);
  await fs.writeFile(
    path.join(output, 'receipt.json'),
    JSON.stringify(
      {
        input: 'Native Tauri/WebView2 CDP synthetic angle typing/scrub/Escape, wheel, artwork drag',
        viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
        results,
        pageErrors: errors,
        notRun: ['physical/OS input', 'native OS Save/Open/clean cold reopen'],
      },
      null,
      2,
    ),
    { flag: 'wx' },
  );
  const own = await page.evaluate(() => ({
    name: window.nect.document.name,
    binding: window.nect.fileState.binding,
    gesture: window.nect.gestureState,
    project: window.nect.projectSnapshot(),
  }));
  assert.ok(own.name.startsWith('NECT_ANGLED_GRID_'));
  assert.equal(own.binding, null);
  assert.equal(own.gesture, null);
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
    JSON.stringify({ results, pageErrors: errors, directOwnFixtureClose: true }, null, 2),
  );
} catch (error) {
  if (page) {
    const own = await page.evaluate(() => window.nect?.document).catch(() => null);
    if (own?.name?.startsWith('NECT_ANGLED_GRID_'))
      await fs.writeFile(path.join(output, 'failure-fixture.json'), JSON.stringify(own, null, 2));
    await page.screenshot({ path: path.join(output, 'failure-renderer.png') }).catch(() => {});
  }
  throw error;
} finally {
  await browser.close();
}
