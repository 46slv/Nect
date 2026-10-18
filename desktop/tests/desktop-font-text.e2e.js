// Production Edge/CSP UI + explicit IPC fixture from separately tested Windows
// DirectWrite. This does not claim native WebView2/IME acceptance.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, serialize } from '../src/core.js';
const outlines = JSON.parse(await readFile(process.env.NECT_DESKTOP_FONT_SMOKE_OUTPUT, 'utf8'));
const variants = outlines.filter((r) => r.face.family === 'Meiryo' && r.face.weight === 400),
  raw = variants[0];
assert.ok(variants.length >= 8, 'Run Windows native typography test with fixture output first');
const doc = document(),
  layer = doc.layers[0],
  text = node('Text', 100, 80),
  fill = node('Fill', 420, 80);
text.text = raw.text;
text.params.size.value = raw.size;
text.params.lineSpacing.value = raw.lineSpacing;
text.params.position.value = [480, 160];
fill.params.path.source = text.id;
layer.nodes.push(text, fill);
layer.nodes.find((n) => n.id === layer.output).params.path.source = fill.id;
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const path = resolve(
    root,
    '.' + (req.url === '/' ? '/index.html' : new URL(req.url, 'http://localhost').pathname),
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
  const page = await browser.newPage({ viewport: { width: 1500, height: 1000 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript(
    ({ variants }) => {
      window.isTauri = true;
      window.fontCalls = [];
      window.fontSaves = [];
      window.fontAvailable = true;
      window.__TAURI_INTERNALS__ = {
        transformCallback: () => 1,
        unregisterCallback: () => {},
        invoke: async (command, args) => {
          window.fontCalls.push(command);
          if (command === 'list_system_fonts' || command === 'list_os_outline_fonts') return [];
          if (command === 'list_desktop_font_faces')
            return window.fontAvailable ? [variants[0].face] : [];
          if (command === 'shape_desktop_text') {
            if (!window.fontAvailable) throw Error('Font unavailable; saved outlines retained');
            const r = variants.find(
              (r) =>
                [
                  'text',
                  'size',
                  'tracking',
                  'lineSpacing',
                  'kerning',
                  'ligatures',
                  'alignment',
                ].every((k) => r[k] === args.request[k]) &&
                JSON.stringify(r.face) === JSON.stringify(args.request.face),
            );
            if (!r) throw Error('No captured native fixture for requested text/settings');
            return structuredClone(r);
          }
          if (command === 'save_project') {
            window.fontSaves.push({ kind: 'json', contents: args.contents });
            return ['test-font-token', 'C:\\test\\fonts.json', args.operation];
          }
          if (command === 'save_document') {
            window.fontSaves.push(args);
            return true;
          }
          if (command === 'save_png') {
            window.fontSaves.push({ kind: 'png', bytes: args.bytes });
            return true;
          }
          if (command === 'choose_unsaved') return 'cancel';
          if (['enable_close_guard', 'release_project'].includes(command)) return null;
          throw Error('Unexpected native command ' + command);
        },
      };
    },
    { variants },
  );
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(() => window.nect);
  await page.evaluate((contents) => window.nect.load(contents), serialize(doc));
  await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
  await page.locator('.inspector-body [data-os-text-font]').click();
  const dialog = page.getByRole('dialog', { name: 'OS font outlines' });
  await dialog.getByRole('combobox').fill('Meiryo');
  await dialog.getByRole('option').click();
  await dialog.getByRole('button', { name: 'Use font for Text', exact: true }).click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont?.version === 2,
    text.id,
  );
  const live = () =>
    page.evaluate((id) => window.nect.document.layers[0].nodes.find((n) => n.id === id), text.id);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  const calls = await page.evaluate(
    () => window.fontCalls.filter((c) => c === 'shape_desktop_text').length,
  );
  await page.locator('.inspector-body [data-os-text]').evaluate((el, value) => {
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
    el.value = value;
    el.dispatchEvent(new InputEvent('input', { bubbles: true, isComposing: true }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
  }, variants[1].text);
  assert.equal(
    await page.evaluate(() => window.fontCalls.filter((c) => c === 'shape_desktop_text').length),
    calls,
  );
  assert.equal((await live()).text, raw.text);
  await page.locator('.inspector-body [data-os-text]').evaluate((el) => {
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: el.value }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
  });
  await page.waitForFunction(
    ({ id, value }) => window.nect.document.layers[0].nodes.find((n) => n.id === id).text === value,
    { id: text.id, value: variants[1].text },
  );
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('Control+z');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  await page.keyboard.press('Control+Shift+z');
  const setting = async (key, value, index) => {
    const el = page.locator(`.inspector-body [data-os-setting="${key}"]`);
    if (typeof value === 'boolean') await el.setChecked(value);
    else if (key === 'alignment') await el.selectOption(value);
    else {
      await el.fill(String(value));
      await el.dispatchEvent('change');
    }
    await page.waitForFunction(
      ({ id, raw }) =>
        JSON.stringify(
          window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont.outline,
        ) === JSON.stringify(raw),
      { id: text.id, raw: variants[index] },
    );
  };
  await setting('tracking', 4, 2);
  await setting('size', 80, 3);
  await setting('lineSpacing', 2, 4);
  await setting('kerning', false, 5);
  await setting('ligatures', false, 6);
  await setting('alignment', 'center', 7);
  const saved = await page.evaluate(() => JSON.stringify(window.nect.document)),
    svg = await page.evaluate(() => window.nect.export());
  await page.locator('[data-action="save"]').click();
  await page.waitForFunction(() => window.fontSaves.some((s) => s.kind === 'json'));
  await page.locator('[data-action="export"]').click();
  await page.waitForFunction(() => window.fontSaves.some((s) => s.kind === 'svg'));
  await page.locator('[data-action="export-png"]').click();
  await page.waitForFunction(() => window.fontSaves.some((s) => s.kind === 'png'));
  const saves = await page.evaluate(() => window.fontSaves);
  assert.deepEqual(JSON.parse(saves.find((s) => s.kind === 'json').contents), JSON.parse(saved));
  assert.equal(saves.find((s) => s.kind === 'svg').contents, svg);
  assert.deepEqual(
    saves.find((s) => s.kind === 'png').bytes.slice(0, 8),
    [137, 80, 78, 71, 13, 10, 26, 10],
  );
  await page.evaluate(() => (window.fontAvailable = false));
  await page.locator('.inspector-body [data-refresh-os-fonts]').click();
  await page.waitForFunction(() =>
    document.querySelector('.text-controls output').textContent.includes('Font unavailable'),
  );
  await page.evaluate((contents) => window.nect.load(contents), saved);
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
  await page.locator('.inspector-body [data-os-text]').fill('missing font draft');
  await page.locator('.inspector-body [data-apply-os-text]').click();
  await page.waitForFunction(() =>
    document.querySelector('.message').textContent.includes('Font unavailable'),
  );
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), saved);
  await page.locator('.inspector-body [data-cancel-os-text]').click();
  await page.locator('.inspector-body [data-bake-text-outline]').click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).type === 'Multi Path',
    text.id,
  );
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  assert.deepEqual(errors, []);
  assert.ok(!(await page.evaluate(() => window.fontCalls)).includes('read_system_font'));
  await mkdir('test-results/desktop-font-text', { recursive: true });
  await page.screenshot({ path: 'test-results/desktop-font-text/converted.png' });
  await writeFile(
    'test-results/desktop-font-text/export.png',
    Buffer.from(saves.find((s) => s.kind === 'png').bytes),
  );
  await writeFile('test-results/desktop-font-text/export.svg', svg);
  const receipt = {
    face: raw.face,
    nativeGeometryFixture: true,
    productionCsp: true,
    imeEvents: true,
    undoRedo: true,
    typographySettings: 6,
    saveReload: true,
    svgPathEquality: true,
    pngSignature: true,
    missingFontPreserved: true,
    pageErrors: errors,
    nativeWebView2: false,
  };
  await writeFile('test-results/desktop-font-text/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
