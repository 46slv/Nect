import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { serialize } from '../src/core.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/key-color-recipe-panel');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({
  viewport: { width: 1440, height: 1000 },
  acceptDownloads: true,
});
page.setDefaultTimeout(30000);
const errors = [],
  results = [];
page.on('pageerror', (error) => errors.push(error.message));
// This acceptance uses only synthetic color/shape fixtures. Do not read font
// bytes, OS font catalogs or any user's artwork/document files.
await page.route(/\.(?:ttf|otf|ttc|otc|woff2?)(?:\?|$)/i, (route) => route.abort());
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const command = () => page.evaluate(() => window.nect.commandState);
const load = async (doc) => {
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  await ensureWorkspacePanel(page, 'colors');
};
const open = async (keyboard = false) => {
  await ensureWorkspacePanel(page, 'colors');
  const launcher = page.locator('[data-key-color-recipe-open]');
  if (keyboard) await launcher.press('Enter');
  else await launcher.click();
  await page.locator('dialog[data-key-color-recipe-panel]').waitFor();
  assert.equal(
    await page
      .locator('dialog[data-key-color-recipe-panel]')
      .evaluate((el) => el.matches(':modal')),
    true,
  );
};
const field = (kind) => page.locator(`[data-key-color-input="${kind}"]`);
const preview = () => page.locator('[data-key-color-preview]').click();
const cancel = () => page.locator('dialog footer [data-key-color-cancel]').click();
const unchanged = async (doc, state) => {
  assert.deepEqual(await snapshot(), doc);
  assert.deepEqual(await command(), state);
};
try {
  const baseURL = process.env.BASE_URL || 'http://127.0.0.1:5173';
  await preloadVariableFontDependency(page, baseURL);
  await page.goto(baseURL);
  await page.waitForFunction(() => window.nect);
  const blank = await snapshot(),
    blankState = await command();
  await open(true);
  assert.match(
    await page.locator('[data-key-color-empty]').textContent(),
    /does not create a preset/,
  );
  assert.equal(await page.locator('[data-key-color-input]').count(), 0);
  await cancel();
  await unchanged(blank, blankState);
  results.push('blank opt-in explanatory state: no recipe/bootstrap/default/history change');

  const fixture = retainedRecipeFixture();
  await load(fixture);
  const before = await snapshot(),
    beforeState = await command();
  await open(true);
  assert.equal(await field('hex').evaluate((el) => el === document.activeElement), true);
  assert.equal(await field('hex').getAttribute('type'), 'text');
  assert.equal(await field('hex').inputValue(), '#aBcD09');
  assert.equal(await field('alpha').inputValue(), '0');
  assert.equal(await page.locator('[data-key-color-dormant] input').count(), 0);
  assert.equal(await page.locator('dialog details input,dialog details select').count(), 0);
  await field('hex').fill('#DeAd09');
  await field('alpha').fill('0');
  await field('deltaL').fill('-0.125');
  await field('deltaRatioCmax').fill('3.375');
  await field('deltaRatioCmax').press('Tab');
  await unchanged(before, beforeState);
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  await preview();
  await unchanged(before, beforeState);
  assert.equal(await page.locator('[data-key-color-compare]').count(), 2);
  assert.equal(
    await page.locator('[data-key-color-compare="key"] [data-key-color-before] code').textContent(),
    '#123456',
  );
  assert.equal(
    await page.locator('[data-key-color-compare="key"] [data-key-color-after] code').textContent(),
    '#DeAd09',
  );
  assert.match(
    await page.locator('[data-key-color-compare="key"] [data-key-color-after]').textContent(),
    /α 0$/,
  );
  assert.equal(await page.locator('[data-key-color-compare="dormant"]').count(), 0);
  await page.screenshot({ path: resolve(output, 'preview-desktop.png'), fullPage: true });
  await field('deltaL').fill('');
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  assert.equal(await page.locator('[data-key-color-preview]').isDisabled(), true);
  assert.equal(await page.locator('[data-key-color-compare]').count(), 0);
  await field('deltaL').fill('-0.125');
  await preview();
  await cancel();
  await unchanged(before, beforeState);
  await open();
  await field('hex').fill('#fFaA00');
  await preview();
  await field('hex').press('Escape');
  assert.equal(await page.locator('dialog[data-key-color-recipe-panel]').count(), 0);
  assert.equal(
    await page
      .locator('[data-key-color-recipe-open]')
      .evaluate((el) => el === document.activeElement),
    true,
  );
  await unchanged(before, beforeState);
  results.push(
    'native modal + keyboard; mixed-case HEX/Alpha0; detached input/blur/Preview; invalid sibling revokes old preview; Cancel/Escape unchanged',
  );

  await open();
  await field('hex').fill('#DeAd09');
  await field('deltaL').fill('-0.125');
  await field('deltaRatioCmax').fill('3.375');
  await preview();
  await page.locator('[data-key-color-apply]').click();
  assert.equal(await page.locator('dialog[data-key-color-recipe-panel]').count(), 0);
  const after = await snapshot(),
    afterState = await command();
  assert.equal(afterState.expectedRevision, beforeState.expectedRevision + 1);
  assert.deepEqual(after.colorSlots.find((slot) => slot.id === 'key').value, {
    hex: '#DeAd09',
    alpha: 0,
  });
  assert.equal(after.keyColorRecipe.committedSnapshot.intent.slots[0].deltaL, -0.125);
  assert.equal(after.keyColorRecipe.committedSnapshot.intent.slots[0].deltaRatioCmax, 3.375);
  assert.equal(after.colorSlots.find((slot) => slot.id === 'derived').value.alpha, 1);
  assert.deepEqual(after.colorSlots.slice(2), before.colorSlots.slice(2));
  assert.deepEqual(after.colorDesign, before.colorDesign);
  assert.deepEqual(after.compositions, before.compositions);
  assert.deepEqual(
    after.keyColorRecipe.committedSnapshot.resolved.find((row) => row.id === 'dormant'),
    before.keyColorRecipe.committedSnapshot.resolved.find((row) => row.id === 'dormant'),
  );
  assert.match(await page.locator('.status .message').textContent(), /Apply accepted/);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  assert.equal(
    await page.locator('[data-action="undo"]').isDisabled(),
    true,
    'one Undo removes the entire Apply',
  );
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), after);
  results.push(
    'actual main-dispatch Apply: one Session revision/Undo for literals and receipt; all legacy data, IDs, references and dormant rows preserved',
  );

  // Real browser Save download, app reload, and file-input Open of that exact
  // generated native-format JSON. Not Windows Tauri file-dialog acceptance.
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save');
  const file = resolve(output, 'saved-experimental-recipe.json');
  await (await downloading).saveAs(file);
  const saved = JSON.parse(await readFile(file, 'utf8'));
  assert.deepEqual(saved, after);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const dirtyAfterRestart = await page.evaluate(() => window.nect.fileState.dirty);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  if (dirtyAfterRestart) await page.locator('dialog[open] button[value="discard"]').click();
  await (await chooser).setFiles(file);
  await page.waitForFunction(() => !!window.nect.document.keyColorRecipe);
  assert.deepEqual(await snapshot(), after);
  await open();
  assert.equal(await field('hex').inputValue(), '#DeAd09');
  assert.equal(await field('alpha').inputValue(), '0');
  await cancel();
  results.push(
    'Save download → app reload → native JSON file-input Open → retained recipe re-edit verified',
  );

  await load(fixture);
  await page.locator('[data-wheel-chroma-mode]').selectOption('override');
  const legacyBefore = await snapshot(),
    legacyState = await command();
  await page.locator('[data-wheel-chroma]').fill('0.03');
  await page.waitForFunction(() => !!window.nect.colorPreviewSnapshot());
  await open();
  assert.equal(await page.evaluate(() => window.nect.colorPreviewSnapshot()), null);
  assert.equal(await page.evaluate(() => window.nect.gestureState), null);
  await unchanged(legacyBefore, legacyState);
  await cancel();
  await unchanged(legacyBefore, legacyState);
  results.push(
    'real pointer launcher cancels pending legacy wheel gesture before native blur/change; no auto-commit',
  );

  await load(fixture);
  await open();
  await preview();
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-color-slot',
      payload: { slotId: 'other', value: { hex: '#010203', alpha: 1 } },
    }),
  );
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  assert.match(await page.locator('[data-key-color-status]').textContent(), /stale/);
  await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo', payload: {} }),
  );
  assert.equal(await page.locator('[data-key-color-preview]').isDisabled(), true);
  await cancel();
  results.push(
    'concurrent public Session edit + Undo invalidate open envelope; no implicit rebase',
  );

  const locked = retainedRecipeFixture();
  for (const row of locked.keyColorRecipe.committedSnapshot.resolved.filter((row) => row.active))
    locked.colorSlots.find((slot) => slot.id === row.id).value = { ...row.value };
  locked.colorDesign.lockedSlotIds.push('derived');
  await load(locked);
  const lockedBefore = await snapshot(),
    lockedState = await command();
  await open();
  await preview();
  assert.match(
    await page.locator('[data-key-color-status]').textContent(),
    /KEY_COLOR_TARGET_LOCKED/,
  );
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  await unchanged(lockedBefore, lockedState);
  await cancel();
  results.push(
    'locked active target rejects the whole equal-literal Apply; no unlock/skip/coercion',
  );

  const alpha = retainedRecipeFixture(),
    old = alpha.keyColorRecipe.committedSnapshot;
  alpha.keyColorRecipe.committedSnapshot = editKeyColorRecipe(old, {
    type: 'preset',
    expectedRevision: old.revision,
    preset: {
      ...old.intent.preset,
      slots: old.intent.preset.slots.map((slot) => ({ ...slot, alpha: 0.5 })),
    },
  });
  await load(alpha);
  const alphaBefore = await snapshot(),
    alphaState = await command();
  await open();
  await preview();
  assert.match(
    await page.locator('[data-key-color-status]').textContent(),
    /KEY_COLOR_DERIVED_ALPHA/,
  );
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  await unchanged(alphaBefore, alphaState);
  await cancel();
  results.push(
    'nonopaque active derived retained sample is displayed read-only and rejected, never normalized',
  );

  await page.setViewportSize({ width: 420, height: 760 });
  await load(fixture);
  await open();
  await preview();
  const bounds = await page.locator('dialog').boundingBox();
  assert.ok(bounds.x >= 0 && bounds.x + bounds.width <= 420);
  assert.ok(bounds.y >= 0 && bounds.y + bounds.height <= 760);
  await page.screenshot({ path: resolve(output, 'preview-narrow.png'), fullPage: true });
  await cancel();
  results.push('420px narrow modal remains bounded, scrollable and cancellable');
  assert.deepEqual(errors, []);
  await writeFile(
    resolve(output, 'results.json'),
    JSON.stringify(
      {
        results,
        pageErrors: errors,
        limits: [
          'Windows Tauri dialogs/WebView2 not run',
          'font requests blocked; no user document access',
          'explicit synthetic experimental fixture, not product defaults',
        ],
      },
      null,
      2,
    ) + '\n',
  );
  console.log(`Key color recipe browser acceptance passed: ${results.length} cases`);
} finally {
  await browser.close();
}
