import { activateBoundaryCommand } from './helpers/boundary-controls.js';
// Native Tauri/WebView2 CDP synthetic input; no physical/OS input claim.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { alignmentFixture } from './fixtures/point-alignment.js';

const output = path.resolve(
  process.env.NECT_ALIGNMENT_OUTPUT || 'test-results/point-alignment/native',
);
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_ALIGNMENT_CDP || 'http://127.0.0.1:9476',
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
        undo: document.querySelector('[data-action="undo"]').disabled,
        redo: document.querySelector('[data-action="redo"]').disabled,
      })),
    xy = (point) =>
      page.evaluate(([x, y]) => {
        const p = new DOMPoint(x, y).matrixTransform(
          document.querySelector('.artboard').getScreenCTM(),
        );
        return [p.x, p.y];
      }, point),
    positions = () =>
      page.evaluate(() =>
        Object.fromEntries(
          [...document.querySelectorAll('.vertex.point-marker')].map((p) => [
            p.dataset.point,
            [Number(p.getAttribute('cx')), Number(p.getAttribute('cy'))],
          ]),
        ),
      ),
    selected = (region) =>
      page.evaluate(
        (r) => ({
          markers: [...document.querySelectorAll('.vertex.point-marker.active')].map(
            (p) => p.dataset.point,
          ),
          rows: [...document.querySelectorAll(r + ' [data-point-row].point-selected')].map(
            (p) => p.dataset.pointRow,
          ),
          focus:
            document.querySelector(r + ' [data-point-row].point-active')?.dataset.pointRow || null,
        }),
        region,
      ),
    view = () =>
      page.evaluate(() => {
        const svg = document.querySelector('.artboard'),
          m = svg.getScreenCTM();
        return {
          matrix: [m.a, m.b, m.c, m.d, m.e, m.f],
          width: svg.style.width,
          height: svg.style.height,
          label: document.querySelector('[data-zoom="canvas"]').textContent,
        };
      });
  for (const c of [
    { remapped: false, zoom: 100, pan: [18, -12], action: 'center-x', axis: 0 },
    { remapped: true, zoom: 120, pan: [-20, -70], action: 'center-y', axis: 1 },
  ]) {
    const f = alignmentFixture(c.remapped);
    f.doc.name = `NECT_ANCHOR_ALIGNMENT_${c.zoom}`;
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
    if (await $('section[aria-label="Layout Grid"]').count())
      await activateBoundaryCommand(page, 'layout-panel');
    await $('[data-tool="direct"]').click();
    await activateBoundaryCommand(page, 'actual-size');
    if (c.zoom === 120) await activateBoundaryCommand(page, 'zoom-in');
    const box = await $('.canvas-stage').boundingBox();
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.wheel(...c.pan);
    await settle();
    for (const [i, id] of f.ids.entries()) {
      if (i) await page.keyboard.down('Shift');
      const point = (await positions())[id];
      assert.ok(point, id);
      const client = await xy(point);
      const under = await page.evaluate(
        ([x, y]) => document.elementFromPoint(x, y)?.closest('[data-point]')?.dataset.point,
        client,
      );
      assert.equal(under, id, `Anchor ${id} must be visible and unobscured`);
      await page.mouse.click(...client);
      if (i) await page.keyboard.up('Shift');
      await settle();
    }
    const before = await state(),
      p = await positions(),
      beforeSelection = await selected(f.region),
      beforeView = await view(),
      coordinates = f.ids.map((id) => p[id][c.axis]),
      target = Math.min(...coordinates) / 2 + Math.max(...coordinates) / 2,
      action = () =>
        $(`${f.region} [data-point-align-node="${f.owner.id}"][data-point-align="${c.action}"]`);
    assert.deepEqual(beforeSelection.markers, f.ids);
    assert.deepEqual(beforeSelection.rows, f.ids);
    assert.equal(beforeSelection.focus, f.ids.at(-1));
    assert.equal(beforeView.label, `${c.zoom}%`);
    assert.equal(await action().isEnabled(), true);
    await action().click();
    await settle();
    const changed = await state(),
      actual = await positions();
    f.ids.forEach((id) => {
      assert.ok(Math.abs(actual[id][c.axis] - target) <= 1e-9);
      assert.ok(Math.abs(actual[id][1 - c.axis] - p[id][1 - c.axis]) <= 1e-9);
    });
    for (const id of Object.keys(p)) if (!f.ids.includes(id)) assert.deepEqual(actual[id], p[id]);
    assert.notDeepEqual(changed.doc, before.doc);
    assert.equal(changed.command.expectedRevision, before.command.expectedRevision + 1);
    assert.equal(changed.file.dirty, true);
    assert.deepEqual(await selected(f.region), beforeSelection);
    assert.deepEqual(await view(), beforeView);
    const oldOwner = before.doc.layers[0].nodes.find((n) => n.id === f.owner.id),
      newOwner = changed.doc.layers[0].nodes.find((n) => n.id === f.owner.id);
    for (const layer of before.doc.layers)
      for (const n of layer.nodes)
        if (n.id !== f.owner.id)
          assert.deepEqual(
            changed.doc.layers.find((l) => l.id === layer.id).nodes.find((x) => x.id === n.id),
            n,
          );
    if (!c.remapped)
      newOwner.points.forEach((q, i) => {
        assert.deepEqual(q.handleA, oldOwner.points[i].handleA);
        assert.deepEqual(q.handleB, oldOwner.points[i].handleB);
        assert.equal(q.handleType, oldOwner.points[i].handleType);
      });
    await page.screenshot({ path: path.join(output, `${c.zoom}-aligned.png`) });
    await action().click();
    await settle();
    assert.deepEqual(await state(), changed);
    await $('[data-action="undo"]').click();
    await settle();
    const undone = await state();
    assert.deepEqual(undone.doc, before.doc);
    assert.equal(undone.svg, before.svg);
    assert.deepEqual(await selected(f.region), beforeSelection);
    await $('[data-action="redo"]').click();
    await settle();
    const redone = await state();
    assert.deepEqual(redone.doc, changed.doc);
    assert.equal(redone.svg, changed.svg);
    assert.deepEqual(await selected(f.region), beforeSelection);
    await fs.writeFile(path.join(output, `${c.zoom}-before.json`), serialize(before.doc), {
      flag: 'wx',
    });
    await fs.writeFile(path.join(output, `${c.zoom}-aligned.json`), serialize(changed.doc), {
      flag: 'wx',
    });
    await fs.writeFile(path.join(output, `${c.zoom}-aligned.svg`), changed.svg, { flag: 'wx' });
    await page.evaluate((s) => window.nect.load(s), serialize(changed.doc));
    await settle();
    assert.deepEqual((await state()).doc, changed.doc);
    assert.equal((await state()).svg, changed.svg);
    results.push({
      ...c,
      nodeCount: f.doc.layers.reduce((n, l) => n + l.nodes.length, 0),
      connections: f.doc.layers
        .flatMap((l) => l.nodes)
        .flatMap((n) => Object.values(n.params))
        .filter((q) => q.source).length,
      target,
      pointIds: f.ids,
      view: beforeView,
      sourceUnchanged: c.remapped,
      handlesPreserved: true,
      unselectedUnchanged: true,
      oneUndoRedo: true,
      repeatNoop: true,
      JSONSVGReloadExact: true,
    });
  }
  assert.deepEqual(errors, []);
  await fs.writeFile(
    path.join(output, 'receipt.json'),
    JSON.stringify(
      {
        input: 'Native Tauri/WebView2 CDP synthetic mouse/keyboard',
        viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
        results,
        pageErrors: errors,
        notRun: ['physical/OS input', 'native Save/Open dialogs and normal clean cold reopen'],
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
  assert.ok(own.name.startsWith('NECT_ANCHOR_ALIGNMENT_'));
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
  console.log(JSON.stringify({ results, pageErrors: errors, ownFixturePreserved: true }, null, 2));
} catch (error) {
  if (page) {
    await page.keyboard.up('Shift').catch(() => {});
    await page.screenshot({ path: path.join(output, 'failure-renderer.png') }).catch(() => {});
    const own = await page.evaluate(() => window.nect?.document).catch(() => null);
    if (own?.name?.startsWith('NECT_ANCHOR_ALIGNMENT_'))
      await fs.writeFile(path.join(output, 'failure-fixture.json'), serialize(own));
  }
  throw error;
} finally {
  await browser.close();
}
