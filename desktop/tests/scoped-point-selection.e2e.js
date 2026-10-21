import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, connect, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

const output = resolve('test-results/scoped-point-selection/native');
await mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_SCOPE_CDP_URL || 'http://127.0.0.1:9448',
);
let page;
const errors = [],
  results = [];
try {
  const pages = browser
    .contexts()
    .flatMap((c) => c.pages())
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1, 'one task-owned native WebView2');
  [page] = pages;
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  const recovery = page.locator('[data-recovery-dialog]');
  try {
    await recovery.waitFor({ state: 'visible', timeout: 1000 });
    await recovery.getByRole('button', { name: 'Later', exact: true }).click();
  } catch (e) {
    if (e.name !== 'TimeoutError') throw e;
  }
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const command = () => page.evaluate(() => window.nect.commandState);
  const inspect = (id) => page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  const regions = new Map();
  const panel = (id) =>
    page.locator(`${regions.get(id) || '.inspector-body'} [data-point-panel="${id}"]`);
  const rows = (id) =>
    panel(id)
      .locator('[data-point-row]')
      .evaluateAll((els) => els.map((e) => e.dataset.pointRow));
  const select = async (id, kind, value) => {
    await panel(id).locator(`[data-point-panel-${kind}]`).selectOption(value);
    await page.waitForFunction(
      ({ id, kind, value, region }) =>
        document.querySelector(`${region} [data-point-panel="${id}"] [data-point-panel-${kind}]`)
          ?.value === value,
      { id, kind, value, region: regions.get(id) || '.inspector-body' },
    );
  };
  const screenshot = async (name, id) => {
    await panel(id).locator('[data-point-panel-contour]').scrollIntoViewIfNeeded();
    await page.screenshot({ path: resolve(output, name + '.png') });
  };
  const diskRoundtrip = async (name, id) => {
    const text = await saved(),
      svg = await page.evaluate(() => window.nect.export());
    const file = resolve(output, name + '.json');
    await writeFile(file, text);
    await page.evaluate((text) => window.nect.load(text), await readFile(file, 'utf8'));
    assert.deepEqual(JSON.parse(await saved()), JSON.parse(text));
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
    await inspect(id);
    assert.equal(await panel(id).locator('[data-point-panel-contour]').inputValue(), '');
    assert.deepEqual(await rows(id), []);
  };

  const button = (id, kind) => panel(id).locator(`[data-point-scope-action="${kind}"]`);
  const act = (id, kind) => button(id, kind).click();
  const canvasIds = () =>
    page
      .locator('.artboard .vertex.point-marker.active[data-point]')
      .evaluateAll((els) => els.map((el) => el.dataset.point));
  const count = async (id) =>
    Number(
      (await panel(id).locator('.point-panel-summary').first().innerText()).match(
        /(\d+) Canvas selected/,
      )[1],
    );
  const checkpoint = async () => ({
    doc: await saved(),
    command: await command(),
    file: await page.evaluate(() => window.nect.fileState),
  });
  const neutral = async (before) => assert.deepEqual(await checkpoint(), before);
  const f = compoundFixture();
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  await inspect(f.path.id);
  const id = f.path.id,
    before = await checkpoint();
  await select(id, 'contour', 'scope-outer');
  await panel(id).getByRole('button', { name: 'Next', exact: true }).click();
  const active = f.path.contours[0].points[29];
  await panel(id).locator(`[data-select-panel-point="${active}"]`).click();
  assert.deepEqual(await rows(id), [active]);
  assert.match(await button(id, 'all').getAttribute('aria-label'), /contour 1: 40 anchors/);
  await act(id, 'all');
  assert.equal(await count(id), 40);
  assert.deepEqual(new Set(await canvasIds()), new Set(f.path.contours[0].points));
  assert.equal(await panel(id).locator('.point-active').getAttribute('data-point-row'), active);
  assert.deepEqual(await rows(id), f.path.contours[0].points.slice(24));
  assert.equal(await panel(id).locator('.point-selected').count(), 16);
  await panel(id).getByRole('button', { name: 'Previous', exact: true }).click();
  assert.deepEqual(await rows(id), f.path.contours[0].points.slice(0, 24));
  assert.equal(await panel(id).locator('.point-selected').count(), 24);
  await act(id, 'invert');
  assert.equal(await count(id), 0);
  assert.deepEqual(await canvasIds(), []);
  assert.deepEqual(await rows(id), []);
  assert.match(await button(id, 'invert').innerText(), /40/);
  await act(id, 'invert');
  assert.equal(await count(id), 40);
  assert.equal(
    await panel(id).locator('.point-active').getAttribute('data-point-row'),
    f.path.contours[0].points[0],
  );
  const other = f.path.contours[2].points[3];
  await page.locator(`.canvas-stage .point-hit[data-point="${other}"]:not([data-handle])`).click();
  assert.deepEqual(await rows(id), [other]);
  assert.equal(await panel(id).locator('[data-point-panel-contour]').inputValue(), 'scope-other');
  assert.equal(await panel(id).locator('.point-active').getAttribute('data-point-row'), other);
  await select(id, 'unit', 'scope-unit-a');
  await act(id, 'all');
  assert.equal(await count(id), 50);
  assert.deepEqual(
    new Set(await canvasIds()),
    new Set(f.path.contours.slice(0, 2).flatMap((c) => c.points)),
  );
  assert.match(await button(id, 'all').getAttribute('aria-label'), /unit 1: 50 anchors/);
  await panel(id).getByRole('button', { name: 'Next', exact: true }).click();
  await panel(id).getByRole('button', { name: 'Next', exact: true }).click();
  assert.deepEqual(await rows(id), f.path.contours[1].points.slice(8));
  assert.equal(await panel(id).locator('.point-selected').count(), 2);
  await select(id, 'contour', 'scope-hole');
  await act(id, 'invert');
  assert.equal(await count(id), 0);
  await act(id, 'invert');
  assert.equal(await count(id), 10);
  assert.deepEqual(await rows(id), f.path.contours[1].points);
  await neutral(before);
  await screenshot('compound-hole-selection', id);
  await diskRoundtrip('compound', id);
  results.push({
    case: 'owned compound',
    all40AcrossPages: true,
    activeStableId: true,
    invertFixedWhenZeroRows: true,
    unitIncludesHole50: true,
    contourOnly10: true,
    canvasRowSync: true,
    documentFileHistoryNeutral: true,
    diskJsonReload: true,
  });

  const d = document(),
    l = d.layers[0],
    text = node('Text', 40, 40),
    convert = node('Text to Path', 320, 40),
    remap = node('Path Remap', 600, 40),
    fill = node('Fill', 880, 40),
    out = node('Output', 1160, 40);
  text.text = 'BO';
  text.params.position.value = [60, 180];
  text.params.size.value = 100;
  d.width = 400;
  d.height = 300;
  l.nodes = [text, convert, remap, fill, out];
  l.output = out.id;
  connect(l, text.id, convert.id, 'path');
  connect(l, convert.id, remap.id, 'path');
  connect(l, remap.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  regions.set(remap.id, '.graph');
  await page.evaluate((s) => window.nect.load(s), serialize(d));
  await inspect(remap.id);
  const generated = await page.evaluate((id) => window.nect.evaluate().run(id), convert.id),
    last = generated.contours.at(-1);
  await select(remap.id, 'contour', last.id);
  await panel(remap.id).locator('[data-batch-point]').first().check();
  const generatedBefore = await checkpoint();
  await act(remap.id, 'all');
  assert.equal(await count(remap.id), last.points.length);
  assert.deepEqual(new Set(await canvasIds()), new Set(last.points.map((p) => p.id)));
  assert.deepEqual(
    await rows(remap.id),
    last.points.slice(0, 24).map((p) => p.id),
  );
  assert.equal(
    await panel(remap.id).locator('.point-active').getAttribute('data-point-row'),
    last.points[0].id,
  );
  await act(remap.id, 'invert');
  assert.equal(await count(remap.id), 0);
  await act(remap.id, 'invert');
  assert.equal(await count(remap.id), last.points.length);
  assert.match(
    await page.locator(`.graph .node[data-id="${remap.id}"] .batch-edit`).innerText(),
    /1 batch checked/,
  );
  await neutral(generatedBefore);
  await screenshot('generated-text-selection', remap.id);
  await page.evaluate(
    ({ layerId, nodeId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-text-content',
        payload: { layerId, nodeId, text: 'I' },
      }),
    { layerId: l.id, nodeId: text.id },
  );
  const edited = await checkpoint(),
    current = await page.evaluate((id) => window.nect.evaluate().run(id), convert.id);
  assert.equal(await panel(remap.id).locator('[data-point-panel-contour]').inputValue(), '');
  await act(remap.id, 'all');
  assert.equal(
    await count(remap.id),
    current.contours.reduce((n, c) => n + c.points.length, 0),
  );
  assert.deepEqual(
    await page.evaluate(
      (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).edits,
      remap.id,
    ),
    [],
  );
  await neutral(edited);
  await diskRoundtrip('generated', remap.id);
  results.push({
    case: 'generated Text Remap',
    actualContourIds: true,
    independentBatch: true,
    newTopologyCurrentOnly: true,
    noBakeRebind: true,
    documentFileHistoryNeutral: true,
  });

  const b = document(),
    bl = b.layers[0],
    bt = node('Text', 40, 40),
    bf = node('Fill', 360, 40),
    bo = node('Output', 680, 40);
  bt.text = 'BO';
  bt.params.position.value = [60, 180];
  bt.params.size.value = 100;
  b.width = 400;
  b.height = 300;
  bl.nodes = [bt, bf, bo];
  bl.output = bo.id;
  connect(bl, bt.id, bf.id, 'path');
  connect(bl, bf.id, bo.id, 'path');
  await page.evaluate((s) => window.nect.load(s), serialize(b));
  await inspect(bt.id);
  await page.locator('.inspector-body [data-bake-text-outline]').click();
  const owned = JSON.parse(await saved()).layers[0].nodes.find((n) => n.id === bt.id),
    bakedBefore = await checkpoint();
  await select(bt.id, 'unit', owned.units[0].id);
  await act(bt.id, 'all');
  const ownedIds = owned.contours
    .filter((c) => owned.units[0].contours.includes(c.id))
    .flatMap((c) => c.points);
  assert.equal(await count(bt.id), ownedIds.length);
  assert.equal(
    await panel(bt.id).locator('.point-active').getAttribute('data-point-row'),
    ownedIds[0],
  );
  await act(bt.id, 'invert');
  assert.equal(await count(bt.id), 0);
  await act(bt.id, 'invert');
  assert.equal(await count(bt.id), ownedIds.length);
  await neutral(bakedBefore);
  await screenshot('outlined-unit-selection', bt.id);
  await diskRoundtrip('outlined', bt.id);
  results.push({
    case: 'owned outlined Text',
    realUnitAndHoleMembership: true,
    selectionOnlyAfterFixtureBake: true,
    documentFileHistoryNeutral: true,
  });

  const locked = compoundFixture([8, 5, 7, 4]);
  locked.doc.schemaVersion = 42;
  locked.path.points.find((p) => p.id === locked.path.contours[0].points[1]).position.locked = true;
  locked.path.points.find((p) => p.id === locked.path.contours[0].points[2]).position.expression =
    'value; value';
  await page.evaluate((s) => window.nect.load(s), serialize(locked.doc));
  await inspect(locked.path.id);
  const lockedBefore = await checkpoint();
  await select(locked.path.id, 'contour', 'scope-outer');
  await act(locked.path.id, 'all');
  assert.equal(await count(locked.path.id), 8);
  assert.equal(await panel(locked.path.id).locator('[data-apply-point-delta]').isDisabled(), true);
  assert.equal(
    await panel(locked.path.id).locator('[data-point-bounds]').getAttribute('data-state'),
    'unavailable',
  );
  await neutral(lockedBefore);
  results.push({
    case: 'locked/driven',
    noSilentSubset: true,
    existingEditGuard: true,
    documentFileHistoryNeutral: true,
  });
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(output, 'receipt.json'),
    JSON.stringify(
      {
        nativeWebView2: true,
        syntheticCdpInput: true,
        results,
        errors,
        notRun: [
          'physical keyboard/IME',
          'native OS Save/Open dialogs',
          'cold-process file reopen',
          'unrelated full audits',
        ],
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify({ pass: results.length, results, errors }));
} catch (error) {
  if (page) await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
  throw error;
} finally {
  await browser.close();
}
