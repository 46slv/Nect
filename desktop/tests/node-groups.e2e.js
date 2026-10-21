import assert from 'node:assert/strict';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, node, connect, serialize } from '../src/core.js';

const output = resolve('test-results/named-node-groups/native');
await mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP(
  process.env.NECT_GROUP_CDP_URL || 'http://127.0.0.1:9450',
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
  try {
    await page.locator('[data-recovery-dialog]').waitFor({ state: 'visible', timeout: 1000 });
    await page
      .locator('[data-recovery-dialog]')
      .getByRole('button', { name: 'Later', exact: true })
      .click();
  } catch (e) {
    if (e.name !== 'TimeoutError') throw e;
  }
  const d = blankDocument(),
    layer = d.layers[0],
    circle = node('Circle', 40, 50),
    fill = node('Fill', 350, 50),
    out = layer.nodes[0];
  out.position = [660, 50];
  circle.collapsed = fill.collapsed = out.collapsed = true;
  layer.nodes = [circle, fill, out];
  connect(layer, circle.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  await page.evaluate((s) => window.nect.load(s), serialize(d));
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document)),
    groups = () => page.evaluate(() => window.nect.document.layers[0].nodeGroups || []),
    checkpoint = async () => ({
      saved: await saved(),
      state: await page.evaluate(() => window.nect.commandState),
      file: await page.evaluate(() => window.nect.fileState),
    }),
    neutral = async (before) => assert.deepEqual(await checkpoint(), before),
    pick = (id, shift = false) =>
      page
        .locator(`.graph .node[data-id="${id}"] .type-tag`)
        .click({ modifiers: shift ? ['Shift'] : [] }),
    input = () => page.locator('[data-node-group-name]'),
    button = (kind) => page.locator(`[data-node-group-action="${kind}"]`),
    act = (kind) => button(kind).click(),
    choose = (id) => page.locator('[data-node-group-select]').selectOption(id),
    open = async () => {
      if (!(await page.locator('[data-node-groups]').evaluate((e) => e.open)))
        await page.locator('[data-node-groups] > summary').click();
    };
  const nodes = JSON.parse(await saved()).layers[0].nodes,
    svg = await page.evaluate(() => window.nect.export());
  const geometryNeutral = async () => {
    assert.deepEqual(JSON.parse(await saved()).layers[0].nodes, nodes);
    assert.equal(await page.evaluate(() => window.nect.export()), svg);
  };
  await pick(circle.id);
  await pick(fill.id, true);
  await open();
  await input().fill('Shapes & paint');
  await act('create');
  const first = (await groups())[0].id;
  assert.deepEqual((await groups())[0].nodeIds, [circle.id, fill.id]);
  assert.equal(await page.locator(`[data-node-group-id="${first}"]`).count(), 2);
  await input().fill('Source');
  await act('rename');
  assert.equal((await groups())[0].name, 'Source');
  await pick(out.id);
  await open();
  await input().fill('Output');
  await act('create');
  const second = (await groups())[1].id;
  await pick(fill.id);
  await open();
  await choose(second);
  assert.equal(await button('add').isDisabled(), true);
  const beforeMove = await saved();
  await act('move');
  const afterMove = await saved();
  assert.deepEqual((await groups())[0].nodeIds, [circle.id]);
  assert.deepEqual((await groups())[1].nodeIds, [out.id, fill.id]);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), beforeMove);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), afterMove);
  await geometryNeutral();
  await page.screenshot({ path: resolve(output, 'membership.png') });
  results.push({
    case: 'create/rename/explicit move',
    realControls: true,
    oneUndoRedo: true,
    nodeIdsPositionsConnectionsAndSVGUnchanged: true,
  });

  await pick(circle.id);
  await open();
  await choose(first);
  const beforeDraft = await checkpoint();
  await input().fill('Cancelled');
  await act('cancel');
  assert.equal(await input().inputValue(), 'Source');
  await neutral(beforeDraft);
  await input().fill(' ');
  await act('rename');
  await neutral(beforeDraft);
  await input().fill('Escape draft');
  await input().press('Escape');
  await neutral(beforeDraft);
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Delete');
  assert.deepEqual((await groups())[0].nodeIds, []);
  const emptyAfterDeletion = await saved();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), beforeDraft.saved);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), emptyAfterDeletion);
  await page.locator('[data-action="undo"]').click();
  await pick(circle.id);
  await open();
  await choose(first);
  await act('remove');
  assert.deepEqual((await groups())[0].nodeIds, []);
  await geometryNeutral();
  await page.screenshot({ path: resolve(output, 'empty-group.png') });
  results.push({
    case: 'drafts/deletion/empty group',
    cancelEscapeInvalidNeutral: true,
    lastMemberDeletionUndoRedo: true,
    emptyRetained: true,
  });

  const text = await saved(),
    path = resolve(output, 'groups.json');
  await writeFile(path, text);
  await page.evaluate((s) => window.nect.load(s), await readFile(path, 'utf8'));
  assert.equal(await saved(), text);
  await geometryNeutral();
  assert.equal((await groups())[0].id, first);
  assert.deepEqual((await groups())[0].nodeIds, []);
  assert.equal(await page.locator(`[data-node-group-id="${second}"]`).count(), 2);
  await open();
  await choose(first);
  await act('ungroup');
  assert.equal((await groups()).length, 1);
  await geometryNeutral();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), text);
  await page.evaluate((s) => window.nect.load(s), serialize(blankDocument()));
  await open();
  assert.deepEqual(await groups(), []);
  assert.equal(await page.locator('.node-group-label').count(), 0);
  assert.equal(await page.locator('[data-node-group-select] option').count(), 1);
  results.push({
    case: 'JSON reload/ungroup/document switch',
    groupIdsAndEmptyRetained: true,
    ungroupOnlyMetadataOneUndo: true,
    transientGroupReset: true,
  });
  assert.deepEqual(errors, []);
  const receipt = {
    nativeWebView2: true,
    syntheticCdpInput: true,
    results,
    errors,
    notRun: [
      'physical keyboard/IME',
      'native OS Save/Open dialogs',
      'cold-process file reopen',
      'OS scaling',
      'unrelated full audits',
    ],
  };
  await writeFile(resolve(output, 'receipt.json'), JSON.stringify(receipt, null, 2));
  console.log(JSON.stringify({ pass: results.length, ...receipt }));
} catch (e) {
  if (page) await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
  throw e;
} finally {
  await browser.close();
}
