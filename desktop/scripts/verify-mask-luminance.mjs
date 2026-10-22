// Delta-only Luminance acceptance in production Web/CSP. No native GUI or dialogs.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { serialize, exportSVG } from '../src/core.js';
import {
  luminanceFixture,
  expectedLuminance,
  maskColors,
  equalLuminanceFixture,
} from '../tests/fixtures/mask-luminance.js';
const output = process.argv[2];
assert.ok(output && path.isAbsolute(output));
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
  page.on('pageerror', (e) => errors.push(e.message));
  await page.route('**/*', (route) => {
    const u = route.request().url();
    if (!/^(http:\/\/127\.0\.0\.1:|data:|blob:|about:)/.test(u)) {
      external.push(u);
      return route.abort();
    }
    return route.continue();
  });
  await page.goto(server.resolvedUrls.local[0]);
  await page.waitForFunction(() => window.nect);
  assert.equal(await page.evaluate(() => typeof window.__TAURI_INTERNALS__), 'undefined');
  const canvasProfile = await page.evaluate(() => {
    const c = document.createElement('canvas');
    c.width = c.height = 1;
    const ctx = c.getContext('2d');
    return {
      attributes: ctx.getContextAttributes(),
      pixelArray: ctx.getImageData(0, 0, 1, 1).data.constructor.name,
    };
  });
  assert.equal(canvasProfile.attributes.colorSpace, 'srgb');
  if (canvasProfile.attributes.colorType !== undefined)
    assert.equal(canvasProfile.attributes.colorType, 'unorm8');
  assert.equal(canvasProfile.pixelArray, 'Uint8ClampedArray');
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
      })),
    mode = () => $('.inspector-body [data-node="example-mask"][data-key="luminance"] select');
  const alpha = luminanceFixture(),
    lum = luminanceFixture({ luminance: true });
  await page.evaluate((s) => window.nect.load(s), serialize(alpha.doc));
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  const before = await state();
  assert.equal(await mode().inputValue(), 'alpha');
  await mode().selectOption('alpha');
  await settle();
  assert.deepEqual(await state(), before);
  await mode().selectOption('luminance');
  await settle();
  const after = await state();
  assert.equal(after.doc.schemaVersion, 52);
  assert.deepEqual(after.doc, lum.doc);
  assert.equal(after.command.expectedRevision, before.command.expectedRevision + 1);
  await mode().selectOption('luminance');
  await settle();
  assert.deepEqual(await state(), after);
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, before.doc);
  assert.equal((await state()).svg, before.svg);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, after.doc);
  await mode().selectOption('alpha');
  await settle();
  assert.equal((await state()).svg, before.svg);
  const placed = luminanceFixture({ luminance: true, postTransform: 'scale' });
  placed.material.params.position.value = [10, 10];
  placed.material.params.size.value = [90, 40];
  placed.doc.width = 400;
  placed.doc.height = 120;
  const source = luminanceFixture().doc,
    out = source.layers[0].nodes.find((n) => n.id === 'example-output');
  out.type = 'Image Output';
  out.params.path.type = 'Image';
  out.params.path.source = 'example-keyer';
  const cases = [
      ['00-alpha', alpha.doc],
      ['01-luminance', lum.doc],
      ['02-luminance-invert', luminanceFixture({ luminance: true, invert: true }).doc],
      ['03-luminance-feather', luminanceFixture({ luminance: true, feather: 4 }).doc],
      [
        '04-luminance-feather-invert',
        luminanceFixture({ luminance: true, feather: 4, invert: true }).doc,
      ],
      ['05-keyer-luminance', luminanceFixture({ luminance: true, keyed: true }).doc],
      ['06-placement-transform', placed.doc],
      ['07-source', source],
      ['08-equal-luminance-rgb', equalLuminanceFixture().doc],
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
      async ({ svg, png, doc }) => {
        const ids = new Set(
            doc.layers.filter((l) => l.visible !== false && !l.guide).map((l) => l.id),
          ),
          roots = [...document.querySelector('.artboard').children].filter((el) =>
            ids.has(el.getAttribute('data-layer')),
          );
        if (roots.length !== ids.size) throw Error('Actual Canvas roots missing');
        const actual = `<svg xmlns="http://www.w3.org/2000/svg" width="${doc.width}" height="${doc.height}" viewBox="0 0 ${doc.width} ${doc.height}">${roots.map((el) => el.outerHTML).join('')}</svg>`;
        const pixels = async (url) => {
          const i = new Image();
          i.src = url;
          await i.decode();
          const c = document.createElement('canvas');
          c.width = doc.width;
          c.height = doc.height;
          const x = c.getContext('2d');
          x.drawImage(i, 0, 0);
          return [...x.getImageData(0, 0, doc.width, doc.height).data];
        };
        const svgURL = (t) => 'data:image/svg+xml;base64,' + btoa(unescape(encodeURIComponent(t)));
        return {
          svg: await pixels(svgURL(svg)),
          png: await pixels(png),
          canvas: await pixels(svgURL(actual)),
        };
      },
      { svg, png: 'data:image/png;base64,' + png.toString('base64'), doc },
    );
    assert.equal(hash(data.svg), hash(data.png));
    assert.equal(hash(data.canvas), hash(data.svg));
    rasters.set(prefix, { rgba: data.png, width: doc.width });
    results.push({
      Case: prefix,
      Size: [doc.width, doc.height],
      PNGBytes: png.length,
      PNG_SHA256: hash(png),
      CanvasSVGPNG_RGBASHA256: hash(data.png),
    });
  }
  const pixel = (key, x, y) => {
      const r = rasters.get(key);
      return r.rgba.slice((y * r.width + x) * 4, (y * r.width + x) * 4 + 4);
    },
    a = (key, x, y) => pixel(key, x, y)[3],
    xs = maskColors.map((_, i) => 10 + 20 * i),
    actualLuma = xs.map((x) => a('01-luminance', x, 60));
  assert.deepEqual(actualLuma, expectedLuminance);
  assert.deepEqual(
    [10, 30, 50, 70].map((x) => a('08-equal-luminance-rgb', x, 60)),
    [128, 128, 64, 64],
  );
  assert.deepEqual(
    pixel('08-equal-luminance-rgb', 10, 60),
    pixel('08-equal-luminance-rgb', 30, 60),
  );
  assert.deepEqual(
    pixel('08-equal-luminance-rgb', 50, 60),
    pixel('08-equal-luminance-rgb', 70, 60),
  );
  assert.deepEqual(
    xs.map((x) => a('00-alpha', x, 60)),
    maskColors.map((c) => c[3]),
  );
  assert.deepEqual(
    xs.map((x) => a('02-luminance-invert', x, 60)),
    expectedLuminance.map((v) => 255 - v),
  );
  assert.equal(a('01-luminance', 150, 10), 32);
  assert.equal(a('00-alpha', 150, 10), 64);
  assert.equal(a('05-keyer-luminance', 90, 60), 0);
  assert.equal(a('05-keyer-luminance', 30, 60), 255);
  assert.equal(a('05-keyer-luminance', 70, 60), 54);
  let complement = 0;
  for (const [normal, inverse] of [
    ['01-luminance', '02-luminance-invert'],
    ['03-luminance-feather', '04-luminance-feather-invert'],
  ])
    for (let y = 0; y < 80; y++)
      for (let x = 0; x < 180; x++)
        complement = Math.max(
          complement,
          Math.abs(a(normal, x, y) + a(inverse, x, y) - a('07-source', x, y)),
        );
  assert.ok(complement <= 1);
  // Pure-color source interiors avoid premultiplied 8-bit reconstruction at the
  // separately resampled source's green/red boundary around x90.
  for (const x of xs.filter((x) => x !== 90))
    if (a('01-luminance', x, 60))
      assert.deepEqual(
        pixel('01-luminance', x, 60).slice(0, 3),
        pixel('07-source', x, 60).slice(0, 3),
      );
  const profile = [10, 14, 18, 22, 26, 30].map((x) => ({
    x,
    hard: a('01-luminance', x, 60),
    soft: a('03-luminance-feather', x, 60),
  }));
  assert.ok(profile.some((p) => p.hard !== p.soft));
  assert.ok(a('03-luminance-feather', 10, 60) > 0);
  assert.equal(a('06-placement-transform', 60, 30), 255);
  assert.equal(a('06-placement-transform', 20, 30), 0);
  await page.evaluate((s) => window.nect.load(s), serialize(lum.doc));
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await settle();
  await page.screenshot({ path: path.join(output, 'luminance-mode.png') });
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  const receipt = {
    Mode: 'Headless Edge production Web/CSP; web Export PNG download; actual Canvas layer SVG compared',
    Cases: results,
    UI: {
      DefaultAlphaNeutral: true,
      OneModeEditUndoRedo: true,
      RepeatedModeNeutral: true,
      BackToAlphaSVGExact: true,
    },
    ColorSpace: 'renderer-decoded encoded sRGB, no linear-light conversion',
    Coefficients: [0.2126, 0.7152, 0.0722],
    CanvasProfile: canvasProfile,
    EqualLuminanceDifferentRGB: {
      Colors: [
        [94, 148, 30],
        [162, 108, 226],
      ],
      OpaqueAlpha: 128,
      HalfAlpha: 64,
      RGBAExact: true,
    },
    BrightFullyTransparentWhite: { LuminanceAlpha: 0, InvertedAlpha: 255 },
    LuminanceAlpha: 'weighted straight RGB multiplied once by original material alpha',
    Expected8Bit: expectedLuminance,
    Actual8Bit: actualLuma,
    PartialSourceAndMaterialAlpha: { AlphaMode: 64, LuminanceMode: 32 },
    FeatherProfile: profile,
    InvertComplementMax8BitError: complement,
    KeyerProcessedAlphaVerified: true,
    SourceRGBInteriorsUnchanged: true,
    CanvasSVGPNGFullRGBAExact: true,
    JSONSVGExact: true,
    PlacementAndAppearanceTransform: true,
    PageErrors: errors,
    ExternalRequests: external,
    NativeGUIStarted: false,
    OSDialogOpened: false,
    NotRun: [
      'Luminance-specific native WebView',
      'Physical/OS input',
      'OS Open/Save/PNG dialogs',
      'Normal OS cold reopen',
      'ICC/profile-specific color-management scenarios',
      'Linear RGB and AE equivalence',
    ],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  console.log(JSON.stringify(receipt, null, 2));
} finally {
  await browser?.close();
  await new Promise((resolve, reject) =>
    server.httpServer.close((e) => (e ? reject(e) : resolve())),
  );
}
