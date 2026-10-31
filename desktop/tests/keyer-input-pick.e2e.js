import { activateBoundaryCommand } from './helpers/boundary-controls.js';
// Native WebView2 CDP synthetic; generated SVG -> native Canvas PNG, no OS save claim.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import { chromium } from 'playwright-core';
import { serialize, exportSVG } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { keyerPickFixture } from './fixtures/keyer-input-pick.js';
if (!process.env.NECT_KEYER_PICK_OUTPUT) throw Error('Set the dedicated D output directory');
const output = path.resolve(process.env.NECT_KEYER_PICK_OUTPUT);
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_KEYER_PICK_CDP || 'http://127.0.0.1:9480',
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
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  const f = keyerPickFixture(),
    $ = (s) => page.locator(s),
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
      }));
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  await activateBoundaryCommand(page, 'graph-fit');
  await $(`.graph .node[data-id="${f.keyer.id}"] .node-head strong`).click();
  const exportPNG = async (name) => {
      const data = await page.evaluate(async () => {
        const url = URL.createObjectURL(
          new Blob([window.nect.export()], { type: 'image/svg+xml' }),
        );
        try {
          const image = new Image();
          image.src = url;
          await image.decode();
          const canvas = document.createElement('canvas');
          canvas.width = 320;
          canvas.height = 180;
          canvas.getContext('2d').drawImage(image, 0, 0, 320, 180);
          return canvas.toDataURL('image/png').split(',')[1];
        } finally {
          URL.revokeObjectURL(url);
        }
      });
      const bytes = Buffer.from(data, 'base64');
      await fs.writeFile(path.join(output, name), bytes, { flag: 'wx' });
      return bytes;
    },
    pixels = async (input) =>
      page.evaluate(async (source) => {
        const image = new Image();
        image.src = source;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = 320;
        canvas.height = 180;
        canvas.getContext('2d').drawImage(image, 0, 0);
        return [...canvas.getContext('2d').getImageData(0, 0, 320, 180).data];
      }, input),
    pngPixels = (bytes) => pixels('data:image/png;base64,' + bytes.toString('base64')),
    svgPixels = (svg) => pixels('data:image/svg+xml;base64,' + Buffer.from(svg).toString('base64')),
    pixel = (rgba, x, y) => rgba.slice((y * 320 + x) * 4, (y * 320 + x) * 4 + 4),
    hash = (rgba) => crypto.createHash('sha256').update(Buffer.from(rgba)).digest('hex');
  const before = await state(),
    beforePNG = await exportPNG('before.png'),
    beforeRGBA = await pngPixels(beforePNG);
  assert.deepEqual(pixel(beforeRGBA, 126, 59), [12, 34, 56, 128]);
  assert.deepEqual(await state(), before);
  await $('.inspector-body [data-keyer-pick]').click();
  await $('[data-keyer-input-picker]').waitFor();
  const xy = (point) =>
    page.evaluate((p) => {
      const m = document.querySelector('[data-keyer-input-preview]').getScreenCTM(),
        v = new DOMPoint(...p).matrixTransform(m);
      return [v.x, v.y];
    }, point);
  await page.mouse.click(...(await xy([0.5, 0.5])));
  assert.equal(await $('[data-keyer-input-picker]').count(), 1);
  assert.deepEqual(await state(), before);
  await page.mouse.move(...(await xy([1.5, 0.5])));
  await page.screenshot({ path: path.join(output, 'input-preview.png') });
  assert.deepEqual(await state(), before);
  await page.mouse.click(...(await xy([1.5, 0.5])));
  await settle();
  assert.equal(await $('[data-keyer-input-picker]').count(), 0);
  const after = await state();
  assert.equal(
    after.doc.layers[0].nodes.find((n) => n.id === f.keyer.id).params.keyColor.value.hex,
    '#0c2238',
  );
  assert.equal(after.command.expectedRevision, before.command.expectedRevision + 1);
  assert.equal(after.file.dirty, true);
  assert.deepEqual(after.doc.imageAssets, before.doc.imageAssets);
  assert.equal(after.svg, exportSVG(after.doc));
  const afterPNG = await exportPNG('picked.png'),
    afterRGBA = await pngPixels(afterPNG),
    afterSVG = await svgPixels(after.svg);
  assert.deepEqual(pixel(afterRGBA, 126, 59), [0, 0, 0, 0]);
  assert.deepEqual(pixel(afterRGBA, 268, 59), [255, 0, 0, 255]);
  assert.deepEqual(afterRGBA, afterSVG);
  await page.screenshot({ path: path.join(output, 'picked-canvas.png') });
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, before.doc);
  assert.equal((await state()).svg, before.svg);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, after.doc);
  assert.equal((await state()).svg, after.svg);
  const beforeNoop = await state();
  await $('.inspector-body [data-keyer-pick]').click();
  await page.mouse.click(...(await xy([1.5, 0.5])));
  await settle();
  assert.deepEqual(await state(), beforeNoop);
  await fs.writeFile(path.join(output, 'picked.json'), serialize(after.doc), { flag: 'wx' });
  await fs.writeFile(path.join(output, 'picked.svg'), after.svg, { flag: 'wx' });
  await page.evaluate((s) => window.nect.load(s), serialize(after.doc));
  await settle();
  assert.deepEqual((await state()).doc, after.doc);
  assert.equal((await state()).svg, after.svg);
  const reloadPNG = await exportPNG('reloaded.png');
  assert.deepEqual(await pngPixels(reloadPNG), afterRGBA);
  assert.deepEqual(errors, []);
  const receipt = {
    Input: 'Native Tauri/WebView2 CDP synthetic',
    Source: '4x2 synthetic PNG; two Keyers; Image position[20,30], nonuniform size[284,118]',
    Picked: { Pixel: [1, 0], RGB: '#0c2238', OriginalAlpha: 128, KeyedAlpha: 0 },
    UpstreamTransparentRejected: true,
    StartHoverDocumentNeutral: true,
    OriginalAssetsUnchanged: true,
    OneUndoRedo: true,
    RepeatNoop: true,
    JSONSVGExact: true,
    PNGSVGFullRGBASHA256: hash(afterRGBA),
    PNGReloadFullRGBAExact: true,
    PNGRoute:
      'Native WebView generated SVG -> Canvas PNG artifact; CLI writes bytes. Actual Export PNG/OS save not verified; immutable IPC bridge not overridden',
    PageErrors: errors,
    NotRun: ['Physical/OS input', 'OS Save/Open/export dialogs', 'Normal OS cold reopen'],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  const own = await page.evaluate(() => ({
    name: window.nect.document.name,
    binding: window.nect.fileState.binding,
    project: window.nect.projectSnapshot(),
  }));
  assert.equal(own.name, 'NECT_KEYER_INPUT_PICK');
  assert.equal(own.binding, null);
  await fs.writeFile(
    path.join(output, 'closed-fixture.json'),
    serializeCompositionProject(own.project),
    { flag: 'wx' },
  );
  try {
    await page.evaluate(() => window.__TAURI_INTERNALS__.invoke('finish_close'));
  } catch (error) {
    if (!/closed/i.test(String(error))) throw error;
  }
  console.log(JSON.stringify(receipt, null, 2));
} catch (error) {
  if (page) {
    await page.screenshot({ path: path.join(output, 'failure.png') }).catch(() => {});
    const own = await page.evaluate(() => window.nect?.document).catch(() => null);
    if (own?.name === 'NECT_KEYER_INPUT_PICK')
      await fs.writeFile(path.join(output, 'failure-fixture.json'), serialize(own));
  }
  throw error;
} finally {
  await browser.close();
}
