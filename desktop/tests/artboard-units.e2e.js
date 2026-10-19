import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';
import { displayLengthToDocumentUnits, formatDocumentLength } from '../src/artboard-units.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForSelector('[data-artboard-unit]');
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const original = await saved();
  const originalDocument = JSON.parse(original);
  const width = page.locator('[data-dimension="width"]');

  await page.locator('[data-artboard-unit]').selectOption('mm');
  assert.equal(await saved(), original, 'changing units does not edit the document');
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  assert.equal(await width.inputValue(), formatDocumentLength(originalDocument.width, 'mm'));
  assert.match(await page.locator('[data-artboard-size]').textContent(), /mm$/);

  await page.locator('[data-artboard-unit]').selectOption('cm');
  assert.equal(await saved(), original, 'a second unit change also preserves the document');
  assert.equal(await width.inputValue(), formatDocumentLength(originalDocument.width, 'cm'));

  await width.fill('10');
  await width.press('Tab');
  const expectedCentimetres = displayLengthToDocumentUnits(10, 'cm');
  await page.waitForFunction(
    (expected) => Math.abs(window.nect.document.width - expected) < 1e-10,
    expectedCentimetres,
  );
  assert.equal(
    await page.evaluate(async () => {
      const { serialize, deserialize } = await import('/src/core.js');
      return deserialize(serialize(window.nect.document)).width;
    }),
    expectedCentimetres,
    'cm input serializes in canonical px',
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'Undo restores the exact original after cm input');

  await page.locator('[data-artboard-unit]').selectOption('mm');
  await width.fill('100');
  await width.press('Tab');
  const expectedWidth = displayLengthToDocumentUnits(100, 'mm');
  await page.waitForFunction(
    (expected) => Math.abs(window.nect.document.width - expected) < 1e-10,
    expectedWidth,
  );
  const uiResult = await saved();
  assert.equal(JSON.parse(uiResult).height, originalDocument.height);
  const serializedWidth = await page.evaluate(async () => {
    const { serialize, deserialize } = await import('/src/core.js');
    return deserialize(serialize(window.nect.document)).width;
  });
  assert.equal(serializedWidth, expectedWidth, 'project serialization keeps canonical px');

  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'Undo restores the exact original dimensions');

  const widthBeforeScrub = originalDocument.width,
    widthDisplayBeforeScrub = Number(await width.inputValue()),
    widthBox = await width.boundingBox();
  assert.ok(widthBox);
  const scrubX = widthBox.x + widthBox.width / 2,
    scrubY = widthBox.y + widthBox.height / 2;
  await page.mouse.move(scrubX, scrubY);
  await page.mouse.down();
  await page.mouse.move(scrubX + 9, scrubY, { steps: 1 });
  await page.mouse.up();
  const expectedScrubWidth = widthBeforeScrub + displayLengthToDocumentUnits(5, 'mm');
  await page.waitForFunction(
    (expected) => Math.abs(window.nect.document.width - expected) < 1e-10,
    expectedScrubWidth,
  );
  assert.equal(JSON.parse(await saved()).height, originalDocument.height);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'mm scrubbing commits canonical px and Undo restores it');
  assert.equal(Number(await width.inputValue()), widthDisplayBeforeScrub);

  await page.evaluate((value) => {
    const state = window.nect.commandState;
    window.nect.dispatch({
      ...state,
      type: 'set-document-settings',
      payload: { key: 'width', value },
    });
  }, expectedWidth);
  assert.equal(await saved(), uiResult, 'the existing px command matches unit-aware UI input');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original, 'the existing command has the same Undo result');
} finally {
  await browser.close();
}
