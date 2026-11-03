import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { revealCommandMenu } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { pngDimensions, pngPixelsPerMetre } from '../src/raster.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({
  viewport: { width: 1440, height: 1000 },
  acceptDownloads: true,
});
const pageErrors = [];
page.on('pageerror', (error) => pageErrors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect?.document);
  await page.evaluate(() => {
    const doc = structuredClone(window.nect.document);
    doc.width = 100;
    doc.height = 50;
    window.nect.load(JSON.stringify(doc));
  });
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-dimension="width"]').fill('650');
  await page.locator('[data-dimension="width"]').press('Tab');
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  const saved = () => page.evaluate(() => JSON.stringify(window.nect.document));
  const documentBefore = await saved();
  const open = () => revealCommandMenu(page, 'Output settings');
  await open();
  await page.locator('#png-export-mode').selectOption('ppi');
  await open();
  const input = page.locator('#png-ppi');
  await input.fill('300');
  await input.press('Tab');
  const rect = await input.boundingBox(),
    x = rect.x + 15,
    y = rect.y + rect.height / 2;
  await page.mouse.move(x, y);
  await page.mouse.down();
  await page.mouse.move(x + 24, y, { steps: 6 });
  await page.mouse.up();
  await open();
  const actual = await input.inputValue();
  await fs.mkdir('test-results/png-ppi', { recursive: true });
  await page.screenshot({ path: 'test-results/png-ppi/scrub.png' });
  console.log(
    JSON.stringify({ browser: browser.version(), baseline: 300, drag: 24, expected: 320, actual }),
  );
  assert.equal(actual, '320', 'PPI uses the shared screen-pixel gesture');
  const unchanged = async () => {
    assert.equal(await saved(), documentBefore);
    assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
    assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  };
  await unchanged();
  const set = async (value) => {
    await open();
    await input.fill(value);
    await input.press('Tab');
  };
  const start = async (dx) => {
    await open();
    const r = await input.boundingBox(),
      point = { x: r.x + 15, y: r.y + r.height / 2 };
    await page.mouse.move(point.x, point.y);
    await page.mouse.down();
    await page.mouse.move(point.x + dx, point.y, { steps: 6 });
    return point;
  };
  await input.click();
  await input.press('ArrowUp');
  await input.press('Tab');
  assert.equal(await input.inputValue(), '321', 'native arrow remains available');
  await set('300.1234567890123');
  await start(3);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '300.1234567890123');
  const point = await start(24);
  await page.mouse.move(point.x, point.y, { steps: 6 });
  await page.mouse.up();
  assert.equal(await input.inputValue(), '300.1234567890123', 'origin retains saved precision');
  await start(24);
  await page.keyboard.press('Escape');
  await page.mouse.up();
  await open();
  assert.equal(await input.inputValue(), '300.1234567890123', 'Escape restores raw PPI');
  await unchanged();
  for (const invalid of ['0', '-1', '0.001', '1e300', '']) {
    await set(invalid);
    assert.equal(await page.locator('[data-png-dimensions]').getAttribute('data-valid'), 'false');
    assert.equal(await page.locator('[data-action="export-png"]').isDisabled(), true);
    await start(24);
    await page.mouse.up();
    assert.equal(await input.inputValue(), invalid);
  }
  await set('1200');
  await start(24);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '1220', 'existing PPI range is preserved');
  await set('300');
  await start(24);
  // Deliberate owner interruption uses a synthetic change; ordinary gestures above are native.
  await page.evaluate(() => {
    const input = document.querySelector('#png-ppi');
    input.value = '450';
    input.dispatchEvent(new Event('input', { bubbles: true }));
  });
  await page.mouse.up();
  assert.equal(await input.inputValue(), '450');
  await start(24);
  await page.evaluate(() => (document.querySelector('[data-output-settings]').open = false));
  await page.mouse.up();
  await open();
  assert.equal(await input.inputValue(), '450');
  await unchanged();
  // Reopen both the settings disclosure and its outer Export menu.
  await page.locator('[data-output-settings] > summary').click();
  await open();
  assert.equal(await input.inputValue(), '450');
  await page
    .locator('header .command-menu')
    .filter({ has: page.locator('summary', { hasText: /^Export$/ }) })
    .locator(':scope > summary')
    .click();
  await open();
  assert.equal(await input.inputValue(), '450');

  // An unblurred typed PPI is the exact next baseline and still creates no document Undo.
  await input.fill('300.5');
  await start(24);
  await page.mouse.up();
  assert.equal(await input.inputValue(), '320.5');
  const ppi = 320.5,
    expected = pngDimensions(JSON.parse(documentBefore), { mode: 'ppi', ppi });
  assert.equal(
    await page.locator('[data-png-dimensions]').textContent(),
    `${expected.width} × ${expected.height} px`,
  );
  const downloaded = page.waitForEvent('download');
  await page.locator('[data-action="export-png"]').click();
  const download = await downloaded,
    filename = 'test-results/png-ppi/scrub-export.png';
  await download.saveAs(filename);
  const bytes = await fs.readFile(filename);
  assert.equal(bytes.subarray(1, 4).toString(), 'PNG');
  assert.equal(bytes.readUInt32BE(16), expected.width);
  assert.equal(bytes.readUInt32BE(20), expected.height);
  let density;
  for (let offset = 8; offset < bytes.length; offset += bytes.readUInt32BE(offset) + 12)
    if (bytes.toString('ascii', offset + 4, offset + 8) === 'pHYs')
      density = [
        bytes.readUInt32BE(offset + 8),
        bytes.readUInt32BE(offset + 12),
        bytes[offset + 16],
      ];
  assert.deepEqual(density, [pngPixelsPerMetre(ppi), pngPixelsPerMetre(ppi), 1]);
  await unchanged();
  await open();
  await page.screenshot({ path: 'test-results/png-ppi/accepted.png' });
  assert.deepEqual(pageErrors, [], 'no uncaught browser errors');
  console.log(
    JSON.stringify({
      result: 'PASS',
      browser: browser.version(),
      ppi,
      dimensions: expected,
      density,
      documentAndRedo: 'unchanged',
      input: 'actual Chrome mouse/keyboard; owner interruptions synthetic',
    }),
  );
} finally {
  await browser.close();
}
