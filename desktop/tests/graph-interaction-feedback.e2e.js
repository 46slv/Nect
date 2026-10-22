import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(5000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const viewport = page.locator('.graph-scroll');
const selected = () =>
  page
    .locator('.graph .node.selected')
    .evaluateAll((nodes) => nodes.map((node) => node.dataset.id).sort());
const state = () =>
  page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    revision: window.nect.commandState.expectedRevision,
  }));
const scroll = () => viewport.evaluate((element) => element.scrollLeft);
let ids;
async function reset() {
  ids = await page.evaluate(async () => {
    const { document, node } = await import('/src/core.js');
    const doc = document(),
      layer = doc.layers[0];
    const nodes = [
      node('Circle', 100, 55),
      node('Rectangle', 435, 135),
      node('Ellipse', 770, 55),
      node('Fill', 1850, 55),
      node('Output', 2200, 55),
    ];
    for (const n of nodes) n.collapsed = true;
    layer.nodes = nodes;
    layer.output = nodes.at(-1).id;
    window.nect.load(JSON.stringify(doc));
    return nodes.map((node) => node.id);
  });
  await page.waitForTimeout(60);
}
const card = (id) => page.locator(`.graph .node[data-id="${id}"]`);
async function boxFor(list) {
  const boxes = await Promise.all(list.map((id) => card(id).boundingBox()));
  return [
    [Math.min(...boxes.map((b) => b.x)) - 10, Math.min(...boxes.map((b) => b.y)) - 10],
    [
      Math.max(...boxes.map((b) => b.x + b.width)) + 10,
      Math.max(...boxes.map((b) => b.y + b.height)) + 10,
    ],
  ];
}
async function rectangle(list, { reverse = false, shift = false, release = true } = {}) {
  let [from, to] = await boxFor(list);
  if (reverse) [from, to] = [to, from];
  assert.equal(
    await page.evaluate(([x, y]) => !!document.elementFromPoint(x, y)?.closest('.node'), from),
    false,
    'starts on graph whitespace',
  );
  if (shift) await page.keyboard.down('Shift');
  await page.mouse.move(...from);
  await page.mouse.down();
  await page.mouse.move(...to, { steps: 6 });
  if (release) await page.mouse.up();
  if (shift) await page.keyboard.up('Shift');
}
async function pick(id) {
  const [blank] = await boxFor([ids[0]]);
  await page.mouse.click(...blank);
  await card(id).locator('.node-head .type-tag').click();
}
const output = () => page.locator(`.graph [data-source="${ids[0]}"]`).first();
const input = () => page.locator(`.graph [data-node="${ids[3]}"][data-key="path"] .socket`);
async function beginWire(locator = output()) {
  const box = await locator.boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  assert.equal(await page.locator('.wire-preview').count(), 1);
}
async function edge(right = true) {
  const box = await viewport.boundingBox();
  const point = [right ? box.x + box.width - 8 : box.x + 8, box.y + 70];
  await page.mouse.move(...point);
  return point;
}
async function stopped() {
  const at = await scroll();
  await page.waitForTimeout(160);
  assert.equal(await scroll(), at);
  assert.equal(await page.locator('.wire-preview').count(), 0);
}
try {
  await fs.mkdir('test-results', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await reset();
  const baseline = await state();
  await rectangle(ids.slice(0, 2), { release: false });
  await page.screenshot({
    path: `test-results/graph-marquee-${process.argv.includes('--repro') ? 'before' : 'after'}.png`,
  });
  await page.mouse.up();
  const firstSelection = await selected();
  await beginWire();
  const initialScroll = await scroll();
  await edge();
  await page.waitForTimeout(550);
  const wireScroll = await scroll();
  await page.keyboard.press('Escape');
  await page.mouse.up();
  if (process.argv.includes('--repro')) {
    const proof = {
      source: 'a7f5ec98fd767cf7174074487f4d8fdb99a13233',
      rectangleExpected: ids.slice(0, 2).sort(),
      rectangleActual: firstSelection,
      wireScrollBefore: initialScroll,
      wireScrollAfter: wireScroll,
      unchanged: JSON.stringify(await state()) === JSON.stringify(baseline),
    };
    await fs.writeFile(
      'test-results/graph-interaction-before.json',
      JSON.stringify(proof, null, 2),
    );
    assert.notDeepEqual(firstSelection, proof.rectangleExpected);
    assert.equal(wireScroll, initialScroll);
    assert.ok(proof.unchanged);
    console.log(
      'REPRODUCED a7f5ec9: whitespace drag does not box-select; wire drag at viewport edge does not pan; document/revision unchanged.',
    );
  } else {
    assert.deepEqual(firstSelection, ids.slice(0, 2).sort());
    assert.ok(wireScroll > initialScroll);
    await stopped();
    assert.deepEqual(await state(), baseline);
    await page.locator('[data-action="graph-reset"]').click();
    await rectangle(ids.slice(0, 2), { reverse: true });
    assert.deepEqual(await selected(), ids.slice(0, 2).sort());
    await pick(ids[0]);
    await rectangle([ids[2]], { shift: true });
    const combined = [ids[0], ids[2]].sort();
    assert.deepEqual(await selected(), combined);
    for (const reason of ['Escape', 'pointercancel', 'lostpointercapture', 'blur']) {
      await rectangle(ids.slice(0, 2), { release: false });
      if (reason === 'Escape') await page.keyboard.press('Escape');
      else if (reason === 'blur')
        await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      else await viewport.dispatchEvent(reason, { pointerId: 1 });
      await page.mouse.up();
      assert.deepEqual(await selected(), combined, reason);
      assert.equal(await page.locator('.graph-marquee').count(), 0);
      assert.deepEqual(await state(), baseline);
    }
    // A click/jitter is not a rectangle. Shift preserves; plain blank click clears.
    const [blank] = await boxFor([ids[0]]);
    await page.keyboard.down('Shift');
    await page.mouse.move(...blank);
    await page.mouse.down();
    await page.mouse.move(blank[0] + 2, blank[1] + 1);
    await page.mouse.up();
    await page.keyboard.up('Shift');
    assert.deepEqual(await selected(), combined);
    await page.mouse.click(...blank);
    assert.deepEqual(await selected(), []);
    await page.locator('[data-action="graph-zoom-out"]').click();
    const vb = await viewport.boundingBox();
    await page.mouse.move(vb.x + 450, vb.y + 260);
    await page.mouse.down({ button: 'middle' });
    await page.mouse.move(vb.x + 410, vb.y + 250);
    await page.mouse.up({ button: 'middle' });
    await rectangle(ids.slice(0, 2), { reverse: true });
    assert.deepEqual(await selected(), ids.slice(0, 2).sort());
    assert.deepEqual(await state(), baseline);
    // Selection must preserve an existing authored redo branch.
    await page.evaluate((id) => {
      const s = window.nect.commandState;
      window.nect.dispatch({
        ...s,
        type: 'set-parameter',
        payload: {
          layerId: window.nect.document.layers[0].id,
          nodeId: id,
          key: 'radius',
          value: 160,
        },
      });
      window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
    }, ids[0]);
    const beforeSelection = await state();
    await rectangle([ids[2]]);
    assert.deepEqual(await state(), beforeSelection);
    await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
    assert.equal(
      await page.evaluate(
        (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.radius.value,
        ids[0],
      ),
      160,
    );

    // At native and zoomed graph scales, pan far enough to reach an offscreen port.
    for (const zoomed of [false, true]) {
      await reset();
      if (zoomed) await page.locator('[data-action="graph-zoom-out"]').click();
      const before = await state();
      await beginWire();
      const cursor = await edge();
      await page.waitForFunction((id) => {
        const p = document
            .querySelector(`[data-node="${id}"][data-key="path"] .socket`)
            .getBoundingClientRect(),
          v = document.querySelector('.graph-scroll').getBoundingClientRect();
        return p.x > v.x + 30 && p.right < v.right - 110;
      }, ids[3]);
      const end = await page.locator('.wire-preview').evaluate((el) => {
        const n = el
            .getAttribute('d')
            .match(/-?\d+(?:\.\d+)?(?:e[+-]?\d+)?/gi)
            .map(Number),
          g = document.querySelector('.graph'),
          r = g.getBoundingClientRect(),
          z = Number(getComputedStyle(g).zoom);
        return [r.x + n.at(-2) * z, r.y + n.at(-1) * z];
      });
      assert.ok(
        Math.hypot(end[0] - cursor[0], end[1] - cursor[1]) < 2,
        JSON.stringify({ end, cursor, zoomed }),
      );
      const target = await input().boundingBox();
      await page.mouse.move(target.x + target.width / 2, target.y + target.height / 2);
      await page.mouse.up();
      assert.equal(
        await page.evaluate(
          (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.path.source,
          ids[3],
        ),
        ids[0],
      );
      await stopped();
      await page.locator('[data-action="undo"]').click();
      assert.equal((await state()).doc, before.doc);
      await page.locator('[data-action="redo"]').click();
      await beginWire(input());
      const at = await scroll();
      await edge(false);
      await page.waitForTimeout(320);
      assert.ok((await scroll()) < at);
      await page.mouse.up();
      await stopped();
      assert.equal((await state()).doc, before.doc);
    }
    for (const reason of [
      'Escape',
      'pointercancel',
      'lostpointercapture',
      'blur',
      'release',
      'outside-viewport',
    ]) {
      await reset();
      const before = await state();
      await beginWire();
      await edge();
      await page.waitForTimeout(200);
      assert.ok((await scroll()) > 0);
      if (reason === 'Escape') await page.keyboard.press('Escape');
      else if (['pointercancel', 'lostpointercapture'].includes(reason))
        await viewport.dispatchEvent(reason, { pointerId: 1 });
      else if (reason === 'blur')
        await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      else if (reason === 'outside-viewport') await page.mouse.move(-20, -20);
      await page.mouse.up();
      await stopped();
      assert.deepEqual(await state(), before);
    }
    assert.deepEqual(errors, []);
    await fs.writeFile(
      'test-results/graph-interaction-after.json',
      JSON.stringify(
        {
          marquee: 'PASS',
          wireAutoPan: 'PASS',
          delivery:
            'scoped Chromium pointer/keyboard; cancellation events include synthetic dispatch',
          cases:
            'forward/reverse/Shift/cancel/jitter/zoom/pan/redo; offscreen connect/disconnect at two scales, endpoint geometry and stop on release/cancel',
        },
        null,
        2,
      ),
    );
    console.log(
      'PASS: marquee selection/Shift/reverse/cancel/zoom/pan/click/redo preservation; wire auto-pan/offscreen connect/disconnect/Undo, endpoint hit geometry and cancellation stop.',
    );
  }
} finally {
  await browser.close();
}
