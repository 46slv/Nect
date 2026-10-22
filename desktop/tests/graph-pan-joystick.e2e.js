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
  await page.waitForSelector('.graph-pan-joystick');
  const nodeId = await page.evaluate(async () => {
    const { document, node } = await import('/src/core.js');
    const d = document(),
      n = node('Rectangle', 100, 70),
      out = node('Output', 900, 500);
    n.collapsed = out.collapsed = true;
    out.params.path.source = n.id;
    d.layers[0].nodes = [n, out];
    d.layers[0].output = out.id;
    window.nect.load(JSON.stringify(d));
    return n.id;
  });
  const viewport = page.locator('.graph-scroll'),
    stick = page.locator('.graph-pan-joystick');
  const saved = () =>
    page.evaluate(() =>
      JSON.stringify({
        project: window.nect.projectSnapshot(),
        svg: window.nect.export(),
        file: window.nect.fileState,
        command: window.nect.commandState,
      }),
    );
  const scroll = () => viewport.evaluate((el) => [el.scrollLeft, el.scrollTop]);
  const original = await saved();
  const position = await page
    .locator(`.node[data-id="${nodeId}"]`)
    .evaluate((el) => [el.offsetLeft, el.offsetTop]);
  for (const [dx, dy] of [
    [18, 0],
    [0, -18],
    [-18, 18],
  ]) {
    await viewport.evaluate((el) => el.scrollTo(500, 500));
    const box = await stick.boundingBox(),
      x = box.x + box.width / 2,
      y = box.y + box.height / 2;
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + dx, y + dy);
    await page.waitForFunction(
      ([dx, dy]) => {
        const el = document.querySelector('.graph-scroll');
        return (
          (!dx || Math.sign(500 - el.scrollLeft) === Math.sign(dx)) &&
          (!dy || Math.sign(500 - el.scrollTop) === Math.sign(dy))
        );
      },
      [dx, dy],
    );
    await page.mouse.up();
    const stopped = await scroll();
    await page.waitForTimeout(80);
    assert.deepEqual(await scroll(), stopped, 'release stops motion');
    assert.equal(await saved(), original, 'pointer navigation is document/history neutral');
  }
  for (const zoomStep of ['100%', '5%', '200%']) {
    await page.locator('[data-action="graph-reset"]').click();
    if (zoomStep === '5%')
      for (let i = 0; i < 17; i++) await page.locator('[data-action="graph-zoom-out"]').click();
    if (zoomStep === '200%')
      for (let i = 0; i < 4; i++) await page.locator('[data-action="graph-zoom-in"]').click();
    await stick.focus();
    await page.keyboard.down('ArrowLeft');
    await page.waitForFunction(() => document.querySelector('.graph-scroll').scrollLeft > 10);
    await page.keyboard.up('ArrowLeft');
    assert.deepEqual(
      await page
        .locator(`.node[data-id="${nodeId}"]`)
        .evaluate((el) => [el.offsetLeft, el.offsetTop]),
      position,
    );
    assert.equal(await saved(), original, `${zoomStep} view-only keyboard pan`);
  }
  await page.locator('[data-action="graph-overview"]').click();
  const [stickBox, overviewBox, viewportBox] = await Promise.all([
    stick.boundingBox(),
    page.locator('.graph-overview').boundingBox(),
    viewport.boundingBox(),
  ]);
  assert.ok(stickBox.x + stickBox.width < viewportBox.x + viewportBox.width);
  assert.ok(stickBox.y + stickBox.height < viewportBox.y + viewportBox.height);
  assert.ok(
    stickBox.y >= overviewBox.y + overviewBox.height ||
      overviewBox.x + overviewBox.width <= stickBox.x,
    'Overview does not overlap joystick',
  );
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  console.log(
    'Graph joystick: direction, release, zoom limits, geometry/history and placement passed',
  );
} finally {
  await browser.close();
}
