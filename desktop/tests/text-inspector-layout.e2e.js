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
  const chooser = page.waitForEvent('filechooser');
  await page.locator('.inspector-body [data-load-font]').click();
  await (await chooser).setFiles('tests/fixtures/fonts/NectJPFixture-Regular.otf');
  await page.waitForFunction(() => Object.keys(window.nect.document.fontAssets || {}).length === 1);
  const before = await page.evaluate(() => JSON.stringify(window.nect.document));
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
