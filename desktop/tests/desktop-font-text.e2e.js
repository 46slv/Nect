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
const din = outlines.find((r) => r.face.family === 'DIN Alternate');
const axisRows = process.env.NECT_OS_AXES_SMOKE_OUTPUT
  ? JSON.parse(await readFile(process.env.NECT_OS_AXES_SMOKE_OUTPUT, 'utf8'))
  : [];
const verticalRows = process.env.NECT_OS_VERTICAL_SMOKE_OUTPUT
  ? JSON.parse(await readFile(process.env.NECT_OS_VERTICAL_SMOKE_OUTPUT, 'utf8'))
  : [];
const wrapRows = process.env.NECT_OS_WRAP_SMOKE_OUTPUT
  ? JSON.parse(await readFile(process.env.NECT_OS_WRAP_SMOKE_OUTPUT, 'utf8'))
  : [];
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
    ({ variants, din, axisRows, verticalRows, wrapRows }) => {
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
            return window.fontAvailable
              ? [
                  variants[0].face,
                  ...(din ? [din.face] : []),
                  ...(axisRows.length ? [axisRows[0].face] : []),
                ]
              : [];
          if (command === 'list_desktop_font_axes')
            return axisRows.length && JSON.stringify(args.face) === JSON.stringify(axisRows[0].face)
              ? axisRows[0].axes
              : [];
          if (command === 'shape_desktop_text') {
            if (!window.fontAvailable) throw Error('Font unavailable; saved outlines retained');
            const ranges = axisRows.find(
              (r) => JSON.stringify(r.face) === JSON.stringify(args.request.face),
            )?.axes;
            if (
              args.request.axes &&
              ranges &&
              Object.entries(args.request.axes).some(([tag, value]) => {
                const a = ranges.find((a) => a.tag === tag);
                return !a || value < a.min || value > a.max;
              })
            )
              throw Error('Axis outside declared range');
            const r = [
              ...variants,
              ...(din ? [din] : []),
              ...axisRows,
              ...verticalRows,
              ...wrapRows,
            ].find(
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
                JSON.stringify(r.face) === JSON.stringify(args.request.face) &&
                r.writingMode === args.request.writingMode &&
                (r.wrapWidth === undefined
                  ? args.request.wrapWidth === undefined
                  : Math.fround(r.wrapWidth) === Math.fround(args.request.wrapWidth)) &&
                (r.axes === undefined
                  ? args.request.axes === undefined
                  : args.request.axes &&
                    r.axes.length === Object.keys(args.request.axes).length &&
                    r.axes.every(
                      (a) => Math.fround(a.value) === Math.fround(args.request.axes[a.tag]),
                    )),
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
    { variants, din, axisRows, verticalRows, wrapRows },
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
  if (din) {
    const dinDoc = structuredClone(doc),
      target = dinDoc.layers[0].nodes.find((n) => n.id === text.id);
    target.text = din.text;
    target.params.size.value = din.size;
    target.params.lineSpacing.value = din.lineSpacing;
    await page.evaluate(() => (window.fontAvailable = true));
    await page.evaluate((contents) => window.nect.load(contents), serialize(dinDoc));
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    await page.locator('.inspector-body [data-os-text-font]').click();
    const picker = page.getByRole('dialog', { name: 'OS font outlines' });
    await picker.getByRole('combobox').fill('DIN Alternate');
    assert.match(
      await picker.getByRole('option').textContent(),
      new RegExp(`${din.face.name}.*${din.face.weight}`),
    );
    await picker.getByRole('option').click();
    await picker.getByRole('button', { name: 'Use font for Text', exact: true }).click();
    await page.waitForFunction(
      (id) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont?.family ===
        'DIN Alternate',
      text.id,
    );
    const portable = await page.evaluate(() => JSON.stringify(window.nect.document)),
      dinSvg = await page.evaluate(() => window.nect.export());
    await page.evaluate(() => (window.fontAvailable = false));
    await page.evaluate((contents) => window.nect.load(contents), portable);
    assert.equal(await page.evaluate(() => window.nect.export()), dinSvg);
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    await page.locator('.inspector-body [data-bake-text-outline]').click();
    await page.waitForFunction(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).type === 'Multi Path',
      text.id,
    );
    assert.equal(await page.evaluate(() => window.nect.export()), dinSvg);
  }
  const exampleDoc = (outline, position) => {
    const result = structuredClone(doc),
      n = result.layers[0].nodes.find((n) => n.id === text.id);
    n.text = outline.text;
    n.params.size.value = outline.size;
    n.params.letterSpacing.value = outline.tracking;
    n.params.lineSpacing.value = outline.lineSpacing;
    n.kerning = outline.kerning;
    n.alignment = ['start', 'center', 'end'][outline.alignment];
    n.params.position.value = position;
    return result;
  };
  const choose = async (contents, family) => {
    await page.evaluate(() => (window.fontAvailable = true));
    await page.evaluate((contents) => window.nect.load(contents), serialize(contents));
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    await page.locator('.inspector-body [data-os-text-font]').click();
    const picker = page.getByRole('dialog', { name: 'OS font outlines' });
    await picker.getByRole('combobox').fill(family);
    await picker.getByRole('option').click();
    await picker.getByRole('button', { name: 'Use font for Text', exact: true }).click();
    await page.waitForFunction(
      ({ id, family }) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont?.family === family,
      { id: text.id, family },
    );
  };
  const waitOutline = (raw) =>
    page.waitForFunction(
      ({ id, raw }) =>
        JSON.stringify(
          window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont.outline,
        ) === JSON.stringify(raw),
      { id: text.id, raw },
    );
  const cachedRoundtrip = async (name) => {
    const saved = await page.evaluate(() => JSON.stringify(window.nect.document)),
      svg = await page.evaluate(() => window.nect.export());
    const jsonCount = await page.evaluate(
      () => window.fontSaves.filter((s) => s.kind === 'json').length,
    );
    await page.locator('[data-action="save"]').click();
    await page.waitForFunction(
      (count) => window.fontSaves.filter((s) => s.kind === 'json').length > count,
      jsonCount,
    );
    assert.deepEqual(
      JSON.parse(
        await page.evaluate(
          () => window.fontSaves.filter((s) => s.kind === 'json').at(-1).contents,
        ),
      ),
      JSON.parse(saved),
    );
    await page.evaluate(() => (window.fontAvailable = false));
    await page.evaluate((contents) => window.nect.load(contents), saved);
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    await page.locator('.inspector-body [data-bake-text-outline]').click();
    await page.waitForFunction(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).type === 'Multi Path',
      text.id,
    );
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
    await mkdir('test-results/desktop-font-extended', { recursive: true });
    await writeFile(`test-results/desktop-font-extended/${name}.json`, saved);
    await writeFile(`test-results/desktop-font-extended/${name}.svg`, svg);
    const count = await page.evaluate(
      () => window.fontSaves.filter((s) => s.kind === 'png').length,
    );
    await page.locator('[data-action="export-png"]').click();
    await page.waitForFunction(
      (count) => window.fontSaves.filter((s) => s.kind === 'png').length > count,
      count,
    );
    const bytes = await page.evaluate(
      () => window.fontSaves.filter((s) => s.kind === 'png').at(-1).bytes,
    );
    assert.deepEqual(bytes.slice(0, 8), [137, 80, 78, 71, 13, 10, 26, 10]);
    await writeFile(`test-results/desktop-font-extended/${name}.png`, Buffer.from(bytes));
  };
  if (axisRows.length) {
    const base = axisRows[0],
      low = axisRows[1],
      high = axisRows[2];
    await choose(exampleDoc(base, [60, 160]), base.face.family);
    await waitOutline(base);
    const input = () => page.locator('.inspector-body [data-os-axis="wght"]');
    await input().fill(String(low.axes.find((a) => a.tag === 'wght').value));
    await input().dispatchEvent('change');
    await waitOutline(low);
    await page.locator('.canvas-stage').focus();
    await page.keyboard.press('Control+z');
    await waitOutline(base);
    await page.keyboard.press('Control+Shift+z');
    await waitOutline(low);
    await input().fill(String(high.axes.find((a) => a.tag === 'wght').value));
    await input().dispatchEvent('change');
    await waitOutline(high);
    const before = await page.evaluate(() => JSON.stringify(window.nect.document));
    await input().fill(String(high.axes.find((a) => a.tag === 'wght').max + 1));
    await input().dispatchEvent('change');
    await page.waitForFunction(() =>
      document.querySelector('.message').textContent.includes('Axis outside declared range'),
    );
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
    assert.equal(await input().isEnabled(), true);
    await page.locator('.inspector-body [data-reset-os-axes]').click();
    await waitOutline(base);
    const width = axisRows.find(
      (r) =>
        r.axes.some((a) => a.tag === 'wdth' && a.value === a.min) &&
        r.axes.find((a) => a.tag === 'wght').value ===
          base.axes.find((a) => a.tag === 'wght').value,
    );
    if (width) {
      const el = page.locator('.inspector-body [data-os-axis="wdth"]');
      await el.fill(String(width.axes.find((a) => a.tag === 'wdth').value));
      await el.dispatchEvent('change');
      await waitOutline(width);
    }
    await cachedRoundtrip('windows-variable-axes');
  }
  if (verticalRows.length) {
    const horizontal = verticalRows.find((r) => r.size === 32 && r.engine === 'directwrite-v1'),
      vertical = verticalRows.find(
        (r) => r.size === 32 && r.writingMode === 'vertical-rl' && r.tracking === 0,
      ),
      track = verticalRows.find(
        (r) =>
          r.size === 32 &&
          r.writingMode === 'vertical-rl' &&
          r.tracking === 4 &&
          r.lineSpacing === 1.2,
      ),
      spaced = verticalRows.find(
        (r) =>
          r.size === 32 &&
          r.writingMode === 'vertical-rl' &&
          r.tracking === 4 &&
          r.lineSpacing === 2,
      );
    await choose(exampleDoc(horizontal, [720, 40]), horizontal.face.family);
    await waitOutline(horizontal);
    const mode = () => page.locator('.inspector-body [data-os-setting="writingMode"]');
    await mode().selectOption('vertical-rl');
    await waitOutline(vertical);
    await page.locator('.canvas-stage').focus();
    await page.keyboard.press('Control+z');
    await waitOutline(horizontal);
    await page.keyboard.press('Control+Shift+z');
    await waitOutline(vertical);
    for (const [key, value, raw] of [
      ['tracking', 4, track],
      ['lineSpacing', 2, spaced],
    ]) {
      const el = page.locator(`.inspector-body [data-os-setting="${key}"]`);
      await el.fill(String(value));
      await el.dispatchEvent('change');
      await waitOutline(raw);
    }
    await cachedRoundtrip('windows-japanese-vertical');
  }
  if (wrapRows.length) {
    const [unwrapped, wide, narrow, centered, ended, spaced, tracked] = wrapRows,
      initial = wrapRows.find((r) => r.wrapWidth === 320),
      off = wrapRows.find((r) => r.engine === 'directwrite-v2');
    await choose(exampleDoc(unwrapped, [160, 32]), unwrapped.face.family);
    await waitOutline(unwrapped);
    await page.locator('.inspector-body [data-os-wrap]').selectOption('width');
    await waitOutline(initial);
    const fractional = wrapRows.find(
      (r) => Math.fround(r.wrapWidth) === Math.fround(160.123456789),
    );
    const fractionalInput = page.locator('.inspector-body [data-os-wrap-width]');
    await fractionalInput.click();
    await fractionalInput.fill('160.123456789');
    await fractionalInput.dispatchEvent('change');
    await waitOutline(fractional);
    for (const row of [wide, narrow]) {
      const el = page.locator('.inspector-body [data-os-wrap-width]');
      await el.click();
      await el.fill(String(row.wrapWidth));
      await el.dispatchEvent('change');
      await waitOutline(row);
    }
    for (const row of [centered, ended, narrow]) {
      await page
        .locator('.inspector-body [data-os-setting="alignment"]')
        .selectOption(['start', 'center', 'end'][row.alignment]);
      await waitOutline(row);
    }
    for (const [key, value, row] of [
      ['lineSpacing', 2, spaced],
      ['tracking', 2, tracked],
    ]) {
      const el = page.locator(`.inspector-body [data-os-setting="${key}"]`);
      await el.click();
      await el.fill(String(value));
      await el.dispatchEvent('change');
      await waitOutline(row);
      await page.locator('[data-action="undo"]').click();
      await waitOutline(narrow);
    }
    await page.locator('.inspector-body [data-os-wrap]').selectOption('off');
    await waitOutline(off);
    await page.locator('[data-action="undo"]').click();
    await waitOutline(narrow);
    await page.waitForFunction(
      () =>
        document.querySelector(
          '.inspector-body [data-os-setting="writingMode"] option[value="vertical-rl"]',
        )?.disabled,
      {},
      { timeout: 2000 },
    );
    assert.equal(
      await page
        .locator('.inspector-body [data-os-setting="writingMode"] option[value="vertical-rl"]')
        .evaluate((option) => option.disabled && option.matches(':disabled')),
      true,
    );
    for (const row of wrapRows.filter(
      (r) =>
        r.engine === 'directwrite-v3' &&
        r.wrapWidth === 96 &&
        (r.text.endsWith('追加') || r.text === ''),
    )) {
      const before = await page.evaluate(() => JSON.stringify(window.nect.document));
      await page.locator('.inspector-body [data-os-text]').fill(row.text);
      await page.locator('.inspector-body [data-apply-os-text]').click();
      await waitOutline(row);
      assert.deepEqual(
        await page.evaluate(
          (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).textWrap,
          text.id,
        ),
        { width: 96 },
      );
      await page.locator('[data-action="undo"]').click();
      await waitOutline(narrow);
      assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
    }
    const saved = await page.evaluate(() => JSON.stringify(window.nect.document)),
      width = page.locator('.inspector-body [data-os-wrap-width]');
    await width.click();
    await width.fill('0');
    await width.dispatchEvent('change');
    await page.waitForFunction(() =>
      document.querySelector('.status').textContent.includes('Wrap width'),
    );
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), saved);
    await cachedRoundtrip('windows-os-wrap');
  }
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
    dinExactFaceMultiline: !!din,
    osVariableAxes: !!axisRows.length,
    japaneseVertical: !!verticalRows.length,
    osHorizontalWrap: !!wrapRows.length,
    pageErrors: errors,
    nativeWebView2: false,
  };
  await writeFile('test-results/desktop-font-text/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
