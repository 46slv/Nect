// Bounded native WebView2 acceptance. Requires a separately launched, proven
// isolated instance. Never invokes Open/Save/PNG dialogs or changes the IPC bridge.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { chromium } from 'playwright-core';
import { serialize, exportSVG } from '../src/core.js';
import { equalLuminanceFixture } from '../tests/fixtures/mask-luminance.js';

const root = process.argv[2];
assert.ok(root && path.isAbsolute(root));
const read = async (p) => JSON.parse((await fs.readFile(p, 'utf8')).replace(/^\uFEFF/, ''));
const launch = await read(path.join(root, 'evidence/launch.json'));
const identity = await read(path.join(root, 'evidence/identity.json'));
assert.equal(launch.PID, identity.PID);
assert.equal(identity.ExactStartTicksAndExeMatch, true);
assert.equal(identity.ProfileAndPortMatched, true);
assert.equal(launch.SHA256, '04678c6585fad8994a463529540b71051722e81693d9357731a2e82597cea7ac');
assert.equal(
  createHash('sha256')
    .update(await fs.readFile(launch.Executable))
    .digest('hex'),
  launch.SHA256,
);
const output = path.join(root, 'native-cases');
await fs.mkdir(output);
const hash = (data) => createHash('sha256').update(Buffer.from(data)).digest('hex');
const prior = path.dirname(root);
const previous = await read(
  path.join(prior, 'mask-luminance-artifact-2e3de3a/headless/receipt.json'),
);
const expectedRaw = previous.Cases.find(
  (c) => c.Case === '08-equal-luminance-rgb',
).CanvasSVGPNG_RGBASHA256;
const browser = await chromium.connectOverCDP(launch.CDP),
  errors = [],
  results = [],
  rasters = new Map();
let page,
  ownedSafe = false,
  persisted = false,
  failure = null;
