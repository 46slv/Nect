// Focused headless production-Web renderer. No native GUI/profile/OS dialog.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { preview } from 'vite';
import { chromium } from 'playwright-core';
import { serialize, exportSVG } from '../src/core.js';
import { featherFixture, strokeFeatherFixture } from '../tests/fixtures/mask-feather.js';

const output = process.argv[2];
assert.ok(output && path.isAbsolute(output), 'Pass a new absolute output directory');
await fs.mkdir(output);
const config = JSON.parse(await fs.readFile('src-tauri/tauri.conf.json', 'utf8')),
  server = await preview({
    preview: {
      host: '127.0.0.1',
      port: 0,
      headers: { 'Content-Security-Policy': config.app.security.csp },
    },
  });
let browser;
const hash = (data) => createHash('sha256').update(Buffer.from(data)).digest('hex');
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
      })),
    field = () => $('.inspector-body [data-node="example-mask"][data-key="feather"] [data-value]');
  const hard = featherFixture(),
    soft = featherFixture(4),
    inverse = featherFixture(4, { invert: true }),
    stroke = strokeFeatherFixture(),
    strokeSource = structuredClone(stroke.doc),
    source = structuredClone(hard.doc);
  const sourceOut = source.layers[0].nodes.find((n) => n.id === 'example-output');
  sourceOut.type = 'Image Output';
  sourceOut.params.path.type = 'Image';
  sourceOut.params.path.source = 'example-keyer';
  strokeSource.layers
    .find((l) => l.id === 'keyer-mask-layer')
    .nodes.find((n) => n.id === 'example-output').params.path.source = stroke.ref.id;
  await page.evaluate((s) => window.nect.load(s), serialize(hard.doc));
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  const before = await state();
  assert.equal(await field().inputValue(), '0');
  assert.equal(await field().getAttribute('min'), '0');
  assert.equal(await field().getAttribute('max'), '256');
  assert.deepEqual(before.doc, hard.doc);
  await field().fill('-1');
  await field().press('Tab');
  await settle();
  assert.deepEqual(await state(), before);
  await field().fill('4');
  await field().press('Tab');
  await settle();
  const changed = await state();
  assert.equal(changed.doc.schemaVersion, 50);
  assert.equal(
    changed.doc.layers[0].nodes.find((n) => n.id === 'example-mask').params.feather.value,
    4,
  );
  assert.equal(changed.command.expectedRevision, before.command.expectedRevision + 1);
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, before.doc);
  assert.equal((await state()).svg, before.svg);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, changed.doc);
  const noop = await state();
  await field().fill('4');
  await field().press('Tab');
  await settle();
  assert.deepEqual(await state(), noop);
  await field().fill('0');
  await field().press('Tab');
  await settle();
  assert.equal((await state()).svg, before.svg);
  const cases = [
      ['00-hard', hard.doc],
      ['01-soft-4', soft.doc],
      ['02-inverted-4', inverse.doc],
      ['03-scaled', featherFixture(4, { postTransform: 'scale' }).doc],
      ['04-rotated', featherFixture(4, { postTransform: 'rotate' }).doc],
      ['05-shape-transform', featherFixture(4, { shapeTransform: true }).doc],
      ['06-stroke-reference', stroke.doc],
      ['07-stroke-source', strokeSource],
      ['08-image-source', source],
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
      await page
        .locator('.command-menu>summary')
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
          const canvas = document.createElement('canvas');
          canvas.width = width;
          canvas.height = height;
          canvas.getContext('2d').drawImage(img, 0, 0, width, height);
          return [...canvas.getContext('2d').getImageData(0, 0, width, height).data];
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
    rasters.set(prefix, { rgba: data.png, width: doc.width, height: doc.height });
    results.push({
      Case: prefix,
      Size: [doc.width, doc.height],
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
  assert.equal(alpha('00-hard', 16, 40), 0);
  assert.equal(alpha('00-hard', 20, 40), 255);
  const profile = [8, 12, 16, 20, 24, 28, 32].map((x) => ({ x, alpha: alpha('01-soft-4', x, 40) }));
  assert.ok(
    profile[2].alpha > 0 &&
      profile[2].alpha < profile[3].alpha &&
      profile[3].alpha < profile[4].alpha &&
      profile[4].alpha < 255,
  );
  assert.equal(alpha('01-soft-4', 40, 40), 255);
  for (const x of [39, 40])
    assert.deepEqual(pixel('01-soft-4', x, 40), pixel('08-image-source', x, 40));
  const normal = rasters.get('01-soft-4').rgba,
    inverted = rasters.get('02-inverted-4').rgba,
    original = rasters.get('08-image-source').rgba;
  let complementMaxError = 0;
  for (let i = 0; i < normal.length; i += 4) {
    complementMaxError = Math.max(
      complementMaxError,
      Math.abs(normal[i + 3] + inverted[i + 3] - original[i + 3]),
    );
    if (normal[i + 3] > 0) assert.deepEqual(normal.slice(i, i + 3), original.slice(i, i + 3));
  }
  assert.ok(complementMaxError <= 1, 'Only one 8-bit alpha rounding unit is allowed');
  assert.deepEqual(
    rasters.get('06-stroke-reference').rgba,
    rasters.get('07-stroke-source').rgba,
    'Stroke/ref paint must not be clipped by the padded filter',
  );
  assert.ok(
    alpha('03-scaled', 42, 50) > 0 &&
      alpha('03-scaled', 42, 50) < alpha('03-scaled', 50, 50) &&
      alpha('03-scaled', 50, 50) < alpha('03-scaled', 58, 50),
  );
  assert.ok(
    alpha('04-rotated', 50, 16) > 0 &&
      alpha('04-rotated', 50, 16) < alpha('04-rotated', 50, 20) &&
      alpha('04-rotated', 50, 20) < alpha('04-rotated', 50, 24),
  );
  await page.evaluate((s) => window.nect.load(s), serialize(soft.doc));
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await page.screenshot({ path: path.join(output, 'headless-feather.png') });
  assert.deepEqual(errors, []);
  assert.deepEqual(external, []);
  const receipt = {
    Mode: 'Headless Edge; production Web bundle/CSP; actual web Export PNG download',
    UI: {
      VirtualZeroNeutral: true,
      InvalidNegativeRejected: true,
      OneEditUndoRedo: true,
      RepeatNoop: true,
      ZeroSVGExact: true,
    },
    Cases: results,
    TransitionProfile: profile,
    ComplementMax8BitError: complementMaxError,
    SourceRGBDetailUnblurred: true,
    StrokeReferenceRGBAUnclipped: true,
    TransformScaleAndRotationVerified: true,
    JSONSVGExact: true,
    SVGPNGFullRGBAExact: true,
    PageErrors: errors,
    ExternalRequests: external,
    PendingNativeProcessTouched: false,
    NotRun: [
      'Native GUI for feather',
      'Physical input',
      'OS Save/Open/PNG dialogs',
      'Normal OS cold reopen',
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
