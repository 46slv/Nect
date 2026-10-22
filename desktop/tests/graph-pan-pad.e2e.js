import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5187');
  await page.waitForSelector('.graph-scroll');
  const nodeId = await page.evaluate(async () => {
    const { document, node } = await import('/src/core.js');
    const d = document();
    const n = node('Rectangle', 100, 70);
    n.collapsed = true;
    const out = node('Output', 900, 500);
    out.collapsed = true;
    out.params.path.source = n.id;
    d.layers[0].nodes = [n, out];
    d.layers[0].output = out.id;
    window.nect.load(JSON.stringify(d));
    return n.id;
  });
  const viewport = page.locator('.graph-scroll');
  const position = () =>
    page.locator(`.node[data-id="${nodeId}"]`).evaluate((el) => [el.offsetLeft, el.offsetTop]);
  const scroll = () => viewport.evaluate((el) => [el.scrollLeft, el.scrollTop]);
  const original = await position();
  for (const action of ['graph-pan-right', 'graph-pan-down', 'graph-pan-left', 'graph-pan-up']) {
    await page.locator(`[data-action="${action}"]`).click();
  }
  assert.deepEqual(await position(), original, 'pan pad does not move nodes');
  assert.deepEqual(await scroll(), [0, 0], 'opposite pan pad actions return to origin');
  for (const zoomStep of ['100%', '5%', '200%']) {
    await page.locator('[data-action="graph-reset"]').click();
    if (zoomStep === '5%')
      for (let i = 0; i < 17; i++) await page.locator('[data-action="graph-zoom-out"]').click();
    if (zoomStep === '200%')
      for (let i = 0; i < 4; i++) await page.locator('[data-action="graph-zoom-in"]').click();
    const before = await scroll();
    await page.locator('[data-action="graph-pan-right"]').click();
    assert.ok((await scroll())[0] >= before[0], `${zoomStep} right pan is bounded`);
    assert.deepEqual(await position(), original, `${zoomStep} pan keeps node geometry`);
  }
  assert.equal(
    await page.locator('[data-action="undo"]').isDisabled(),
    true,
    'pan does not create undo',
  );
  console.log('Graph pan pad: directions, zoom limits, geometry and undo passed');
} finally {
  await browser.close();
}
