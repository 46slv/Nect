import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { graphPerformanceFixture } from '../tests/fixtures/graph-performance.js';

assert.ok(process.env.NECT_PERF_CDP_URL && process.env.NECT_SCALE_RECEIPT);
const browser = await chromium.connectOverCDP(process.env.NECT_PERF_CDP_URL);
const page = browser
  .contexts()[0]
  .pages()
  .find((p) => /tauri\.localhost/.test(p.url()));
const receipt = { input: 'synthetic CDP in native Tauri/WebView2', cases: [], errors: [] };
page.on('pageerror', (e) => receipt.errors.push(e.message));
const click = (action) => page.locator(`[data-action="${action}"]`).click();
const settled = () => page.waitForTimeout(150);
const authored = () =>
  page.evaluate(() => ({
    json: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
    revision: window.nect.commandState.expectedRevision,
  }));
const scale = () =>
  page
    .locator('.graph')
    .evaluate((el) => Number(el.style.zoom) || new DOMMatrix(getComputedStyle(el).transform).a);
const selection = () =>
  page.locator('.graph .node.selected').evaluateAll((els) => els.map((e) => e.dataset.id).sort());
const viewport = () => page.locator('.graph-scroll').boundingBox();
const world = (p) =>
  page.evaluate((p) => {
    const g = document.querySelector('.graph'),
      r = g.getBoundingClientRect(),
      z = Number(g.style.zoom) || new DOMMatrix(getComputedStyle(g).transform).a;
    return [(p[0] - r.left) / z, (p[1] - r.top) / z];
  }, p);
const near = (a, b, tolerance) =>
  a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < tolerance, `${a} vs ${b}`));
