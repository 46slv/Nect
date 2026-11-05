import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, CommandSession, connect, node, serialize } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand } from './command-menu-actions.js';

const output = resolve('test-results/composition-import-selection');
await mkdir(output, { recursive: true });
function artwork(name) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill');
  layer.name = name;
  layer.nodes.push(shape, fill);
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  return { doc, layer, fill };
}
// Test-only explicit coefficients are never installed as a product preset.
const plan = {
  version: 1,
  modelVersion: 2,
  solverVersion: 'experimental-affine-v1',
  colorSemanticsVersion: 'linear-srgb-straight-v1',
  paletteId: 'fixture-palette',
  intent: {
    keyColor: {
      id: 'fresh-key',
      role: 'Identity',
      value: { hex: '#aBcD09', alpha: 1 },
      hueHint: 17,
    },
    preset: {
      id: 'fixture-only',
      version: 1,
      harmony: 'triadic',
      hueSteps: 12,
      slots: [
        {
          id: 'fresh-derived',
          role: 'Background',
          position: 1,
          lightnessScale: 0.6,
          lightnessOffset: 0.1,
          chromaScale: 0.8,
          chromaOffset: 0,
          alpha: 1,
        },
      ],
    },
    slots: [{ id: 'fresh-derived', role: 'Background', deltaL: 0.03, deltaRatioCmax: 0 }],
    settings: { areaAware: false },
  },
  allocations: [
    { slotId: 'fresh-key', name: 'Key' },
    { slotId: 'fresh-derived', name: 'Derived' },
  ],
};
const first = artwork('First linked'),
  second = artwork('Second linked'),
  third = artwork('Not selected');
const sourceSession = new CommandSession(first.doc);
sourceSession.dispatch({
  ...sourceSession.state,
  type: 'initialize-live-key-color-recipe',
  payload: plan,
});
sourceSession.dispatch({
  ...sourceSession.state,
  type: 'bind-color-slot',
  payload: {
    layerId: first.layer.id,
    nodeId: first.fill.id,
    key: 'color',
    slotId: 'fresh-derived',
  },
});
const source = migrateLegacyDocumentToCompositionProject(sourceSession.snapshot());
source.compositions[0].name = 'First linked';
source.compositions[0].id = 'source-first';
const secondComposition = migrateLegacyDocumentToCompositionProject(second.doc).compositions[0],
  thirdComposition = migrateLegacyDocumentToCompositionProject(third.doc).compositions[0];
secondComposition.layers[0].nodes.find((n) => n.id === second.fill.id).params.color.colorSlotId =
  'fresh-derived';
