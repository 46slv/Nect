import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { resolve, join, basename } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright-core';
import {
  document as createDocument,
  blankLayer,
  node,
  connect,
  deserialize,
  exportSVG,
  serialize,
} from '../src/core.js';
import { crc32 } from '../src/images.js';

const chrome = process.env.CHROMIUM_PATH;
assert.ok(chrome, 'Existing CI must select installed Chrome explicitly');
await fs.mkdir('test-results', { recursive: true });
const cli = fileURLToPath(new URL('../cli/svg-to-png.js', import.meta.url)),
  directory = await fs.mkdtemp(resolve('test-results/svg-png-cli-')),
  receiptPath = resolve('test-results/workflow-svg-to-png.json'),
  hash = (bytes) => createHash('sha256').update(bytes).digest('hex'),
  receipt = { ok: false, pageErrors: [], outputs: [], rejected: [] };
const source = createDocument();
source.width = 32;
source.height = 16;
source.layers = [];
for (const [x, y, color, alpha] of [
  [8, 4, '#ff0000', 1],
  [24, 4, '#0000ff', 1],
  [8, 12, '#00ff00', 0.5],
]) {
  const layer = blankLayer('CLI fixture'),
    rectangle = node('Rectangle'),
    fill = node('Fill'),
    output = node('Output');
  rectangle.params.center.value = [x, y];
  rectangle.params.size.value = [16, 8];
  fill.params.path.source = rectangle.id;
  fill.params.color.value = { hex: color, alpha };
  output.params.path.source = fill.id;
  layer.nodes = [rectangle, fill, output];
  layer.output = output.id;
  source.layers.push(layer);
}
const json = serialize(source),
  fixed = exportSVG(source),
  physical = exportSVG(source, {}, { unit: 'mm' }),
  input = join(directory, 'source.svg'),
  physicalInput = join(directory, 'physical.svg'),
  jsonPath = join(directory, 'source.json');
await fs.writeFile(input, fixed);
await fs.writeFile(physicalInput, physical);
await fs.writeFile(jsonPath, json);
await fs.writeFile('test-results/workflow-svg-to-png-source.svg', physical);
await fs.writeFile('test-results/workflow-svg-to-png-project.json', json);
Object.assign(receipt, {
  svgSha256: hash(fixed),
  physicalSvgSha256: hash(physical),
  jsonSha256: hash(json),
  sourceCommit: process.env.GITHUB_SHA || null,
  runId: process.env.GITHUB_RUN_ID || null,
  runAttempt: process.env.GITHUB_RUN_ATTEMPT || null,
});