async function load(count) {
  const f = graphPerformanceFixture(count);
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  if (await page.locator('.alignment-panel').count())
    await page.locator('.canvas-toolbar [data-action="align-panel"]').click();
  await click('graph-reset');
  await settled();
  return f;
}
async function wires(count) {
  const errors = await page.evaluate(() => {
    const g = document.querySelector('.graph'),
      r = g.getBoundingClientRect(),
      z = Number(g.style.zoom) || new DOMMatrix(getComputedStyle(g).transform).a;
    const paths = [...g.querySelectorAll('.wire')],
      errors = [];
    let i = 0;
    for (const n of window.nect.document.layers[0].nodes)
      for (const [key, q] of Object.entries(n.params)) {
        if (!q?.source) continue;
        const from = g.querySelector(`[data-source="${q.source}"]:not([data-source-port])`),
          to = [...g.querySelectorAll('.field')]
            .find((el) => el.dataset.node === n.id && el.dataset.key === key)
            ?.querySelector('.input');
        const p = paths[i++]
          ?.getAttribute('d')
          .match(/-?\d+(?:\.\d+)?(?:e[+-]?\d+)?/gi)
          .map(Number);
        if (!from || !to || !p) {
          errors.push({ missing: [n.id, key] });
          continue;
        }
        const a = from.getBoundingClientRect(),
          b = to.getBoundingClientRect();
        const deltas = [
          p[0] * z + r.left - (a.x + a.width / 2),
          p[1] * z + r.top - (a.y + a.height / 2),
          p[6] * z + r.left - (b.x + b.width / 2),
          p[7] * z + r.top - (b.y + b.height / 2),
        ];
        if (
          deltas.some((v) => Math.abs(v) > 0.1) ||
          Math.abs(p[2] - p[0] - 80) > 1e-6 ||
          Math.abs(p[6] - p[4] - 80) > 1e-6
        )
          errors.push({ wire: i, deltas });
      }
    return { errors, count: paths.length };
  });
  assert.equal(errors.count, count);
  assert.deepEqual(errors.errors, []);
}
try {
  const f = await load(160),
    before = await authored(),
    chosen = await selection();
  const r = await viewport(),
    center = [r.x + (r.width - 15) / 2, r.y + (r.height - 15) / 2],
    point = await world(center);
  await click('graph-zoom-in');
  await settled();
  near(await world(center), point, 1.5 / (await scale()));
  await wires(f.connectionCount);
  const pointer = [r.x + 20, r.y + 20],
    atPointer = await world(pointer);
  await page.mouse.move(...pointer);
  await page.keyboard.down('Control');
  await page.mouse.wheel(0, -120);
  await page.keyboard.up('Control');
  await settled();
  near(await world(pointer), atPointer, 1.5 / (await scale()));
  await wires(f.connectionCount);
  assert.deepEqual(await authored(), before);
  assert.deepEqual(await selection(), chosen);
  await click('graph-reset');
  await settled();
  assert.equal(await scale(), 1);
  near(
    await page.locator('.graph-scroll').evaluate((el) => [el.scrollLeft, el.scrollTop]),
    [0, 0],
    0.01,
  );
  receipt.cases.push(
    '160-node center/pointer zoom anchors, cubic wire attachment, reset and document/selection neutrality',
  );

  if (!process.env.NECT_SCALE_BASELINE_ONLY) {
    await page.locator('.graph-scroll').focus();
    for (let i = 0; i < 17; i++) await page.keyboard.press('-');
    assert.equal(await scale(), 0.05);
    await settled();
    await wires(f.connectionCount);
    for (let i = 0; i < 21; i++) await page.keyboard.press('+');
    assert.equal(await scale(), 2);
    await settled();
    await wires(f.connectionCount);
    const extents = await page.evaluate(() => {
      const g = document.querySelector('.graph'),
        s = document.querySelector('.graph-space');
      return [
        s.clientWidth,
        s.clientHeight,
        (parseFloat(g.style.width) + 32) * 2,
        (parseFloat(g.style.height) + 32) * 2,
      ];
    });
    near(extents.slice(0, 2), extents.slice(2), 1);
    assert.deepEqual(await authored(), before);
    assert.deepEqual(await selection(), chosen);
    receipt.cases.push('5%/200% limits, explicit scaled scroll extents and wires');

    await click('graph-reset');
    await settled();
    await page.mouse.move(r.x + 10, r.y + 10);
    await page.mouse.down({ button: 'middle' });
    await page.mouse.move(r.x - 30, r.y - 15);
    await page.mouse.up({ button: 'middle' });
    const pan = await page.locator('.graph-scroll').evaluate((el) => [el.scrollLeft, el.scrollTop]);
    assert.ok(pan[0] > 38 && pan[1] > 23);
    assert.deepEqual(await authored(), before);
    await click('graph-reset');
    await click('graph-zoom-out');
    await settled();
    const z = await scale(),
      head = await page.locator(`.node[data-id="${f.dragNodeId}"] .type-tag`).boundingBox(),
      x = head.x + head.width / 2,
      y = head.y + head.height / 2;
    await page.keyboard.down('Control');
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + 36, y + 18, { steps: 3 });
    assert.equal((await authored()).json, before.json);
    await wires(f.connectionCount);
    await page.mouse.up();
    await page.keyboard.up('Control');
    await settled();
    const moved = JSON.parse((await authored()).json),
      expected = JSON.parse(before.json),
      position = moved.layers[0].nodes.find((n) => n.id === f.dragNodeId).position;
    near(position, [40 + 36 / z, 50 + 18 / z], 0.1);
    expected.layers[0].nodes.find((n) => n.id === f.dragNodeId).position = position;
    assert.deepEqual(moved, expected);
    assert.equal((await authored()).svg, before.svg);
    await click('undo');
    await settled();
    assert.equal((await authored()).json, before.json);
    assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
    await click('redo');
    await settled();
    assert.deepEqual(JSON.parse((await authored()).json), expected);
    await click('undo');
    await settled();
    await wires(f.connectionCount);
    receipt.cases.push(
      'middle pan and fractional-scale node preview/one Undo/Redo preserve geometry/output',
    );

    const small = await load(16),
      original = await authored();
    await click('graph-fit');
    await settled();
    async function inView(selector) {
      const r = await viewport(),
        boxes = await page
          .locator(selector)
          .evaluateAll((els) => els.map((e) => e.getBoundingClientRect().toJSON()));
      for (const b of boxes)
        assert.ok(
          b.left >= r.x - 1 &&
            b.top >= r.y - 1 &&
            b.right <= r.x + r.width - 14 &&
            b.bottom <= r.y + r.height - 14,
          JSON.stringify({ b, r }),
        );
    }
    await inView('.graph .node');
    await wires(small.connectionCount);
    await click('graph-reset');
    await page.locator(`.node[data-id="${small.doc.layers[0].nodes[0].id}"] .type-tag`).click();
    await page
      .locator(`.node[data-id="${small.doc.layers[0].nodes[1].id}"] .type-tag`)
      .click({ modifiers: ['Shift'] });
    const selected = await selection();
    assert.equal(selected.length, 2);
    await click('graph-fit-selection');
    await settled();
    await inView('.graph .node.selected');
    await wires(small.connectionCount);
    assert.deepEqual(await authored(), original);
    assert.deepEqual(await selection(), selected);
    receipt.cases.push(
      'measured fit-all/fit-selection keep cards in viewport, stable selection and neutral output/history',
    );
  }
  assert.deepEqual(receipt.errors, []);
  receipt.viewport = await page.evaluate(() => ({
    width: innerWidth,
    height: innerHeight,
    dpr: devicePixelRatio,
  }));
  await page.screenshot({ path: process.env.NECT_SCALE_RECEIPT.replace(/\.json$/, '.png') });
  await fs.writeFile(process.env.NECT_SCALE_RECEIPT, JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser.close();
}
