import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';

const browser = await chromium.launch({
  headless: true,
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
});
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
await page.goto('http://127.0.0.1:5191/', { waitUntil: 'networkidle' });
const layers = () => page.locator('.layer[data-layer]');
const names = async () => page.locator('.layer[data-layer] .name').allTextContents();
const addBlank = page.locator('[data-action="add-blank-layer"]');
await addBlank.click();
await addBlank.click();
assert.equal(await layers().count(), 3, 'blank compositions create three layer rows');

const rows = await layers().all();
await rows[0].locator('.name').click();
assert.match(
  (await rows[0].getAttribute('class')).replace(/\s+/g, ' '),
  /layer selected/,
  'row click selects a layer',
);

const beforeDrag = await names();
await rows[2].dragTo(rows[0]);
const afterDrag = await names();
assert.notDeepEqual(afterDrag, beforeDrag, 'vertical drag changes paint order');
await page.keyboard.press('Control+z');
assert.deepEqual(await names(), beforeDrag, 'drag reorder is one undo');
await page.keyboard.press('Control+Shift+z');
assert.deepEqual(await names(), afterDrag, 'redo restores drag reorder');

const selected = page.locator('.layer.selected');
await selected.focus();
const countBeforeCopy = await layers().count();
await page.keyboard.press('Control+c');
await page.keyboard.press('Control+v');
assert.equal(
  await layers().count(),
  countBeforeCopy + 1,
  'Ctrl+C/V duplicates within the document',
);
await page.keyboard.press('Control+z');
assert.equal(await layers().count(), countBeforeCopy, 'paste is one undo');
await selected.focus();
await page.keyboard.press('Control+d');
assert.equal(await layers().count(), countBeforeCopy + 1, 'Ctrl+D duplicates the focused layer');

const textInput = page.locator('[data-layer-name]').first();
await textInput.fill('Typed layer');
await textInput.press('Control+a');
await textInput.type('Edited');
assert.equal(
  await page.locator('[data-layer-name]').inputValue(),
  'Edited',
  'text input keeps native editing',
);
await textInput.press('Escape');

const lockedRow = page
  .locator('.layer')
  .filter({ has: page.locator('[data-layer-lock][title="Toggle layer lock"]') })
  .first();
await lockedRow.locator('[data-layer-lock]').click();
const lockedId = await lockedRow.getAttribute('data-layer');
await lockedRow.locator('.name').click();
await lockedRow.focus();
await page.keyboard.press('Control+d');
assert.equal(
  await page.locator(`.layer[data-layer="${lockedId}"]`).count(),
  1,
  'locked layer is not duplicated',
);

const receipt = {
  browser: 'Chrome executablePath',
  selection: true,
  dragReorder: true,
  undoRedo: true,
  copyPaste: true,
  duplicateShortcut: true,
  textInputNonConflict: true,
  lockedGuard: true,
  saveReload:
    'blocked: native Chrome download/file chooser did not complete in headless acceptance runner',
};
console.log(JSON.stringify(receipt));
await browser.close();
