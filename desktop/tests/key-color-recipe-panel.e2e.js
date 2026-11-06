import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, node, connect, serialize } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/key-color-recipe-panel');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
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
  assert.match(await page.locator('[data-key-color-empty]').textContent(), /Research candidates/);
  assert.equal(await page.locator('[data-research-hex]').inputValue(), '');
  assert.equal(await page.locator('[data-research-preview]').isDisabled(), true);
  assert.equal(await page.locator('[data-research-create]').isDisabled(), true);
  await cancel();
  await unchanged(blank, blankState);
  results.push('blank research draft: no recipe/bootstrap/default/history change');

  // Explicit linked B trial, with a separate legacy color and a real Fill.
  const liveFixture = blankDocument();
  const liveLayer = liveFixture.layers[0],
    liveShape = node('Rectangle'),
    liveFill = node('Fill');
  liveShape.params.center.value = [250, 270];
  liveShape.params.size.value = [180, 180];
  const liveOutput = liveLayer.nodes[0];
  liveLayer.nodes.push(liveShape, liveFill);
  connect(liveLayer, liveShape.id, liveFill.id, 'path');
  connect(liveLayer, liveFill.id, liveOutput.id, 'path');
  const harmonyLayer = blankLayer('Legacy palette example'),
    harmonyShape = node('Rectangle'),
    harmonyFill = node('Fill');
  const harmonyOutput = harmonyLayer.nodes[0];
  harmonyShape.params.center.value = [650, 270];
  harmonyShape.params.size.value = [180, 180];
  harmonyLayer.nodes.push(harmonyShape, harmonyFill);
  connect(harmonyLayer, harmonyShape.id, harmonyFill.id, 'path');
  connect(harmonyLayer, harmonyFill.id, harmonyOutput.id, 'path');
  liveFixture.layers.push(harmonyLayer);
  await load(liveFixture);
  await page.locator('[data-shared-color-add]').click();
  const legacyId = (await page.evaluate(() => window.nect.document)).colorSlots[0].id;
  await open();
  await page.locator('[data-research-hex]').fill('#3273c7');
  await page.locator('[data-research-alpha]').fill('1');
  await page.locator('[data-research-plan]').selectOption('research-neutral-start');
  assert.equal(await page.locator('[data-research-live-warning]').isVisible(), false);
  await page.locator('[data-research-live]').check();
  assert.equal(await page.locator('[data-research-live-warning]').isVisible(), true);
  assert.match(
    await page.locator('[data-research-live-warning]').textContent(),
    /Older Nect versions cannot open/,
  );
  assert.match(await page.locator('[data-research-live-warning]').textContent(), /after linking/);
  await page.locator('[data-research-preview]').click();
  await page.screenshot({ path: resolve(output, 'linked-trial-preview.png'), fullPage: true });
  await page.setViewportSize({ width: 420, height: 820 });
  const narrowLive = await page.locator('dialog[data-key-color-recipe-panel]').boundingBox();
  assert.ok(narrowLive.x >= 0 && narrowLive.x + narrowLive.width <= 420);
  assert.equal(await page.locator('[data-research-create]').isEnabled(), true);
  await page.screenshot({ path: resolve(output, 'linked-trial-narrow.png'), fullPage: true });
  await page.setViewportSize({ width: 1440, height: 1000 });
  assert.equal(await page.locator('[data-research-create]').isEnabled(), true);
  await page.locator('[data-research-create]').click();
  let liveDoc = await page.evaluate(() => window.nect.document);
  assert.equal(liveDoc.keyColorRecipe.mode, 'live');
  const liveKey = liveDoc.keyColorRecipe.committedSnapshot.intent.keyColor.id;
  const liveDerived = liveDoc.keyColorRecipe.committedSnapshot.intent.preset.slots[0].id;
  const advancedLive = page.locator('[data-color-advanced]');
  if (!(await advancedLive.evaluate((el) => el.open)))
    await advancedLive.locator('summary').click();
  await page
    .locator(`[data-shared-color-row="${liveKey}"] [data-live-color-note]`)
    .scrollIntoViewIfNeeded();
  await page.screenshot({ path: resolve(output, 'linked-trial-mixed.png'), fullPage: true });
  assert.equal(
    await page
      .locator(`[data-shared-color-row="${liveKey}"] [data-shared-color-edit="hex"]`)
      .isDisabled(),
    true,
  );
  assert.match(await page.locator(`[data-shared-color-row="${liveKey}"]`).textContent(), /Linked/);
  const priorPacket = structuredClone(liveDoc.keyColorRecipe);
  await page
    .locator(`[data-shared-color-row="${legacyId}"] [data-shared-color-edit="hex"]`)
    .fill('#456789');
  liveDoc = await page.evaluate(() => window.nect.document);
  assert.equal(liveDoc.colorSlots.find((slot) => slot.id === legacyId).value.hex, '#456789');
  assert.deepEqual(liveDoc.keyColorRecipe, priorPacket);
  const beforeLegacyChroma = await command();
  await page.locator('[data-chroma-strength]').fill('0.37');
  await page.locator('[data-chroma-strength]').press('Tab');
  await page.waitForFunction(
    (revision) => window.nect.commandState.expectedRevision > revision,
    beforeLegacyChroma.expectedRevision,
  );
  assert.deepEqual((await page.evaluate(() => window.nect.document)).keyColorRecipe, priorPacket);
  await page
    .locator(`[data-shared-color-row="${liveDerived}"] [data-shared-color-copy-toggle]`)
    .click();
  await page
    .locator(`[data-shared-color-row="${liveDerived}"] [data-shared-color-copy="reference"]`)
    .click();
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${liveLayer.id}"]`).click();
  await page.locator(`.graph .node[data-id="${liveFill.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  await page
    .locator(`.inspector-body .field[data-node="${liveFill.id}"][data-key="color"]`)
    .getByRole('button', { name: /Paste link into/ })
    .click();
  assert.equal(
    (await page.evaluate(() => window.nect.document)).layers
      .find((layer) => layer.id === liveLayer.id)
      .nodes.find((entry) => entry.id === liveFill.id).params.color.colorSlotId,
    liveDerived,
  );
  await ensureWorkspacePanel(page, 'colors');
  await page.locator('[data-harmony-create]').click();
  const withHarmony = await page.evaluate(() => window.nect.document);
  assert.equal(withHarmony.colorDesign.version, 2);
  const harmonyId = withHarmony.colorDesign.palette[0].id;
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${harmonyLayer.id}"]`).click();
  await page.locator(`.graph .node[data-id="${harmonyFill.id}"] .node-head strong`).click();
  await ensureWorkspacePanel(page, 'inspector');
  await page
    .locator(
      `.inspector-body .field[data-node="${harmonyFill.id}"][data-key="color"] [data-shared-color-binding]`,
    )
    .selectOption(harmonyId);
  await ensureWorkspacePanel(page, 'colors');
  const harmonyAdvanced = page.locator('[data-color-advanced]');
  if (!(await harmonyAdvanced.evaluate((el) => el.open)))
    await harmonyAdvanced.locator('summary').click();
  await page.waitForFunction(
    ({ liveDerived, harmonyId }) => {
      const value = (id) =>
        Number.parseFloat(document.querySelector(`[data-color-coverage-id="${id}"]`)?.textContent);
      return value(liveDerived) > 0 && value(harmonyId) > 0;
    },
    { liveDerived, harmonyId },
  );
  const beforeHarmonyEdit = await page.evaluate(() => window.nect.document);
  const harmonyRevision = await command();
  await page.locator('[data-chroma-strength]').fill('0.93');
  await page.locator('[data-chroma-strength]').press('Tab');
  await page.waitForFunction(
    (revision) => window.nect.commandState.expectedRevision > revision,
    harmonyRevision.expectedRevision,
  );
  const afterHarmonyEdit = await page.evaluate(() => window.nect.document);
  assert.deepEqual(afterHarmonyEdit.keyColorRecipe, beforeHarmonyEdit.keyColorRecipe);
  for (const row of beforeHarmonyEdit.keyColorRecipe.committedSnapshot.resolved)
    assert.deepEqual(
      afterHarmonyEdit.colorSlots.find((slot) => slot.id === row.id),
      beforeHarmonyEdit.colorSlots.find((slot) => slot.id === row.id),
    );
  assert.ok(
    beforeHarmonyEdit.colorDesign.palette.some(
      ({ id }) =>
        JSON.stringify(beforeHarmonyEdit.colorSlots.find((slot) => slot.id === id).value) !==
        JSON.stringify(afterHarmonyEdit.colorSlots.find((slot) => slot.id === id).value),
    ),
  );
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${liveLayer.id}"]`).click();
  const boundLive = await snapshot();
  const oldDerived = (await page.evaluate(() => window.nect.document)).colorSlots.find(
    (slot) => slot.id === liveDerived,
  ).value;
  await page.waitForFunction(
    (color) =>
      [...document.querySelectorAll('.shape')].some(
        (shape) => shape.getAttribute('fill') === color.hex,
      ),
    oldDerived,
  );
  await open();
  assert.match(await page.locator('dialog').textContent(), /Linked trial palette/);
  await field('hex').fill('#e43f74');
  await preview();
  const proposedHex = await page
    .locator(`[data-key-color-compare="${liveDerived}"] [data-key-color-after] code`)
    .textContent();
  await page.locator('[data-artwork-show]').click();
  await page.waitForFunction(
    (hex) =>
      [...document.querySelectorAll('.shape')].some((shape) => shape.getAttribute('fill') === hex),
    proposedHex,
  );
  assert.deepEqual(await snapshot(), boundLive);
  await page.locator('[data-artwork-restore]').click();
  await page.waitForFunction(
    (color) =>
      [...document.querySelectorAll('.shape')].some(
        (shape) => shape.getAttribute('fill') === color.hex,
      ),
    oldDerived,
  );
  assert.deepEqual(await snapshot(), boundLive);
  await page.locator('[data-key-color-apply]').click();
  const editedLive = await snapshot();
  liveDoc = await page.evaluate(() => window.nect.document);
  const newDerived = liveDoc.colorSlots.find((slot) => slot.id === liveDerived).value;
  assert.notDeepEqual(newDerived, oldDerived);
  await page.waitForFunction(
    (color) =>
      [...document.querySelectorAll('.shape')].some(
        (shape) => shape.getAttribute('fill') === color.hex,
      ),
    newDerived,
  );
  assert.equal(
    liveDoc.layers[0].nodes.find((n) => n.id === liveFill.id).params.color.colorSlotId,
    liveDerived,
  );
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), boundLive);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), editedLive);
  const liveDownload = page.waitForEvent('download');
  await activateCommand(page, 'save');
  const liveFile = resolve(output, 'saved-linked-trial.json');
  await (await liveDownload).saveAs(liveFile);
  assert.deepEqual(JSON.parse(await readFile(liveFile, 'utf8')), editedLive);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const dirtyAfterLiveRestart = await page.evaluate(() => window.nect.fileState.dirty);
  const liveChooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  if (dirtyAfterLiveRestart) await page.locator('dialog[open] button[value="discard"]').click();
  await (await liveChooser).setFiles(liveFile);
  await page.waitForFunction(() => window.nect.document.keyColorRecipe?.mode === 'live');
  assert.deepEqual(await snapshot(), editedLive);
  await open();
  await field('hex').fill('#3a9842');
  await preview();
  await page.locator('[data-key-color-apply]').click();
  assert.equal(
    (await page.evaluate(() => window.nect.document)).colorSlots.find((slot) => slot.id === liveKey)
      .value.hex,
    '#3a9842',
  );
  results.push(
    'explicit linked trial: mixed legacy edit, visible Fill binding, one-Undo update and browser JSON reopen/re-edit',
  );

  // Import the actual downloaded project into a fresh synthetic destination.
  await load(liveFixture);
  const beforeLiveImport = await snapshot();
  const openLiveImport = async (file = liveFile) => {
    const chooser = page.waitForEvent('filechooser');
    await activateCommand(page, 'import-composition');
    await (await chooser).setFiles(file);
    await page.locator('dialog[open] [data-composition-choice]').waitFor();
  };
  await openLiveImport();
  assert.match(await page.locator('dialog[open] output').textContent(), /full linked palette/);
  assert.match(
    await page.locator('dialog[open] output').textContent(),
    /Older Nect versions cannot open/,
  );
  await page.screenshot({ path: resolve(output, 'linked-import-warning.png'), fullPage: true });
  await page.setViewportSize({ width: 420, height: 820 });
  const narrowImport = await page.locator('.composition-import-dialog').boundingBox();
  assert.ok(narrowImport.x >= 0 && narrowImport.x + narrowImport.width <= 420);
  await page.screenshot({ path: resolve(output, 'linked-import-narrow.png'), fullPage: true });
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.locator('dialog[open] button[value="import"]').click();
  await page.waitForFunction(() => window.nect.projectSnapshot().compositions.length === 2);
  const afterLiveImport = await snapshot();
  const importedComposition = afterLiveImport.compositions.at(-1);
  const importedFill = importedComposition.layers[0].nodes.find((entry) => entry.type === 'Fill');
  const importedPacket = afterLiveImport.keyColorRecipe.committedSnapshot;
  const importedDerivedId = importedPacket.intent.preset.slots[0].id;
  assert.notEqual(importedPacket.paletteId, editedLive.keyColorRecipe.committedSnapshot.paletteId);
  assert.equal(importedFill.params.color.colorSlotId, importedDerivedId);
  assert.equal(afterLiveImport.colorSlots.length, 6); // Five linked plus the referenced legacy color.
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeLiveImport);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), afterLiveImport);
  await page.locator(`[data-active-composition="${importedComposition.id}"]`).click();
  await open();
  await field('hex').fill('#c8447a');
  await preview();
  await page.locator('[data-key-color-apply]').click();
  const updatedImport = await page.evaluate(() => window.nect.document);
  const importedValue = updatedImport.colorSlots.find(
    (slot) => slot.id === importedDerivedId,
  ).value;
  assert.notDeepEqual(
    importedValue,
    afterLiveImport.colorSlots.find((slot) => slot.id === importedDerivedId).value,
  );
  await page.waitForFunction(
    (hex) =>
      [...document.querySelectorAll('.shape')].some((shape) => shape.getAttribute('fill') === hex),
    importedValue.hex,
  );
  const beforeConflict = await snapshot();
  const withPlain = structuredClone(editedLive);
  const plainComposition =
    migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  plainComposition.id = 'unlinked-composition-probe';
  plainComposition.name = 'No linked colors';
  const extension = {
    colorSlotId: liveDerived,
    nested: { type: 'Color', colorSlotId: liveKey },
  };
  plainComposition.extension = structuredClone(extension);
  plainComposition.layers[0].nodes[0].extension = structuredClone(extension);
  withPlain.compositions.push(plainComposition);
  await openLiveImport({
    name: 'linked-and-plain.json',
    mimeType: 'application/json',
    buffer: Buffer.from(JSON.stringify(withPlain)),
  });
  assert.equal(await page.locator('dialog[open] button[value="import"]').isDisabled(), true);
  assert.match(await page.locator('dialog[open] output').textContent(), /already has Key colors/);
  await page.locator('dialog[open] [data-composition-select-none]').click();
  await page
    .locator(`dialog[open] [data-composition-choice] input[value="${plainComposition.id}"]`)
    .check();
  assert.equal(await page.locator('dialog[open] button[value="import"]').isEnabled(), true);
  assert.doesNotMatch(
    await page.locator('dialog[open] output').textContent(),
    /full linked palette/,
  );
  await page.locator('dialog[open] button[value="import"]').click();
  await page.waitForFunction(
    (count) => window.nect.projectSnapshot().compositions.length === count,
    beforeConflict.compositions.length + 1,
  );
  const afterPlainImport = await snapshot();
  const importedPlain = afterPlainImport.compositions.at(-1);
  assert.deepEqual(importedPlain.extension, extension);
  assert.deepEqual(importedPlain.layers[0].nodes[0].extension, extension);
  assert.deepEqual(afterPlainImport.keyColorRecipe, beforeConflict.keyColorRecipe);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeConflict);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), afterPlainImport);
  assert.deepEqual(JSON.parse(await readFile(liveFile, 'utf8')), editedLive);
  results.push(
    'downloaded linked project import: warning, full palette, one Undo/Redo, re-edit, conflict refusal and unlinked metadata',
  );

  // End the linked trial explicitly, never using an unsaved draft or preview.
  const beforeUnlink = await snapshot();
  await open();
  const unlink = page.locator('[data-key-color-unlink]');
  assert.equal(await unlink.isEnabled(), true);
  const unlinkDelta = page.locator('[data-key-color-input="deltaL"]').first();
  await unlinkDelta.scrollIntoViewIfNeeded();
  const deltaBounds = await unlinkDelta.boundingBox();
  await page.mouse.move(deltaBounds.x + 8, deltaBounds.y + deltaBounds.height / 2);
  await page.mouse.down();
  assert.equal(await unlink.isDisabled(), true);
  await page.keyboard.press('Escape');
  assert.equal(await unlink.isEnabled(), true);
  await page.mouse.up();
  await page.mouse.move(deltaBounds.x + 8, deltaBounds.y + deltaBounds.height / 2);
  await page.mouse.down();
  assert.equal(await unlink.isDisabled(), true);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  assert.equal(await unlink.isEnabled(), true);
  await page.mouse.up();
  const unlinkHue = page.locator('[data-key-color-hue]');
  await unlinkHue.scrollIntoViewIfNeeded();
  const hueBounds = await unlinkHue.boundingBox();
  const hueValue = Number(await unlinkHue.inputValue());
  await page.mouse.move(
    hueBounds.x + 13.5 + ((hueBounds.width - 27) * hueValue) / 360,
    hueBounds.y + hueBounds.height / 2,
  );
  await page.mouse.down();
  assert.equal(
    Number(await unlinkHue.inputValue()),
    hueValue,
    'hold the existing thumb without changing it',
  );
  assert.equal(await unlink.isDisabled(), true);
  await unlink.dispatchEvent('click');
  assert.deepEqual(await snapshot(), beforeUnlink);
  await page.mouse.up();
  assert.equal(await unlink.isEnabled(), true);
  await page.mouse.move(
    hueBounds.x + 13.5 + ((hueBounds.width - 27) * hueValue) / 360,
    hueBounds.y + hueBounds.height / 2,
  );
  await page.mouse.down();
  assert.equal(await unlink.isDisabled(), true);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  assert.equal(await unlink.isEnabled(), true);
  await page.mouse.up();
  await field('alpha').scrollIntoViewIfNeeded();
  const alphaBounds = await field('alpha').boundingBox();
  await page.mouse.move(alphaBounds.x + 8, alphaBounds.y + alphaBounds.height / 2);
  await page.mouse.down();
  assert.equal(await unlink.isDisabled(), true);
  await unlink.dispatchEvent('click');
  assert.deepEqual(await snapshot(), beforeUnlink);
  await page.mouse.up();
  assert.equal(await unlink.isEnabled(), true);
  await field('hex').fill('#b7621a');
  assert.equal(await unlink.isDisabled(), true);
  await preview();
  await page.locator('[data-artwork-show]').click();
  assert.equal(await unlink.isDisabled(), true);
  await cancel();
  assert.deepEqual(await snapshot(), beforeUnlink);
  await open();
  await field('hex').dispatchEvent('compositionstart', { bubbles: true });
  assert.equal(await unlink.isDisabled(), true);
  await field('hex').dispatchEvent('compositionend', { bubbles: true });
  assert.equal(await unlink.isEnabled(), true);
  await page.screenshot({ path: resolve(output, 'linked-unlink.png'), fullPage: true });
  await page.setViewportSize({ width: 420, height: 820 });
  await page.screenshot({ path: resolve(output, 'linked-unlink-narrow.png'), fullPage: true });
  const unlinkBounds = await unlink.boundingBox();
  assert.ok(unlinkBounds.x >= 0 && unlinkBounds.x + unlinkBounds.width <= 420);
  await page.setViewportSize({ width: 1440, height: 1000 });
  await unlink.focus();
  await page.keyboard.press('Enter');
  const detachedProject = await snapshot();
  assert.equal(detachedProject.schemaVersion, 59);
  assert.equal(detachedProject.keyColorRecipe.mode, 'retained-only');
  assert.deepEqual(
    detachedProject.keyColorRecipe.committedSnapshot,
    beforeUnlink.keyColorRecipe.committedSnapshot,
  );
  assert.deepEqual(detachedProject.colorSlots, beforeUnlink.colorSlots);
  assert.deepEqual(detachedProject.compositions, beforeUnlink.compositions);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforeUnlink);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), detachedProject);
  await ensureWorkspacePanel(page, 'colors');
  const detachedAdvanced = page.locator('[data-color-advanced]');
  if (!(await detachedAdvanced.evaluate((element) => element.open)))
    await detachedAdvanced.locator('summary').click();
  const detachedColor = page.locator(
    `[data-shared-color-row="${importedDerivedId}"] [data-shared-color-edit="hex"]`,
  );
  assert.equal(await detachedColor.isEnabled(), true);
  await detachedColor.fill('#2468ac');
  const editedDetached = await snapshot();
  assert.equal(
    editedDetached.colorSlots.find((slot) => slot.id === importedDerivedId).value.hex,
    '#2468ac',
  );
  assert.equal(
    editedDetached.compositions
      .find((composition) => composition.id === importedComposition.id)
      .layers[0].nodes.find((entry) => entry.id === importedFill.id).params.color.colorSlotId,
    importedDerivedId,
  );
  const detachedDownload = page.waitForEvent('download');
  await activateCommand(page, 'save');
  const detachedFile = resolve(output, 'saved-unlinked-trial.json');
  await (await detachedDownload).saveAs(detachedFile);
  assert.deepEqual(JSON.parse(await readFile(detachedFile, 'utf8')), editedDetached);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const detachedChooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  if (await page.locator('dialog[open] button[value="discard"]').count())
    await page.locator('dialog[open] button[value="discard"]').click();
  await (await detachedChooser).setFiles(detachedFile);
  await page.waitForFunction(() => window.nect.document.keyColorRecipe?.mode === 'retained-only');
  assert.deepEqual(await snapshot(), editedDetached);
  results.push(
    'unlink saved trial: draft/preview/IME guard, one Undo/Redo, editable colors and JSON reopen',
  );

  const fixture = retainedRecipeFixture();
  await load(fixture);
  let before = await snapshot(),
    beforeState = await command();
  await open(true);
  assert.equal(await field('hsl-h').evaluate((el) => el === document.activeElement), true);
  assert.equal(await field('hex').getAttribute('type'), 'text');
  assert.equal(await field('hex').inputValue(), '#aBcD09');
  assert.equal(await field('alpha').inputValue(), '0');
  assert.equal(await page.locator('[data-key-color-dormant] input').count(), 0);
  assert.equal(await page.locator('dialog details input,dialog details select').count(), 0);
  const delta = field('deltaL');
  await delta.evaluate((el) => el.scrollIntoView({ block: 'center' }));
  const deltaBefore = Number(await delta.inputValue()),
    deltaBox = await delta.boundingBox(),
    deltaX = deltaBox.x + deltaBox.width * 0.4,
    deltaY = deltaBox.y + deltaBox.height / 2;
  assert.equal(
    await delta.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, {
      x: deltaX,
      y: deltaY,
    }),
    true,
  );
  await page.mouse.move(deltaX, deltaY);
  await page.mouse.down();
  await page.mouse.move(deltaX + 24, deltaY, { steps: 5 });
  assert.ok(
    Math.abs(Number(await delta.inputValue()) - (deltaBefore + 0.2)) < 1e-12,
    'delta drag changes the draft',
  );
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert.equal(Number(await delta.inputValue()), deltaBefore);
  await unchanged(before, beforeState);
  await preview();
  assert.equal(await page.locator('[data-key-color-apply]').isEnabled(), true);
  await delta.evaluate((el) => el.scrollIntoView({ block: 'center' }));
  const dragDelta = async (input, dx, finish = 'commit') => {
    await input.evaluate((el) => el.scrollIntoView({ block: 'center' }));
    const box = await input.boundingBox(),
      x = box.x + box.width * 0.4,
      y = box.y + box.height / 2;
    assert.equal(
      await input.evaluate((el, p) => document.elementFromPoint(p.x, p.y) === el, { x, y }),
      true,
    );
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + dx, y, { steps: 5 });
    if (finish === 'origin') await page.mouse.move(x, y, { steps: 5 });
    if (finish === 'cancel') await page.keyboard.press('Escape');
    await page.mouse.up();
  };
  await dragDelta(delta, 4);
  assert.equal(
    await page.locator('[data-key-color-apply]').isEnabled(),
    true,
    'no-op retains Preview',
  );
  await dragDelta(delta, 24, 'origin');
  assert.equal(Number(await delta.inputValue()), deltaBefore);
  assert.equal(
    await page.locator('[data-key-color-apply]').isDisabled(),
    true,
    'old Preview stays invalidated',
  );
  await field('deltaL').fill('-2.1234567890123457');
  await field('deltaRatioCmax').fill('3.375');
  await dragDelta(field('deltaRatioCmax'), 24, 'cancel');
  assert.equal(await field('deltaRatioCmax').inputValue(), '3.375');
  assert.equal(await field('deltaL').inputValue(), '-2.1234567890123457');
  assert.equal(await page.locator('dialog[data-key-color-recipe-panel]').count(), 1);
  await field('deltaL').dispatchEvent('compositionstart');
  await dragDelta(field('deltaRatioCmax'), 24);
  assert.equal(
    await field('deltaRatioCmax').inputValue(),
    '3.375',
    'composition blocks sibling drag',
  );
  await field('deltaL').dispatchEvent('compositionend');
  await dragDelta(field('deltaRatioCmax'), 24);
  assert.ok(Math.abs(Number(await field('deltaRatioCmax').inputValue()) - 3.575) < 1e-12);
  assert.equal(await field('deltaL').inputValue(), '-2.1234567890123457');
  const simultaneous = await page.evaluate(() => {
    const a = document.querySelector('[data-key-color-input="deltaL"]');
    const b = document.querySelector('[data-key-color-input="hsl-h"]');
    const fire = (target, type, id, dx = 0) => {
      const box = target.getBoundingClientRect();
      return target.dispatchEvent(
        new PointerEvent(type, {
          bubbles: true,
          cancelable: true,
          pointerId: id,
          isPrimary: true,
          button: 0,
          clientX: box.left + 5 + dx,
          clientY: box.top + 5,
        }),
      );
    };
    const before = [a.value, b.value];
    fire(a, 'pointerdown', 41);
    const c1Blocked = !fire(b, 'pointerdown', 42);
    document.dispatchEvent(new PointerEvent('pointercancel', { bubbles: true, pointerId: 41 }));
    fire(b, 'pointerdown', 43);
    const deltaBlocked = !fire(a, 'pointerdown', 44);
    document.dispatchEvent(new PointerEvent('pointercancel', { bubbles: true, pointerId: 43 }));
    return { before, after: [a.value, b.value], c1Blocked, deltaBlocked };
  });
  assert.deepEqual(simultaneous.after, simultaneous.before);
  assert.equal(simultaneous.c1Blocked, true);
  assert.equal(simultaneous.deltaBlocked, true);
  await unchanged(before, beforeState);
  await cancel();
  await unchanged(before, beforeState);
  await open();
  const applyDeltaStart = Number(await field('deltaL').inputValue());
  await dragDelta(field('deltaL'), 24);
  assert.ok(Math.abs(Number(await field('deltaL').inputValue()) - (applyDeltaStart + 0.2)) < 1e-12);
  await unchanged(before, beforeState);
  await preview();
  await page.locator('[data-key-color-apply]').click();
  const deltaApplied = await snapshot();
  assert.ok(
    Math.abs(
      deltaApplied.keyColorRecipe.committedSnapshot.intent.slots[0].deltaL -
        (applyDeltaStart + 0.2),
    ) < 1e-12,
  );
  assert.deepEqual(
    deltaApplied.keyColorRecipe.committedSnapshot.intent.keyColor,
    before.keyColorRecipe.committedSnapshot.intent.keyColor,
  );
  assert.equal((await command()).expectedRevision, beforeState.expectedRevision + 1);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  beforeState = await command();
  await open();
  for (const [dx, finish] of [
    [4, 'commit'],
    [24, 'cancel'],
    [24, 'origin'],
  ]) {
    await dragDelta(field('deltaL'), dx, finish);
    await unchanged(before, beforeState);
  }
  await cancel();
  await unchanged(before, beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await open();
  await field('hsl-h').fill('0');
  const typedHex = await field('hex').inputValue(),
    hBox = await field('hsl-h').boundingBox(),
    hX = hBox.x + 12,
    hY = hBox.y + hBox.height / 2;
  await page.mouse.move(hX, hY);
  await page.mouse.down();
  await page.mouse.move(hX + 24, hY, { steps: 6 });
  assert.equal(await field('hsl-h').inputValue(), '20', 'C1 hue drag changes the draft');
  await unchanged(before, beforeState);
  await field('hsl-h').dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await field('hsl-h').inputValue(), '0');
  assert.equal(await field('hex').inputValue(), typedHex, 'cancel restores the typed HEX');
  await unchanged(before, beforeState);
  await field('hsl-s').fill('50');
  await field('hsl-l').fill('50');
  for (const [channel, expected] of [
    ['h', 20],
    ['s', 70],
    ['l', 70],
  ]) {
    const input = field(`hsl-${channel}`),
      raw = await input.inputValue(),
      literal = await field('hex').inputValue(),
      box = await input.boundingBox(),
      x = box.x + 12,
      y = box.y + box.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + 24, y, { steps: 6 });
    assert.equal(Number(await input.inputValue()), expected, `${channel} drag previews`);
    assert.equal(await field('alpha').inputValue(), '0');
    await unchanged(before, beforeState);
    await input.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
    await page.mouse.up();
    assert.equal(await input.inputValue(), raw, `${channel} cancel restores the value`);
    assert.equal(await field('hex').inputValue(), literal, `${channel} cancel restores HEX`);
  }
  const hueInput = field('hsl-h'),
    hueBox = await hueInput.boundingBox(),
    hueX = hueBox.x + 12,
    hueY = hueBox.y + hueBox.height / 2;
  const preciseSibling = '0.12345678901234566';
  await field('hsl-s').fill(preciseSibling);
  await preview();
  assert.equal(await page.locator('[data-key-color-apply]').isEnabled(), true);
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 4, hueY);
  await page.mouse.up();
  assert.equal(
    await field('hsl-s').inputValue(),
    preciseSibling,
    'threshold no-op preserves sibling',
  );
  assert.equal(
    await page.locator('[data-key-color-apply]').isEnabled(),
    true,
    'no-op keeps Preview',
  );
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  assert.equal(
    await field('hsl-s').inputValue(),
    preciseSibling,
    'changed hue keeps sibling precision',
  );
  await hueInput.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(
    await field('hsl-s').inputValue(),
    preciseSibling,
    'cancel restores sibling precision',
  );
  await preview();
  const thresholdAlpha = field('alpha'),
    thresholdBox = await thresholdAlpha.boundingBox(),
    thresholdX = thresholdBox.x + 12,
    thresholdY = thresholdBox.y + thresholdBox.height / 2;
  await page.mouse.move(thresholdX, thresholdY);
  await page.mouse.down();
  await page.mouse.move(thresholdX + 4, thresholdY);
  await page.mouse.up();
  assert.equal(
    await page.locator('[data-key-color-apply]').isEnabled(),
    true,
    'alpha no-op keeps Preview',
  );
  await field('hsl-s').fill('50');
  const originHex = await field('hex').inputValue();
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  await page.mouse.move(hueX, hueY, { steps: 6 });
  await page.mouse.up();
  assert.equal(await hueInput.inputValue(), '0');
  assert.equal(await field('hex').inputValue(), originHex, 'origin restores the exact HEX draft');
  await hueInput.fill('350');
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  assert.equal(await hueInput.inputValue(), '360', 'hue reaches its exact upper bound');
  await hueInput.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await hueInput.inputValue(), '350');
  await hueInput.fill('0');
  const saturation = field('hsl-s'),
    sBox = await saturation.boundingBox(),
    sX = sBox.x + 12,
    sY = sBox.y + sBox.height / 2;
  await saturation.fill('0.12345678901234566');
  await page.mouse.move(sX, sY);
  await page.mouse.down();
  await page.mouse.move(sX - 5, sY, { steps: 5 });
  assert.equal(await saturation.inputValue(), '0', 'saturation reaches its exact lower bound');
  await saturation.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await saturation.inputValue(), '0.12345678901234566');
  await saturation.fill('50');
  await field('alpha').fill('0.25');
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  assert.equal(await hueInput.inputValue(), '20', 'first HSL drag survives the alpha blur');
  assert.equal(await field('alpha').inputValue(), '0.25');
  await hueInput.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await field('alpha').inputValue(), '0.25');
  await field('alpha').fill('0');
  const beforeBlurHex = await field('hex').inputValue();
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.mouse.up();
  assert.equal(await hueInput.inputValue(), '0', 'window blur cancels HSL preview');
  assert.equal(await field('hex').inputValue(), beforeBlurHex);
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  await hueInput.fill('15');
  await page.mouse.up();
  assert.equal(await hueInput.inputValue(), '15', 'typing cancels the old HSL drag');
  await hueInput.fill('0');
  await page.mouse.move(hueX, hueY);
  await page.mouse.down();
  await page.mouse.move(hueX + 24, hueY, { steps: 6 });
  await page.mouse.up();
  assert.equal(await hueInput.inputValue(), '20');
  await unchanged(before, beforeState);
  await preview();
  await page.locator('[data-key-color-apply]').click();
  assert.equal((await command()).expectedRevision, beforeState.expectedRevision + 1);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before, 'one Undo restores the C1 recipe');
  await load(fixture);
  before = await snapshot();
  beforeState = await command();
  await open();
  const pendingHue = field('hsl-h'),
    pendingRaw = await pendingHue.inputValue(),
    pendingHex = await field('hex').inputValue();
  await pendingHue.evaluate((el) => {
    const send = (target, type, x, buttons) =>
      target.dispatchEvent(
        new PointerEvent(type, {
          bubbles: true,
          pointerId: 43,
          isPrimary: true,
          button: 0,
          buttons,
          clientX: x,
        }),
      );
    const x = el.getBoundingClientRect().left + 12;
    send(el, 'pointerdown', x, 1);
    send(document.body, 'pointermove', x, 1);
    send(document.body, 'pointerup', x, 0);
    send(el, 'pointermove', x + 80, 0);
  });
  assert.equal(await pendingHue.inputValue(), pendingRaw, 'outside release clears pending HSL');
  assert.equal(await field('hex').inputValue(), pendingHex);
  await unchanged(before, beforeState);
  const staleHue = field('hsl-h'),
    staleRaw = await staleHue.inputValue(),
    staleHex = await field('hex').inputValue(),
    staleBox = await staleHue.boundingBox(),
    staleX = staleBox.x + 12,
    staleY = staleBox.y + staleBox.height / 2;
  await page.mouse.move(staleX, staleY);
  await page.mouse.down();
  await page.mouse.move(staleX + 24, staleY, { steps: 6 });
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    }),
  );
  await page.mouse.up();
  assert.equal(await staleHue.inputValue(), staleRaw, 'stale revision restores HSL');
  assert.equal(await field('hex').inputValue(), staleHex, 'stale revision restores literal');
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  await cancel();
  await load(fixture);
  before = await snapshot();
  beforeState = await command();
  await open();
  await field('hex').fill('#AaAaAa');
  const grayHue = field('hsl-h'),
    grayBox = await grayHue.boundingBox(),
    grayX = grayBox.x + 12,
    grayY = grayBox.y + grayBox.height / 2,
    startHue = Number(await grayHue.inputValue());
  await page.mouse.move(grayX, grayY);
  await page.mouse.down();
  await page.mouse.move(grayX + 24, grayY, { steps: 6 });
  await page.mouse.up();
  assert.ok(Math.abs(Number(await grayHue.inputValue()) - startHue - 20) < 0.02);
  assert.equal(
    await field('hex').inputValue(),
    '#AaAaAa',
    'gray hue retains the exact HEX literal',
  );
  assert.equal(await field('alpha').inputValue(), '0');
  await unchanged(before, beforeState);
  await grayHue.dispatchEvent('compositionstart');
  const composingHue = await grayHue.inputValue();
  await page.mouse.move(grayX, grayY);
  await page.mouse.down();
  await page.mouse.move(grayX + 24, grayY, { steps: 6 });
  await page.mouse.up();
  assert.equal(await grayHue.inputValue(), composingHue, 'composition blocks HSL scrub');
  await grayHue.dispatchEvent('compositionend');
  await field('deltaL').dispatchEvent('compositionstart');
  await grayHue.evaluate((el) => {
    const x = el.getBoundingClientRect().left + 12;
    const send = (target, type, clientX, buttons) =>
      target.dispatchEvent(
        new PointerEvent(type, {
          bubbles: true,
          pointerId: 44,
          isPrimary: true,
          button: 0,
          buttons,
          clientX,
        }),
      );
    send(el, 'pointerdown', x, 1);
    send(document, 'pointermove', x + 24, 1);
    send(document, 'pointerup', x + 24, 0);
  });
  assert.equal(await grayHue.inputValue(), composingHue, 'sibling composition blocks HSL scrub');
  await page.mouse.move(grayX, grayY);
  await page.mouse.down();
  await page.mouse.move(grayX + 24, grayY, { steps: 6 });
  await page.mouse.up();
  assert.equal(
    await grayHue.inputValue(),
    composingHue,
    'sibling composition preserves native input',
  );
  await field('deltaL').dispatchEvent('compositionend');
  const graySat = field('hsl-s'),
    graySatBox = await graySat.boundingBox(),
    graySatX = graySatBox.x + 12,
    graySatY = graySatBox.y + graySatBox.height / 2;
  await page.mouse.move(graySatX, graySatY);
  await page.mouse.down();
  await page.mouse.move(graySatX + 24, graySatY, { steps: 6 });
  assert.notEqual((await field('hex').inputValue()).toLowerCase(), '#aaaaaa');
  await graySat.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await field('hex').inputValue(), '#AaAaAa', 'cancel restores gray literal');
  await unchanged(before, beforeState);
  await field('hex').fill('#00ff00');
  await page.mouse.move(grayX, grayY);
  await page.mouse.down();
  await page.mouse.move(grayX + 24, grayY, { steps: 6 });
  assert.equal(await grayHue.inputValue(), '140', 'first hue drag follows the typed HEX');
  await grayHue.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await field('hex').inputValue(), '#00ff00');
  assert.equal(await grayHue.inputValue(), '120');
  await field('hex').fill('bad');
  const invalidHue = await grayHue.inputValue();
  await page.mouse.move(grayX, grayY);
  await page.mouse.down();
  await page.mouse.move(grayX + 24, grayY, { steps: 6 });
  await page.mouse.up();
  assert.equal(await grayHue.inputValue(), invalidHue, 'invalid HEX cannot start a scrub');
  assert.equal(await field('hex').inputValue(), 'bad');
  results.push(
    'C1 HSL scrub: H/S/L, exact literal and alpha, bounds, origin, cancel, typing, stale state, composition and one-Undo Apply',
  );
  await cancel();
  await open();
  await preview();
  assert.equal(await page.locator('[data-key-color-apply]').isEnabled(), true);
  const c1Alpha = field('alpha');
  const alphaStart = async (dx = 24) => {
    await c1Alpha.scrollIntoViewIfNeeded();
    const box = await c1Alpha.boundingBox(),
      x = box.x + 12,
      y = box.y + box.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + dx, y, { steps: 6 });
    return { x, y };
  };
  const alphaBox = await c1Alpha.boundingBox(),
    hostBox = await page.locator('[data-key-color-c1]').boundingBox(),
    outsideX = Math.max(1, hostBox.x - 15),
    alphaY = alphaBox.y + alphaBox.height / 2;
  await page.mouse.move(alphaBox.x + 12, alphaY);
  await page.mouse.down();
  await page.mouse.move(outsideX, alphaY);
  await page.mouse.up();
  await page.mouse.move(alphaBox.x + 40, alphaY);
  await page.mouse.down();
  await page.mouse.move(alphaBox.x + 43, alphaY);
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'release outside host clears the pending gesture');
  await unchanged(before, beforeState);
  await c1Alpha.evaluate((el) => {
    const send = (target, type, x, buttons) =>
      target.dispatchEvent(
        new PointerEvent(type, {
          bubbles: true,
          pointerId: 42,
          isPrimary: true,
          button: 0,
          buttons,
          clientX: x,
        }),
      );
    const x = el.getBoundingClientRect().left + 12;
    send(el, 'pointerdown', x, 1);
    send(document.body, 'pointermove', x - 100, 1);
    send(document.body, 'pointerup', x - 100, 0);
    send(el, 'pointermove', x + 80, 0);
  });
  assert.equal(await c1Alpha.inputValue(), '0', 'outside release does not leave a pending scrub');
  await unchanged(before, beforeState);
  await alphaStart();
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'window blur cancels the active preview');
  await unchanged(before, beforeState);
  await alphaStart();
  await page.locator('dialog footer [data-key-color-cancel]').evaluate((button) => button.click());
  await page.mouse.up();
  await unchanged(before, beforeState);
  await open();
  assert.equal(await c1Alpha.inputValue(), '0', 'closing during a drag discards its listeners');
  await preview();
  await alphaStart(3);
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0');
  assert.equal(await page.locator('[data-key-color-apply]').isEnabled(), true, 'jitter is a no-op');
  const origin = await alphaStart();
  await page.mouse.move(origin.x, origin.y, { steps: 6 });
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'return to origin restores raw alpha');
  await unchanged(before, beforeState);
  await preview();
  await alphaStart();
  assert.equal(await c1Alpha.inputValue(), '0.2', 'C1 alpha drag changes the draft');
  await unchanged(before, beforeState);
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  await c1Alpha.dispatchEvent('pointercancel', { pointerId: 1, bubbles: true });
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'pointer cancel restores exact alpha');
  await unchanged(before, beforeState);
  await c1Alpha.dispatchEvent('compositionstart');
  await alphaStart();
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'composition blocks scrubbing');
  await c1Alpha.dispatchEvent('compositionend');
  const preciseAlpha = '0.12345678901234566';
  await c1Alpha.fill(preciseAlpha);
  const preciseOrigin = await alphaStart();
  await page.mouse.move(preciseOrigin.x, preciseOrigin.y, { steps: 6 });
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), preciseAlpha, 'origin preserves the exact alpha');
  await alphaStart(-104);
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'the lower endpoint is exact');
  await c1Alpha.fill(preciseAlpha);
  await alphaStart(104);
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '1', 'the upper endpoint is exact');
  await c1Alpha.fill('0');
  await alphaStart();
  await c1Alpha.fill('0.4');
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0.4', 'typing cancels the old drag');
  await unchanged(before, beforeState);
  await c1Alpha.fill('0');
  await alphaStart();
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0.2');
  await unchanged(before, beforeState);
  await preview();
  await page.locator('[data-key-color-apply]').click();
  assert.equal((await snapshot()).colorSlots.find((slot) => slot.id === 'key').value.alpha, 0.2);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before, 'one Undo restores the applied recipe');
  await open();
  await alphaStart();
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    }),
  );
  await page.mouse.up();
  assert.equal(await c1Alpha.inputValue(), '0', 'stale revision cancels the drag');
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  await cancel();
  await load(fixture);
  before = await snapshot();
  beforeState = await command();
  await open();
  await page.screenshot({ path: resolve(output, 'hsl-desktop.png'), fullPage: true });
  const plane = page.locator('[data-key-color-c1] canvas');
  await plane.click({ position: { x: 90, y: 66 } });
  assert.ok(Math.abs(Number(await field('hsl-s').inputValue()) - 50) < 2);
  assert.ok(Math.abs(Number(await field('hsl-l').inputValue()) - 50) < 2);
  await unchanged(before, beforeState);
  const hue = page.locator('[data-key-color-hue]');
  await hue.press('Home');
  await hue.press('ArrowRight');
  assert.ok(Number(await field('hsl-h').inputValue()) > 0);
  for (const [channel, value] of [
    ['h', '240'],
    ['s', '100'],
    ['l', '50'],
  ])
    await field(`hsl-${channel}`).fill(value);
  assert.equal(await field('hex').inputValue(), '#0000ff');
  assert.equal(await field('alpha').inputValue(), '0');
  await preview();
  await unchanged(before, beforeState);
  await field('hsl-s').fill('');
  assert.equal(await page.locator('[data-key-color-apply]').isDisabled(), true);
  assert.equal(await page.locator('[data-key-color-preview]').isDisabled(), true);
  await cancel();
  await unchanged(before, beforeState);
  await open();
  assert.equal(await field('hex').inputValue(), '#aBcD09');
  results.push(
    'C1 HSL plane, keyboard hue slider and numeric edits are draft-only; invalid HSL revokes Preview; Cancel preserves exact HEX/alpha0',
  );
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
  assert.equal(
    await page.locator('[data-key-color-compare="key"]').evaluate((row) => {
      const a = row.getBoundingClientRect();
      const b = row.closest('.key-color-recipe-body').getBoundingClientRect();
      return a.top >= b.top - 1 && a.bottom <= b.bottom + 1;
    }),
    true,
    'explicit Preview brings the actual before/after row into its scrollport',
  );
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
  for (const [channel, value] of [
    ['h', '240'],
    ['s', '100'],
    ['l', '50'],
  ])
    await field(`hsl-${channel}`).fill(value);
  await field('deltaL').fill('-0.125');
  await field('deltaRatioCmax').fill('3.375');
  await preview();
  await page.locator('[data-key-color-apply]').click();
  assert.equal(await page.locator('dialog[data-key-color-recipe-panel]').count(), 0);
  const after = await snapshot(),
    afterState = await command();
  assert.equal(afterState.expectedRevision, beforeState.expectedRevision + 1);
  assert.deepEqual(after.colorSlots.find((slot) => slot.id === 'key').value, {
    hex: '#0000ff',
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
  assert.equal(await field('hex').inputValue(), '#0000ff');
  assert.equal(await field('alpha').inputValue(), '0');
  await cancel();
  results.push(
    'Save download → app reload → native JSON file-input Open → retained recipe re-edit verified',
  );

  // A native details toggle is queued. Force the timing in which a color
  // command redraws the panel before the opening toggle event is delivered.
  await load(fixture);
  const quickAdvanced = page.locator('[data-color-advanced]');
  await quickAdvanced.evaluate(
    (details) =>
      new Promise((resolve) => {
        const close = () => {
          details.addEventListener('toggle', resolve, { once: true });
          details.open = false;
        };
        if (details.open) close();
        else {
          details.addEventListener('toggle', close, { once: true });
          details.open = true;
        }
      }),
  );
  const quickBefore = await command(),
    quickProject = await snapshot();
  const quickResult = await page.evaluate(() => {
    const details = document.querySelector('[data-color-advanced]'),
      mode = document.querySelector('[data-wheel-chroma-mode]');
    details.open = true;
    mode.value = 'override';
    mode.dispatchEvent(new Event('change', { bubbles: true }));
    return {
      open: document.querySelector('[data-color-advanced]')?.open,
      input: !!document.querySelector('[data-wheel-chroma]'),
    };
  });
  assert.deepEqual(quickResult, { open: true, input: true });
  assert.equal(await page.locator('[data-wheel-chroma]').isVisible(), true);
  assert.deepEqual(await command(), quickBefore);
  assert.deepEqual(await snapshot(), quickProject);
  const quickClosed = await page.evaluate(() => {
    const details = document.querySelector('[data-color-advanced]'),
      mode = document.querySelector('[data-wheel-chroma-mode]');
    details.open = false;
    mode.value = 'auto';
    mode.dispatchEvent(new Event('change', { bubbles: true }));
    return document.querySelector('[data-color-advanced]')?.open;
  });
  assert.equal(quickClosed, false);
  const autoProject = await snapshot();
  assert.notDeepEqual(autoProject, quickProject);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), quickProject);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await snapshot(), autoProject);
  results.push('queued Advanced toggle retains its open state across a color command');

  await load(fixture);
  const advanced = page.locator('[data-color-advanced]');
  if (!(await advanced.evaluate((el) => el.open))) await advanced.locator('summary').click();
  assert.equal(await advanced.evaluate((el) => el.open), true);
  assert.equal(await page.locator('[data-wheel-chroma-mode]').isVisible(), true);
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

  const dark = retainedRecipeFixture(),
    darkSaved = dark.keyColorRecipe.committedSnapshot;
  dark.keyColorRecipe.committedSnapshot = editKeyColorRecipe(darkSaved, {
    type: 'key-color',
    expectedRevision: darkSaved.revision,
    value: { hex: '#000011', alpha: 0 },
  });
  await load(dark);
  const darkBefore = await snapshot(),
    darkState = await command();
  for (const [channel, value, hex] of [
    ['h', '120', '#001100'],
    ['l', '50', '#0000ff'],
  ]) {
    await open();
    assert.equal(await field('hex').inputValue(), '#000011');
    assert.equal(await field('hsl-s').inputValue(), '100');
    await field(`hsl-${channel}`).fill(value);
    assert.equal(await field('hex').inputValue(), hex);
    await preview();
    assert.equal(
      await page
        .locator('[data-key-color-compare="key"] [data-key-color-after] code')
        .textContent(),
      hex,
    );
    await unchanged(darkBefore, darkState);
    await field('hex').fill('#000011');
    await field(`hsl-${channel}`).fill(value);
    assert.equal(await field('hex').inputValue(), hex);
    assert.equal(await field('alpha').inputValue(), '0');
    await cancel();
    await unchanged(darkBefore, darkState);
  }
  results.push(
    'fully saturated #000011 supports H/L edits from retained and direct HEX without range errors or document mutation',
  );

  const gray = retainedRecipeFixture(),
    graySaved = gray.keyColorRecipe.committedSnapshot;
  gray.keyColorRecipe.committedSnapshot = editKeyColorRecipe(graySaved, {
    type: 'key-color',
    expectedRevision: graySaved.revision,
    value: { hex: '#aAaAaA', alpha: 0 },
  });
  await load(gray);
  const grayBefore = await snapshot(),
    grayState = await command();
  await open();
  await field('hsl-h').fill('240');
  assert.equal(await field('hex').inputValue(), '#aAaAaA');
  await preview();
  assert.equal(
    await page.locator('[data-key-color-compare="key"] [data-key-color-after] code').textContent(),
    '#aAaAaA',
  );
  await field('hsl-s').fill('100');
  assert.equal(await field('hex').inputValue(), '#5555ff');
  assert.equal(await field('alpha').inputValue(), '0');
  await unchanged(grayBefore, grayState);
  await cancel();
  await unchanged(grayBefore, grayState);
  results.push(
    'gray hue stays local without literal/case/alpha change; increasing saturation uses the selected hue; Cancel unchanged',
  );

  await page.setViewportSize({ width: 420, height: 760 });
  await load(fixture);
  await open();
  await page.screenshot({ path: resolve(output, 'hsl-narrow.png'), fullPage: true });
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
