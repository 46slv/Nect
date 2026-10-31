import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { preview } from 'vite';
import { serialize } from '../src/core.js';
import { alternateRepeaterFixture } from './fixtures/repeater-alternate-scale.js';

const cdp = process.env.NECT_REPEATER_CDP_URL;
const output = path.resolve(
  process.env.NECT_REPEATER_OUTPUT || 'test-results/repeater-alternate-scale/browser-png',
);
await fs.mkdir(output, { recursive: true });
let server;
if (!cdp) {
  const config = JSON.parse(await fs.readFile('src-tauri/tauri.conf.json', 'utf8'));
  server = await preview({
    preview: {
      host: '127.0.0.1',
      port: 0,
      headers: { 'Content-Security-Policy': config.app.security.csp },
    },
  });
}
const browser = cdp
  ? await chromium.connectOverCDP(cdp)
  : await chromium.launch({
      executablePath:
        process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
      headless: true,
    });
const errors = [],
  results = [];
try {
  let page;
  if (cdp) {
    const pages = browser
      .contexts()[0]
      .pages()
      .filter((p) => /tauri\.localhost/.test(p.url()));
    assert.equal(pages.length, 1);
    [page] = pages;
  } else {
    page = await browser.newPage({ viewport: { width: 1469, height: 827 } });
    await page.goto(`http://127.0.0.1:${server.httpServer.address().port}`);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  }
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(
    (native) => window.nect && (!native || window.__TAURI_INTERNALS__),
    !!cdp,
  );
  await page.waitForTimeout(2000);
  const $ = (s) => page.locator(s);
  const settle = () =>
    page.evaluate(
      () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))),
    );
  const checkpoint = () =>
    page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      file: window.nect.fileState,
      state: window.nect.commandState,
      undo: document.querySelector('[data-action="undo"]').disabled,
      redo: document.querySelector('[data-action="redo"]').disabled,
      selected: [...document.querySelectorAll('.node.selected')].map((e) => e.dataset.id),
    }));
  for (const kind of cdp ? ['linear', 'radial'] : ['radial']) {
    const f = alternateRepeaterFixture(kind);
    await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
    await activateBoundaryCommand(page, 'graph-reset');
    await $(`.graph .node[data-id="${f.repeat.id}"] .type-tag`).click();
    await activateBoundaryCommand(page, 'fit');
    await settle();
    const field = $(
      `.inspector-body .field[data-node="${f.repeat.id}"][data-key="alternateScale"]`,
    );
    assert.ok((await field.textContent()).includes('Alternate Scale (%)'));
    const input = () => field.locator('input[data-value]').first();
    assert.equal(await input().getAttribute('min'), '0.01');
    assert.equal(await input().getAttribute('max'), '1000');
    assert.equal(await input().inputValue(), '100');
    assert.equal(await $(`.inspector-body .field[data-key="step"]`).count(), 0);
    const before = await checkpoint();
    const baseline = await page.evaluate(() => window.nect.evaluate().path.contours);
    assert.equal(baseline.length, 24);
    assert.equal(JSON.parse(before.doc).schemaVersion, 37);
    await input().fill('60');
    await input().press('Tab');
    await settle();
    const changed = await checkpoint(),
      expected = JSON.parse(before.doc);
    expected.schemaVersion = 45;
    expected.layers[0].nodes.find((n) => n.id === f.repeat.id).params.alternateScale.value = 60;
    assert.deepEqual(
      JSON.parse(changed.doc),
      expected,
      'only the parameter and required schema change',
    );
    const actual = await page.evaluate(() => window.nect.evaluate().path.contours);
    const near = (a, b) =>
      a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-8, `${a} != ${b}`));
    for (let i = 0; i < 24; i++) {
      const a = actual[i],
        b = baseline[i],
        ratio = i % 2 ? 0.6 : 1;
      assert.deepEqual({ ...a, points: [] }, { ...b, points: [] });
      const center = b.points.reduce(
        (p, q) => p.map((v, axis) => v + q.position[axis] / 4),
        [0, 0],
      );
      a.points.forEach((p, j) => {
        assert.equal(p.id, b.points[j].id);
        near(
          p.position,
          b.points[j].position.map((v, axis) => center[axis] + (v - center[axis]) * ratio),
        );
        for (const k of ['handleA', 'handleB'])
          near(
            p[k],
            b.points[j][k].map((v) => v * ratio),
          );
      });
      if (!(i % 2)) assert.deepEqual(a, b);
    }
    assert.deepEqual(changed.selected, before.selected);
    assert.notEqual(changed.svg, before.svg);
    await fs.writeFile(path.join(output, `${kind}-24.json`), changed.doc);
    await fs.writeFile(path.join(output, `${kind}-24.svg`), changed.svg);
    await fs.writeFile(path.join(output, `${kind}-identity.svg`), before.svg);
    await input().scrollIntoViewIfNeeded();
    await page.screenshot({ path: path.join(output, `${kind}-24.png`) });
    if (!cdp) {
      // Actual existing renderer export/download path, in a production-CSP
      // browser where downloading does not require an OS file dialog.
      await $('.command-menu > summary')
        .filter({ hasText: /^Export$/ })
        .click();
      const event = page.waitForEvent('download');
      await $('[data-action="export-png"]').click();
      const pngPath = path.join(output, 'radial-export.png');
      await (await event).saveAs(pngPath);
      const png = await fs.readFile(pngPath);
      const comparison = await page.evaluate(
        async ({ svg, png }) => {
          const pixels = async (url) => {
            const image = new Image();
            image.src = url;
            await image.decode();
            const canvas = document.createElement('canvas');
            canvas.width = image.width;
            canvas.height = image.height;
            const ctx = canvas.getContext('2d');
            ctx.drawImage(image, 0, 0);
            const rgba = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
            return {
              size: [canvas.width, canvas.height],
              digest: [...new Uint8Array(await crypto.subtle.digest('SHA-256', rgba))]
                .map((v) => v.toString(16).padStart(2, '0'))
                .join(''),
            };
          };
          const shapes = [...document.querySelectorAll('.artboard .shape')]
            .map((e) => {
              const c = e.cloneNode(true);
              c.removeAttribute('class');
              return c.outerHTML;
            })
            .join('');
          return {
            svg: await pixels(`data:image/svg+xml;base64,${svg}`),
            png: await pixels(`data:image/png;base64,${png}`),
            canvas: await pixels(
              URL.createObjectURL(
                new Blob(
                  [
                    `<svg xmlns="http://www.w3.org/2000/svg" width="960" height="540" viewBox="0 0 960 540">${shapes}</svg>`,
                  ],
                  { type: 'image/svg+xml' },
                ),
              ),
            ),
          };
        },
        { svg: Buffer.from(changed.svg).toString('base64'), png: png.toString('base64') },
      );
      assert.deepEqual(comparison.svg, comparison.png);
      assert.deepEqual(comparison.canvas, comparison.svg);
      results.push({
        kind,
        case: 'production-CSP existing PNG download matches SVG/Canvas pixels',
        comparison,
      });
      continue;
    }
    await $('[data-action="undo"]').click();
    await settle();
    let undone = await checkpoint();
    assert.equal(undone.doc, before.doc);
    assert.equal(undone.svg, before.svg);
    assert.equal(undone.undo, true);
    await $('[data-action="redo"]').click();
    await settle();
    assert.equal((await checkpoint()).doc, changed.doc);
    assert.equal((await checkpoint()).svg, changed.svg);
    results.push({
      kind,
      case: '24-copy geometry/handles/IDs, UI commit, one Undo/Redo and unchanged source/step/offset/opacity/order',
    });
    // Use the real numeric scrub gesture and Escape owner, not a synthetic
    // session shortcut. Late pointerup must not commit the cancelled preview.
    await input().scrollIntoViewIfNeeded();
    const cancelStart = await checkpoint(),
      box = await input().boundingBox();
    const x = box.x + box.width * 0.4,
      y = box.y + box.height / 2;
    assert.equal(
      await input().evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, { x, y }),
      true,
    );
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + 44, y, { steps: 4 });
    await settle();
    assert.notEqual(await input().inputValue(), '60');
    await page.keyboard.press('Escape');
    await page.mouse.up();
    await settle();
    assert.deepEqual(await checkpoint(), cancelStart);
    results.push({
      kind,
      case: 'numeric scrub Escape restores exact document/SVG/file/history/selection; late release neutral',
    });
    const invalidStart = await checkpoint();
    await input().fill('0');
    await input().press('Tab');
    await settle();
    assert.deepEqual(await checkpoint(), invalidStart);
    assert.match(await $('.status .message').textContent(), /Alternate Scale/);
    results.push({ kind, case: 'zero UI input rejects atomically' });
    await page.evaluate((s) => window.nect.load(s), changed.doc);
    await settle();
    assert.equal((await checkpoint()).doc, changed.doc);
    assert.equal((await checkpoint()).svg, changed.svg);
    results.push({ kind, case: 'JSON reload restores exact authored data and SVG' });
  }
  assert.deepEqual(errors, []);
  const receipt = {
    input: cdp
      ? 'CDP synthetic input in task-owned native Tauri/WebView2'
      : 'Headless Chrome production-CSP export UI',
    viewport: await page.evaluate(() => [innerWidth, innerHeight, devicePixelRatio]),
    results,
    pageErrors: errors,
    notRun: [
      'physical input',
      'actual OS focus loss',
      'OS file dialogs/cold reopen',
      'AE runtime comparison',
      ...(cdp ? ['native OS PNG save dialog'] : []),
    ],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser.close();
  if (server) await new Promise((resolve) => server.httpServer.close(resolve));
}
