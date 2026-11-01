import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { document as newDocument, node, serialize } from '../src/core.js';
import { makeFontAsset } from '../src/fonts.js';

// Browser/UI evidence with a deterministic native geometry adapter, not real
// font discovery, Adobe/Morisawa installation, licensing or native-font fidelity.
const face = {
  family: 'Synthetic native fixture',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const fontBytes = await fs.readFile(
  new URL('./fixtures/fonts/NectJPFixture-Regular.otf', import.meta.url),
);
const asset = makeFontAsset(
  fontBytes.buffer.slice(fontBytes.byteOffset, fontBytes.byteOffset + fontBytes.byteLength),
  'fixture.otf',
);
function fixture(patch = {}, includeAsset = false) {
  const doc = newDocument(),
    text = node('Text', 40, 60),
    output = node('Output', 450, 60);
  Object.assign(text, { text: 'ABC', ...patch });
  output.params.path.source = text.id;
  doc.layers[0].nodes = [text, output];
  doc.layers[0].output = output.id;
  if (includeAsset) doc.fontAssets = { [asset.id]: asset.asset };
  if (text.fontFeatures) doc.schemaVersion = Math.max(doc.schemaVersion, 38);
  return { doc, text };
}
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
let narrowTypography;
page.on('pageerror', (error) => errors.push(error.message));
await page.route('**/src/storage.js', (route) =>
  route.fulfill({
    contentType: 'text/javascript',
    body: `export * from '/src/storage.js?unified-font-original';
export const desktop = true;
export const beginRecovery = async () => 'unified-font-browser-test';
export const writeRecovery = async () => Date.now(), listRecovery = async () => [];
export const readRecovery = async () => '', acknowledgeRecovery = async () => {};
export const listSystemFonts = async () => [['system-byte-only', 'Byte-only discovery fixture', null,
  {family:'Discovery fixture',subfamily:'Regular',weightClass:400,localizedNames:['合成ディスカバリー','ＭＳ 合成']}]];
export const readSystemFont = async () => { window.unifiedByteReads = (window.unifiedByteReads || 0) + 1;
  throw Error('Browsing and native selection must not read installed bytes'); };
export const listOsOutlineFonts = async () => [];
export const listDesktopFontFaces = async () => {
  window.unifiedCatalogCalls = (window.unifiedCatalogCalls || 0) + 1;
  if (window.unifiedCatalogMode === 'failed') throw Error('Synthetic native discovery rejected');
  if (window.unifiedCatalogMode === 'empty') return [];
  if (window.unifiedCatalogMode === 'pending') return new Promise(resolve => {
    window.unifiedCatalogFinish = () => resolve([${JSON.stringify(face)}]);
  });
  return [${JSON.stringify(face)}];
};
export const listDesktopFontAxes = async () => [];
function prepared(request) {
  const modern = request.axes !== undefined || request.writingMode !== undefined;
  return {...request,engine:modern?'directwrite-v2':'directwrite-v1',
    ...(modern?{axes:[],advanceHeight:request.size*4}:{}),
    ascent:request.size*0.8,advanceWidth:request.size*2,lineCount:request.text.split('\\n').length,
    glyphCount:[...request.text].length,points:[[0,0],[20,0],[20,10],[0,10]],types:[0,1,1,129]};
}
export async function shapeDesktopText(request) {
  (window.unifiedRequests ||= []).push(structuredClone(request));
  if (window.unifiedDelay) return new Promise(resolve => {
    window.unifiedFinish = () => resolve(prepared(request));
  });
  return prepared(request);
}
export const outlineOsText = async () => { throw Error('No legacy font-name fallback'); };`,
  }),
);
// Test-only read access to the project history. Active CommandSession revisions
// restart when project Undo/Redo restores a composition; they are not a global edit count.
await page.route('**/src/main.js', async (route) => {
  const response = await route.fetch();
  const source = await response.text();
  await route.fulfill({
    response,
    body:
      source +
      '\nwindow.unifiedFontHistory = () => ({ revision: session.projectRevision, past: history.past.length, future: history.future.length });',
  });
});
const picker = () => page.getByRole('dialog', { name: 'Font', exact: true });
const state = () =>
  page.evaluate(() => ({
    document: JSON.stringify(window.nect.document),
    revision: window.unifiedFontHistory().revision,
  }));
const live = (id) =>
  page.evaluate((nodeId) => window.nect.document.layers[0].nodes.find((n) => n.id === nodeId), id);
const inspect = async (f) => {
  await page.evaluate((contents) => window.nect.load(contents), serialize(f.doc));
  await page.locator(`.graph .node[data-id="${f.text.id}"] .node-head strong`).click();
};
const open = async () => {
  await page.locator('.inspector-body [data-change-font]').click();
  await picker().waitFor();
};
const choose = async (label) => {
  await picker().locator('input[type="search"]').fill(label);
  await picker()
    .getByRole('treeitem', { name: label })
    .and(picker().locator('.picker-option[aria-disabled="false"]'))
    .first()
    .click();
};
const apply = async () => {
  await picker().locator('.picker-apply').click();
  await picker().waitFor({ state: 'hidden' });
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  let f = fixture();
  await inspect(f);
  await page.waitForFunction(() => window.nect.evaluate().error === null);
  assert.equal(await page.locator('.inspector-body [data-change-font]').count(), 1);
  assert.equal(
    await page.locator(`.graph .node[data-id="${f.text.id}"] [data-change-font]`).count(),
    1,
  );
  assert.equal(await page.locator('[data-os-text-font], [data-os-text-outline]').count(), 0);
  assert.equal(
    await page.locator('.inspector-body .text-font-advanced [data-load-font]').count(),
    1,
  );
  assert.equal(
    await page.locator('.inspector-body .text-actions [data-bake-text-outline]').count(),
    1,
  );
  assert.doesNotMatch(
    await page.locator('.inspector-body .text-controls').innerText(),
    /Adobe Fonts|Choose OS font/,
  );
  const before = await state();
  await open();
  const family = () => picker().getByRole('treeitem', { name: face.family, exact: true });
  assert.equal(await picker().locator('.picker-list').getAttribute('role'), 'tree');
  assert.equal(await family().getAttribute('aria-expanded'), 'false');
  assert.ok(await picker().locator('.picker-family[aria-expanded="true"]').count());
  await family().focus();
  await family().press('Enter');
  assert.equal(await family().getAttribute('aria-expanded'), 'true');
  await family().press('Space');
  assert.equal(await family().getAttribute('aria-expanded'), 'false');
  await family().press('ArrowRight');
  assert.equal(await family().getAttribute('aria-expanded'), 'true');
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-unified-font-family-picker.png' });
  await family().press('ArrowLeft');
  assert.equal(await family().getAttribute('aria-expanded'), 'false');
  assert.deepEqual(await state(), before);
  await picker().locator('.picker-cancel').click();
  assert.deepEqual(await state(), before);
  await open();
  await picker().locator('input[type="search"]').fill('合成ディスカバリー');
  const installed = picker().locator('.picker-option');
  assert.equal(await installed.count(), 1);
  assert.equal(await installed.getAttribute('aria-disabled'), 'false');
  assert.match(await installed.innerText(), /Included in saved document/);
  assert.equal(await picker().locator('.picker-apply').isDisabled(), false);
  assert.equal(await page.evaluate(() => window.unifiedByteReads || 0), 0);
  await picker().locator('.picker-cancel').click();
  assert.deepEqual(await state(), before);

  // Discovery has its own status; a failed native query must not look like a
  // successful empty query or disable unrelated installed-file candidates.
  await open();
  const discovery = () => picker().locator('.picker-font-discovery-status'),
    refreshFonts = () => picker().getByRole('button', { name: 'Refresh fonts', exact: true });
  assert.equal(await discovery().textContent(), '1 native font face');
  await page.evaluate(() => {
    window.unifiedCatalogMode = 'failed';
  });
  await refreshFonts().click();
  await page.waitForFunction(
    () =>
      document.querySelector('.picker-font-discovery-status')?.textContent ===
      'Native font discovery failed',
  );
  await picker().locator('input[type="search"]').fill('合成ディスカバリー');
  await picker().locator('.picker-option').click();
  const installedId = await picker()
    .locator('.picker-option[aria-selected="true"]')
    .getAttribute('data-id');
  assert.equal(await picker().locator('.picker-option').getAttribute('aria-disabled'), 'false');
  assert.equal(await picker().locator('.picker-apply').isDisabled(), false);
  assert.equal(await discovery().textContent(), 'Native font discovery failed');
  await page.screenshot({ path: 'test-results/workflow-unified-font-discovery-failed.png' });
  assert.deepEqual(await state(), before);

  await page.evaluate(() => {
    window.unifiedCatalogMode = 'empty';
  });
  await refreshFonts().click();
  await page.waitForFunction(
    () =>
      document.querySelector('.picker-font-discovery-status')?.textContent ===
      'No native font faces found',
  );
  assert.equal(
    await picker().locator('.picker-option[aria-selected="true"]').getAttribute('data-id'),
    installedId,
  );
  assert.equal(await picker().locator('.picker-apply').isDisabled(), false);
  await page.screenshot({ path: 'test-results/workflow-unified-font-discovery-empty.png' });
  assert.deepEqual(await state(), before);

  const catalogCalls = await page.evaluate(() => window.unifiedCatalogCalls);
  await page.evaluate(() => {
    window.unifiedCatalogMode = 'pending';
  });
  await refreshFonts().click();
  await page.waitForFunction(() => !!window.unifiedCatalogFinish);
  assert.equal(await discovery().textContent(), 'Checking native fonts…');
  assert.equal(await refreshFonts().isDisabled(), true);
  assert.equal(await picker().locator('.picker-apply').isDisabled(), false);
  // Even programmatic repeated clicks use the same existing pending guard.
  await refreshFonts().evaluate((button) => {
    button.click();
    button.click();
  });
  assert.equal(await page.evaluate(() => window.unifiedCatalogCalls), catalogCalls + 1);
  await page.screenshot({ path: 'test-results/workflow-unified-font-discovery-checking.png' });
  await page.evaluate(() => {
    window.unifiedCatalogMode = 'available';
    window.unifiedCatalogFinish();
  });
  await page.waitForFunction(
    () =>
      document.querySelector('.picker-font-discovery-status')?.textContent === '1 native font face',
  );
  assert.equal(await refreshFonts().isDisabled(), false);
  assert.equal(
    await picker().locator('.picker-option[aria-selected="true"]').getAttribute('data-id'),
    installedId,
  );
  assert.equal(await picker().locator('.picker-apply').isDisabled(), false);
  assert.equal(await page.evaluate(() => window.unifiedByteReads || 0), 0);
  assert.deepEqual(await state(), before);
  await picker().locator('.picker-cancel').click();

  await open();
  await choose(face.family);
  await apply();
  assert.equal((await live(f.text.id)).type, 'Text');
  assert.deepEqual((await live(f.text.id)).osFont.face, face);
  assert.equal((await live(f.text.id)).osFont.outline.ligatures, false);
  const native = await state();
  assert.equal(native.revision, before.revision + 1);
  assert.equal(await page.evaluate(() => window.unifiedByteReads || 0), 0);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-unified-font-native-text.png' });
  await page.setViewportSize({ width: 1024, height: 900 });
  narrowTypography = await page.evaluate(() => {
    const panel = document.querySelector('.inspector-body');
    const typography = panel.querySelector('.native-text-typography');
    const bounds = (element) => {
      const r = element.getBoundingClientRect();
      return { left: r.left, right: r.right, top: r.top, bottom: r.bottom, width: r.width };
    };
    return {
      panel: bounds(panel),
      stage: bounds(document.querySelector('.canvas-stage')),
      fieldset: bounds(typography),
      clientWidth: typography.clientWidth,
      scrollWidth: typography.scrollWidth,
      rows: [...typography.querySelectorAll(':scope > label')].map((label) => ({
        label: label.querySelector('span').textContent.trim(),
        row: bounds(label),
        text: bounds(label.querySelector('span')),
        control: bounds(label.querySelector('input, select')),
      })),
    };
  });
  assert.ok(
    narrowTypography.panel.width >= 220 &&
      narrowTypography.panel.width <= narrowTypography.stage.width / 2,
  );
  assert.ok(
    narrowTypography.panel.left >= narrowTypography.stage.left &&
      narrowTypography.panel.right <= narrowTypography.stage.right,
  );
  assert.ok(narrowTypography.scrollWidth <= narrowTypography.clientWidth + 1);
  assert.ok(narrowTypography.fieldset.right <= narrowTypography.panel.right + 1);
  assert.equal(narrowTypography.rows.length, 8);
  for (const [index, row] of narrowTypography.rows.entries()) {
    assert.ok(row.label);
    assert.ok(row.control.left >= row.row.left - 1);
    assert.ok(row.control.right <= row.row.right + 1);
    const overlap =
      Math.min(row.text.right, row.control.right) - Math.max(row.text.left, row.control.left);
    assert.ok(overlap <= 1, `${row.label} text overlaps its control`);
    if (index) assert.ok(narrowTypography.rows[index - 1].row.bottom <= row.row.top + 1);
  }
  await page.locator('.inspector').screenshot({
    path: 'test-results/workflow-unified-font-native-inspector-narrow.png',
  });
  assert.deepEqual(await state(), native);
  await page.setViewportSize({ width: 1600, height: 1000 });
  await page.locator('[data-action="undo"]').click();
  assert.equal((await state()).document, before.document);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await state()).document, native.document);
  await open();
  await choose('DejaVu Sans');
  await apply();
  assert.equal((await live(f.text.id)).osFont, undefined);
  assert.equal((await live(f.text.id)).font, 'default');
  const embedded = await state();
  assert.equal(embedded.revision, native.revision + 3);
  await page.locator('[data-action="undo"]').click();
  assert.equal((await state()).document, native.document);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await state()).document, embedded.document);

  f = fixture();
  await inspect(f);
  const cancelBefore = await state();
  await page.evaluate(() => {
    window.unifiedDelay = true;
    window.unifiedFinish = null;
  });
  await open();
  await choose(face.family);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.unifiedFinish === 'function');
  const pendingCalls = await page.evaluate(() => window.unifiedRequests.length);
  await page.keyboard.press('Escape');
  await picker().waitFor({ state: 'hidden' });
  await page.evaluate(() => {
    window.unifiedFinish();
    window.unifiedDelay = false;
  });
  await page.waitForTimeout(50);
  assert.deepEqual(await state(), cancelBefore);
  assert.equal(await page.evaluate(() => window.unifiedRequests.length), pendingCalls);

  f = fixture({}, true);
  await inspect(f);
  const rapidBefore = await state();
  await page.evaluate(() => {
    window.unifiedDelay = true;
    window.unifiedFinish = null;
  });
  await open();
  await choose(face.family);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.unifiedFinish === 'function');
  await choose(asset.asset.name);
  await page.evaluate(() => {
    window.unifiedFinish();
    window.unifiedDelay = false;
  });
  await page.waitForTimeout(50);
  assert.deepEqual(await state(), rapidBefore);
  await apply();
  assert.equal((await live(f.text.id)).font, asset.id);
  assert.equal((await live(f.text.id)).osFont, undefined);
  assert.equal((await state()).revision, rapidBefore.revision + 1);

  f = fixture();
  await inspect(f);
  await page.evaluate(() => {
    window.unifiedDelay = true;
    window.unifiedFinish = null;
  });
  await open();
  await choose(face.family);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.unifiedFinish === 'function');
  await page.evaluate(
    ({ layerId, nodeId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'set-text-content',
        payload: { layerId, nodeId, text: 'Newer edit' },
      }),
    { layerId: f.doc.layers[0].id, nodeId: f.text.id },
  );
  const newer = await state();
  await page.evaluate(() => {
    window.unifiedFinish();
    window.unifiedDelay = false;
  });
  await page.waitForTimeout(50);
  assert.deepEqual(await state(), newer);
  assert.equal((await live(f.text.id)).osFont, undefined);
  if (await picker().count()) await picker().locator('.picker-cancel').click();

  f = fixture({ fontFeatures: { liga: false } });
  await inspect(f);
  const explicit = await state();
  await open();
  await picker().locator('input[type="search"]').fill(face.family);
  assert.equal(await picker().locator('.picker-option').getAttribute('aria-disabled'), 'true');
  assert.match(
    await picker().locator('.picker-option').innerText(),
    /cannot preserve explicit OpenType settings/,
  );
  await picker().locator('.picker-cancel').click();
  assert.deepEqual(await state(), explicit);

  f = fixture({ text: '日本語\n世界', writingMode: 'vertical-rl', font: asset.id }, true);
  await inspect(f);
  const verticalBefore = await state(),
    original = await live(f.text.id);
  await open();
  await choose(face.family);
  await apply();
  const vertical = await live(f.text.id);
  assert.equal(vertical.type, 'Text');
  assert.equal(vertical.text, original.text);
  assert.equal(vertical.writingMode, 'vertical-rl');
  assert.deepEqual(vertical.params, original.params);
  assert.equal(vertical.osFont.outline.ligatures, false);
  await page.screenshot({ path: 'test-results/workflow-unified-font-vertical-text.png' });
  await page.locator('[data-action="undo"]').click();
  assert.equal((await state()).document, verticalBefore.document);
  assert.equal(await page.evaluate(() => window.unifiedByteReads || 0), 0);
  assert.deepEqual(errors, []);
  await fs.writeFile(
    'test-results/workflow-unified-font-picker.json',
    JSON.stringify(
      {
        actualBrowserUi: true,
        mockNativeGeometryAdapter: true,
        actualNativeFonts: false,
        narrowTypography,
        installedByteReads: 0,
        oneNormalFontEntry: true,
        nativeDiscoveryFailureEmptyPendingCountDistinct: true,
        nativeDiscoveryRefreshRecovery: true,
        nativeDiscoveryDoesNotGateInstalledApply: true,
        familyDisclosuresKeyboardAria: true,
        oneUndoRedoAtomic: true,
        legacyLigaturesOffPreserved: true,
        verticalJapaneseIntentPreserved: true,
        canceledAndStaleResponsesIgnored: true,
        explicitOpenTypePreserved: true,
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'Unified Font browser UI: exact native/document routes, no byte copy, atomic Undo and interrupted flows PASS',
  );
} finally {
  await browser.close();
}
