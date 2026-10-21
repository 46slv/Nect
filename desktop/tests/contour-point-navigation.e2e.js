import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { document, node, connect, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

const output = resolve('test-results/contour-point-navigation/native');
await mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_CONTOUR_CDP_URL || 'http://127.0.0.1:9444',
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

  const f = compoundFixture();
  await page.evaluate((s) => window.nect.load(s), serialize(f.doc));
  await inspect(f.path.id);
  const id = f.path.id,
    before = await saved(),
    originalState = await command();
  assert.deepEqual(await rows(id), []);
  await select(id, 'unit', 'scope-unit-a');
  assert.equal((await rows(id)).length, 24);
  assert.equal((await rows(id))[0], 'scope-outer-p0');
  await panel(id).getByRole('button', { name: 'Next', exact: true }).click();
  assert.equal((await rows(id))[0], 'scope-outer-p24');
  await select(id, 'contour', 'scope-hole');
  assert.deepEqual(await rows(id), f.path.contours[1].points);
  assert.match(
    await panel(id).locator('[data-point-panel-contour] option:checked').innerText(),
    /closed.*10 points/,
  );
  assert.deepEqual(await command(), originalState);
  assert.equal(await saved(), before);
  await screenshot('compound-hole', id);

  await select(id, 'contour', 'scope-outer');
  await panel(id).getByRole('button', { name: 'Next', exact: true }).click();
  const target = 'scope-outer-p29';
  await panel(id).locator(`[data-select-panel-point="${target}"]`).click();
  assert.deepEqual(await rows(id), [target]);
  assert.equal(await panel(id).locator('.point-active').getAttribute('data-point-row'), target);
  const other = 'scope-other-p3';
  await page
    .locator(`.canvas-stage .point-hit[data-point="${other}"]:not([data-handle])`)
    .click({ modifiers: ['Shift'] });
  await page.waitForFunction(
    ({ id, other }) =>
      document.querySelector(`.inspector-body [data-point-panel="${id}"] .point-active`)?.dataset
        .pointRow === other,
    { id, other },
  );
  assert.equal(await panel(id).locator('[data-point-panel-unit]').inputValue(), 'scope-unit-b');
  assert.equal(await panel(id).locator('[data-point-panel-contour]').inputValue(), 'scope-other');
  assert.deepEqual(await rows(id), [other]);
  await select(id, 'unit', 'scope-unit-a');
  assert.match(
    await panel(id).locator('.point-panel-summary').innerText(),
    /2 Canvas selected.*1 selected in list/,
  );
  assert.equal(await saved(), before);
  assert.deepEqual(await command(), originalState);
  await page.locator(`.inspector-body [data-path-unit="scope-unit-a"]`).click();
  await page
    .locator(`.inspector-body [data-move-path-unit="scope-unit-a"][data-delta="1,0"]`)
    .click();
  assert.equal((await command()).expectedRevision, originalState.expectedRevision + 1);
  const moved = JSON.parse(await saved()).layers[0].nodes.find((n) => n.id === id);
  assert.deepEqual(moved.contours, f.path.contours);
  assert.deepEqual(moved.units, f.path.units);
  const ids = new Set(f.path.contours.slice(0, 2).flatMap((c) => c.points));
  for (const p of f.path.points)
    assert.deepEqual(moved.points.find((q) => q.id === p.id).position.value, [
      p.position.value[0] + (ids.has(p.id) ? 1 : 0),
      p.position.value[1],
    ]);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), before);
  await diskRoundtrip('compound', id);
  results.push({
    case: 'compound',
    unitContourFilter: true,
    orderedPaging: true,
    stableRowCanvasShiftSelection: true,
    scopeDoesNotSelectOrDirty: true,
    separateUnitMoveOneUndo: true,
    idsClosureHoleGroupsPreserved: true,
    diskJsonReloadSvg: true,
  });

  const d = document(),
    l = d.layers[0],
    text = node('Text', 40, 40),
    convert = node('Text to Path', 320, 40),
    remap = node('Path Remap', 600, 40),
    fill = node('Fill', 880, 40),
    out = node('Output', 1160, 40);
  d.name = 'Generated text contour navigation';
  regions.set(remap.id, '.graph');
  d.width = 400;
  d.height = 300;
  text.text = 'BO';
  text.params.position.value = [60, 180];
  text.params.size.value = 100;
  l.nodes = [text, convert, remap, fill, out];
  l.output = out.id;
  connect(l, text.id, convert.id, 'path');
  connect(l, convert.id, remap.id, 'path');
  connect(l, remap.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  await page.evaluate((s) => window.nect.load(s), serialize(d));
  await inspect(remap.id);
  const generated = await page.evaluate((id) => window.nect.evaluate().run(id), convert.id),
    last = generated.contours.at(-1),
    remapBefore = await saved(),
    remapState = await command();
  assert.equal(await panel(remap.id).locator('[data-point-panel-unit]').count(), 0);
  await select(remap.id, 'contour', last.id);
  assert.deepEqual(
    await rows(remap.id),
    last.points.slice(0, 24).map((p) => p.id),
  );
  await panel(remap.id).locator('[data-batch-point]').first().check();
  await select(remap.id, 'contour', generated.contours[0].id);
  await page.waitForFunction(
    (id) =>
      document
        .querySelector(`.graph .node[data-id="${id}"] .batch-edit`)
        ?.textContent.includes('1 batch checked'),
    remap.id,
  );
  assert.equal(await saved(), remapBefore);
  assert.deepEqual(await command(), remapState);
  await screenshot('generated-remap', remap.id);
  await diskRoundtrip('generated-remap', remap.id);
  results.push({
    case: 'generatedTextRemap',
    actualContourIds: true,
    noInventedUnits: true,
    independentBatchChecks: true,
    noBakeRebindOrAuthoredEdit: true,
    diskJsonReloadSvg: true,
  });

  const bakedDoc = document(),
    bl = bakedDoc.layers[0],
    bt = node('Text', 40, 40),
    bf = node('Fill', 360, 40),
    bo = node('Output', 680, 40);
  bt.text = 'BO';
  bt.params.position.value = [60, 180];
  bt.params.size.value = 100;
  bakedDoc.width = 400;
  bakedDoc.height = 300;
  bl.nodes = [bt, bf, bo];
  bl.output = bo.id;
  connect(bl, bt.id, bf.id, 'path');
  connect(bl, bf.id, bo.id, 'path');
  await page.evaluate((s) => window.nect.load(s), serialize(bakedDoc));
  await inspect(bt.id);
  // Existing explicit conversion is fixture preparation, not a new navigation command.
  await page.locator('.inspector-body [data-bake-text-outline]').click();
  const baked = JSON.parse(await saved()).layers[0].nodes.find((n) => n.id === bt.id),
    bakedBefore = await saved(),
    bakedState = await command();
  assert.equal(baked.type, 'Multi Path');
  assert.ok(baked.units.length >= 2);
  await select(bt.id, 'unit', baked.units[0].id);
  const bc = baked.contours.find((c) => baked.units[0].contours.includes(c.id));
  await select(bt.id, 'contour', bc.id);
  assert.deepEqual(await rows(bt.id), bc.points.slice(0, 24));
  assert.equal(await saved(), bakedBefore);
  assert.deepEqual(await command(), bakedState);
  await screenshot('outlined-text', bt.id);
  await diskRoundtrip('outlined-text', bt.id);
  results.push({
    case: 'ownedOutlinedText',
    realUnitMembership: true,
    contourPaging: true,
    navigationDocumentNeutral: true,
    diskJsonReloadSvg: true,
  });
  assert.deepEqual(errors, []);
  const receipt = {
    nativeWebView2: true,
    url: page.url(),
    syntheticCdpInput: true,
    pageErrorsDuringFlow: errors,
    results,
    physicalInputIme: 'NOT_RUN',
    nativeOsFileDialogsColdProcessReopen: 'NOT_RUN: Windows node_repl controller unavailable',
    fullSuite: 'NOT_RUN',
  };
  await writeFile(resolve(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify(receipt));
} catch (error) {
  if (page) {
    await page.screenshot({ path: resolve(output, 'failure.png') });
    await writeFile(
      resolve(output, 'failure.json'),
      JSON.stringify(
        {
          error: error.message,
          text: await page.locator('body').innerText(),
          document: await page.evaluate(() => window.nect?.document),
        },
        null,
        2,
      ),
    );
  }
  throw error;
} finally {
  await browser.close();
}
