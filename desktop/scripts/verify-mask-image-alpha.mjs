// Production Web/CSP headless verification; no native GUI/profile/OS dialogs.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { serialize, exportSVG, connect } from '../src/core.js';
import { imageAlphaFixture } from '../tests/fixtures/mask-image-alpha.js';
import { strokeFeatherFixture } from '../tests/fixtures/mask-feather.js';

const output = process.argv[2];
assert.ok(output && path.isAbsolute(output), 'Pass a new absolute output directory');
await fs.mkdir(output);
const config = JSON.parse(await fs.readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = await preview({
  preview: {
    host: '127.0.0.1',
    port: 0,
    headers: { 'Content-Security-Policy': config.app.security.csp },
  },
});
const hash = (data) => createHash('sha256').update(Buffer.from(data)).digest('hex');
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({
      viewport: { width: 1600, height: 900 },
      deviceScaleFactor: 1,
    }),
    errors = [],
    external = [];
  await page.route('**/*', (route) => {
    const url = route.request().url();
    if (!/^(http:\/\/127\.0\.0\.1:|data:|blob:|about:)/.test(url)) {
      external.push(url);
      return route.abort();
    }
    return route.continue();
  });
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(server.resolvedUrls.local[0]);
  await page.waitForFunction(() => window.nect);
  assert.equal(await page.evaluate(() => typeof window.__TAURI_INTERNALS__), 'undefined');
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
      }));
  const initial = imageAlphaFixture({ connectMask: false });
  await page.evaluate((s) => window.nect.load(s), serialize(initial.doc));
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  const selector = () => $('.graph [data-node="example-mask"][data-key="mask"] select'),
    before = await state();
  await selector().waitFor({ state: 'visible' });
  assert.ok(await selector().locator('option[value="alpha-material"]').count());
  assert.ok(await selector().locator('option[value="alpha-keyer"]').count());
  await selector().selectOption('alpha-material');
  await settle();
  const after = await state();
  assert.equal(after.doc.schemaVersion, 51);
  assert.equal(
    after.doc.layers[0].nodes.find((n) => n.id === initial.mask.id).params.mask.source,
    initial.material.id,
  );
  assert.equal(after.command.expectedRevision, before.command.expectedRevision + 1);
  await selector().selectOption('alpha-material');
  await settle();
  assert.deepEqual(await state(), after);
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, before.doc);
  assert.equal((await state()).svg, before.svg);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, after.doc);
  await selector().selectOption('alpha-keyer');
  await settle();
  assert.equal(
    (await state()).doc.layers[0].nodes.find((n) => n.id === initial.mask.id).params.mask.source,
    initial.alphaKeyer.id,
  );

  const source = imageAlphaFixture().doc,
    sourceOut = source.layers[0].nodes.find((n) => n.id === 'example-output');
  sourceOut.type = 'Image Output';
  sourceOut.params.path.type = 'Image';
  sourceOut.params.path.source = 'example-keyer';
  const placed = imageAlphaFixture();
  placed.material.params.position.value = [10, 10];
  placed.material.params.size.value = [40, 40];
  const composition = imageAlphaFixture(),
    stroke = strokeFeatherFixture(0);
  composition.doc.layers.unshift(stroke.source);
  composition.layer.nodes.splice(composition.layer.nodes.indexOf(composition.mask), 0, stroke.ref);
  connect(composition.layer, stroke.ref.id, composition.mask.id, 'path');
  const cases = [
      ['00-image-alpha', imageAlphaFixture().doc],
      ['01-same-alpha-different-rgb', imageAlphaFixture({ rgbVariant: true }).doc],
      ['02-keyer-alpha', imageAlphaFixture({ keyed: true }).doc],
      ['03-invert', imageAlphaFixture({ invert: true }).doc],
      ['04-feather-4', imageAlphaFixture({ feather: 4 }).doc],
      ['05-feather-invert', imageAlphaFixture({ feather: 4, invert: true }).doc],
      ['06-placed', placed.doc],
      ['07-scaled', imageAlphaFixture({ postTransform: 'scale' }).doc],
      ['08-rotated', imageAlphaFixture({ postTransform: 'rotate' }).doc],
      ['09-composition', composition.doc],
      ['10-source', source],
      ['11-keyer-feather', imageAlphaFixture({ keyed: true, feather: 4 }).doc],
    ],
    results = [],
    rasters = new Map();
  for (const [prefix, doc] of cases) {
    const json = serialize(doc),
      svg = exportSVG(doc);
    await fs.writeFile(path.join(output, prefix + '.json'), json, { flag: 'wx' });
    await fs.writeFile(path.join(output, prefix + '.svg'), svg, { flag: 'wx' });
    await page.evaluate((s) => window.nect.load(s), json);
    await settle();
    assert.deepEqual((await state()).doc, doc);
    assert.equal((await state()).svg, svg);
    const button = $('[data-action="export-png"]');
    if (!(await button.isVisible()))
      await $('.command-menu>summary')
        .filter({ hasText: /^Export$/ })
        .click();
    assert.equal(await $('#png-scale').inputValue(), '1');
    const pending = page.waitForEvent('download');
    await button.click();
    const file = path.join(output, prefix + '.png');
    await (await pending).saveAs(file);
    const png = await fs.readFile(file);
    const data = await page.evaluate(
      async ({ svg, png, width, height }) => {
        const pixels = async (url) => {
          const img = new Image();
          img.src = url;
          await img.decode();
          const c = document.createElement('canvas');
          c.width = width;
          c.height = height;
          const x = c.getContext('2d');
          x.drawImage(img, 0, 0, width, height);
          return [...x.getImageData(0, 0, width, height).data];
        };
        return {
          svg: await pixels('data:image/svg+xml;base64,' + btoa(unescape(encodeURIComponent(svg)))),
          png: await pixels(png),
        };
      },
      {
        svg,
        png: 'data:image/png;base64,' + png.toString('base64'),
        width: doc.width,
        height: doc.height,
      },
    );
    assert.equal(hash(data.svg), hash(data.png));
    rasters.set(prefix, { rgba: data.png, width: doc.width });
    results.push({
      Case: prefix,
      PNGBytes: png.length,
      PNG_SHA256: hash(png),
      SVGPNGFullRGBASHA256: hash(data.png),
    });
  }
  const pixel = (key, x, y) => {
      const r = rasters.get(key);
      return r.rgba.slice((y * r.width + x) * 4, (y * r.width + x) * 4 + 4);
    },
    alpha = (key, x, y) => pixel(key, x, y)[3];
  assert.deepEqual(
    [10, 30, 50, 70].map((x) => alpha('00-image-alpha', x, 60)),
    [0, 128, 128, 255],
  );
  assert.equal(
    hash(rasters.get('00-image-alpha').rgba),
    hash(rasters.get('01-same-alpha-different-rgb').rgba),
  );
  assert.equal(alpha('02-keyer-alpha', 30, 60), 0);
  assert.equal(alpha('02-keyer-alpha', 50, 60), 128);
  assert.equal(alpha('02-keyer-alpha', 70, 60), 255);
  assert.ok(alpha('11-keyer-feather', 30, 60) < 10);
  // x70 is within the Gaussian tail of the image's right edge at80. Removing
  // distant green pixels must preserve the same blurred right-side coverage.
  assert.equal(alpha('11-keyer-feather', 70, 60), alpha('04-feather-4', 70, 60));
  assert.equal(alpha('00-image-alpha', 50, 10), 64);
  let complementMaxError = 0;
  for (const [normal, inverse] of [
    ['00-image-alpha', '03-invert'],
    ['04-feather-4', '05-feather-invert'],
  ])
    for (let y = 0; y < 80; y++)
      for (let x = 0; x < 80; x++) {
        const a = pixel(normal, x, y),
          base = pixel('10-source', x, y);
        complementMaxError = Math.max(
          complementMaxError,
          Math.abs(a[3] + alpha(inverse, x, y) - base[3]),
        );
        if (a[3]) assert.deepEqual(a.slice(0, 3), base.slice(0, 3));
      }
  assert.ok(complementMaxError <= 1);
  assert.ok(alpha('04-feather-4', 10, 60) > alpha('00-image-alpha', 10, 60));
  assert.ok(alpha('04-feather-4', 20, 60) > 0 && alpha('04-feather-4', 20, 60) < 128);
  assert.deepEqual(
    [5, 15, 25, 35, 45, 55].map((x) => alpha('06-placed', x, 45)),
    [0, 0, 128, 128, 255, 0],
  );
  assert.deepEqual(
    [30, 70, 110, 150].map((x) => alpha('07-scaled', x, 70)),
    [0, 128, 128, 255],
  );
  assert.deepEqual(
    [10, 30, 50, 70].map((y) => alpha('08-rotated', 30, y)),
    [0, 128, 128, 255],
  );
  assert.equal(alpha('09-composition', 25, 20), 64);
  // Sample the plateaus, avoiding the scaled bitmap's interpolated band edges.
  assert.equal(alpha('09-composition', 50, 20), 64);
  assert.equal(alpha('09-composition', 65, 40), 128);
  await page.evaluate((s) => window.nect.load(s), serialize(imageAlphaFixture({ feather: 4 }).doc));
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await settle();
  await page.screenshot({ path: path.join(output, 'image-alpha-mask.png') });
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  const receipt = {
    Mode: 'Headless Edge; production Web/CSP; actual web Export PNG download',
    UI: { PickerImageAndKeyer: true, OneConnectionUndoRedo: true, RepeatedConnectionNeutral: true },
    Cases: results,
    RGBIndependent: true,
    AlphaSamples: [0, 128, 128, 255],
    SourcePartialAlphaProduct: 64,
    InvertComplementMax8BitError: complementMaxError,
    ColorDetailUnblurred: true,
    CompositionStrokeAlphaPreserved: true,
    PlacementScaleRotationVerified: true,
    JSONSVGExact: true,
    SVGPNGFullRGBAExact: true,
    PageErrors: errors,
    ExternalRequests: external,
    NativeGUIStarted: false,
    OSDialogOpened: false,
    PendingNativeProcessTouched: false,
    NotRun: [
      'Native GUI',
      'Physical input',
      'OS Save/Open/PNG dialogs',
      'Normal OS cold reopen',
      'New arbitrary pre-Mask Image Transform route (unsupported by current Image graph)',
    ],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser?.close();
  await new Promise((resolve, reject) =>
    server.httpServer.close((error) => (error ? reject(error) : resolve())),
  );
}
