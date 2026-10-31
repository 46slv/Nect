// Production Edge/CSP UI + explicit IPC fixture from separately tested Windows
// DirectWrite. This does not claim native WebView2/IME acceptance.
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, serialize, CommandSession } from '../src/core.js';
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
const jpRows = process.env.NECT_JP_WRAP_SMOKE_OUTPUT
  ? JSON.parse(await readFile(process.env.NECT_JP_WRAP_SMOKE_OUTPUT, 'utf8'))
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
text.params.letterSpacing.value = raw.tracking;
text.kerning = raw.kerning;
text.alignment = ['start', 'center', 'end'][raw.alignment];
text.params.position.value = [480, 160];
fill.params.path.source = text.id;
layer.nodes.push(text, fill);
layer.nodes.find((n) => n.id === layer.output).params.path.source = fill.id;
// Captured native fixtures may intentionally have ligatures enabled. A normal
// static Font choice now preserves its effective legacy liga-off setting, so a
// liga-on capture must start as saved native Text with that existing intent.
// Never relabel the fixture or alter its geometry/settings to make it match.
const withCapturedNativeIntent = (contents, outline) => {
  if (outline.ligatures !== true) return contents;
  const session = new CommandSession(contents);
  session.dispatch({
    ...session.state,
    type: 'set-os-font-text',
    payload: {
      layerId: layer.id,
      nodeId: text.id,
      outline,
      settings: { ligatures: true },
    },
  });
  return session.snapshot();
};
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
  const openDetails = async (selector) => {
    const details = page.locator(`.inspector-body ${selector}`);
    if (!(await details.evaluate((el) => el.open)))
      await details.locator(':scope > summary').click();
  };
  const bakeCurrentAppearance = async () => {
    await openDetails('.text-actions');
    await page.locator('.inspector-body .text-actions [data-bake-text-outline]').click();
  };
  const nativeOption = (picker, face) => {
    const id = JSON.stringify([
      'windows',
      JSON.stringify(
        ['family', 'name', 'weight', 'style', 'stretch', 'format', 'color'].map((key) => face[key]),
      ),
    ]);
    return picker.locator(`.picker-option[data-id=${JSON.stringify(id)}]`);
  };
  const openNativePicker = async (face) => {
    // Discovery is cached independently of document loads. Restoring availability
    // after the missing-font cases requires the actual Advanced refresh action.
    const reads = await page.evaluate(
      () => window.fontCalls.filter((command) => command === 'list_desktop_font_faces').length,
    );
    await openDetails('.text-font-advanced');
    await page.locator('.inspector-body .text-font-advanced [data-refresh-os-fonts]').click();
    await page.waitForFunction(
      (count) =>
        window.fontCalls.filter((command) => command === 'list_desktop_font_faces').length > count,
      reads,
    );
    await page.waitForFunction(
      () =>
        document.querySelector('.inspector-body .text-font-choice output')?.textContent !==
        'Checking font…',
    );
    await page.locator('.inspector-body [data-change-font]').click();
    const picker = page.getByRole('dialog', { name: 'Font', exact: true });
    await picker.getByRole('combobox').fill(`${face.family} ${face.name}`);
    assert.equal(await nativeOption(picker, face).count(), 1, 'one exact native face identity');
    return picker;
  };

  await page.addInitScript(
    ({ variants, din, axisRows, verticalRows, wrapRows, jpRows }) => {
      window.isTauri = true;
      window.fontCalls = [];
      window.fontRequests = [];
      window.fontSaves = [];
      window.fontAvailable = true;
      window.__TAURI_INTERNALS__ = {
        transformCallback: () => 1,
        unregisterCallback: () => {},
        invoke: async (command, args) => {
          window.fontCalls.push(command);
          if (command === 'list_system_fonts' || command === 'list_os_outline_fonts') return [];
          if (command === 'list_desktop_font_faces') {
            const faces = [
              ...variants,
              ...(din ? [din] : []),
              ...axisRows,
              ...verticalRows,
              ...wrapRows,
              ...jpRows,
            ].map((row) => row.face);
            return window.fontAvailable
              ? [...new Map(faces.map((face) => [JSON.stringify(face), face])).values()]
              : [];
          }
          if (command === 'list_desktop_font_axes')
            return axisRows.length && JSON.stringify(args.face) === JSON.stringify(axisRows[0].face)
              ? axisRows[0].axes
              : [];
          if (command === 'shape_desktop_text') {
            window.fontRequests.push(structuredClone(args.request));
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
              ...jpRows,
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
                r.wrapPolicy === args.request.wrapPolicy &&
                (r.wrapHeight === undefined
                  ? args.request.wrapHeight === undefined
                  : Math.fround(r.wrapHeight) === Math.fround(args.request.wrapHeight)) &&
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
    { variants, din, axisRows, verticalRows, wrapRows, jpRows },
  );
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(() => window.nect);
  await page.evaluate(
    (contents) => window.nect.load(contents),
    serialize(withCapturedNativeIntent(doc, raw)),
  );
  await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
  assert.equal(await page.locator('.inspector-body [data-change-font]').count(), 1);
  assert.equal(await page.locator('[data-os-text-font], [data-os-text-outline]').count(), 0);
  const dialog = await openNativePicker(raw.face);
  await nativeOption(dialog, raw.face).click();
  await dialog.getByRole('button', { name: 'Apply font', exact: true }).click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont?.version === 2,
    text.id,
  );
  const live = () =>
    page.evaluate((id) => window.nect.document.layers[0].nodes.find((n) => n.id === id), text.id);
  await page.waitForFunction(() => !document.querySelector('dialog[aria-label="Font"]'));
  assert.deepEqual((await live()).osFont.face, raw.face);
  assert.equal((await live()).osFont.outline.ligatures, raw.ligatures);
  assert.equal(await page.evaluate(() => window.fontRequests.at(-1).ligatures), raw.ligatures);
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
  await openDetails('.text-font-advanced');
  await page.locator('.inspector-body .text-font-advanced [data-refresh-os-fonts]').click();
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
  await bakeCurrentAppearance();
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
    target.params.letterSpacing.value = din.tracking;
    target.kerning = din.kerning;
    target.alignment = ['start', 'center', 'end'][din.alignment];
    await page.evaluate(() => (window.fontAvailable = true));
    await page.evaluate(
      (contents) => window.nect.load(contents),
      serialize(withCapturedNativeIntent(dinDoc, din)),
    );
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    const picker = await openNativePicker(din.face);
    assert.match(
      await nativeOption(picker, din.face).textContent(),
      new RegExp(`${din.face.name}.*${din.face.weight}`),
    );
    await nativeOption(picker, din.face).click();
    await picker.getByRole('button', { name: 'Apply font', exact: true }).click();
    await page.waitForFunction(
      (id) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).osFont?.family ===
        'DIN Alternate',
      text.id,
    );
    await page.waitForFunction(() => !document.querySelector('dialog[aria-label="Font"]'));
    assert.deepEqual((await live()).osFont.face, din.face);
    const portable = await page.evaluate(() => JSON.stringify(window.nect.document)),
      dinSvg = await page.evaluate(() => window.nect.export());
    await page.evaluate(() => (window.fontAvailable = false));
    await page.evaluate((contents) => window.nect.load(contents), portable);
    assert.equal(await page.evaluate(() => window.nect.export()), dinSvg);
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    await bakeCurrentAppearance();
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
  const choose = async (contents, outline) => {
    await page.evaluate(() => (window.fontAvailable = true));
    await page.evaluate(
      (contents) => window.nect.load(contents),
      serialize(withCapturedNativeIntent(contents, outline)),
    );
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    const picker = await openNativePicker(outline.face);
    await nativeOption(picker, outline.face).click();
    await picker.getByRole('button', { name: 'Apply font', exact: true }).click();
    await page.waitForFunction(() => !document.querySelector('dialog[aria-label="Font"]'));
    assert.deepEqual((await live()).osFont.face, outline.face);
    assert.equal((await live()).osFont.outline.ligatures, outline.ligatures);
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
    await bakeCurrentAppearance();
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
    await choose(exampleDoc(base, [60, 160]), base);
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
    await choose(exampleDoc(horizontal, [720, 40]), horizontal);
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
    await choose(exampleDoc(unwrapped, [160, 32]), unwrapped);
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
        )?.disabled === false,
      {},
      { timeout: 2000 },
    );
    assert.equal(
      await page
        .locator('.inspector-body [data-os-setting="writingMode"] option[value="vertical-rl"]')
        .evaluate((option) => option.disabled || option.matches(':disabled')),
      false,
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
      document.querySelector('.status').textContent.includes('Wrap'),
    );
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), saved);
    await cachedRoundtrip('windows-os-wrap');
  }
  if (jpRows.length) {
    const loadPrepared = async (raw) => {
      const contents = exampleDoc(raw, [600, 100]),
        session = new CommandSession(contents),
        textWrap = {
          [raw.writingMode === 'vertical-rl' ? 'height' : 'width']: raw.wrapWidth ?? raw.wrapHeight,
          policy: raw.wrapPolicy,
        };
      session.dispatch({
        ...session.state,
        type: 'set-os-font-text',
        payload: {
          layerId: layer.id,
          nodeId: text.id,
          outline: raw,
          settings: { writingMode: raw.writingMode, textWrap },
        },
      });
      await page.evaluate(() => (window.fontAvailable = true));
      await page.evaluate((json) => window.nect.load(json), serialize(session.snapshot()));
      await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
      await waitOutline(raw);
    };
    for (const mode of ['horizontal-tb', 'vertical-rl']) {
      const rows = jpRows.filter((r) => r.writingMode === mode),
        wide = rows[0],
        narrow = rows[1];
      await loadPrepared(wide);
      assert.equal(await page.locator('.inspector-body [data-os-wrap]').inputValue(), 'basic');
      assert.ok(
        await page
          .locator('.inspector-body [data-os-setting="writingMode"] option[value="vertical-rl"]')
          .isEnabled(),
      );
      const extent = page.locator('.inspector-body [data-os-wrap-width]');
      await extent.fill('96');
      await extent.dispatchEvent('change');
      await waitOutline(narrow);
      await page.locator('[data-action="undo"]').click();
      await waitOutline(wide);
      await page.locator('[data-action="redo"]').click();
      await waitOutline(narrow);
      for (const [key, value, row] of [
        ['tracking', '2', rows[5]],
        ['lineSpacing', '2', rows[6]],
        ['alignment', 'end', rows[3]],
      ]) {
        const el = page.locator(`.inspector-body [data-os-setting="${key}"]`);
        if (key === 'alignment') await el.selectOption(value);
        else {
          await el.fill(value);
          await el.dispatchEvent('change');
        }
        await waitOutline(row);
        await page.locator('[data-action="undo"]').click();
        await waitOutline(narrow);
      }
      const other = mode === 'horizontal-tb' ? 'vertical-rl' : 'horizontal-tb',
        opposite = jpRows.filter((r) => r.writingMode === other)[1];
      await page.locator('.inspector-body [data-os-setting="writingMode"]').selectOption(other);
      await waitOutline(opposite);
      await page.locator('[data-action="undo"]').click();
      await waitOutline(narrow);
      await page.screenshot({
        path: `test-results/desktop-font-extended/japanese-basic-${mode}.png`,
      });
      await cachedRoundtrip(`windows-japanese-basic-${mode}`);
    }
    // Static Text uses the actual production controls and common Session too.
    const contents = exampleDoc(jpRows[0], [600, 100]),
      n = contents.layers[0].nodes.find((n) => n.id === text.id);
    n.text = 'Alpha beta gamma delta';
    await page.evaluate((json) => window.nect.load(json), serialize(contents));
    await page.locator(`.graph .node[data-id="${text.id}"] .node-head .type-tag`).click();
    await page.locator('.inspector-body [data-text-wrap]').selectOption('basic');
    await page.locator('.inspector-body [data-text-wrap-width]').fill('120');
    await page.locator('.inspector-body [data-text-wrap-width]').dispatchEvent('change');
    await page.locator('.inspector-body [data-writing-mode]').selectOption('vertical-rl');
    const staticState = await page.evaluate(
      (id) => ({
        n: window.nect.document.layers[0].nodes.find((n) => n.id === id),
        lines: window.nect.evaluate().textLayout(id).lines,
      }),
      text.id,
    );
    assert.deepEqual(staticState.n.textWrap, { height: 120, policy: 'japanese-basic-v1' });
    assert.ok(staticState.lines.length > 1);
    assert.equal(staticState.n.writingMode, 'vertical-rl');
    const before = await page.evaluate(() => JSON.stringify(window.nect.document));
    await page.locator('.inspector-body [data-text-wrap-width]').fill('900');
    await page.locator('.inspector-body [data-text-wrap-width]').press('Escape');
    await page.locator('.inspector-body [data-text-wrap-width]').press('Tab');
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
    await cachedRoundtrip('static-vertical-wrap');
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
    japaneseBasicWrap: !!jpRows.length,
    staticVerticalWrap: !!jpRows.length,
    pageErrors: errors,
    nativeWebView2: false,
  };
  await writeFile('test-results/desktop-font-text/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