secondComposition.id = 'source-second';
thirdComposition.id = 'source-third';
secondComposition.name = 'Second linked';
thirdComposition.name = 'Not selected';
source.compositions.push(secondComposition, thirdComposition);
const sourceText = serializeCompositionProject(source);
const longSource = migrateLegacyDocumentToCompositionProject(blankDocument());
longSource.compositions[0].id = 'long-0';
longSource.compositions[0].name = 'Long source 0';
for (let index = 1; index < 20; index++) {
  const composition = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  composition.id = `long-${index}`;
  composition.name = `Long source ${index}`;
  longSource.compositions.push(composition);
}
const longSourceText = serializeCompositionProject(longSource);
const destination = blankDocument();
destination.schemaVersion = 54;
destination.colorSlots = [
  { id: 'fresh-derived', name: 'Existing', value: { hex: '#112233', alpha: 1 } },
];
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 900, height: 620 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () => page.evaluate(() => window.nect.commandState);
const dialog = page.locator('.composition-import-dialog[open]');
const choices = () => dialog.locator('[data-composition-choice] input');
async function openImport(text = sourceText) {
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'import-composition');
  await (
    await chooser
  ).setFiles({
    name: 'linked-source.json',
    mimeType: 'application/json',
    buffer: Buffer.from(text),
  });
  await dialog.waitFor();
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), serialize(destination));
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 1001 },
    }),
  );
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  const before = await snapshot(),
    beforeState = await state();
  await openImport();
  assert.equal(await choices().count(), 3);
  for (let i = 0; i < 3; i++) assert.equal(await choices().nth(i).isChecked(), true);
  assert.equal(
    await choices()
      .first()
      .evaluate((el) => el === document.activeElement),
    true,
  );
  await choices().first().press('Space');
  await page.keyboard.press('Tab');
  await page.keyboard.press('Space');
  await page.keyboard.press('Tab');
  await page.keyboard.press('Space');
  assert.equal(await dialog.locator('button[value="import"]').isDisabled(), true);
  await page.keyboard.press('Enter');
  assert.equal(await dialog.isVisible(), true);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await dialog.locator('[data-composition-select-all]').click();
  for (let i = 0; i < 3; i++) assert.equal(await choices().nth(i).isChecked(), true);
  await dialog.locator('[data-composition-select-none]').click();
  assert.equal(await dialog.locator('button[value="import"]').isDisabled(), true);
  await choices().nth(1).check();
  await choices().nth(0).check();
  assert.equal(await choices().nth(2).isChecked(), false);
  assert.match(await dialog.locator('output').textContent(), /full linked palette/);
  await page.screenshot({ path: resolve(output, 'subset.png') });
  await page.setViewportSize({ width: 420, height: 520 });
  await page.screenshot({ path: resolve(output, 'subset-narrow.png') });
  assert.equal(await dialog.locator('button[value="import"]').isVisible(), true);
  await page.setViewportSize({ width: 900, height: 620 });
  await choices().first().focus();
  await page.keyboard.press('Enter');
  await dialog.waitFor({ state: 'detached' });
  await page.waitForFunction(() => {
    const project = window.nect.projectSnapshot();
    return project.compositions.length === 3 && project.keyColorRecipe?.mode === 'live';
  });
  const imported = await snapshot(),
    packet = imported.keyColorRecipe.committedSnapshot;
  assert.deepEqual(
    imported.compositions.slice(1).map((c) => c.name),
    ['First linked', 'Second linked'],
  );
  assert.notEqual(packet.paletteId, 'fixture-palette');
  const derived = packet.intent.slots[0].id;
  assert.notEqual(derived, 'fresh-derived');
  for (const composition of imported.compositions.slice(1))
    assert.equal(
      composition.layers[0].nodes.find((n) => n.type === 'Fill').params.color.colorSlotId,
      derived,
    );
  assert.deepEqual(imported.colorSlots.find((slot) => slot.id === 'fresh-derived').value, {
    hex: '#112233',
    alpha: 1,
  });
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), before);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), imported);
  await page.evaluate(async () => {
    const { editKeyColorRecipe } = await import('/src/key-color-recipe.js');
    const current = window.nect.projectSnapshot().keyColorRecipe.committedSnapshot;
    const next = editKeyColorRecipe(current, {
      type: 'key-color',
      expectedRevision: current.revision,
      value: { hex: '#d03270', alpha: 0.25 },
    });
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'apply-live-key-color-recipe-snapshot',
      payload: { snapshot: next, expectedPaletteRevision: current.revision },
    });
  });
  const changed = await snapshot();
  assert.notDeepEqual(
    changed.colorSlots.find((slot) => slot.id === derived).value,
    imported.colorSlots.find((slot) => slot.id === derived).value,
  );
  for (const composition of changed.compositions.slice(1))
    assert.equal(
      composition.layers[0].nodes.find((n) => n.type === 'Fill').params.color.colorSlotId,
      derived,
    );
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 1001 },
    }),
  );
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  const beforeConflict = await snapshot(),
    conflictState = await state();
  await openImport();
  assert.equal(await dialog.locator('button[value="import"]').isDisabled(), true);
  assert.match(await dialog.locator('output').textContent(), /already has Key colors/);
  await dialog.locator('[data-composition-select-none]').click();
  await choices().nth(2).check();
  assert.equal(await dialog.locator('button[value="import"]').isEnabled(), true);
  await dialog.locator('button[value="cancel"]').click();
  assert.deepEqual(await snapshot(), beforeConflict);
  assert.deepEqual(await state(), conflictState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await openImport();
  await dialog.locator('[data-composition-select-none]').click();
  await choices().nth(2).check();
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  const staleBefore = await snapshot(),
    staleState = await state();
  await dialog.locator('button[value="import"]').click();
  await dialog.waitFor({ state: 'detached' });
  await page.waitForFunction(() =>
    document
      .querySelector('.status .message')
      ?.textContent.includes('Project changed while reading'),
  );
  assert.deepEqual(await snapshot(), staleBefore);
  assert.deepEqual(await state(), staleState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  assert.match(
    await page.locator('.status .message').textContent(),
    /Project changed while reading/,
  );
  await activateCommand(page, 'redo');
  const finalProject = await snapshot(),
    downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve(output, 'selected-linked-project.json');
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), finalProject);
  await page.waitForFunction(() => !window.nect.fileState.saving && !window.nect.fileState.dirty);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(file);
  await page.waitForFunction(
    (expected) => JSON.stringify(window.nect.projectSnapshot()) === expected,
    JSON.stringify(finalProject),
  );
  await page.setViewportSize({ width: 420, height: 340 });
  await openImport(longSourceText);
  assert.equal(await choices().count(), 20);
  await choices().nth(18).focus();
  await page.keyboard.press('Tab');
  assert.equal(
    await choices()
      .last()
      .evaluate((element) => element === document.activeElement),
    true,
  );
  await page.keyboard.press('Space');
  assert.equal(await choices().last().isChecked(), false);
  const lastHit = await choices()
    .last()
    .evaluate((element) => {
      const box = element.getBoundingClientRect();
      return (
        document.elementFromPoint(box.left + box.width / 2, box.top + box.height / 2) === element
      );
    });
  assert.equal(lastHit, true);
  await dialog.locator('button[value="import"]').scrollIntoViewIfNeeded();
  const importHit = await dialog.locator('button[value="import"]').evaluate((element) => {
    const box = element.getBoundingClientRect();
    return (
      document.elementFromPoint(box.left + box.width / 2, box.top + box.height / 2) === element
    );
  });
  assert.equal(importHit, true);
  await page.screenshot({ path: resolve(output, 'long-short.png') });
  await page.keyboard.press('Escape');
  await dialog.waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), finalProject);
  assert.equal(serializeCompositionProject(source), sourceText);
  assert.deepEqual(errors, []);
  console.log(
    'PASS selected linked Compositions: one palette, collision remap, history, conflict, stale owner and browser reopen',
  );
} finally {
  await browser.close();
}
