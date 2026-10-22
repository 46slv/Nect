// Bounded native WebView2 acceptance. Requires a separately launched, proven
// isolated instance. Never invokes Open/Save/PNG dialogs or changes the IPC bridge.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { chromium } from 'playwright-core';
import { serialize, exportSVG, evaluate } from '../src/core.js';
import { featherFixture } from '../tests/fixtures/mask-feather.js';
import { imageAlphaFixture } from '../tests/fixtures/mask-image-alpha.js';

const root = process.argv[2];
assert.ok(root && path.isAbsolute(root));
const read = async (p) => JSON.parse((await fs.readFile(p, 'utf8')).replace(/^\uFEFF/, ''));
const launch = await read(path.join(root, 'evidence/launch.json'));
const identity = await read(path.join(root, 'evidence/identity.json'));
assert.equal(launch.PID, identity.PID);
assert.equal(identity.ExactStartTicksAndExeMatch, true);
assert.equal(identity.ProfileAndPortMatched, true);
assert.equal(launch.SHA256, 'fb8e5a01bc0679c5593c8668b4256197576f3f9860155abb1a89a4e1623120ea');
assert.equal(
  createHash('sha256')
    .update(await fs.readFile(launch.Executable))
    .digest('hex'),
  launch.SHA256,
);
const output = path.join(root, 'native-cases');
await fs.mkdir(output);
const hash = (data) => createHash('sha256').update(Buffer.from(data)).digest('hex');
const prior = path.dirname(root),
  oldPath = await read(path.join(prior, 'mask-feather-artifact-b91ea40/headless/receipt.json')),
  oldImage = await read(
    path.join(prior, 'mask-image-alpha-artifact-deac549/headless/receipt.json'),
  );
const expected = (receipt, name) => receipt.Cases.find((c) => c.Case === name).SVGPNGFullRGBASHA256;
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
    doc.name = 'NATIVE_MASK_RUNTIME_' + doc.name;
    await page.evaluate((s) => window.nect.load(s), serialize(doc));
    await settle();
    assert.ok((await state()).doc.name.startsWith('NATIVE_MASK_RUNTIME_'));
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
  await load(featherFixture(4, { shapeTransform: true }).doc);
  await capture('01-path-feather-transform', expected(oldPath, '05-shape-transform'));
  const image = imageAlphaFixture();
  const positions = {
    'example-image': [40, 40],
    'example-keyer': [350, 40],
    'example-mask-shape': [660, 400],
    'alpha-material': [40, 400],
    'alpha-keyer': [350, 400],
    'example-mask': [660, 40],
    'example-output': [970, 40],
  };
  for (const n of image.layer.nodes) n.position = positions[n.id];
  await load(image.doc);
  await $('[data-action="graph-fit"]').click();
  await $('.graph .node[data-id="example-mask"] .node-head strong').click();
  await settle();
  const hard = await capture('02-image-alpha', expected(oldImage, '00-image-alpha'));
  const material = () => $('.graph [data-node="example-mask"][data-key="mask"] select');
  await material().selectOption('alpha-keyer');
  await settle();
  const keyed = await state();
  assert.equal(keyed.command.expectedRevision, hard.command.expectedRevision + 1);
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, hard.doc);
  assert.equal((await state()).svg, hard.svg);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, keyed.doc);
  const feather = () =>
    $('.inspector-body [data-node="example-mask"][data-key="feather"] [data-value]');
  const preFeather = await state();
  await feather().fill('4');
  await feather().press('Tab');
  await settle();
  const soft = await capture('03-keyer-feather', expected(oldImage, '11-keyer-feather'));
  assert.equal(soft.command.expectedRevision, preFeather.command.expectedRevision + 1);
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, preFeather.doc);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, soft.doc);
  const invert = () =>
    $('.inspector-body [data-node="example-mask"][data-key="invert"] input[type="checkbox"]');
  await invert().check();
  await settle();
  const inverted = await capture('04-keyer-feather-invert');
  await $('[data-action="undo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, soft.doc);
  assert.equal((await state()).svg, soft.svg);
  await $('[data-action="redo"]').click();
  await settle();
  assert.deepEqual((await state()).doc, inverted.doc);
  const repeated = await state();
  await material().selectOption('alpha-keyer');
  await feather().fill('4');
  await feather().press('Tab');
  await settle();
  assert.deepEqual(await state(), repeated);
  const sourcePNG = await fs.readFile(
    path.join(prior, 'mask-image-alpha-artifact-deac549/headless/10-source.png'),
  );
  const sourcePixels = await page.evaluate(async (png) => {
    const i = new Image();
    i.src = 'data:image/png;base64,' + png;
    await i.decode();
    const c = document.createElement('canvas');
    c.width = c.height = 80;
    const x = c.getContext('2d');
    x.drawImage(i, 0, 0);
    return [...x.getImageData(0, 0, 80, 80).data];
  }, sourcePNG.toString('base64'));
  let complement = 0;
  const a = rasters.get('03-keyer-feather').rgba,
    b = rasters.get('04-keyer-feather-invert').rgba;
  for (let i = 3; i < a.length; i += 4)
    complement = Math.max(complement, Math.abs(a[i] + b[i] - sourcePixels[i]));
  assert.ok(complement <= 1);
  const alpha = (name, x, y) => {
    const r = rasters.get(name);
    return r.rgba[(y * r.width + x) * 4 + 3];
  };
  assert.deepEqual(
    [10, 30, 50, 70].map((x) => alpha('02-image-alpha', x, 60)),
    [0, 128, 128, 255],
  );
  assert.equal(alpha('02-image-alpha', 50, 10), 64);
  const transformed = imageAlphaFixture({ postTransform: 'scale' });
  transformed.material.params.position.value = [10, 10];
  transformed.material.params.size.value = [40, 40];
  const t = transformed.layer.nodes.find((n) => n.id === 'feather-appearance-transform');
  t.params.rotation.value = 30;
  t.params.position.value = [50, 30];
  transformed.doc.width = 240;
  transformed.doc.height = 220;
  const matrix = evaluate(transformed.layer, {}, transformed.doc.imageAssets, transformed.doc).path
    .matrix;
  assert.ok(Math.abs(matrix[0] - Math.sqrt(3)) < 1e-12);
  assert.ok(Math.abs(matrix[1] - 1) < 1e-12);
  assert.ok(Math.abs(matrix[2] + 0.5) < 1e-12);
  assert.deepEqual(matrix.slice(4), [50, 30]);
  await load(transformed.doc);
  const transformedState = await capture('05-document-placement-appearance-transform');
  assert.deepEqual(
    transformedState.doc.layers[0].nodes.find((n) => n.id === 'alpha-material').params.position
      .value,
    [10, 10],
  );
  const project = transformedState.project,
    projectText = JSON.stringify(project, null, 2);
  const saved = path.join(launch.Profile, 'documents/native-mask-runtime-final.json');
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
  await page.screenshot({ path: path.join(output, 'native-mask-runtime.png') });
  const closingState = await state();
  assert.deepEqual(closingState.project, project);
  receipt.UI = {
    ImageKeyerSwitch: true,
    FeatherInvert: true,
    OneCommandOneUndoRedo: true,
    RepeatedNoop: true,
  };
  receipt.AlphaSamples = [0, 128, 128, 255];
  receipt.PartialSourceAlphaProduct = 64;
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
        saved = path.join(launch.Profile, 'documents/native-mask-runtime-failure.json');
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
