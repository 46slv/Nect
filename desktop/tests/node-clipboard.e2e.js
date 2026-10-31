import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, node, connect, serialize, exportSVG } from '../src/core.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';

const doc = blankDocument(),
  src = doc.layers[0],
  dst = blankLayer('Paste destination'),
  svg = blankLayer('SVG source');
doc.schemaVersion = 35;
doc.width = doc.height = 160;
doc.layers.push(dst, svg);
svg.visible = false;
const image = node('Image', 30, 40),
  key = node('Keyer', 360, 40),
  shape = node('Ellipse', 30, 540),
  mask = node('Mask', 700, 40),
  text = node('Text', 360, 540),
  ref = node('Composition Reference', 700, 540);
image.asset = 'image-synthetic';
image.params.position.value = [0, 0];
image.params.size.value = [160, 160];
shape.params.center.value = [80, 80];
shape.params.size.value = [100, 100];
text.text = 'Copied text';
ref.compositionReference = { compositionId: svg.id };
doc.imageAssets = {
  'image-synthetic': {
    name: 'synthetic.png',
    mime: 'image/png',
    width: 1,
    height: 1,
    data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 128]))),
  },
};
src.nodes.push(image, key, shape, mask, text, ref);
connect(src, image.id, key.id, 'path');
connect(src, key.id, mask.id, 'path');
connect(src, shape.id, mask.id, 'mask');
const rect = node('Rectangle'),
  fill = node('Fill');
rect.params.center.value = [80, 80];
rect.params.size.value = [100, 100];
fill.params.color.value = { hex: '#0055ff', alpha: 0.5 };
svg.nodes.push(rect, fill);
connect(svg, rect.id, fill.id, 'path');
connect(svg, fill.id, svg.output, 'path');
const root = resolve('dist'),
  config = JSON.parse(await readFile('src-tauri/tauri.conf.json', 'utf8'));
