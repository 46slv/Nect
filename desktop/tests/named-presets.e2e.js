import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { revealNodePresets } from './helpers/boundary-controls.js';
import { activateBoundaryCommand } from './helpers/boundary-controls.js';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, node, connect, serialize } from '../src/core.js';
const doc = blankDocument(),
  src = doc.layers[0],
  dst = blankLayer('Preset target');
const shape = node('Rectangle', 40, 40),
  fill = node('Fill', 400, 40);
const value = node('Value', 40, 500),
  math = node('Math', 400, 500);
shape.params.size.value = [100, 80];
fill.params.color.value = { hex: '#2255bb', alpha: 1 };
src.nodes[0].position = [800, 40];
src.nodes.push(shape, fill, value, math);
connect(src, shape.id, fill.id, 'path');
connect(src, fill.id, src.output, 'path');
connect(src, value.id, math.id, 'a');
const targetShape = node('Ellipse', 40, 40),
  targetFill = node('Fill', 400, 40);
dst.nodes[0].position = [800, 40];
dst.nodes.push(targetShape, targetFill);
connect(dst, targetShape.id, targetFill.id, 'path');
connect(dst, targetFill.id, dst.output, 'path');
doc.layers.push(dst);
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
const checks = [],
  errors = [];
try {
  browser = await chromium.launch({
    executablePath:
      process.env.CHROME_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
    headless: true,
  });
  const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  const settled = () =>
    page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
  const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const state = () =>
    page.evaluate(() => ({
      doc: JSON.stringify(window.nect.document),
      state: window.nect.commandState,
    }));
  const dispatch = (type, payload) =>
    page.evaluate(
      ({ type, payload }) => window.nect.dispatch({ ...window.nect.commandState, type, payload }),
      { type, payload },
    );
  const select = async (ids) => {
    await activateBoundaryCommand(page, 'graph-fit');
    const previous = await page
      .locator('.graph .node.selected')
      .evaluateAll((nodes) => nodes.map((n) => n.dataset.id));
    for (const id of previous)
      await page
        .locator(`.graph .node[data-id="${id}"] .node-head .type-tag`)
        .click({ modifiers: ['Shift'] });
    for (let i = 0; i < ids.length; i++)
      await page
        .locator(`.graph .node[data-id="${ids[i]}"] .node-head .type-tag`)
        .click({ modifiers: i ? ['Shift'] : [] });
    assert.deepEqual(
      await page
        .locator('.graph .node.selected')
        .evaluateAll((nodes) => nodes.map((n) => n.dataset.id).sort()),
      [...ids].sort(),
    );
  };
  await page.evaluate((s) => window.nect.load(s), serialize(doc));
  const initial = await snapshot();
  await select([shape.id, fill.id]);
  await revealNodePresets(page);
  await page.locator('#node-preset-name').fill('Rectangle blue');
  await revealNodePresets(page);
  await page.locator('[data-action="save-node-preset"]').click();
  await settled();
  let data = await page.evaluate(() => window.nect.document);
  assert.equal(data.nodePresets.length, 1);
  const presetId = data.nodePresets[0].id,
    saved = await snapshot();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), initial);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), saved);
  checks.push('UI named save is one Undo/Redo');
  // A later edit to the source does not change the saved snapshot.
  await dispatch('set-parameter', {
    layerId: src.id,
    nodeId: shape.id,
    key: 'size',
    value: [999, 999],
  });
  await page.locator(`[data-select-layer="${dst.id}"]`).click();
  await revealNodePresets(page);
  await page.locator('#node-preset').selectOption(presetId);
  const beforeApply = await snapshot();
  await revealNodePresets(page);
  await page.locator('[data-action="apply-node-preset"]').click();
  await settled();
  data = await page.evaluate(() => window.nect.document);
  let added = data.layers[1].nodes.filter((n) => !dst.nodes.some((old) => old.id === n.id));
  assert.equal(added.length, 2);
  assert.ok(added.every((n) => ![shape.id, fill.id].includes(n.id)));
  assert.deepEqual(data.layers[1].nodes.slice(0, dst.nodes.length), dst.nodes);
  assert.deepEqual(added[0].params.size.value, [100, 80]);
  assert.equal(added[1].params.path.source, added[0].id);
  const applied = await snapshot();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), beforeApply);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), applied);
  checks.push(
    'UI applies detached snapshot to another Comp with fresh IDs, internal connection and original Output/graph preserved; one Undo/Redo',
  );
  await select([added[0].id]);
  const width = page.locator('.inspector .field[data-key="size"] [data-value][data-axis="0"]');
  await width.fill('240');
  await width.press('Enter');
  await settled();
  data = await page.evaluate(() => window.nect.document);
  assert.equal(data.layers[1].nodes.find((n) => n.id === added[0].id).params.size.value[0], 240);
  assert.equal(data.nodePresets[0].nodes[0].params.size.value[0], 100);
  const edited = await snapshot();
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(
    (await page.evaluate(() => window.nect.document)).layers[1].nodes.find(
      (n) => n.id === added[0].id,
    ).params.size.value,
    [100, 80],
  );
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), edited);
  checks.push(
    'applied node edits through ordinary Inspector, independent of preset, with Undo/Redo',
  );
  await select(added.map((n) => n.id));
  await revealNodePresets(page);
  await page.locator('#node-preset').selectOption(presetId);
  await revealNodePresets(page);
  await page.locator('#node-preset-name').fill('Rectangle wide');
  const beforeUpdate = await snapshot();
  await revealNodePresets(page);
  await page.locator('[data-action="update-node-preset"]').click();
  await settled();
  data = await page.evaluate(() => window.nect.document);
  assert.equal(data.nodePresets[0].id, presetId);
  assert.equal(data.nodePresets[0].name, 'Rectangle wide');
  assert.equal(data.nodePresets[0].nodes[0].params.size.value[0], 240);
  const updated = await snapshot();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), beforeUpdate);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), updated);
  checks.push('UI update retains preset identity, changes name and snapshot in one Undo/Redo');
  let before = await state();
  await revealNodePresets(page);
  await page.locator('[data-action="save-node-preset"]').click();
  await settled();
  assert.match(await page.locator('.status').innerText(), /name already/);
  assert.deepEqual(await state(), before);
  const reject = async (type, payload, pattern) => {
    const before = await state();
    const message = await page.evaluate(
      ({ type, payload }) => {
        try {
          window.nect.dispatch({ ...window.nect.commandState, type, payload });
          return null;
        } catch (e) {
          return e.message;
        }
      },
      { type, payload },
    );
    assert.match(message, pattern);
    assert.deepEqual(await state(), before);
  };
  await reject(
    'save-node-preset',
    { layerId: dst.id, nodeIds: added.map((n) => n.id), name: 'Collision', presetId },
    /ID already/,
  );
  await reject(
    'save-node-preset',
    { layerId: dst.id, nodeIds: added.map((n) => n.id), name: 'Collision', presetId: added[0].id },
    /duplicate ID/,
  );
  await reject(
    'save-node-preset',
    { layerId: src.id, nodeIds: [math.id], name: 'External' },
    /external/,
  );
  await reject(
    'save-node-preset',
    { layerId: src.id, nodeIds: [src.output], name: 'Output' },
    /Output/,
  );
  await reject('apply-node-preset', { layerId: dst.id, presetId, offset: [null, 0] }, /offset/);
  checks.push(
    'name/ID collisions, Output, invalid offset and external-reference commands reject atomically through shared API',
  );
  // Invalid load uses the production load boundary and preserves the current document.
  const malformed = JSON.parse(await snapshot());
  malformed.nodePresets[0].nodes[0].params.size.source = 'external-node';
  before = await state();
  const message = await page.evaluate((s) => {
    try {
      window.nect.load(s);
      return null;
    } catch (e) {
      return e.message;
    }
  }, JSON.stringify(malformed));
  assert.match(message, /external/);
  assert.deepEqual(await state(), before);
  const missingAsset = JSON.parse(await snapshot());
  const missingImage = node('Image');
  missingImage.asset = 'image-missing';
  missingAsset.nodePresets[0].nodes = [missingImage];
  missingAsset.nodePresets[0].imageAssets = {};
  before = await state();
  const assetError = await page.evaluate((s) => {
    try {
      window.nect.load(s);
      return null;
    } catch (e) {
      return e.message;
    }
  }, JSON.stringify(missingAsset));
  assert.match(assetError, /preset assets|asset is missing/);
  assert.deepEqual(await state(), before);
  checks.push(
    'missing snapshot asset refuses at load boundary without changing document or revision',
  );
  const encoded = await snapshot();
  await page.evaluate((s) => window.nect.load(s), encoded);
  assert.equal(await snapshot(), encoded);
  await page.locator(`[data-select-layer="${dst.id}"]`).click();
  await revealNodePresets(page);
  await page.locator('#node-preset').selectOption(presetId);
  await revealNodePresets(page);
  await page.locator('[data-action="apply-node-preset"]').click();
  await settled();
  data = await page.evaluate(() => window.nect.document);
  const reapplied = data.layers[1].nodes.slice(-2);
  assert.equal(data.layers[1].nodes.length, dst.nodes.length + 4);
  assert.equal(reapplied[0].params.size.value[0], 240);
  assert.ok(reapplied.every((n) => !added.some((old) => old.id === n.id)));
  checks.push(
    'saved document reloads with named preset and updated ordinary-node snapshot; invalid saved snapshot refuses without replacing document',
  );
  await activateBoundaryCommand(page, 'graph-fit-selection');
  await mkdir('test-results/named-presets', { recursive: true });
  await page.screenshot({ path: 'test-results/named-presets/editor.png' });
  assert.deepEqual(errors, []);
  const receipt = {
    status: 'PASS',
    browser: await browser.version(),
    checks,
    pageErrors: errors,
    native: 'unverified',
    physicalIME: 'unverified',
  };
  await writeFile('test-results/named-presets/receipt.json', JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} finally {
  await browser?.close();
  await new Promise((r) => server.close(r));
}
