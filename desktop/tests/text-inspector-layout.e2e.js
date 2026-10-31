import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
try {
  const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.evaluate(async () => {
    const { document, node, connect } = await import('/src/core.js');
    const d = document(),
      l = d.layers[0],
      t = node('Text'),
      f = node('Fill'),
      o = node('Output');
    t.text = 'AB Text';
    t.params.position.value = [120, 180];
    t.params.size.value = 64;
    l.nodes = [t, f, o];
    l.output = o.id;
    connect(l, t.id, f.id, 'path');
    connect(l, f.id, o.id, 'path');
    window.nect.load(JSON.stringify(d));
  });
  await page.locator('.inspector-body [data-text]').waitFor({ state: 'visible' });
  const beforeDisclosure = await page.evaluate(() => JSON.stringify(window.nect.document));
  const advanced = page.locator('.inspector-body .text-font-advanced');
  if (!(await advanced.evaluate((details) => details.open)))
    await advanced.locator(':scope > summary').click();
  assert.equal(await advanced.getAttribute('open'), '');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), beforeDisclosure);
  const importFont = advanced.locator('[data-load-font]');
  await importFont.waitFor({ state: 'visible' });
  const [chooser] = await Promise.all([page.waitForEvent('filechooser'), importFont.click()]);
  await chooser.setFiles('tests/fixtures/fonts/NectJPFixture-Regular.otf');
  await page.waitForFunction(() => Object.keys(window.nect.document.fontAssets || {}).length === 1);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
  assert.equal(await advanced.getAttribute('open'), '');
  await advanced.locator(':scope > summary').focus();
  await advanced.locator(':scope > summary').press('Space');
  assert.equal(await advanced.getAttribute('open'), null);
  await advanced.locator(':scope > summary').press('Enter');
  assert.equal(await advanced.getAttribute('open'), '');
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  await page.screenshot({ path: 'test-results/text-inspector-after.png' });
  await page.locator('[data-text-inspector-section="typography"] > summary').click();
  assert.equal(
    await page.locator('[data-text-inspector-section="typography"]').getAttribute('open'),
    '',
  );
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), before);
  await page.setViewportSize({ width: 1024, height: 768 });
  await page.screenshot({ path: 'test-results/text-inspector-after-narrow.png' });
  console.log('Actual Chrome layout and disclosure, no document mutation PASS');
} finally {
  await browser.close();
}
