import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage();

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForSelector('.command-menu summary');

  const importMenu = page.locator('.command-menu').filter({ hasText: 'Import' });
  await importMenu.locator('summary').click();
  assert.equal(await importMenu.locator('button[data-action="import-svg"]').isVisible(), true);
  assert.equal(await importMenu.locator('button[data-action="import-image"]').isVisible(), true);
  assert.equal(
    await importMenu.locator('button[data-action="import-composition"]').isVisible(),
    true,
  );

  const svgChooser = page.waitForEvent('filechooser');
  await importMenu.locator('button[data-action="import-svg"]').click();
  await (
    await svgChooser
  ).setFiles({
    name: 'fixture.svg',
    mimeType: 'image/svg+xml',
    buffer: Buffer.from('<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"/>'),
  });
  await page.waitForSelector('.message:text("SVG document imported")');

  await importMenu.locator('summary').click();
  const imageChooser = page.waitForEvent('filechooser');
  await importMenu.locator('button[data-action="import-image"]').click();
  assert.ok(await imageChooser);

  await importMenu.locator('summary').click();
  const compositionChooser = page.waitForEvent('filechooser');
  await importMenu.locator('button[data-action="import-composition"]').click();
  assert.ok(await compositionChooser);
} finally {
  await browser.close();
}
