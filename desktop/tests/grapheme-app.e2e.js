import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect, serialize } from '../src/core.js';
import { graphemeSamples, scalarSamples, expectedBoundaries } from './fixtures/grapheme-editing.js';

const root = resolve('dist');
const config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const path = resolve(
    root,
    '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
  );
  if (!path.startsWith(root + sep)) return res.writeHead(403).end();
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
await new Promise((done) => server.listen(0, '127.0.0.1', done));
let browser;
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [],
    requests = [];
  page.on('pageerror', (error) => errors.push(error.message));
  page.on('request', (request) => {
    if (!/^(http:\/\/127\.0\.0\.1:|data:|blob:)/.test(request.url())) requests.push(request.url());
  });
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
  let editor;
  const range = () => editor.evaluate((el) => [el.selectionStart, el.selectionEnd]);
  const setRange = (start, end = start) =>
    editor.evaluate((el, [start, end]) => el.setSelectionRange(start, end), [start, end]);
  const rows = [];
  async function load(raw) {
    const doc = blankDocument(),
      layer = doc.layers[0],
      text = node('Text', 360, 50),
      fill = node('Fill', 720, 50);
    text.text = raw;
    text.params.size.value = 64;
    text.params.position.value = [150, 180];
    layer.nodes.push(text, fill);
    connect(layer, text.id, fill.id, 'path');
    connect(layer, fill.id, layer.output, 'path');
    await page.evaluate((saved) => window.nect.load(saved), serialize(doc));
    return text.id;
  }
  async function glyphPoint(target = page) {
    const point = await target.evaluate(() => {
      const shape = document.querySelector('.artboard .shape');
      const path = shape.matches('path') ? shape : shape.querySelector('path');
      const box = path.getBBox(),
        matrix = path.getScreenCTM();
      for (let y = box.y + 1; y < box.y + box.height; y += 2)
        for (let x = box.x + 1; x < box.x + box.width; x += 2)
          if (path.isPointInFill(new DOMPoint(x, y))) {
            const p = new DOMPoint(x, y).matrixTransform(matrix);
            return [p.x, p.y];
          }
    });
    assert.ok(point, 'fixture has a visible glyph');
    return point;
  }
  async function open() {
    const point = await glyphPoint();
    await page.mouse.dblclick(...point);
    await editor.waitFor();
  }
  for (const sample of [
    ...graphemeSamples,
    ...scalarSamples,
    { name: 'NFC composed accent', text: 'e\u0301', boundaries: [0, 2] },
  ]) {
    const raw = `A${sample.text}B`,
      end = raw.length - 1;
    const id = await load(raw),
      before = await snapshot();
    const canvas = await page.evaluate(() => !window.nect.evaluate().error);
    if (canvas) {
      editor = page.locator('.canvas-text-editor');
      await page.locator('.artboard .shape').waitFor();
      await open();
    } else {
      await page.locator('[data-action="graph-fit"]').click();
      await page.locator(`.node[data-id="${id}"] .type-tag`).click();
      editor = page.locator(`.graph textarea[data-text="${id}"]`);
    }
    await editor.fill(raw);
    const boundaries = expectedBoundaries(sample);
    for (const [key, start, expected] of [
      ['ArrowLeft', end, 1 + sample.boundaries.at(-2)],
      ['ArrowRight', 1, 1 + sample.boundaries[1]],
      ['Shift+ArrowLeft', end, 1 + sample.boundaries.at(-2)],
      ['Shift+ArrowRight', 1, 1 + sample.boundaries[1]],
    ]) {
      await setRange(start);
      await editor.press(key);
      await page.waitForTimeout(10);
      assert.deepEqual(
        await range(),
        key.startsWith('Shift')
          ? [Math.min(start, expected), Math.max(start, expected)]
          : [expected, expected],
        `${sample.name} ${key}`,
      );
    }
    if (sample.name === 'Indic conjunct ZWJ') {
      await setRange(end);
      await page.keyboard.down('ArrowLeft');
      await page.waitForTimeout(25);
      assert.deepEqual(await range(), [1, 1]);
      await page.keyboard.down('ArrowLeft');
      await page.waitForTimeout(25);
      assert.deepEqual(await range(), [0, 0], 'held key repeats start from corrected boundary');
      await page.keyboard.up('ArrowLeft');
    }
    // Actual pointer placement/Shift selection only returns legal grapheme endpoints.
    if (canvas) {
      const box = await editor.boundingBox();
      for (const fraction of [0.15, 0.35, 0.55, 0.75]) {
        await page.mouse.click(box.x + box.width * fraction, box.y + box.height / 2);
        assert.ok(
          (await range()).every((offset) => boundaries.includes(offset)),
          sample.name,
        );
      }
      await page.keyboard.down('Shift');
      await page.mouse.click(box.x + box.width * 0.9, box.y + box.height / 2);
      await page.keyboard.up('Shift');
      assert.ok((await range()).every((offset) => boundaries.includes(offset)));
    }
    await setRange(end);
    await editor.press('Backspace');
    const expected = raw.slice(0, 1 + sample.boundaries.at(-2)) + raw.slice(end);
    assert.equal(await editor.inputValue(), expected, `${sample.name} Backspace`);
    await editor.press('Control+z');
    assert.equal(await editor.inputValue(), raw, `${sample.name} native Undo`);
    await editor.press('Control+Shift+z');
    assert.equal(await editor.inputValue(), expected, `${sample.name} native Redo`);
    await editor.press('Control+z');
    await setRange(1);
    await editor.press('Delete');
    assert.equal(
      await editor.inputValue(),
      raw.slice(0, 1) + raw.slice(1 + sample.boundaries[1]),
      `${sample.name} Delete`,
    );
    await editor.press('Control+z');
    assert.equal(await editor.inputValue(), raw);
    assert.equal(await snapshot(), before, 'draft edits stay outside saved authoring');
    // Composition bypass is synthetic; physical IME acceptance remains separate.
    await setRange(Math.min(2, end));
    await editor.dispatchEvent('compositionstart');
    const during = await range();
    assert.equal(
      await editor.evaluate((el) => {
        const event = new InputEvent('beforeinput', {
          inputType: 'deleteContentBackward',
          isComposing: true,
          bubbles: true,
          cancelable: true,
        });
        el.dispatchEvent(event);
        return event.defaultPrevented;
      }),
      false,
    );
    assert.deepEqual(await range(), during);
    await editor.dispatchEvent('compositionend');
    await setRange(1, end);
    await editor.press('Backspace');
    assert.equal(await editor.inputValue(), 'AB');
    await editor.press('Control+Enter');
    if (canvas) await editor.waitFor({ state: 'detached' });
    const after = await snapshot();
    assert.equal(
      await page.evaluate(
        (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).text,
        id,
      ),
      'AB',
    );
    await page.locator('[data-action="undo"]').click();
    assert.equal(await snapshot(), before, `${sample.name} Session Undo`);
    await page.locator('[data-action="redo"]').click();
    assert.equal(await snapshot(), after, `${sample.name} Session Redo`);
    await page.evaluate((saved) => window.nect.load(saved), after);
    assert.equal(await snapshot(), after, `${sample.name} JSON reopen`);
    rows.push({
      sample: sample.name,
      surface: canvas ? 'Canvas' : 'Graph',
      layoutUnsupported: !canvas,
    });
  }
  await mkdir('test-results/grapheme-app', { recursive: true });
  const saved = await snapshot();
  const downloadEvent = page.waitForEvent('download');
  await page.locator('[data-action="save"]').click();
  await (await downloadEvent).saveAs('test-results/grapheme-app/saved.json');
  assert.equal(
    JSON.stringify(JSON.parse(await readFile('test-results/grapheme-app/saved.json', 'utf8'))),
    saved,
  );
  await load('Another document');
  const chooserEvent = page.waitForEvent('filechooser');
  await page.locator('[data-action="open"]').click();
  await page.getByRole('button', { name: 'Discard', exact: true }).click();
  await (await chooserEvent).setFiles('test-results/grapheme-app/saved.json');
  await page.waitForFunction((saved) => JSON.stringify(window.nect.document) === saved, saved);
  // Same correction applies to the visible Graph Text control, with native Ctrl+A.
  const graphId = await load('Ax\u0301B');
  await page.locator('[data-action="graph-fit"]').click();
  await page.locator(`.node[data-id="${graphId}"] .type-tag`).click();
  const graphInput = page.locator(`.graph textarea[data-text="${graphId}"]`);
  await graphInput.focus();
  await graphInput.evaluate((el) => el.setSelectionRange(3, 3));
  await graphInput.press('Backspace');
  assert.equal(await graphInput.inputValue(), 'AB');
  await graphInput.press('Control+z');
  assert.equal(await graphInput.inputValue(), 'Ax\u0301B');
  await graphInput.press('Control+a');
  assert.deepEqual(await graphInput.evaluate((el) => [el.selectionStart, el.selectionEnd]), [0, 4]);
  assert.equal(
    await page.locator('.graph .node.selected').count(),
    1,
    'Text Ctrl+A does not select graph nodes',
  );
  await graphInput.press('Escape');
  // Unsupported runtimes preserve the document and refuse native text changes.
  const gatedId = await load('Nect');
  const gatedDoc = await snapshot();
  const gated = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  gated.on('pageerror', (error) => errors.push(error.message));
  await gated.addInitScript(() => {
    Intl.Segmenter = undefined;
  });
  await gated.goto(`http://127.0.0.1:${server.address().port}`);
  await gated.waitForFunction(() => window.nect);
  await gated.evaluate((saved) => window.nect.load(saved), gatedDoc);
  await gated.locator('.artboard .shape').waitFor();
  await gated.mouse.dblclick(...(await glyphPoint(gated)));
  assert.equal(await gated.locator('.canvas-text-editor').count(), 0);
  assert.match(await gated.locator('.status').innerText(), /grapheme segmentation support/);
  await gated.locator('[data-action="graph-fit"]').click();
  await gated.locator(`.node[data-id="${gatedId}"] .type-tag`).click();
  const gatedInput = gated.locator(`.graph textarea[data-text="${gatedId}"]`);
  await gatedInput.focus();
  await gatedInput.press('End');
  await gatedInput.press('Backspace');
  assert.equal(await gatedInput.inputValue(), 'Nect');
  assert.equal(await gated.evaluate(() => JSON.stringify(window.nect.document)), gatedDoc);
  await gated.close();
  assert.deepEqual(errors, []);
  assert.deepEqual(requests, []);
  await mkdir('test-results/grapheme-app', { recursive: true });
  await page.screenshot({ path: 'test-results/grapheme-app/result.png' });
  await writeFile(
    'test-results/grapheme-app/receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        samples: rows,
        actualPointerAndShift: true,
        actualKeyboardArrowsAndRepeat: true,
        nativeDeletionUndoRedo: true,
        draftCommitSessionUndoRedo: true,
        savedJsonReopen: true,
        actualBrowserSaveOpen: true,
        graphNativeSelectAll: true,
        missingSegmenterRefusesEdits: true,
        syntheticImeBypass: true,
        physicalIme: false,
        pageErrors: errors,
        externalRequests: requests,
      },
      null,
      2,
    ),
  );
  console.log(
    'Grapheme application production-CSP pointer, keyboard, native history, draft commit and saved JSON passed for 14 fixtures',
  );
} finally {
  await browser?.close();
  await new Promise((done) => server.close(done));
}