function run(args) {
  return new Promise((resolveRun, reject) => {
    const child = spawn(process.execPath, [cli, ...args], {
      env: process.env,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stdout = '',
      stderr = '';
    child.stdout.on('data', (data) => {
      stdout += data;
    });
    child.stderr.on('data', (data) => {
      stderr += data;
    });
    const timer = setTimeout(() => child.kill(), 45_000);
    child.once('error', reject);
    child.once('close', (code, signal) => {
      clearTimeout(timer);
      resolveRun({ code, signal, stdout, stderr });
    });
  });
}
function pngInfo(bytes) {
  assert.deepEqual([...bytes.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  const chunks = [];
  let width,
    height,
    density = null;
  for (let at = 8; at < bytes.length; ) {
    const length = bytes.readUInt32BE(at),
      tag = bytes.toString('ascii', at + 4, at + 8),
      end = at + 12 + length;
    assert.ok(end <= bytes.length);
    assert.equal(crc32(bytes, at + 4, end - 4), bytes.readUInt32BE(end - 4));
    chunks.push(tag);
    if (tag === 'IHDR') {
      width = bytes.readUInt32BE(at + 8);
      height = bytes.readUInt32BE(at + 12);
    }
    if (tag === 'pHYs') {
      assert.equal(density, null);
      assert.ok(!chunks.includes('IDAT'));
      assert.equal(length, 9);
      density = {
        x: bytes.readUInt32BE(at + 8),
        y: bytes.readUInt32BE(at + 12),
        unit: bytes[at + 16],
      };
    }
    if (tag === 'IEND') assert.equal(end, bytes.length);
    at = end;
  }
  assert.equal(chunks[0], 'IHDR');
  assert.equal(chunks.at(-1), 'IEND');
  return { width, height, density };
}

const browser = await chromium.launch({ executablePath: chrome, headless: true });
try {
  receipt.browserVersion = browser.version();
  const context = await browser.newContext({ serviceWorkers: 'block' }),
    page = await context.newPage(),
    origin = 'https://nect-cli-test.invalid',
    modules = [
      '/cli/svg-png-source.js',
      '/cli/svg-to-png-options.js',
      '/src/raster.js',
      '/src/images.js',
      '/src/artboard-units.js',
    ],
    requests = [];
  await context.route('**/*', async (route) => {
    const url = route.request().url();
    if (url === origin + '/')
      return route.fulfill({
        contentType: 'text/html',
        body: '<!doctype html><link rel="icon" href="data:,">',
      });
    const path = modules.find((path) => url === origin + path);
    if (path)
      return route.fulfill({
        contentType: 'text/javascript',
        body: await fs.readFile(new URL('..' + path, import.meta.url), 'utf8'),
      });
    requests.push(url);
    await route.abort();
  });
  page.on('pageerror', (error) => receipt.pageErrors.push(error.message));
  await page.goto(origin + '/');
  const prepare = (svg) =>
    page.evaluate(async (svg) => {
      const { prepareSvgSource } = await import('/cli/svg-png-source.js');
      const result = await prepareSvgSource(svg);
      return result;
    }, svg);
  const root = (body, attrs = 'width="32" height="16" viewBox="0 0 32 16"') =>
    `<svg xmlns="http://www.w3.org/2000/svg" ${attrs}>${body}</svg>`;
  for (const [name, svg] of [
    ['script', root('<script>alert(1)</script>')],
    ['foreignObject', root('<foreignObject/>')],
    ['event-handler', root('<path onload="alert(1)"/>')],
    ['external-image', root('<image href="https://example.invalid/x.png"/>')],
    ['file-image', root('<image href="file:///secret.png"/>')],
    ['embedded-svg', root('<image href="data:image/svg+xml;base64,PHN2Zy8+"/>')],
    ['external-paint', root('<path fill="url(https://example.invalid/p.svg#g)"/>')],
    [
      'style-font',
      root('<style>@font-face{font-family:x;src:url(https://example.invalid/font)}</style>'),
    ],
    ['text-font', root('<text>Needs a font</text>')],
    ['inline-style', root('<g style="fill:url(https://example.invalid/a)"/>')],
    ['missing-fragment', root('<path fill="url(#absent)"/>')],
    ['doctype', '<!DOCTYPE svg SYSTEM "https://example.invalid/x">' + root('')],
    ['stylesheet', '<?xml-stylesheet href="https://example.invalid/x"?>' + root('')],
    ['responsive', root('', 'width="100%" height="100%" viewBox="0 0 32 16"')],
    ['no-viewBox', root('', 'width="32" height="16"')],
    ['root-transform', root('', 'width="32" height="16" viewBox="0 0 32 16" transform="scale(2)"')],
    ['malformed', '<svg'],
  ]) {
    await assert.rejects(prepare(svg), undefined, name);
    receipt.rejected.push(name);
  }
  const internal = root(
    '<defs><linearGradient id="nect-gradient-0"><stop offset="0" stop-color="#ff0000"/></linearGradient></defs><path fill="url(#nect-gradient-0)" d="M0 0H32V16H0Z"/>',
  );
  assert.deepEqual((await prepare(internal)).size, { width: 32, height: 16 });
  const angularInput = resolve('examples/angular-gradient-wheel.svg'),
    angularSource = await fs.readFile(angularInput, 'utf8'),
    angularHash = hash(angularSource);
  assert.match(angularSource, /<pattern /);
  assert.deepEqual((await prepare(angularSource)).size, { width: 960, height: 540 });
  const angularDoc = deserialize(await fs.readFile('examples/angular-gradient-wheel.json', 'utf8'));
  const embeddedSource = angularDoc.layers[0];
  const angularNode = embeddedSource.nodes.find((item) => item.type === 'Angular Gradient Fill');
  for (const key of ['startColor', 'color1', 'color2', 'endColor'])
    angularNode.params[key].value.alpha = 0.5;
  embeddedSource.visible = false;
  const consumer = blankLayer('Nested angular gradient'),
    reference = node('Composition Reference');
  reference.compositionReference = { compositionId: embeddedSource.id };
  consumer.nodes.unshift(reference);
  connect(consumer, reference.id, consumer.output, 'path');
  angularDoc.layers.push(consumer);
  angularDoc.schemaVersion = Math.max(angularDoc.schemaVersion, 31);
  const nestedAngular = exportSVG(angularDoc, {}, { unit: 'mm', precision: 2 });
  assert.match(nestedAngular, /<g[^>]*>\s*<defs><pattern /);
  assert.deepEqual((await prepare(nestedAngular)).size, { width: 960, height: 540 });
  const recursivePattern = angularSource.replace(
    /fill="#[0-9a-f]{6}"/,
    'fill="url(#nect-gradient-0)"',
  );
  for (const [name, svg] of [
    [
      'pattern-external-href',
      angularSource.replace('<pattern ', '<pattern href="https://example.invalid/a" '),
    ],
    [
      'pattern-units',
      angularSource.replace('patternUnits="userSpaceOnUse"', 'patternUnits="objectBoundingBox"'),
    ],
    [
      'pattern-zero-width',
      angularSource.replace('width="400" height="400" viewBox=', 'width="0" height="400" viewBox='),
    ],
    [
      'pattern-viewBox',
      angularSource.replace('viewBox="280 70 400 400"', 'viewBox="280 70 399 400"'),
    ],
    ['pattern-missing-id', angularSource.replace('id="nect-gradient-0"', '')],
    [
      'pattern-duplicate-id',
      angularSource.replace('</defs><path', '</defs><path id="nect-gradient-0"'),
    ],
    ['pattern-recursion', recursivePattern],
    [
      'pattern-stroke',
      angularSource.replace(
        'fill="url(#nect-gradient-0)"',
        'fill="#fff" stroke="url(#nect-gradient-0)"',
      ),
    ],
    ['pattern-not-path-fill', angularSource.replace('</defs><path', '</defs><rect')],
    [
      'pattern-foreign-child',
      angularSource.replace(
        '<g shape-rendering="crispEdges">',
        '<g shape-rendering="crispEdges"><image href="https://example.invalid/a.png"/>',
      ),
    ],
    ['pattern-bad-sector-alpha', angularSource.replace('fill-opacity="1"', 'fill-opacity="2"')],
    [
      'pattern-outside-defs',
      angularSource
        .replace('<defs><pattern', '<pattern')
        .replace('</pattern></defs>', '</pattern>'),
    ],
    ['foreign-shape-rendering', root('<g shape-rendering="crispEdges"/>')],
  ]) {
    assert.notEqual(svg, angularSource, name);
    await assert.rejects(prepare(svg), undefined, name);
    receipt.rejected.push(name);
  }
  const embedded = await page.evaluate(() => {
    const canvas = document.createElement('canvas');
    canvas.width = 2;
    canvas.height = 2;
    canvas.getContext('2d').fillRect(0, 0, 2, 2);
    return canvas.toDataURL('image/png');
  });
  await prepare(root(`<image href="${embedded}" width="2" height="2"/>`.repeat(101)));
  await assert.rejects(prepare(root('<image href="data:image/png;base64,bm90IFBORw=="/>')));
  receipt.internalFragmentAndEmbeddedPng = true;
  assert.deepEqual(requests, [], 'Unsafe source validation must not fetch external resources');

  for (const item of [
    { name: 'default', input, args: [], width: 32, height: 16, ppi: null },
    { name: 'scale2', input, args: ['--scale', '2'], width: 64, height: 32, ppi: null },
    {
      name: 'physical-300ppi',
      input: physicalInput,
      args: ['--ppi', '300'],
      width: 100,
      height: 50,
      ppi: 300,
    },
  ]) {
    const artifact = resolve(`test-results/workflow-svg-to-png-${item.name}.png`),
      output = join(directory, `${item.name}.png`);
    const result = await run([item.input, output, ...item.args, '--chromium-path', chrome]);
    assert.equal(result.code, 0, JSON.stringify(result));
    const bytes = await fs.readFile(output),
      info = pngInfo(bytes);
    assert.equal(info.width, item.width);
    assert.equal(info.height, item.height);
    if (item.ppi !== null) assert.deepEqual(info.density, { x: 11811, y: 11811, unit: 1 });
    const pixels = await page.evaluate(async (data) => {
      const image = new Image();
      image.src = 'data:image/png;base64,' + data;
      await image.decode();
      const canvas = document.createElement('canvas');
      canvas.width = image.naturalWidth;
      canvas.height = image.naturalHeight;
      const context = canvas.getContext('2d');
      context.drawImage(image, 0, 0);
      return [...context.getImageData(0, 0, canvas.width, canvas.height).data];
    }, bytes.toString('base64'));
    for (let y = 0; y < item.height; y++)
      for (let x = 0; x < item.width; x++) {
        const expected =
          y < item.height / 2
            ? x < item.width / 2
              ? [255, 0, 0, 255]
              : [0, 0, 255, 255]
            : x < item.width / 2
              ? [0, 255, 0, 128]
              : [0, 0, 0, 0];
        assert.deepEqual(
          pixels.slice((y * item.width + x) * 4, (y * item.width + x + 1) * 4),
          expected,
          `${item.name} pixel ${x},${y}`,
        );
      }
    receipt.outputs.push({
      file: basename(artifact),
      ...info,
      ppi: item.ppi,
      pngSha256: hash(bytes),
      rgbaSha256: hash(Buffer.from(pixels)),
      stdout: result.stdout.trim(),
    });
    const refused = await run([item.input, output]);
    assert.equal(refused.code, 1);
    assert.match(refused.stderr, /already exists/);
    assert.equal(hash(await fs.readFile(output)), hash(bytes));
    await fs.writeFile(artifact, bytes);
  }
  const angularOutput = join(directory, 'angular.png'),
    angularResult = await run([angularInput, angularOutput, '--chromium-path', chrome]);
  assert.equal(angularResult.code, 0, JSON.stringify(angularResult));
  const angularBytes = await fs.readFile(angularOutput),
    angularInfo = pngInfo(angularBytes);
  assert.equal(angularInfo.width, 960);
  assert.equal(angularInfo.height, 540);
  const angularPixels = await page.evaluate(async (data) => {
    const image = new Image();
    image.src = 'data:image/png;base64,' + data;
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    const context = canvas.getContext('2d');
    context.drawImage(image, 0, 0);
    return [
      [480, 90],
      [660, 270],
      [480, 450],
      [300, 270],
      [10, 10],
    ].map(([x, y]) => [...context.getImageData(x, y, 1, 1).data]);
  }, angularBytes.toString('base64'));
  const expectedAngular = [
    [255, 0, 0, 255],
    [64, 191, 0, 255],
    [0, 128, 128, 255],
    [64, 0, 191, 255],
    [0, 0, 0, 0],
  ];
  angularPixels.forEach((rgba, index) =>
    rgba.forEach((value, channel) =>
      assert.ok(
        Math.abs(value - expectedAngular[index][channel]) <= 5,
        `angular sample ${index}: ${rgba}`,
      ),
    ),
  );
  const angularRefused = await run([angularInput, angularOutput, '--chromium-path', chrome]);
  assert.equal(angularRefused.code, 1);
  assert.match(angularRefused.stderr, /already exists/);
  assert.equal(hash(await fs.readFile(angularOutput)), hash(angularBytes));
  assert.equal(hash(await fs.readFile(angularInput)), angularHash);
  receipt.angular = { ...angularInfo, pngSha256: hash(angularBytes), samples: angularPixels };
  const nestedInput = join(directory, 'nested-angular.svg'),
    nestedOutput = join(directory, 'nested-angular.png');
  await fs.writeFile(nestedInput, nestedAngular);
  const nestedResult = await run([nestedInput, nestedOutput, '--chromium-path', chrome]);
  assert.equal(nestedResult.code, 0, JSON.stringify(nestedResult));
  const nestedBytes = await fs.readFile(nestedOutput);
  assert.deepEqual(pngInfo(nestedBytes), { width: 960, height: 540, density: null });
  const nestedPixels = await page.evaluate(async (data) => {
    const image = new Image();
    image.src = 'data:image/png;base64,' + data;
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = image.naturalWidth;
    canvas.height = image.naturalHeight;
    const context = canvas.getContext('2d');
    context.drawImage(image, 0, 0);
    return [
      [480, 90],
      [660, 270],
      [480, 450],
      [300, 270],
      [10, 10],
    ].map(([x, y]) => [...context.getImageData(x, y, 1, 1).data]);
  }, nestedBytes.toString('base64'));
  nestedPixels.forEach((rgba, index) =>
    rgba.forEach((value, channel) => {
      const expected = channel === 3 && index < 4 ? 128 : expectedAngular[index][channel];
      assert.ok(Math.abs(value - expected) <= 5, `nested angular sample ${index}: ${rgba}`);
    }),
  );
  assert.equal(hash(await fs.readFile(nestedInput)), hash(nestedAngular));
  receipt.nestedAngular = { pngSha256: hash(nestedBytes), samples: nestedPixels };
  for (const [name, svg, message] of [
    ['external', root('<image href="https://example.invalid/x.png"/>'), /external URLs/],
    ['responsive', root('', 'width="100%" height="100%" viewBox="0 0 32 16"'), /fixed dimensions/],
    ['oversized', root('', 'width="8193" height="1" viewBox="0 0 8193 1"'), /8192 px/],
    ['recursive-pattern', recursivePattern, /pattern|solid|fragment/i],
  ]) {
    const invalid = join(directory, `invalid-${name}.svg`),
      output = join(directory, `invalid-${name}.png`);
    await fs.writeFile(invalid, svg);
    const rejected = await run([invalid, output]);
    assert.equal(rejected.code, 1);
    assert.match(rejected.stderr, message);
    await assert.rejects(fs.lstat(output), { code: 'ENOENT' });
  }
  assert.ok(!(await fs.readdir(directory)).some((name) => name.endsWith('.tmp')));
  assert.equal(hash(await fs.readFile(input)), receipt.svgSha256);
  assert.equal(hash(await fs.readFile(physicalInput)), receipt.physicalSvgSha256);
  assert.equal(hash(await fs.readFile(jsonPath)), receipt.jsonSha256);
  assert.deepEqual(receipt.pageErrors, []);
  receipt.inputUnchanged = true;
  receipt.existingOutputPreserved = true;
  receipt.failureLeavesNoOutputOrTemp = true;
  receipt.ok = true;
  console.log(
    'PASS actual SVG-to-PNG CLI processes, Nect fixed/mm source, scale/PPI/pixels, no-clobber/immutable input, blocked active/external/font source',
  );
} finally {
  await fs.writeFile(receiptPath, JSON.stringify(receipt, null, 2));
  await browser.close();
  await fs.rm(directory, { recursive: true, force: true });
}
