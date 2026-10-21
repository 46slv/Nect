import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { serialize, node } from '../src/core.js';
import { fillFixture } from './fixtures/repeater-alternate-fill.js';
import { formatSvgNumber } from '../src/svg-number-format.js';
const output = path.resolve(
  process.env.NECT_REPEATER_FILL_OUTPUT || 'test-results/repeater-alternate-fill/native',
);
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_REPEATER_FILL_CDP || 'http://127.0.0.1:9467',
);
const results = [],
  errors = [];
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  const [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  await page.waitForTimeout(2000);
  const $ = (s) => page.locator(s);
  const settle = () =>
    page.evaluate(
      () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
    );
  const state = () =>
    page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      file: window.nect.fileState,
      command: window.nect.commandState,
      undo: document.querySelector('[data-action="undo"]').disabled,
      redo: document.querySelector('[data-action="redo"]').disabled,
      selected: [...document.querySelectorAll('.node.selected')].map((e) => e.dataset.id),
    }));
  for (const kind of ['linear', 'radial']) {
    const f = fillFixture(kind);
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
    await $('[data-action="graph-reset"]').click();
    await $(`.graph .node[data-id="${f.repeat.id}"] .type-tag`).click();
    await $('[data-action="fit"]').click();
    await settle();
    const field = (key) =>
      $(`.inspector-body .field[data-node="${f.repeat.id}"][data-key="${key}"]`);
    const toggle = () => field('alternateFill').locator('input[type="checkbox"]');
    const alpha = () => field('alternateColor').locator('[data-color="alpha"]');
    assert.equal(await toggle().isChecked(), false);
    assert.match(
      await $('.inspector-body .repeater-fill-limits').textContent(),
      /existing solid fills only.*None\/gradients/,
    );
    const before = await state(),
      baseline = await page.evaluate(() => window.nect.evaluate().path.contours);
    await toggle().check();
    await settle();
    const on = await state(),
      expected = JSON.parse(before.doc);
    expected.layers[0].nodes.find((n) => n.id === f.repeat.id).params.alternateFill.value = true;
    assert.deepEqual(JSON.parse(on.doc), expected);
    const contours = await page.evaluate(() => window.nect.evaluate().path.contours);
    assert.deepEqual(
      contours.map(({ repeaterFill, ...c }) => c),
      baseline,
    );
    assert.deepEqual(on.selected, before.selected);
    const paints = await page.evaluate((svg) => {
      const root = new DOMParser().parseFromString(svg, 'image/svg+xml');
      return [...root.querySelectorAll('path')].map((p) => ({
        fill: p.getAttribute('fill'),
        alpha: p.getAttribute('fill-opacity'),
        stroke: p.getAttribute('stroke'),
        strokeAlpha: p.getAttribute('stroke-opacity'),
        opacity: p.parentElement.getAttribute('opacity'),
        opacityAncestors: [
          ...(function* (e) {
            for (let n = e.parentElement; n; n = n.parentElement) yield n;
          })(p),
        ].filter((e) => e.hasAttribute('opacity')).length,
      }));
    }, on.svg);
    assert.equal(paints.length, 24);
    paints.forEach((p, i) =>
      assert.deepEqual(p, {
        fill: i % 2 ? '#c43d27' : '#285d8f',
        alpha: i % 2 ? '0.35' : '0.6',
        stroke: '#17232d',
        strokeAlpha: '0.4',
        opacity: formatSvgNumber(baseline[i].repeaterOpacity),
        opacityAncestors: 1,
      }),
    );
    await $('[data-action="undo"]').click();
    await settle();
    assert.equal((await state()).doc, before.doc);
    assert.equal((await state()).svg, before.svg);
    assert.equal((await state()).undo, true);
    await $('[data-action="redo"]').click();
    await settle();
    assert.equal((await state()).doc, on.doc);
    assert.equal((await state()).svg, on.svg);
    results.push({
      kind,
      case: '24-copy odd/even solid colors, unchanged combined scale/geometry/IDs/source/stroke/order, one opacity wrapper, one Undo/Redo',
    });
    await alpha().fill('0.25');
    await alpha().press('Tab');
    await settle();
    assert.equal(
      await page.evaluate(
        (id) =>
          window.nect.document.layers[0].nodes.find((n) => n.id === id).params.alternateColor.value
            .alpha,
        f.repeat.id,
      ),
      0.25,
    );
    await $('[data-action="undo"]').click();
    await settle();
    assert.equal((await state()).doc, on.doc);
    // Synthetic color-control change, not the OS color picker. Uses the same
    // native-renderer event handler/Session command as the existing Color field.
    await field('alternateColor')
      .locator('[data-color="hex"]')
      .evaluate((el) => {
        el.value = '#7828a3';
        el.dispatchEvent(new Event('change', { bubbles: true }));
      });
    await settle();
    assert.ok((await state()).svg.includes('fill="#7828a3"'));
    await $('[data-action="undo"]').click();
    await settle();
    assert.equal((await state()).doc, on.doc);
    results.push({
      kind,
      case: 'shared Color hex/alpha controls edit and Undo without changing the base paint/source',
    });
    await alpha().scrollIntoViewIfNeeded();
    const cancelStart = await state(),
      box = await alpha().boundingBox(),
      x = box.x + box.width * 0.4,
      y = box.y + box.height / 2;
    assert.equal(
      await alpha().evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, { x, y }),
      true,
    );
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + 44, y, { steps: 4 });
    await settle();
    assert.notEqual(await alpha().inputValue(), '0.35');
    await page.keyboard.press('Escape');
    await page.mouse.up();
    await settle();
    assert.deepEqual(await state(), cancelStart);
    results.push({
      kind,
      case: 'Color-alpha scrub Escape and late release restore exact document/SVG/file/history/selection',
    });
    await fs.writeFile(path.join(output, `${kind}-24.json`), on.doc);
    await fs.writeFile(path.join(output, `${kind}-24.svg`), on.svg);
    await page.evaluate((s) => window.nect.load(s), on.doc);
    await settle();
    assert.equal((await state()).doc, on.doc);
    assert.equal((await state()).svg, on.svg);
    await $(`.graph .node[data-id="${f.repeat.id}"] .type-tag`).click();
    await alpha().scrollIntoViewIfNeeded();
    await settle();
    const comparison = await page.evaluate(async (svg) => {
      const pixels = async (source) => {
        const url = URL.createObjectURL(new Blob([source], { type: 'image/svg+xml' }));
        try {
          const image = new Image();
          image.src = url;
          await image.decode();
          const c = document.createElement('canvas');
          c.width = 960;
          c.height = 540;
          const ctx = c.getContext('2d');
          ctx.drawImage(image, 0, 0);
          return [
            ...new Uint8Array(
              await crypto.subtle.digest('SHA-256', ctx.getImageData(0, 0, 960, 540).data),
            ),
          ]
            .map((v) => v.toString(16).padStart(2, '0'))
            .join('');
        } finally {
          URL.revokeObjectURL(url);
        }
      };
      const shapes = [...document.querySelectorAll('.artboard .shape')]
        .map((el) => {
          const c = el.cloneNode(true);
          c.removeAttribute('class');
          return c.outerHTML;
        })
        .join('');
      return {
        svg: await pixels(svg),
        canvas: await pixels(
          `<svg xmlns="http://www.w3.org/2000/svg" width="960" height="540" viewBox="0 0 960 540">${shapes}</svg>`,
        ),
      };
    }, on.svg);
    assert.equal(comparison.svg, comparison.canvas);
    await page.screenshot({ path: path.join(output, `${kind}-24.png`) });
    results.push({
      kind,
      case: 'exact JSON reload/SVG and full Canvas-vs-SVG RGBA comparison',
      comparison,
    });
  }
  // One representative final gradient route is explicitly unaffected when ON.
  const f = fillFixture('radial'),
    gradient = node('Linear Gradient Fill');
  gradient.id = f.fill.id;
  gradient.params.path.source = f.repeat.id;
  f.layer.nodes[f.layer.nodes.findIndex((n) => n.id === f.fill.id)] = gradient;
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  await settle();
  const before = await state();
  await page.evaluate(
    ({ layerId, nodeId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-parameter',
        payload: { layerId, nodeId, key: 'alternateFill', value: true },
      }),
    { layerId: f.layer.id, nodeId: f.repeat.id },
  );
  await settle();
  assert.equal((await state()).svg, before.svg);
  results.push({
    case: 'native final gradient remains byte-identical when Alternate Fill is enabled',
  });
  assert.deepEqual(errors, []);
  const receipt = {
    input:
      'Native Tauri/WebView2 CDP synthetic mouse/keyboard and color-control DOM change; no OS picker',
    viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
    results,
    pageErrors: errors,
    notRun: [
      'physical input',
      'OS color picker',
      'OS dialogs/cold reopen',
      'actual OS focus loss',
      'unrelated full audit',
    ],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser.close();
}
