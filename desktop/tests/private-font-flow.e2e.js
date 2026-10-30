import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { document as newDocument, node, serialize } from '../src/core.js';

// Real app UI with synthetic native adapter only: this is not a real-font,
// DirectWrite, licensing, or native file-dialog fidelity test.
const face = {
  family: 'Synthetic collection',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const hash = 'a'.repeat(64),
  otherHash = 'b'.repeat(64);
const file = {
  token: 'opaque-session-token',
  sha256: hash,
  faces: [0, 1].map((faceIndex) => ({ faceIndex, face })),
};
const other = { token: 'other-session-token', sha256: otherHash, faces: [{ faceIndex: 1, face }] };
const ref = { kind: 'file-native-v1', sha256: hash, faceIndex: 1 };
const doc = newDocument(),
  text = node('Text', 40, 60),
  output = node('Output', 450, 60);
text.text = 'ABC';
output.params.path.source = text.id;
doc.layers[0].nodes = [text, output];
doc.layers[0].output = output.id;
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.route('**/src/storage.js', (route) =>
  route.fulfill({
    contentType: 'text/javascript',
    body: `
export * from '/src/storage.js?private-font-original';
export const desktop = true;
export const beginRecovery = async () => 'private-font-test', writeRecovery = async () => Date.now();
export const listRecovery = async () => [], readRecovery = async () => '', acknowledgeRecovery = async () => {};
export const listSystemFonts = async () => [], listOsOutlineFonts = async () => [];
export const listDesktopFontFaces = async () => [], listDesktopFontAxes = async () => [];
export const shapeDesktopText = async () => { throw Error('Installed fallback must not run'); };
export const outlineOsText = async () => { throw Error('Legacy fallback must not run'); };
export const openPrivateFontFile = async () => { window.privateOpens = (window.privateOpens || 0) + 1; return window.privateNextFile ?? null; };
export const listPrivateFontAxes = async () => [];
export async function shapePrivateFontText(token, faceIndex, request) {
  (window.privateCalls ||= []).push({token, faceIndex, request: structuredClone(request)});
  const sha256 = token === ${JSON.stringify(file.token)} ? ${JSON.stringify(hash)} : ${JSON.stringify(otherHash)};
  const source = {kind:'file-native-v1',sha256,faceIndex:window.privateMismatch ? 0 : faceIndex};
  const modern = request.axes !== undefined || request.writingMode !== undefined;
  const outline = {...request,engine:modern?'directwrite-v2':'directwrite-v1',
    ...(modern?{axes:[],advanceHeight:request.size*4}:{}),ascent:request.size*0.8,
    advanceWidth:request.size*2,lineCount:request.text.split('\\n').length,
    glyphCount:[...request.text].length,points:[[0,0],[20,0],[20,10],[0,10]],types:[0,1,1,129]};
  if (window.privateDelay) return new Promise(resolve => { window.privateFinish = () => resolve({source,outline}); });
  return {source,outline};
}
`,
  }),
);
await page.route('**/src/main.js', async (route) => {
  const response = await route.fetch();
  await route.fulfill({
    response,
    body:
      (await response.text()) +
      '\nwindow.privateHistory = () => ({ past: history.past.length, future: history.future.length });',
  });
});
const picker = () => page.getByRole('dialog', { name: 'Font', exact: true });
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
const live = () =>
  page.evaluate((id) => window.nect.document.layers[0].nodes.find((n) => n.id === id), text.id);
const selectText = () =>
  page.locator(`.graph .node[data-id="${text.id}"] .node-head strong`).click();
const openFile = async (candidate) => {
  await page.evaluate((value) => {
    window.privateNextFile = value;
  }, candidate);
  await page.locator('.inspector-body [data-open-private-font]').click();
};
const chooseIndex = async (index) => {
  await picker().locator('input[type="search"]').fill(face.family);
  const rows = picker().locator(`.picker-option[aria-disabled="false"][data-id*="${hash}"]`);
  assert.equal(await rows.count(), 2);
  const keys = await rows.evaluateAll((els) => els.map((e) => e.dataset.id));
  assert.notEqual(keys[0], keys[1]);
  assert.ok(keys.every((key) => !key.includes(file.token)));
  await rows.nth(index).click();
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((contents) => window.nect.load(contents), serialize(doc));
  await selectText();
  const before = await snapshot();
  const past = await page.evaluate(() => window.privateHistory().past);
  await openFile(null);
  await page.waitForFunction(() => window.privateOpens === 1);
  assert.equal(await picker().count(), 0);
  assert.equal(await snapshot(), before);
  assert.equal(await page.evaluate(() => window.privateHistory().past), past);

  await openFile(file);
  await picker().waitFor();
  await chooseIndex(1);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-private-font-picker.png' });
  await page.evaluate(() => {
    window.privateMismatch = true;
  });
  await picker().locator('.picker-apply').click();
  await picker()
    .getByRole('status')
    .filter({ hasText: /did not match/i })
    .waitFor();
  assert.equal(await snapshot(), before);
  assert.equal(await page.evaluate(() => window.privateHistory().past), past);
  await page.evaluate(() => {
    window.privateMismatch = false;
  });
  await chooseIndex(1);
  await picker().locator('.picker-apply').click();
  await picker().waitFor({ state: 'hidden' });
  assert.deepEqual((await live()).osFont.source, ref);
  assert.equal(await page.evaluate(() => window.privateHistory().past), past + 1);
  const applied = await snapshot();
  assert.ok(!applied.includes(file.token));
  await page.locator('[data-action="undo"]').click();
  assert.equal(await snapshot(), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await snapshot(), applied);

  // A save/load round-trip restores geometry but not process capabilities.
  const saved = serialize(JSON.parse(applied));
  assert.ok(!saved.includes(file.token));
  await page.evaluate((contents) => window.nect.load(contents), saved);
  await selectText();
  assert.equal(await snapshot(), applied);
  await page
    .locator('.inspector-body .text-font-choice')
    .getByRole('status')
    .filter({ hasText: /missing/i })
    .waitFor();
  await page.screenshot({ path: 'test-results/workflow-private-font-missing.png' });
  const callsBefore = await page.evaluate(() => window.privateCalls.length);
  await page.locator('.inspector-body [data-os-text]').fill('Changed');
  await page.locator('.inspector-body [data-apply-os-text]').click();
  await page
    .getByText(/Font file missing; reattach the same file/i)
    .last()
    .waitFor();
  assert.equal(await snapshot(), applied);
  assert.equal(await page.evaluate(() => window.privateCalls.length), callsBefore);
  await page.locator('.inspector-body [data-cancel-os-text]').click();

  await openFile(other);
  await picker().waitFor();
  await picker().locator('.picker-cancel').click();
  await page
    .locator('.inspector-body .text-font-choice')
    .getByRole('status')
    .filter({ hasText: /missing/i })
    .waitFor();
  assert.equal(await snapshot(), applied);
  await openFile(file);
  await picker().waitFor();
  await picker().locator('.picker-cancel').click();
  await page
    .locator('.inspector-body .text-font-choice')
    .getByRole('status')
    .filter({ hasText: /attached/i })
    .waitFor();
  await page.screenshot({ path: 'test-results/workflow-private-font-reattached.png' });
  assert.equal(await snapshot(), applied);

  // A delayed native result cannot apply after a newer document command.
  await page.evaluate(() => {
    window.privateDelay = true;
    window.privateFinish = null;
  });
  await page.locator('.inspector-body [data-change-font]').click();
  await picker().waitFor();
  await chooseIndex(1);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.privateFinish === 'function');
  await page.evaluate(
    ({ layerId, nodeId }) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'rename',
        payload: { layerId, nodeId, name: 'Newer name' },
      }),
    { layerId: doc.layers[0].id, nodeId: text.id },
  );
  const newer = await snapshot();
  await page.evaluate(() => {
    window.privateFinish();
    window.privateDelay = false;
  });
  await picker()
    .getByRole('status')
    .filter({ hasText: /changed/i })
    .waitFor();
  assert.equal(await snapshot(), newer);
  assert.deepEqual((await live()).osFont.source, ref);
  await picker().locator('.picker-cancel').click();

  await page.evaluate(() => {
    window.privateDelay = true;
    window.privateFinish = null;
  });
  await page.locator('.inspector-body [data-change-font]').click();
  await picker().waitFor();
  await chooseIndex(1);
  await picker().locator('.picker-apply').click();
  await page.waitForFunction(() => typeof window.privateFinish === 'function');
  const selectionBefore = await snapshot();
  await page.evaluate((id) => {
    const head = document.querySelector(`.graph .node[data-id="${id}"] .node-head`);
    head.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, pointerId: 1, button: 0 }));
    document.dispatchEvent(
      new PointerEvent('pointerup', { bubbles: true, pointerId: 1, button: 0 }),
    );
  }, output.id);
  await page.evaluate(() => {
    window.privateFinish();
    window.privateDelay = false;
  });
  await picker()
    .getByRole('status')
    .filter({ hasText: /changed/i })
    .waitFor();
  assert.equal(await snapshot(), selectionBefore, 'selection change must reject late preparation');
  assert.deepEqual((await live()).osFont.source, ref);
  assert.deepEqual(errors, []);
  await fs.writeFile(
    'test-results/workflow-private-font-receipt.json',
    JSON.stringify(
      {
        browser: `Chromium ${browser.version()}`,
        viewport: page.viewportSize(),
        sourceIdentity: ref,
        exercised: [
          'chooser cancellation',
          'distinct collection members',
          'mismatched source rejection and same-row retry',
          'atomic apply, undo and redo',
          'saved geometry, missing capability and explicit reattachment',
          'wrong-file refusal and missing-capability edit refusal',
          'stale revision and selection rejection',
        ],
        syntheticAdapter: true,
        limitation:
          'Synthetic native responses; no real font, DirectWrite, file-dialog or licensing fidelity is established.',
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
  console.log(
    'Private Font browser UI: synthetic picker, identity, history, reattachment and stale response PASS',
  );
} finally {
  await browser.close();
}
