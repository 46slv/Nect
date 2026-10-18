// Actual production bundle/CSP, browser UI and exported SVG raster alpha checks.
import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, layer, node, connect, serialize, exportSVG } from '../src/core.js';
const doc = document(),
  source = doc.layers[0],
  target = layer('Placed parts'),
  shape = node('Imported Path', 30, 30),
  fill = node('Fill', 300, 30),
  out = node('Output', 580, 30),
  ref = node('Composition Reference', 30, 30),
  second = node('Composition Reference', 30, 350),
  join = node('Join Geometry', 330, 30),
  dest = node('Output', 650, 30);
for (const [item, id] of [
  [source, 'composition-master-ring'],
  [target, 'composition-placed-parts'],
  [shape, 'master-ring-path'],
  [fill, 'master-ring-fill'],
  [out, 'master-ring-output'],
  [ref, 'placed-part-a'],
  [second, 'placed-part-b'],
  [join, 'placed-parts-join'],
  [dest, 'placed-parts-output'],
])
  item.id = id;
doc.name = 'Composition parts and alpha';
doc.width = 240;
doc.height = 140;
doc.schemaVersion = 31;
source.name = 'Master ring';
source.visible = false;
shape.rawPath = 'M10 10H90V90H10Z M30 30V70H70V30Z';
fill.params.color.value = { hex: '#ff0000', alpha: 0.5 };
source.nodes = [shape, fill, out];
source.output = out.id;
connect(source, shape.id, fill.id, 'path');
connect(source, fill.id, out.id, 'path');
ref.compositionReference = { compositionId: source.id };
second.compositionReference = { compositionId: source.id };
second.params.position.value = [40, 0];
target.nodes = [ref, second, join, dest];
target.output = dest.id;
doc.layers.push(target);
connect(target, ref.id, join.id, 'a');
connect(target, second.id, join.id, 'b');
connect(target, join.id, dest.id, 'path');
const spare = join.joinInputOrder[2];
if (spare) {
  join.params.input_spare = join.params[spare];
  delete join.params[spare];
  join.joinInputOrder[2] = 'input_spare';
}
const sample = serialize(doc);
await writeFile('examples/composition-parts-alpha.json', sample);
await writeFile('examples/composition-parts-alpha.svg', exportSVG(doc));
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const path = resolve(
    root,
    '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
  );
  if (!path.startsWith(root + sep)) {
    res.writeHead(403).end();
    return;
  }
  try {
    res.setHeader('Content-Security-Policy', config.app.security.csp);
    res.setHeader(
      'Content-Type',
      { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.ttf': 'font/ttf' }[
        extname(path)
      ] || 'application/octet-stream',
    );
    res.end(await readFile(path));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROME_PATH ||
      'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1050 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  await page.evaluate((s) => window.nect.load(s), sample);
  await page.locator(`[data-select-layer="${target.id}"]`).first().click();
  const send = async (type, payload = {}) =>
    page.evaluate(
      ({ type, payload }) => window.nect.dispatch({ ...window.nect.commandState, type, payload }),
      { type, payload: { layerId: target.id, ...payload } },
    );
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const select = async (id) => page.locator(`.node[data-id="${id}"] .node-head strong`).click();
  await select(ref.id);
  const picker = () => page.locator('.inspector-body [data-composition-target]');
  assert.equal(await picker().inputValue(), source.id);
  assert.equal(await picker().locator('option').count(), 2);
  const original = await saved();
  await picker().selectOption('');
  assert.match(await page.evaluate(() => window.nect.evaluate().error), /Choose/);
  await send('undo');
  assert.equal(await saved(), original);
  await select(ref.id);
  const x = page.locator('.inspector-body [data-key="position"] [data-axis="0"]');
  await x.click();
  await x.fill('12');
  assert.equal(await x.inputValue(), '12');
  await x.press('Tab');
  assert.equal(
    (
      await page.evaluate(
        (id) => window.nect.document.layers[1].nodes.find((n) => n.id === id).params.position.value,
        ref.id,
      )
    )[0],
    12,
  );
  await send('undo');
  // A selected instance inside a Join moves independently through its node-stage handle.
  await select(ref.id);
  const handle = page.locator(`[data-node-stage-move="${ref.id}"] rect`),
    box = await handle.boundingBox();
  assert.ok(box);
  const scale = await page.locator('.artboard').evaluate((svg) => svg.getScreenCTM().a);
  await page.keyboard.down('Alt');
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 15 * scale, box.y + box.height / 2 + 8 * scale, {
    steps: 5,
  });
  await page.mouse.up();
  await page.keyboard.up('Alt');
  const stageMoved = await page.evaluate(
    (id) => window.nect.document.layers[1].nodes.find((n) => n.id === id).params.position.value,
    ref.id,
  );
  assert.ok(
    Math.abs(stageMoved[0] - 15) < 0.001 && Math.abs(stageMoved[1] - 8) < 0.001,
    JSON.stringify(stageMoved),
  );
  await send('undo');
  assert.equal(await saved(), original);
  // UI duplication retains the original target; one Undo removes the instance.
  await select(ref.id);
  await page.locator(`.graph [data-duplicate="${ref.id}"]`).click();
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers[1].nodes.filter((n) => n.type === 'Composition Reference')
          .length,
    ),
    3,
  );
  await send('undo');
  // Ordinary typed graph connection upgrades Transform and serializes the mode.
  const made = await send('create-node', { nodeType: 'Transform', position: [330, 650] });
  const tr = await page.evaluate(
    () => window.nect.document.layers[1].nodes.find((n) => n.type === 'Transform').id,
  );
  await send('connect', { sourceId: ref.id, nodeId: tr, key: 'path' });
  await select(tr);
  assert.deepEqual(
    await page
      .locator('[data-transform-handle]')
      .evaluateAll((nodes) => nodes.map((n) => n.dataset.transformHandle).sort()),
    ['anchor', 'rotation', 'scale'],
  );
  await send('set-parameter-base', { nodeId: tr, key: 'offset', value: [5, 8] });
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[1].nodes.find((n) => n.id === id).paintMode,
      tr,
    ),
    true,
  );
  await send('undo');
  await send('undo');
  await send('undo');
  assert.equal(await saved(), original);
  // Direct instance supports actual Artwork canvas drag and a single Undo.
  await send('connect', { sourceId: ref.id, nodeId: dest.id, key: 'path' });
  await page.locator(`[data-select-layer="${target.id}"]`).first().click();
  const screen = async (p) =>
    page.locator('.artboard').evaluate((svg, p) => {
      const a = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
      return [a.x, a.y];
    }, p);
  const a = await screen([20, 20]),
    b = await screen([30, 25]);
  await page.mouse.move(...a);
  await page.mouse.down();
  await page.mouse.move(...b, { steps: 5 });
  await page.mouse.up();
  const moved = await page.evaluate(
    (id) => window.nect.document.layers[1].nodes.find((n) => n.id === id).params.position.value,
    ref.id,
  );
  assert.ok(
    Math.abs(moved[0] - 10) < 0.001 && Math.abs(moved[1] - 5) < 0.001,
    JSON.stringify(moved),
  );
  await send('undo');
  await send('undo');
  assert.equal(await saved(), original);
  // Source navigation and source edit use the common document command owner.
  await select(ref.id);
  await page.locator('.inspector-body [data-composition-jump]').click();
  assert.equal(await page.locator('.inspector-body [data-composition-target]').count(), 0);
  await page.evaluate(
    ({ layerId, nodeId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-parameter',
        payload: { layerId, nodeId, key: 'color', value: { hex: '#00aa00', alpha: 0.5 } },
      }),
    { layerId: source.id, nodeId: fill.id },
  );
  assert.match(await page.evaluate(() => window.nect.export()), /#00aa00/);
  await send('undo');
  await page.evaluate((s) => window.nect.load(s), sample);
  const svg = await page.evaluate(() => window.nect.export());
  assert.equal(svg, exportSVG(doc));
  const raster = await page.evaluate(async (svg) => {
    const image = new Image();
    image.src = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = 240;
    canvas.height = 140;
    const c = canvas.getContext('2d');
    c.drawImage(image, 0, 0);
    URL.revokeObjectURL(image.src);
    return {
      pixels: [
        [0, 0],
        [15, 15],
        [40, 50],
        [50, 50],
        [80, 20],
      ].map(([x, y]) => Array.from(c.getImageData(x, y, 1, 1).data)),
      png: canvas.toDataURL('image/png').split(',')[1],
    };
  }, svg);
  assert.deepEqual(
    raster.pixels.map((p) => p[3]),
    [0, 128, 0, 128, 192],
  );
  await mkdir('test-results/composition-reference', { recursive: true });
  await writeFile(
    'test-results/composition-reference/alpha.png',
    Buffer.from(raster.png, 'base64'),
  );
  await page.screenshot({ path: 'test-results/composition-reference/editor.png' });
  await writeFile(
    'test-results/composition-reference/receipt.json',
    JSON.stringify(
      {
        schemaVersion: 31,
        checks: [
          'source dropdown',
          'actual canvas instance drag / one Undo',
          'selected instance inside Join canvas drag / one Undo',
          'paint Transform canvas handles',
          'missing selection / Undo',
          'numeric position / Undo',
          'duplicate / Undo',
          'paint Transform connection / Undo',
          'source navigation / edit / live export',
          'save / reload',
          'SVG exact match',
          'PNG actual alpha / holes / overlapping independent paint',
        ],
        pixels: raster.pixels,
        errors,
      },
      null,
      2,
    ),
  );
  assert.deepEqual(errors, []);
  console.log(
    'Composition production UI, persistence and SVG/PNG alpha passed',
    raster.pixels.map((p) => p[3]),
  );
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
