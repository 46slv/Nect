import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { Window } from 'happy-dom';
import { blankDocument, document, serialize, node, connect, exportSVG } from '../src/core.js';
import { importSVG } from '../src/svg-import.js';
const source = `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="160" viewBox="0 0 200 160"><path d="M10 10H90V90H10Z M30 30V70H70V30Z" fill="#ff0000" fill-opacity="0.5"/><rect x="60" y="10" width="80" height="80" fill="#0000ff" fill-opacity="0.5"/><path d="M10 120H140" fill="none" stroke="#008000" stroke-width="4" stroke-linecap="round"/><rect x="150" y="110" width="20" height="20" fill="none"/></svg>`;
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const p = resolve(
    root,
    '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
  );
  if (!p.startsWith(root + sep)) {
    res.writeHead(403).end();
    return;
  }
  try {
    res.setHeader('Content-Security-Policy', config.app.security.csp);
    res.setHeader(
      'Content-Type',
      { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.ttf': 'font/ttf' }[
        extname(p)
      ] || 'application/octet-stream',
    );
    res.end(await readFile(p));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } }),
    errors = [],
    requests = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('request', (r) => {
    if (
      !r.url().startsWith('http://127.0.0.1:') &&
      !r.url().startsWith('data:') &&
      !r.url().startsWith('blob:')
    )
      requests.push(r.url());
  });
  page.on('dialog', (d) => d.accept());
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const load = (s) => page.evaluate((s) => window.nect.load(s), s);
  const send = (type, payload = {}) =>
    page.evaluate(
      ({ type, payload }) =>
        window.nect.dispatch({
          ...window.nect.commandState,
          type,
          payload: { layerId: window.nect.document.layers[0].id, ...payload },
        }),
      { type, payload },
    );
  const add = async (markup = source, name = 'mark.svg') => {
    const chooser = page.waitForEvent('filechooser');
    await page.getByRole('button', { name: 'Add SVG node', exact: true }).click();
    await (
      await chooser
    ).setFiles({ name, mimeType: 'image/svg+xml', buffer: Buffer.from(markup) });
    await page.waitForFunction(
      () => !document.querySelector('.message')?.textContent?.includes('Project restored'),
    );
  };
  const pixels = (svg) =>
    page.evaluate(async (svg) => {
      const image = new Image();
      image.src = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
      await image.decode();
      const c = document.createElement('canvas');
      c.width = image.naturalWidth;
      c.height = image.naturalHeight;
      const x = c.getContext('2d');
      x.drawImage(image, 0, 0);
      const data = x.getImageData(0, 0, c.width, c.height).data;
      const hash = Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', data)))
        .map((n) => n.toString(16).padStart(2, '0'))
        .join('');
      const alpha = (a, b) => data[(b * c.width + a) * 4 + 3];
      return { hash, alpha: [alpha(20, 20), alpha(40, 40), alpha(80, 20), alpha(160, 120)] };
    }, svg);
  const empty = blankDocument();
  empty.width = 200;
  empty.height = 160;
  await load(serialize(empty));
  const before = await saved();
  await add();
  await page.waitForFunction(() => window.nect.document.layers.length === 2);
  const after = await saved(),
    imported = await page.evaluate(() => window.nect.document),
    ref = imported.layers[0].nodes.find((n) => n.name === 'SVG · mark.svg');
  assert.ok(ref);
  assert.equal(imported.layers[1].visible, false);
  const rendered = await pixels(await page.evaluate(() => window.nect.export()));
  assert.equal(rendered.hash, (await pixels(source)).hash, 'original SVG and embedded part pixels');
  assert.deepEqual(rendered.alpha, [128, 0, 192, 0]);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), after);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await load(after);
  assert.equal((await pixels(await page.evaluate(() => window.nect.export()))).hash, rendered.hash);
  // Identical paint branches compact before paint, preserving independent
  // alpha overlap, holes, opposite winding, stroke order and gradient servers.
  const compactCases = [
    '<g fill="#ff0000" fill-opacity="0.5" stroke="#008000" stroke-opacity="0.3" stroke-width="4" stroke-linecap="round" stroke-linejoin="bevel" stroke-dasharray="3 2"><path d="M10 10H90V90H10Z M30 30V70H70V30Z"/><path d="M60 10V90H140V10Z"/></g>',
    '<defs><linearGradient id="ramp" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="180" y2="100" spreadMethod="pad" color-interpolation="sRGB"><stop offset="0" stop-color="#ff0000" stop-opacity="0.2"/><stop offset="1" stop-color="#123456" stop-opacity="0.8"/></linearGradient></defs><g fill="url(#ramp)" fill-opacity="0.5"><path d="M10 10H90V90H10Z"/><path d="M60 10H140V90H60Z"/></g>',
    '<path fill="#ff0000" d="M10 10H90V90H10Z"/><path fill="#0000ff" fill-opacity="0.5" d="M60 10H140V90H60Z"/><path fill="#ff0000" fill-opacity="0.5" d="M80 40H160V120H80Z"/>',
  ];
  for (const [index, body] of compactCases.entries()) {
    const markup = `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="160" viewBox="0 0 200 160">${body}</svg>`;
    await load(serialize(empty));
    await add(markup, `compact-${index}.svg`);
    await page.waitForFunction(() => window.nect.document.layers.length === 2);
    const svg = await page.evaluate(() => window.nect.export());
    const baseline = exportSVG(importSVG(markup, new Window().DOMParser));
    assert.equal(
      (await pixels(svg)).hash,
      (await pixels(baseline)).hash,
      `existing import pixels ${index}`,
    );
    // Scaling gradient stop alpha is the existing importer's representation of
    // fill-opacity. Browser interpolation can round it differently from native
    // SVG fill-opacity; compacting must match the existing imported appearance.
    if (index !== 1)
      assert.equal(
        (await pixels(svg)).hash,
        (await pixels(markup)).hash,
        `native SVG pixels ${index}`,
      );
    const source = await page.evaluate(() => window.nect.document.layers[1]);
    assert.equal(source.nodes.filter((n) => n.type === 'Multi Path').length, index === 2 ? 3 : 2);
    assert.equal(source.nodes.filter((n) => n.type === 'Path Remap').length, 0);
    assert.equal(
      source.nodes.filter((n) => n.type === 'Join Geometry').length,
      index === 2 ? 4 : 1,
    );
    const json = await saved();
    await load(json);
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
  }
  // Preserve an existing Composition and an unrelated layer, then add a part.
  const existing = document();
  existing.width = 500;
  existing.height = 240;
  existing.layers[0].nodes.find((n) => n.id === existing.layers[0].output).params.path.locked =
    false;
  existing.layers.push({ ...blankDocument().layers[0], name: 'Keep unrelated' });
  await load(serialize(existing));
  const original = await saved();
  await add();
  await page.waitForFunction(() => window.nect.document.layers.length === 3);
  const combined = await page.evaluate(() => window.nect.document),
    instance = combined.layers[0].nodes.find((n) => n.name === 'SVG · mark.svg');
  assert.deepEqual(combined.layers[1], existing.layers[1]);
  assert.equal(combined.name, existing.name);
  assert.equal(combined.width, 500);
  const select = async (id) => {
    await page.locator(`.node[data-id="${id}"] .node-head strong`).click();
  };
  await select(instance.id);
  const x = page.locator('.inspector-body [data-key="position"] [data-axis="0"]');
  await x.fill('180');
  await x.press('Tab');
  assert.equal(
    await page.evaluate(
      (id) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).params.position.value[0],
      instance.id,
    ),
    180,
  );
  await select(instance.id);
  const handle = page.locator(`[data-node-stage-move="${instance.id}"] rect`),
    box = await handle.boundingBox();
  assert.ok(box);
  const scale = await page.locator('.artboard').evaluate((svg) => svg.getScreenCTM().a);
  await page.keyboard.down('Alt');
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 12 * scale, box.y + box.height / 2 + 8 * scale, {
    steps: 5,
  });
  await page.mouse.up();
  await page.keyboard.up('Alt');
  const moved = await page.evaluate(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).params.position.value,
    instance.id,
  );
  assert.ok(Math.abs(moved[0] - 192) < 0.001 && Math.abs(moved[1] - 8) < 0.001);
  await select(instance.id);
  await page.locator(`.graph [data-duplicate="${instance.id}"]`).click();
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers[0].nodes.filter((n) => n.type === 'Composition Reference')
          .length,
    ),
    2,
  );
  await send('undo');
  const tr = await send('create-node', { nodeType: 'Transform', position: [330, 650] });
  await send('connect', { sourceId: instance.id, nodeId: tr.nodeId, key: 'path' });
  await send('set-parameter-base', { nodeId: tr.nodeId, key: 'rotation', value: 30 });
  await send('set-parameter-base', { nodeId: tr.nodeId, key: 'scale', value: 0.75 });
  const join = combined.layers[0].nodes.find((n) => n.type === 'Join Geometry');
  await send('connect', { sourceId: tr.nodeId, nodeId: join.id, key: 'b' });
  const final = await saved(),
    finalSvg = await page.evaluate(() => window.nect.export());
  await load(final);
  assert.equal(await page.evaluate(() => window.nect.export()), finalSvg);
  await mkdir('test-results/svg-part', { recursive: true });
  const outputs = {};
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const pending = page.waitForEvent('download');
    await page.locator(`[data-action="${action}"]`).click();
    const file = `test-results/svg-part/parts.${ext}`;
    await (await pending).saveAs(file);
    outputs[ext] = await readFile(file);
  }
  assert.equal(outputs.svg.toString(), finalSvg);
  await load(outputs.json.toString());
  assert.equal(await page.evaluate(() => window.nect.export()), finalSvg);
  const pngMatch = await page.evaluate(
    async ({ svg, png }) => {
      const images = [];
      for (const src of [
        URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' })),
        'data:image/png;base64,' + png,
      ]) {
        const i = new Image();
        i.src = src;
        await i.decode();
        const c = document.createElement('canvas');
        c.width = i.naturalWidth;
        c.height = i.naturalHeight;
        const x = c.getContext('2d');
        x.drawImage(i, 0, 0);
        images.push(x.getImageData(0, 0, c.width, c.height).data);
      }
      return images[0].length === images[1].length && images[0].every((n, i) => n === images[1][i]);
    },
    { svg: finalSvg, png: outputs.png.toString('base64') },
  );
  assert.equal(pngMatch, true);
  const stable = await saved();
  await add('<svg><image href="https://example.invalid/x.png"/></svg>', 'unsupported.svg');
  await page.waitForFunction(() =>
    document.querySelector('.message')?.textContent.includes('Unsupported SVG'),
  );
  assert.equal(await saved(), stable);
  assert.deepEqual(requests, []);
  // Existing import remains an explicit, undoable document replacement.
  const chooser = page.waitForEvent('filechooser');
  await page.getByRole('button', { name: 'Import SVG document', exact: true }).click();
  await page.locator('dialog[open] button[value="discard"]').click();
  await (
    await chooser
  ).setFiles({
    name: 'replace.svg',
    mimeType: 'image/svg+xml',
    buffer: Buffer.from('<svg viewBox="0 0 50 50"><rect width="20" height="20"/></svg>'),
  });
  await page.waitForFunction(() => window.nect.document.width === 50);
  await send('undo');
  assert.equal(await saved(), stable);
  await page.screenshot({ path: 'test-results/svg-part/editor.png' });
  assert.deepEqual(errors, []);
  // Exercise the real desktop JS branch with an explicit IPC stub. HTML file
  // activation is prohibited in this page: both SVG actions must use native IO.
  const native = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  native.on('pageerror', (e) => errors.push(e.message));
  await native.addInitScript(
    ({ source }) => {
      window.isTauri = true;
      window.svgCalls = [];
      window.svgResponse = ['mark.svg', source];
      window.__TAURI_INTERNALS__ = {
        transformCallback: () => 1,
        unregisterCallback: () => {},
        invoke: async (command, args) => {
          window.svgCalls.push(command);
          if (command.startsWith('list_')) return [];
          if (command === 'enable_close_guard' || command === 'release_project') return null;
          if (command === 'choose_unsaved') return 'discard';
          if (command === 'open_svg_source') {
            if (window.svgReadError) throw Error(window.svgReadError);
            return window.svgResponse;
          }
          throw Error('Unexpected IPC ' + command);
        },
      };
      const original = HTMLInputElement.prototype.click;
      HTMLInputElement.prototype.click = function () {
        if (this.type === 'file') throw Error('Desktop SVG used HTML file activation');
        return original.call(this);
      };
    },
    { source },
  );
  await native.goto(`http://127.0.0.1:${server.address().port}`);
  await native.waitForFunction(() => window.nect);
  await native.evaluate((s) => window.nect.load(s), serialize(empty));
  await native.getByRole('button', { name: 'Add SVG node', exact: true }).click();
  await native.waitForFunction(() => window.nect.document.layers.length === 2);
  const nativeBefore = await native.evaluate(() => JSON.stringify(window.nect.document));
  await native.evaluate(
    () => (window.svgResponse = ['bad.svg', '<svg><script>alert(1)</script></svg>']),
  );
  await native.getByRole('button', { name: 'Import SVG document', exact: true }).click();
  await native.waitForFunction(() =>
    document.querySelector('.status.error .message')?.textContent.includes('Unsupported SVG'),
  );
  assert.equal(await native.evaluate(() => JSON.stringify(window.nect.document)), nativeBefore);
  await native.evaluate(() => {
    window.svgReadError = 'SVG file is unreadable';
  });
  await native.getByRole('button', { name: 'Import SVG document', exact: true }).click();
  await native.waitForFunction(() =>
    document.querySelector('.status.error .message')?.textContent.includes('unreadable'),
  );
  assert.equal(await native.evaluate(() => JSON.stringify(window.nect.document)), nativeBefore);
  await native.evaluate(() => {
    window.svgReadError = null;
    window.svgResponse = null;
  });
  await native.getByRole('button', { name: 'Import SVG document', exact: true }).click();
  await native.waitForFunction(
    () => document.querySelector('.message')?.textContent === 'SVG selection cancelled',
  );
  assert.equal(await native.evaluate(() => JSON.stringify(window.nect.document)), nativeBefore);
  await native.evaluate(
    () =>
      (window.svgResponse = [
        'valid.svg',
        '<svg viewBox="0 0 50 50"><rect width="20" height="20"/></svg>',
      ]),
  );
  await native.getByRole('button', { name: 'Import SVG document', exact: true }).click();
  await native.waitForFunction(() => window.nect.document.width === 50);
  await native.getByRole('button', { name: 'Import SVG document', exact: true }).click();
  await native.waitForFunction(
    () => window.svgCalls.filter((c) => c === 'open_svg_source').length === 6,
  );
  await native.locator('[data-action="undo"]').click();
  assert.equal(await native.evaluate(() => window.nect.document.width), 50);
  await native.locator('[data-action="undo"]').click();
  assert.equal(await native.evaluate(() => JSON.stringify(window.nect.document)), nativeBefore);
  assert.deepEqual(errors, []);
  await native.screenshot({ path: 'test-results/svg-part/desktop-ipc.png' });
  await native.close();
  let privateAttachment = null;
  if (process.env.NECT_PRIVATE_SVG_FILE) {
    const text = await readFile(process.env.NECT_PRIVATE_SVG_FILE, 'utf8');
    const d = blankDocument();
    d.width = 41;
    d.height = 41;
    await load(serialize(d));
    await add(text, 'private-attached.svg');
    await page.waitForFunction(() => window.nect.document.layers.length === 2);
    const vector = await page.evaluate(() => window.nect.export());
    const raster = await page.evaluate(
      async ({ original, vector }) => {
        const all = [];
        for (const text of [original, vector]) {
          const xml = new DOMParser().parseFromString(text, 'image/svg+xml');
          xml.documentElement.setAttribute('width', '820');
          xml.documentElement.setAttribute('height', '820');
          const image = new Image();
          image.src = URL.createObjectURL(
            new Blob([new XMLSerializer().serializeToString(xml)], { type: 'image/svg+xml' }),
          );
          await image.decode();
          const c = document.createElement('canvas');
          c.width = c.height = 820;
          const x = c.getContext('2d');
          x.drawImage(image, 0, 0);
          all.push(x.getImageData(0, 0, 820, 820).data);
        }
        let total = 0,
          changed = 0,
          max = 0;
        for (let i = 0; i < all[0].length; i++) {
          const d = Math.abs(all[0][i] - all[1][i]);
          total += d;
          if (d) changed++;
          max = Math.max(max, d);
        }
        return {
          meanChannelError: total / all[0].length,
          differentChannels: changed,
          maxChannelError: max,
        };
      },
      { original: text, vector },
    );
    assert.ok(raster.meanChannelError < 1, JSON.stringify(raster));
    const chooser = page.waitForEvent('filechooser');
    await page.getByRole('button', { name: 'Import SVG document', exact: true }).click();
    await page.locator('dialog[open] button[value="discard"]').click();
    await (
      await chooser
    ).setFiles({
      name: 'private-attached.svg',
      mimeType: 'image/svg+xml',
      buffer: Buffer.from(text),
    });
    await page.waitForFunction(
      () => window.nect.document.layers.length > 2 && window.nect.document.width === 41,
    );
    privateAttachment = {
      addSvgNode: true,
      importSvgDocument: true,
      raster,
      sourceKeptPrivate: true,
    };
    const folder = process.env.NECT_PRIVATE_SVG_RECEIPT_DIR;
    if (folder) {
      await mkdir(folder, { recursive: true });
      await writeFile(folder + '/imported.json', await saved());
      await writeFile(folder + '/imported.svg', await page.evaluate(() => window.nect.export()));
      await writeFile(folder + '/receipt.json', JSON.stringify(privateAttachment, null, 2));
    }
  }
  // A structural render must preserve a scrolled Layers panel and stable rename
  // target. This fixture is synthetic and independent of the private SVG input.
  const many = blankDocument();
  many.layers = Array.from({ length: 40 }, (_, i) => ({
    ...blankDocument().layers[0],
    name: `Part ${i + 1}`,
  }));
  await load(serialize(many));
  const row = await page.locator('.layers').evaluate((el) => {
    el.scrollTop = Math.floor((el.scrollHeight - el.clientHeight) / 2);
    const bounds = el.getBoundingClientRect();
    const item = [...el.querySelectorAll('[data-layer]')].find((r) => {
      const b = r.getBoundingClientRect();
      return b.top > bounds.top + 10 && b.bottom < bounds.bottom - 5;
    });
    return {
      id: item.dataset.layer,
      scroll: el.scrollTop,
      name: item.querySelector('.name').textContent,
    };
  });
  assert.ok(row.scroll > 100);
  const assertScroll = async () => {
    const actual = await page.locator('.layers').evaluate((el) => el.scrollTop);
    assert.ok(Math.abs(actual - row.scroll) <= 1, `Layers scroll ${row.scroll} → ${actual}`);
  };
  await page.locator(`[data-visible="${row.id}"]`).click();
  await assertScroll();
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers.find((l) => l.id === id).visible,
      row.id,
    ),
    false,
  );
  await page.locator(`[data-select-layer="${row.id}"]`).dblclick();
  const rename = page.locator(`[data-rename-name="${row.id}"]`);
  await rename.waitFor();
  await assertScroll();
  assert.equal(await rename.evaluate((el) => el === document.activeElement), true);
  await rename.fill('Renamed part');
  await rename.press('Enter');
  await assertScroll();
  assert.equal(
    await page.evaluate((id) => window.nect.document.layers.find((l) => l.id === id).name, row.id),
    'Renamed part',
  );
  await page.locator('[data-action="undo"]').click();
  await assertScroll();
  assert.equal(await page.locator(`[data-select-layer="${row.id}"]`).textContent(), row.name);
  await page.locator('[data-action="redo"]').click();
  await assertScroll();
  await page.locator(`[data-select-layer="${row.id}"]`).dblclick();
  await rename.fill('Cancelled');
  await rename.press('Escape');
  await assertScroll();
  assert.equal(await page.locator(`[data-select-layer="${row.id}"]`).textContent(), 'Renamed part');
  await page.locator(`[data-layer-solo="${row.id}"]`).click();
  await assertScroll();
  assert.equal(
    await page.locator(`[data-layer-solo="${row.id}"]`).getAttribute('aria-pressed'),
    'true',
  );
  assert.equal(
    await page
      .locator(`.layer[data-layer="${row.id}"]`)
      .evaluate((el) => el.classList.contains('selected')),
    true,
  );
  // The header Clear button is deliberately outside this scrolled viewport;
  // dispatch the same command here to isolate rerender from browser click-scroll.
  await send('clear-layer-solo');
  await assertScroll();
  await page.screenshot({ path: 'test-results/svg-part/layer-scroll.png' });
  const solo = blankDocument();
  solo.width = 180;
  solo.height = 80;
  solo.layers = [];
  for (const [i, color] of ['#ff0000', '#0000ff', '#00ff00'].entries()) {
    const l = blankDocument().layers[0],
      shape = node('Rectangle'),
      fill = node('Fill'),
      out = l.nodes[0];
    l.name = ['Red', 'Blue', 'Hidden'][i];
    l.visible = i < 2;
    shape.params.center.value = [25 + i * 60, 25];
    shape.params.size.value = [40, 40];
    fill.params.color.value = { hex: color, alpha: 0.5 };
    l.nodes = [shape, fill, out];
    connect(l, shape.id, fill.id, 'path');
    connect(l, fill.id, out.id, 'path');
    solo.layers.push(l);
  }
  await load(serialize(solo));
  const soloClick = (i) => page.locator(`[data-layer-solo="${solo.layers[i].id}"]`).click();
  await soloClick(1);
  assert.equal(await page.locator('.artboard .shape').count(), 1);
  assert.match(await page.evaluate(() => window.nect.export()), /#0000ff/);
  assert.doesNotMatch(await page.evaluate(() => window.nect.export()), /#ff0000/);
  await soloClick(0);
  assert.equal(await page.locator('.artboard .shape').count(), 2);
  await soloClick(2);
  assert.equal(await page.locator('.artboard .shape').count(), 2);
  const savedSolo = await saved(),
    svgSolo = await page.evaluate(() => window.nect.export());
  await load(savedSolo);
  assert.equal(await page.evaluate(() => window.nect.export()), svgSolo);
  await page.locator(`[data-visible="${solo.layers[1].id}"]`).click();
  assert.equal(await page.locator('.artboard .shape').count(), 1);
  await page.getByRole('button', { name: 'Clear all layer Solo', exact: true }).click();
  assert.equal(await page.locator('.artboard .shape').count(), 1);
  assert.deepEqual(await page.evaluate(() => window.nect.document.layers.map((l) => l.visible)), [
    true,
    false,
    false,
  ]);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.locator('.artboard .shape').count(), 1);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.locator('.artboard .shape').count(), 2);
  await mkdir('test-results/layer-solo', { recursive: true });
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const pending = page.waitForEvent('download');
    await page.locator(`[data-action="${action}"]`).click();
    await (await pending).saveAs(`test-results/layer-solo/solo.${ext}`);
  }
  assert.equal(await readFile('test-results/layer-solo/solo.svg', 'utf8'), svgSolo);
  const soloPng = await readFile('test-results/layer-solo/solo.png');
  const soloAlpha = await page.evaluate(async (png) => {
    const image = new Image();
    image.src = 'data:image/png;base64,' + png;
    await image.decode();
    const c = document.createElement('canvas');
    c.width = image.naturalWidth;
    c.height = image.naturalHeight;
    const x = c.getContext('2d');
    x.drawImage(image, 0, 0);
    return [25, 85, 145].map((p) => x.getImageData(p, 25, 1, 1).data[3]);
  }, soloPng.toString('base64'));
  assert.deepEqual(soloAlpha, [128, 128, 0]);
  await load(await readFile('test-results/layer-solo/solo.json', 'utf8'));
  assert.equal(await page.evaluate(() => window.nect.export()), svgSolo);
  await page.screenshot({ path: 'test-results/layer-solo/editor.png' });
  // Opening a real project can start with a hidden locked source Composition.
  // Prefer its visible editable artwork; explicit locked selection must explain refusal.
  const entry = document(),
    lockedEntry = entry.layers[0],
    visibleEntry = document().layers[0],
    imageEntry = document().layers[0];
  lockedEntry.visible = false;
  lockedEntry.locked = true;
  const imageOutput = node('Image Output');
  imageEntry.nodes = [imageOutput];
  imageEntry.output = imageOutput.id;
  entry.layers.push(visibleEntry, imageEntry);
  await load(serialize(entry));
  assert.equal(
    await page.locator(`.graph .node[data-id="${visibleEntry.nodes[0].id}"]`).count(),
    1,
  );
  const countBefore = visibleEntry.nodes.length;
  await page.locator('#add-node').click();
  await page.locator('dialog[open] input[type="search"]').fill('Circle');
  await page
    .locator('dialog[open]')
    .getByRole('option', { name: /^Circle/ })
    .click();
  await page.locator('dialog[open] .picker-apply').click();
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers.find((l) => l.id === id).nodes.length,
      visibleEntry.id,
    ),
    countBefore + 1,
  );
  await page.locator('[data-action="undo"]').click();
  await page.locator(`[data-select-layer="${lockedEntry.id}"]`).click();
  await page.locator('#add-node').click();
  assert.equal(await page.locator('dialog[open]').count(), 0);
  assert.match(await page.locator('.status').innerText(), /Composition is locked/);
  await page.getByRole('button', { name: 'Add SVG node', exact: true }).click();
  assert.match(await page.locator('.status').innerText(), /unlocked visible/);
  await page.locator(`[data-select-layer="${imageEntry.id}"]`).click();
  const entryBefore = await saved();
  let unexpectedChooser = 0;
  const countChooser = () => unexpectedChooser++;
  page.on('filechooser', countChooser);
  await page.getByRole('button', { name: 'Add SVG node', exact: true }).click();
  assert.match(await page.locator('.status').innerText(), /regular Output/);
  assert.equal(unexpectedChooser, 0);
  page.off('filechooser', countChooser);
  assert.equal(await saved(), entryBefore);
  const receipt = {
    productionCsp: true,
    actualFileChooser: true,
    existingCompositionPreserved: true,
    unrelatedLayerPreserved: true,
    embeddedVectorSource: true,
    originalSvgPixelsEqual: true,
    alphaAndHole: rendered.alpha,
    positionAndCanvasMove: true,
    duplicate: true,
    transform: true,
    oneUndo: true,
    jsonColdReload: true,
    svgPngPixelsEqual: true,
    compactPathJoinPixelsEqual: compactCases.length,
    unsupportedAtomic: true,
    noExternalRequests: true,
    oldImportStillReplaces: true,
    desktopNativeIoBranch: true,
    desktopIoIsExplicitStub: true,
    desktopErrorCancelAndSameFileRetry: true,
    layersHideRenameUndoRedoKeepScroll: true,
    multipleSoloPreviewExport: true,
    soloDoesNotRewriteHide: true,
    soloSavedAndUndoable: true,
    initialEditableComposition: true,
    lockedNodePickerFeedback: true,
    svgPreflightBeforeChooser: true,
    privateAttachment,
    pageErrors: errors,
  };
  await writeFile('test-results/svg-part/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
