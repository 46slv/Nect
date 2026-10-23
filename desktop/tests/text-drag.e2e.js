import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, connect, serialize } from '../src/core.js';

function fixture({
  os = false,
  transformed = false,
  locked = false,
  positionLocked = false,
  layerLocked = false,
  negative = false,
} = {}) {
  const d = document(),
    l = d.layers[0],
    text = node('Text', 30, 30),
    fill = node('Fill', 380, 30),
    out = node('Output', 730, 30),
    tr = node('Transform', 30, 500);
  d.width = 800;
  d.height = 500;
  text.text = 'Move Text';
  text.params.position.value = [220, 240];
  text.params.size.value = 72;
  text.locked = locked;
  text.params.position.locked = positionLocked;
  l.locked = layerLocked;
  if (os) {
    d.schemaVersion = 28;
    text.osFont = {
      version: 1,
      family: 'Synthetic saved OS face',
      style: 0,
      outline: {
        family: 'Synthetic saved OS face',
        style: 0,
        text: text.text,
        size: 72,
        ascent: 60,
        points: [
          [0, 0],
          [100, 0],
          [100, 40],
          [0, 40],
        ],
        types: [0, 1, 1, 161],
      },
    };
  }
  l.nodes = [text, ...(transformed ? [tr] : []), fill, out];
  l.output = out.id;
  if (transformed) {
    tr.params.scale.value = negative ? -1.3 : 1.3;
    tr.params.anchor.value = [220, 240];
    tr.params.rotation.value = 23;
    tr.params.offset.value = [30, -20];
    connect(l, text.id, tr.id, 'path');
    connect(l, tr.id, fill.id, 'path');
  } else connect(l, text.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  return {
    d,
    l,
    text,
    out,
    tr,
    target: transformed ? tr.id : text.id,
    key: transformed ? 'offset' : 'position',
  };
}
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
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } }),
    errors = [],
    rows = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => window.nect);
  const point = () =>
    page
      .locator('.artboard .shape')
      .first()
      .evaluate((el) => {
        const path = el.matches('path') ? el : el.querySelector('path'),
          b = path.getBBox(),
          m = path.getScreenCTM();
        for (let y = b.y + 1; y < b.y + b.height; y += 2)
          for (let x = b.x + 1; x < b.x + b.width; x += 2)
            if (path.isPointInFill(new DOMPoint(x, y))) {
              const p = new DOMPoint(x, y).matrixTransform(m);
              return [p.x, p.y];
            }
        throw Error('No painted point');
      });
  for (const options of [
    {},
    { os: true },
    { transformed: true },
    { os: true, transformed: true },
    { zoom: 'out' },
    { zoom: 'in', os: true },
    { transformed: true, negative: true },
    { transformed: true, positionLocked: true },
  ]) {
    const f = fixture(options);
    await page.evaluate((s) => window.nect.load(s), serialize(f.d));
    await page.locator('[data-tool="select"]').click();
    // Test final artwork, not the explicit pre-transform source-stage marker.
    await page.locator(`.graph .node[data-id="${f.out.id}"] .node-head .type-tag`).click();
    if (options.zoom) {
      await page.locator('[data-action="actual-size"]').click();
      await page.locator(`[data-action="zoom-${options.zoom}"]`).click();
    }
    const before = await page.evaluate(() => JSON.stringify(window.nect.document)),
      p = await point(),
      delta = await page.locator('.artboard').evaluate((el) => {
        const m = el.getScreenCTM();
        return [30 * m.a + 20 * m.c, 30 * m.b + 20 * m.d];
      });
    const hit = await page.evaluate(([x, y]) => {
      const el = document.elementFromPoint(x, y);
      return {
        tag: el?.tagName,
        stage: el?.dataset.nodeStageMove,
        layer: el?.dataset.layer,
        control: el
          ?.closest('[data-object-resize],[data-position-handle],[data-node-stage-move]')
          ?.outerHTML.slice(0, 360),
      };
    }, p);
    await page.mouse.click(...p);
    await page.mouse.move(...p);
    await page.mouse.down();
    await page.mouse.move(p[0] + delta[0], p[1] + delta[1], { steps: 5 });
    await page.mouse.up();
    const observed = await page.evaluate(
        ({ id, key, textId }) => ({
          value: window.nect.document.layers[0].nodes.find((n) => n.id === id).params[key].value,
          text: window.nect.document.layers[0].nodes.find((n) => n.id === textId),
          editor: !!document.querySelector('.canvas-text-editor'),
        }),
        { id: f.target, key: f.key, textId: f.text.id },
      ),
      expected = f.d.layers[0].nodes
        .find((n) => n.id === f.target)
        .params[f.key].value.map((v, i) => v + [30, 20][i]);
    rows.push({
      options,
      hit,
      value: observed.value,
      sourcePosition: observed.text.params.position.value,
      expected,
      editor: observed.editor,
    });
    await mkdir('test-results/text-drag', { recursive: true });
    await writeFile('test-results/text-drag/probe.json', JSON.stringify(rows, null, 2));
    assert.equal(
      observed.editor,
      false,
      'A click then drag must move Text, not start text editing',
    );
    observed.value.forEach((v, i) =>
      assert.ok(Math.abs(v - expected[i]) < 0.001, `${v} versus ${expected[i]}`),
    );
    assert.equal(observed.text.text, f.text.text);
    assert.equal(observed.text.font, f.text.font);
    assert.deepEqual(observed.text.osFont, f.text.osFont);
    const after = await page.evaluate(() => JSON.stringify(window.nect.document));
    await page.locator('.canvas-stage').focus();
    await page.keyboard.press('Control+z');
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
    await page.keyboard.press('Control+Shift+z');
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), after);
  }
  for (const options of [
    { locked: true },
    { os: true, locked: true },
    { positionLocked: true },
    { layerLocked: true },
  ]) {
    const f = fixture(options);
    await page.evaluate((s) => window.nect.load(s), serialize(f.d));
    await page.locator(`.graph .node[data-id="${f.out.id}"] .node-head .type-tag`).click();
    const before = await page.evaluate(() => JSON.stringify(window.nect.document)),
      p = await point();
    await page.mouse.click(...p);
    await page.mouse.move(...p);
    await page.mouse.down();
    await page.mouse.move(p[0] + 40, p[1] + 20, { steps: 4 });
    await page.mouse.up();
    assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
    assert.equal(await page.locator('.canvas-text-editor').count(), 0);
  }
  const editable = fixture();
  await page.evaluate((s) => window.nect.load(s), serialize(editable.d));
  await page.locator(`.graph .node[data-id="${editable.out.id}"] .node-head .type-tag`).click();
  let p = await point();
  await page.mouse.dblclick(...p);
  await page.getByRole('textbox', { name: 'Edit canvas text', exact: true }).waitFor();
  await page.getByRole('textbox', { name: 'Edit canvas text', exact: true }).press('Escape');
  assert.equal(await page.locator('.canvas-text-editor').count(), 0);
  const still = await page.evaluate(() => JSON.stringify(window.nect.document));
  p = await point();
  await page.mouse.click(...p);
  await page.mouse.move(...p);
  await page.mouse.down();
  await page.mouse.move(p[0] + 40, p[1] + 20, { steps: 3 });
  await page.mouse.move(...p, { steps: 3 });
  await page.mouse.up();
  assert.equal(await page.locator('.canvas-text-editor').count(), 0);
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.document)),
    still,
    'Returning a drag to its start is not a double click or history edit',
  );
  assert.deepEqual(errors, []);
  await page.screenshot({ path: 'test-results/text-drag/result.png' });
  await writeFile(
    'test-results/text-drag/receipt.json',
    JSON.stringify(
      {
        productionCsp: true,
        actualPointer: true,
        reproducedBeforeFix: true,
        textAndSavedOsTextMoved: true,
        transformRotationAndNegativeScale: true,
        multipleZooms: true,
        oneUndoRedo: true,
        textFontAndOsSnapshotUnchanged: true,
        nodeLayerPositionLocks: true,
        doubleClickStillEdits: true,
        dragOutAndBackDoesNotEdit: true,
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log('Text/OS Text click-drag, Transform coordinates and one Undo passed');
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
