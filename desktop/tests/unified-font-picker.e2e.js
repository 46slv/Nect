import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { document as newDocument, node, serialize } from '../src/core.js';
import { makeFontAsset } from '../src/fonts.js';

// Browser/UI evidence with a deterministic native geometry adapter, not real
// font discovery, Adobe/Morisawa installation, licensing or native-font fidelity.
const face = {
  family: 'Synthetic native fixture', name: 'Regular', weight: 400,
  style: 0, stretch: 5, format: 1, color: false,
};
const fontBytes = await fs.readFile(new URL('./fixtures/fonts/NectJPFixture-Regular.otf', import.meta.url));
const asset = makeFontAsset(fontBytes.buffer.slice(fontBytes.byteOffset,
  fontBytes.byteOffset + fontBytes.byteLength), 'fixture.otf');
function fixture(patch = {}, includeAsset = false) {
  const doc = newDocument(), text = node('Text', 40, 60), output = node('Output', 450, 60);
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
page.on('pageerror', (error) => errors.push(error.message));
await page.route('**/src/storage.js', (route) => route.fulfill({
  contentType: 'text/javascript',
  body: `export * from '/src/storage.js?unified-font-original';
export const desktop = true;
export const beginRecovery = async () => 'unified-font-browser-test';
export const writeRecovery = async () => Date.now(), listRecovery = async () => [];
export const readRecovery = async () => '', acknowledgeRecovery = async () => {};
export const listSystemFonts = async () => [['system-byte-only', 'Byte-only discovery fixture', null,
  {family:'Discovery fixture',subfamily:'Regular',weightClass:400,localizedNames:['合成ディスカバリー','ＭＳ 合成']}]];
export const readSystemFont = async () => { window.unifiedByteReads = (window.unifiedByteReads || 0) + 1;
  throw Error('Normal selection must not read installed bytes'); };
export const listOsOutlineFonts = async () => [];
export const listDesktopFontFaces = async () => [${JSON.stringify(face)}];
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
}));
const picker = () => page.getByRole('dialog', { name: 'Font', exact: true });
const state = () => page.evaluate(() => ({
  document: JSON.stringify(window.nect.document),
  revision: window.nect.commandState.expectedRevision,
}));
const live = (id) => page.evaluate((nodeId) =>
  window.nect.document.layers[0].nodes.find((n) => n.id === nodeId), id);
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
  await picker().locator('.picker-option[aria-disabled="false"]')
    .filter({ hasText: label }).first().click();
};
const apply = async () => {
  await picker().locator('.picker-apply').click();
  await picker().waitFor({ state: 'hidden' });
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  let f = fixture();
  await inspect(f);
  await page.waitForFunction(() => window.nect.evaluate().error === null);
  assert.equal(await page.locator('.inspector-body [data-change-font]').count(), 1);
  assert.equal(await page.locator(`.graph .node[data-id="${f.text.id}"] [data-change-font]`).count(), 1);
  assert.equal(await page.locator('[data-os-text-font], [data-os-text-outline]').count(), 0);
  assert.equal(await page.locator('.inspector-body .text-font-advanced [data-load-font]').count(), 1);
  assert.equal(await page.locator('.inspector-body .text-actions [data-bake-text-outline]').count(), 1);
  assert.doesNotMatch(await page.locator('.inspector-body .text-controls').innerText(), /Adobe Fonts|Choose OS font/);
  const before = await state();
  await open();
  const family = () => picker().getByRole('treeitem', { name: face.family, exact: true });
  assert.equal(await picker().locator('.picker-list').getAttribute('role'), 'tree');
  assert.equal(await family().getAttribute('aria-expanded'), 'false');
  assert.ok(await picker().locator('.picker-family[aria-expanded="true"]').count());
  await family().focus(); await family().press('Enter');
  assert.equal(await family().getAttribute('aria-expanded'), 'true');
  await family().press('Space');
  assert.equal(await family().getAttribute('aria-expanded'), 'false');
  await family().press('ArrowRight');
  assert.equal(await family().getAttribute('aria-expanded'), 'true');
  await family().press('ArrowLeft');
  assert.equal(await family().getAttribute('aria-expanded'), 'false');
  assert.deepEqual(await state(), before);
  await picker().locator('.picker-cancel').click();
  assert.deepEqual(await state(), before);
  await open();
  await picker().locator('input[type="search"]').fill('合成ディスカバリー');
  const disabled = picker().locator('.picker-option');
  assert.equal(await disabled.count(), 1);
  assert.equal(await disabled.getAttribute('aria-disabled'), 'true');
  assert.match(await disabled.innerText(), /Font file import is required; use Advanced for a file you may embed/);
  assert.equal(await picker().locator('.picker-apply').isDisabled(), true);
  await picker().locator('.picker-cancel').click();
  assert.deepEqual(await state(), before);

  await open(); await choose(face.family); await apply();
  assert.equal((await live(f.text.id)).type, 'Text');
  assert.deepEqual((await live(f.text.id)).osFont.face, face);
  assert.equal((await live(f.text.id)).osFont.outline.ligatures, false);
  const native = await state();
  assert.equal(native.revision, before.revision + 1);
  assert.equal(await page.evaluate(() => window.unifiedByteReads || 0), 0);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-unified-font-native-text.png' });
  await page.locator('[data-action="undo"]').click();
  assert.equal((await state()).document, before.document);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await state()).document, native.document);
  await open(); await choose('DejaVu Sans'); await apply();
  assert.equal((await live(f.text.id)).osFont, undefined);
  assert.equal((await live(f.text.id)).font, 'default');
  const embedded = await state();
  assert.equal(embedded.revision, native.revision + 3);
  await page.locator('[data-action="undo"]').click();
  assert.equal((await state()).document, native.document);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await state()).document, embedded.document);

  f = fixture(); await inspect(f);
  const cancelBefore = await state();
  await page.evaluate(() => { window.unifiedDelay = true; window.unifiedFinish = null; });
  await open(); await choose(face.family);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.unifiedFinish === 'function');
  const pendingCalls = await page.evaluate(() => window.unifiedRequests.length);
  await page.keyboard.press('Escape');
  await picker().waitFor({ state: 'hidden' });
  await page.evaluate(() => { window.unifiedFinish(); window.unifiedDelay = false; });
  await page.waitForTimeout(50);
  assert.deepEqual(await state(), cancelBefore);
  assert.equal(await page.evaluate(() => window.unifiedRequests.length), pendingCalls);

  f = fixture({}, true); await inspect(f);
  const rapidBefore = await state();
  await page.evaluate(() => { window.unifiedDelay = true; window.unifiedFinish = null; });
  await open(); await choose(face.family);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.unifiedFinish === 'function');
  await choose(asset.asset.name);
  await page.evaluate(() => { window.unifiedFinish(); window.unifiedDelay = false; });
  await page.waitForTimeout(50);
  assert.deepEqual(await state(), rapidBefore);
  await apply();
  assert.equal((await live(f.text.id)).font, asset.id);
  assert.equal((await live(f.text.id)).osFont, undefined);
  assert.equal((await state()).revision, rapidBefore.revision + 1);

  f = fixture(); await inspect(f);
  await page.evaluate(() => { window.unifiedDelay = true; window.unifiedFinish = null; });
  await open(); await choose(face.family);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.unifiedFinish === 'function');
  await page.evaluate(({ layerId, nodeId }) => window.nect.dispatch({
    ...window.nect.commandState, type: 'set-text-content', payload: { layerId, nodeId, text: 'Newer edit' },
  }), { layerId: f.doc.layers[0].id, nodeId: f.text.id });
  const newer = await state();
  await page.evaluate(() => { window.unifiedFinish(); window.unifiedDelay = false; });
  await page.waitForTimeout(50);
  assert.deepEqual(await state(), newer);
  assert.equal((await live(f.text.id)).osFont, undefined);
  if (await picker().count()) await picker().locator('.picker-cancel').click();

  f = fixture({ fontFeatures: { liga: false } }); await inspect(f);
  const explicit = await state();
  await open(); await picker().locator('input[type="search"]').fill(face.family);
  assert.equal(await picker().locator('.picker-option').getAttribute('aria-disabled'), 'true');
  assert.match(await picker().locator('.picker-option').innerText(), /cannot preserve explicit OpenType settings/);
  await picker().locator('.picker-cancel').click();
  assert.deepEqual(await state(), explicit);

  f = fixture({ text: '日本語\n世界', writingMode: 'vertical-rl', font: asset.id }, true);
  await inspect(f);
  const verticalBefore = await state(), original = await live(f.text.id);
  await open(); await choose(face.family); await apply();
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
  await fs.writeFile('test-results/workflow-unified-font-picker.json', JSON.stringify({
    actualBrowserUi: true, mockNativeGeometryAdapter: true, actualNativeFonts: false,
    installedByteReads: 0, oneNormalFontEntry: true, familyDisclosuresKeyboardAria: true, oneUndoRedoAtomic: true,
    legacyLigaturesOffPreserved: true, verticalJapaneseIntentPreserved: true,
    canceledAndStaleResponsesIgnored: true, explicitOpenTypePreserved: true, pageErrors: errors,
  }, null, 2));
  console.log('Unified Font browser UI: exact native/document routes, no byte copy, atomic Undo and interrupted flows PASS');
} finally {
  await browser.close();
}