const receipt = {
  Input: 'Native Tauri/WebView2 via CDP synthetic; not OS/physical input',
  Executable: launch.Executable,
  SHA256: launch.SHA256,
  PID: launch.PID,
  StartUTC: launch.StartUTC,
  Profile: launch.Profile,
  RecoveryRoot: launch.RecoveryRoot,
  Cases: results,
  CloseRequests: 0,
  OSDialogCommandsInvoked: [],
  Errors: errors,
};
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  assert.equal(
    await page.locator('[data-recovery-dialog]:visible').count(),
    0,
    'Unexpected Recovery; do not interact',
  );
  receipt.Native = await page.evaluate(() => ({
    URL: location.href,
    UserAgent: navigator.userAgent,
    NativeBridge: !!window.__TAURI_INTERNALS__,
    Viewport: [innerWidth, innerHeight],
    DPR: devicePixelRatio,
    Visibility: document.visibilityState,
  }));
  ownedSafe = true;
  receipt.CanvasProfile = await page.evaluate(() => {
    const c = document.createElement('canvas'),
      x = c.getContext('2d');
    return {
      ...x.getContextAttributes(),
      ArrayType: x.getImageData(0, 0, 1, 1).data.constructor.name,
    };
  });
  assert.equal(receipt.CanvasProfile.colorSpace, 'srgb');
  assert.equal(receipt.CanvasProfile.colorType, 'unorm8');
  assert.equal(receipt.CanvasProfile.ArrayType, 'Uint8ClampedArray');
  const $ = (s) => page.locator(s),
    settle = () =>
      page.evaluate(
        () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
      ),
    state = () =>
      page.evaluate(() => ({
        doc: window.nect.document,
        project: window.nect.projectSnapshot(),
        svg: window.nect.export(),
        command: window.nect.commandState,
        file: window.nect.fileState,
      }));
  const load = async (doc) => {
    doc.name = 'NATIVE_LUMINANCE_' + doc.name;
    await page.evaluate((s) => window.nect.load(s), serialize(doc));
    await settle();
    assert.ok((await state()).doc.name.startsWith('NATIVE_LUMINANCE_'));
  };
  const capture = async (name, expectedRGBA) => {
    const s = await state();
    assert.equal(s.svg, exportSVG(s.doc));
    const raster = await page.evaluate(
      async ({ svg, doc }) => {
        const layerIds = new Set(
          doc.layers.filter((l) => l.visible !== false && !l.guide).map((l) => l.id),
        );
        const artboard = document.querySelector('.artboard');
        const layers = [...artboard.children].filter((el) =>
          layerIds.has(el.getAttribute('data-layer')),
        );
        if (layers.length !== layerIds.size)
          throw Error('Actual Canvas layer roots not identified');
        const actual = `<svg xmlns="http://www.w3.org/2000/svg" width="${doc.width}" height="${doc.height}" viewBox="0 0 ${doc.width} ${doc.height}">${layers.map((el) => el.outerHTML).join('')}</svg>`;
        const render = async (text) => {
          const url = URL.createObjectURL(new Blob([text], { type: 'image/svg+xml' }));
          try {
            const image = new Image();
            image.src = url;
            await image.decode();
            const c = document.createElement('canvas');
            c.width = doc.width;
            c.height = doc.height;
            const x = c.getContext('2d');
            x.drawImage(image, 0, 0);
            return {
              rgba: [...x.getImageData(0, 0, doc.width, doc.height).data],
              png: c.toDataURL('image/png').split(',')[1],
            };
          } finally {
            URL.revokeObjectURL(url);
          }
        };
        return { actual, canvas: await render(actual), svg: await render(svg) };
      },
      { svg: s.svg, doc: s.doc },
    );
    assert.deepEqual(
      raster.canvas.rgba,
      raster.svg.rgba,
      'Actual native Canvas layer markup versus exported SVG',
    );
    const rgbaHash = hash(raster.svg.rgba);
    if (expectedRGBA) assert.equal(rgbaHash, expectedRGBA, 'Native versus saved headless RGBA');
    const png = Buffer.from(raster.canvas.png, 'base64');
    await fs.writeFile(path.join(output, name + '.json'), serialize(s.doc), { flag: 'wx' });
    await fs.writeFile(path.join(output, name + '.svg'), s.svg, { flag: 'wx' });
    await fs.writeFile(path.join(output, name + '-canvas.svg'), raster.actual, { flag: 'wx' });
    await fs.writeFile(path.join(output, name + '.png'), png, { flag: 'wx' });
    rasters.set(name, { rgba: raster.svg.rgba, width: s.doc.width });
    results.push({
      Name: name,
      Size: [s.doc.width, s.doc.height],
      RGBA_SHA256: rgbaHash,
      PNG_SHA256: hash(png),
      ActualCanvasEqualsSVG: true,
      HeadlessReferenceExact: !!expectedRGBA,
    });
    return s;
  };
  const fixture = equalLuminanceFixture();
  delete fixture.mask.params.luminance;
  fixture.doc.schemaVersion = 51;
  await load(fixture.doc);
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await settle();
  const mode = () => $('.inspector-body [data-node="example-mask"][data-key="luminance"] select');
  const material = () => $('.graph [data-node="example-mask"][data-key="mask"] select');
  const feather = () =>
    $('.inspector-body [data-node="example-mask"][data-key="feather"] [data-value]');
  const invert = () =>
    $('.inspector-body [data-node="example-mask"][data-key="invert"] input[type="checkbox"]');
  const history = async (before, after) => {
    assert.equal(after.command.expectedRevision, before.command.expectedRevision + 1);
    await $('[data-action="undo"]').click();
    await settle();
    const u = await state();
    assert.deepEqual(u.doc, before.doc);
    assert.equal(u.svg, before.svg);
    await $('[data-action="redo"]').click();
    await settle();
    const r = await state();
    assert.deepEqual(r.doc, after.doc);
    assert.equal(r.svg, after.svg);
  };
  const hard = await capture('01-alpha');
  await mode().selectOption('luminance');
  await settle();
  const luma = await capture('02-luminance', expectedRaw);
  await history(hard, luma);
  const repeatMode = await state();
  await mode().selectOption('luminance');
  await settle();
  assert.deepEqual(await state(), repeatMode);
  const preAlpha = await state();
  await mode().selectOption('alpha');
  await settle();
  const backAlpha = await state();
  assert.equal(backAlpha.svg, hard.svg);
  await history(preAlpha, backAlpha);
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, luma.doc);
  const preMaterial = await state();
  await material().selectOption('alpha-keyer');
  await settle();
  const keyed = await capture('03-keyer-luminance');
  await history(preMaterial, keyed);
  const preInvert = await state();
  await invert().check();
  await settle();
  const inverted = await capture('04-keyer-luminance-invert');
  await history(preInvert, inverted);
  await invert().uncheck();
  await settle();
  const preFeather = await state();
  await feather().fill('4');
  await feather().press('Tab');
  await settle();
  const soft = await capture('05-keyer-luminance-feather');
  await history(preFeather, soft);
  const preSoftInvert = await state();
  await invert().check();
  await settle();
  const softInverted = await capture('06-keyer-luminance-feather-invert');
  await history(preSoftInvert, softInverted);
  const repeated = await state();
  await material().selectOption('alpha-keyer');
  await mode().selectOption('luminance');
  await feather().fill('4');
  await feather().press('Tab');
  await settle();
  assert.deepEqual(await state(), repeated);
  const alpha = (name, x, y) => {
    const r = rasters.get(name);
    return r.rgba[(y * r.width + x) * 4 + 3];
  };
  const row = (name) => [10, 30, 50, 70, 90, 110, 130, 150, 170].map((x) => alpha(name, x, 60));
  assert.deepEqual(row('01-alpha'), [255, 255, 128, 128, 255, 255, 128, 128, 0]);
  assert.deepEqual(row('02-luminance'), [128, 128, 64, 64, 182, 18, 128, 64, 0]);
  assert.deepEqual(row('03-keyer-luminance'), [128, 128, 64, 64, 0, 18, 128, 64, 0]);
  assert.deepEqual(
    row('04-keyer-luminance-invert'),
    row('03-keyer-luminance').map((a) => 255 - a),
  );
  const pixel = (name, x, y) => {
    const r = rasters.get(name);
    return r.rgba.slice((y * r.width + x) * 4, (y * r.width + x) * 4 + 4);
  };
  assert.deepEqual(pixel('02-luminance', 10, 60), pixel('02-luminance', 30, 60));
  assert.deepEqual(pixel('02-luminance', 50, 60), pixel('02-luminance', 70, 60));
  assert.equal(alpha('01-alpha', 150, 10), 64);
  assert.equal(alpha('02-luminance', 150, 10), 32);
  assert.equal(alpha('03-keyer-luminance', 170, 60), 0);
  assert.equal(alpha('04-keyer-luminance-invert', 170, 60), 255);
  assert.notEqual(alpha('05-keyer-luminance-feather', 79, 60), alpha('03-keyer-luminance', 79, 60));
  const sourcePNG = await fs.readFile(
    path.join(prior, 'mask-luminance-artifact-2e3de3a/headless/07-source.png'),
  );
  const sourcePixels = await page.evaluate(async (png) => {
    const i = new Image();
    i.src = 'data:image/png;base64,' + png;
    await i.decode();
    const c = document.createElement('canvas');
    c.width = i.width;
    c.height = i.height;
    const x = c.getContext('2d');
    x.drawImage(i, 0, 0);
    return [...x.getImageData(0, 0, c.width, c.height).data];
  }, sourcePNG.toString('base64'));
  let complement = 0;
  const normal = rasters.get('05-keyer-luminance-feather').rgba,
    inv = rasters.get('06-keyer-luminance-feather-invert').rgba;
  assert.equal(normal.length, sourcePixels.length);
  for (let i = 3; i < normal.length; i += 4)
    complement = Math.max(complement, Math.abs(normal[i] + inv[i] - sourcePixels[i]));
  assert.ok(complement <= 1);
  receipt.AlphaSamples = {
    Alpha: row('01-alpha'),
    Luminance: row('02-luminance'),
    KeyerLuminance: row('03-keyer-luminance'),
  };
  receipt.EqualLuminanceDifferentRGB = {
    FullAlpha: [128, 128],
    HalfAlpha: [64, 64],
    OutputRGBAEqual: true,
  };
  receipt.TransparentWhite = { BeforeInvert: 0, AfterInvert: 255 };
  receipt.PartialSourceAlpha = { Alpha: 64, Luminance: 32 };
  const transformedState = await state();
  const project = transformedState.project,
    projectText = JSON.stringify(project, null, 2);
  const saved = path.join(launch.Profile, 'documents/native-mask-luminance-final.json');
  await fs.writeFile(saved, projectText, { flag: 'wx' });
  const readback = await fs.readFile(saved, 'utf8');
  assert.equal(readback, projectText);
  await page.evaluate((s) => window.nect.load(s), readback);
  await settle();
  const reopened = await state();
  assert.deepEqual(reopened.project, project);
  assert.equal(reopened.svg, transformedState.svg);
  await fs.writeFile(path.join(output, 'native-final-project.json'), readback, { flag: 'wx' });
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await settle();
  await page.screenshot({ path: path.join(output, 'native-mask-luminance.png') });
  const closingState = await state();
  assert.deepEqual(closingState.project, project);
  receipt.UI = {
    AlphaLuminanceSwitch: true,
    ImageKeyerMaterialSwitch: true,
    FeatherInvert: true,
    OneCommandOneUndoRedo: true,
    RepeatedNoop: true,
  };
  receipt.InvertComplementMax8BitError = complement;
  receipt.JSONProjectReloadExact = true;
  receipt.StableIDsSettingsAndSVGExact = true;
  receipt.SavedFixture = {
    Path: saved,
    Bytes: Buffer.byteLength(readback),
    SHA256: hash(readback),
    CLIReadbackExact: true,
  };
  receipt.BeforeCloseFileState = closingState.file;
  receipt.PNGRoute =
    'Actual native Canvas layer SVG and exported SVG -> WebView Canvas PNG; CLI writes bytes. Native Export PNG/OS save not invoked.';
  assert.deepEqual(errors, []);
  persisted = true;
} catch (error) {
  failure = error;
  receipt.Failure = { Message: error.message, Stack: error.stack };
  if (page && ownedSafe) {
    await page.screenshot({ path: path.join(output, 'failure.png') }).catch(() => {});
    try {
      const state = await page.evaluate(() => ({
        project: window.nect.projectSnapshot(),
        file: window.nect.fileState,
      }));
      const text = JSON.stringify(state.project, null, 2),
        saved = path.join(launch.Profile, 'documents/native-mask-luminance-failure.json');
      await fs.writeFile(saved, text, { flag: 'wx' });
      assert.equal(await fs.readFile(saved, 'utf8'), text);
      receipt.FailureFixture = { Path: saved, SHA256: hash(text) };
      persisted = true;
    } catch (saveError) {
      receipt.FailureSave = saveError.message;
    }
  }
} finally {
  if (page && ownedSafe && persisted) {
    receipt.CloseRequests = 1;
    receipt.CloseRoute =
      'Existing finish_close IPC, once, after CLI fixture readback; not OS Save/close workflow acceptance';
    const closed = page.waitForEvent('close', { timeout: 10000 });
    try {
      await page.evaluate(() => {
        void window.__TAURI_INTERNALS__.invoke('finish_close');
        return 'requested';
      });
      receipt.FinishCloseRequested = true;
    } catch (error) {
      receipt.FinishCloseEvaluation = error.message;
    }
    try {
      await closed;
      receipt.PageClosed = true;
    } catch (error) {
      receipt.PageClosed = false;
      receipt.CloseUncertain = error.message;
    }
  }
  receipt.NotRun = [
    'Physical/OS input',
    'OS Save/Open/PNG dialogs',
    'Native Export PNG save handler',
    'Normal OS cold reopen',
    'Normal OS Save/close workflow',
  ];
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  await browser.close();
}
console.log(JSON.stringify(receipt, null, 2));
if (failure) throw failure;