const server = createServer(async (req, res) => {
  const p = resolve(
    root,
    '.' + new URL(req.url === '/' ? '/index.html' : req.url, 'http://local').pathname,
  );
  if (!p.startsWith(root + sep)) return res.writeHead(403).end();
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
  const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } }),
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  await activateBoundaryCommand(page, 'graph-fit');
  const ids = [image.id, key.id, shape.id, mask.id, text.id, ref.id];
  const selectedIds = () =>
    page
      .locator('.graph .node.selected')
      .evaluateAll((nodes) => nodes.map((n) => n.dataset.id).sort());
  await page.locator(`.graph .node[data-id="${image.id}"] .node-head .type-tag`).click();
  // Native controls keep Ctrl+A even when nested inside the graph.
  for (const tag of ['input', 'textarea', 'div']) {
    await page.locator('.graph-scroll').evaluate((graph, tag) => {
      const el = document.createElement(tag);
      el.id = 'native-select-all-probe';
      if (tag === 'div') {
        el.contentEditable = 'true';
        el.innerHTML = '<span>Native text selection</span>';
      } else el.value = 'Native text selection';
      graph.append(el);
      el.focus();
    }, tag);
    await page.keyboard.press('Control+a');
    assert.equal(
      await page
        .locator('#native-select-all-probe')
        .evaluate((el) =>
          el.isContentEditable
            ? window.getSelection().toString()
            : el.value.slice(el.selectionStart, el.selectionEnd),
        ),
      'Native text selection',
    );
    assert.deepEqual(await selectedIds(), [image.id]);
    await page.locator('#native-select-all-probe').evaluate((el) => el.remove());
  }
  await page.evaluate(() => window.getSelection().removeAllRanges());
  for (const options of [
    { isComposing: true },
    { keyCode: 229 },
    { shiftKey: true },
    { altKey: true },
  ]) {
    assert.equal(
      await page.locator('.graph-scroll').evaluate((el, options) => {
        const event = new KeyboardEvent('keydown', {
          key: 'a',
          ctrlKey: true,
          bubbles: true,
          cancelable: true,
          ...options,
        });
        el.dispatchEvent(event);
        return event.defaultPrevented;
      }, options),
      false,
    );
    assert.deepEqual(await selectedIds(), [image.id]);
  }
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('Control+a');
  assert.deepEqual(await selectedIds(), [image.id], 'Canvas does not select graph nodes');
  const preSelection = await page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    revision: window.nect.commandState.expectedRevision,
  }));
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+a');
  assert.deepEqual(
    await selectedIds(),
    [...ids, src.output].sort(),
    'current Composition includes Output',
  );
  assert.deepEqual(
    await page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      revision: window.nect.commandState.expectedRevision,
    })),
    preSelection,
    'selection does not edit the document',
  );
  const before = await page.evaluate(() => ({
    doc: JSON.stringify(window.nect.document),
    revision: window.nect.commandState.expectedRevision,
  }));
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+c');
  assert.match(await page.locator('.status').innerText(), /6 nodes copied.*1 Output skipped/);
  assert.deepEqual(
    await page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      revision: window.nect.commandState.expectedRevision,
    })),
    before,
  );
  await page.locator(`[data-select-layer="${dst.id}"]`).click();
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+v');
  const pasted = await page.evaluate(
    (id) => window.nect.document.layers.find((l) => l.id === id),
    dst.id,
  );
  assert.equal(pasted.nodes.length, 7);
  assert.equal(pasted.output, dst.output);
  const copies = pasted.nodes.filter((n) => n.id !== dst.output),
    copy = (type) => copies.find((n) => n.type === type);
  assert.equal(copy('Keyer').params.path.source, copy('Image').id);
  assert.equal(copy('Mask').params.path.source, copy('Keyer').id);
  assert.equal(copy('Mask').params.mask.source, copy('Ellipse').id);
  assert.equal(copy('Image').asset, image.asset);
  assert.equal(copy('Text').text, text.text);
  assert.equal(copy('Composition Reference').compositionReference.compositionId, svg.id);
  const after = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.keyboard.press('Control+z');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before.doc);
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+Shift+z');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  await activateBoundaryCommand(page, 'graph-fit');
  await page.locator(`.graph .node[data-id="${copy('Mask').id}"] .node-head .type-tag`).click();
  await page.locator(`.graph [data-preview="${copy('Mask').id}"]`).click();
  const exported = await page.evaluate(() => window.nect.export());
  assert.match(exported, /<mask /);
  assert.match(exported, /<image /);
  const input = page.locator('.inspector-body input').first();
  assert.equal(
    await input.evaluate((el) => {
      const e = new KeyboardEvent('keydown', {
        key: 'v',
        ctrlKey: true,
        bubbles: true,
        cancelable: true,
      });
      el.dispatchEvent(e);
      return e.defaultPrevented;
    }),
    false,
  );
  assert.equal(
    await page.locator('.graph-scroll').evaluate((el) => {
      const e = new KeyboardEvent('keydown', {
        key: 'v',
        ctrlKey: true,
        isComposing: true,
        bubbles: true,
        cancelable: true,
      });
      el.dispatchEvent(e);
      return e.defaultPrevented;
    }),
    false,
  );
  await page.evaluate(
    (id) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-layer-state',
        payload: { layerId: id, key: 'locked', value: true },
      }),
    dst.id,
  );
  const locked = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+v');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), locked);
  assert.match(await page.locator('.status').innerText(), /locked/);
  await page.evaluate(
    (id) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-layer-state',
        payload: { layerId: id, key: 'locked', value: false },
      }),
    dst.id,
  );
  // A selected input's external Image dependency must not jump to another layer.
  await page.locator(`[data-select-layer="${src.id}"]`).click();
  await page.locator(`[data-select-layer="${dst.id}"]`).click();
  await activateBoundaryCommand(page, 'graph-fit');
  await page.locator(`.graph .node[data-id="${copy('Keyer').id}"] .node-head .type-tag`).click();
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+c');
  assert.match(await page.locator('.status').innerText(), /1 nodes copied.*1 external input/);
  await page.locator(`[data-select-layer="${src.id}"]`).click();
  await page.locator('.graph-scroll').focus();
  const ext = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.keyboard.press('Control+v');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), ext);
  assert.match(await page.locator('.status').innerText(), /Copy those sources too/);
  const saved = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(await page.evaluate(() => window.nect.export()), exported);
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+v');
  assert.match(await page.locator('.status').innerText(), /clipboard is empty/);
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), saved);
  assert.equal(exportSVG(JSON.parse(saved)), exported);
  assert.deepEqual(errors, []);
  await mkdir('test-results/node-clipboard', { recursive: true });
  await writeFile('test-results/node-clipboard/example.json', saved);
  await writeFile('test-results/node-clipboard/example.svg', exported);
  await page.screenshot({ path: 'test-results/node-clipboard/result.png' });
  await writeFile(
    'test-results/node-clipboard/receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        actualKeyboard: true,
        selectAllCurrentComposition: true,
        selectAllInputTextareaContenteditableNative: true,
        selectAllCanvasAndImeExcluded: true,
        selectedOutputSkippedOnCopy: true,
        copiedNodes: 6,
        crossComposition: true,
        internalConnections: true,
        sameDocumentAssets: true,
        outputUnchanged: true,
        oneUndoRedo: true,
        lockedRefused: true,
        externalInputRefused: true,
        imeAndInputNative: true,
        jsonReopenExactSvg: true,
        newOpenClearsClipboard: true,
        osClipboardUsed: false,
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'Graph clipboard production shortcuts, assets, cross-Composition, Undo/reopen and guards passed',
  );
} catch (error) {
  if (typeof error.actual === 'string' && typeof error.expected === 'string') {
    let at = 0;
    while (at < error.actual.length && error.actual[at] === error.expected[at]) at++;
    console.error(
      JSON.stringify({
        differenceAt: at,
        actual: error.actual.slice(Math.max(0, at - 60), at + 100),
        expected: error.expected.slice(Math.max(0, at - 60), at + 100),
      }),
    );
  }
  console.error(
    error.stack
      ?.split('\n')
      .filter((s) => /^\s+at /.test(s))
      .join('\n'),
  );
  throw Error(error.message.split('\n')[0]);
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
