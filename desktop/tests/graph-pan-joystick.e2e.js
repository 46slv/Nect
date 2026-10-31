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
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
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
    [-18, 0],
    [0, -18],
    [0, 18],
    [18, -18],
    [18, 18],
    [-18, -18],
    [-18, 18],
  ]) {
    await viewport.evaluate((el) => el.scrollTo(500, 500));
    const box = await stick.boundingBox(),
      x = box.x + box.width / 2,
      y = box.y + box.height / 2;
    const contentBefore = await page.locator(`.node[data-id="${nodeId}"]`).evaluate((el) => {
      const rect = el.getBoundingClientRect();
      return [rect.x, rect.y];
    });
    await page.mouse.move(x, y);
    await page.mouse.down();
    await page.mouse.move(x + dx, y + dy);
    await page.waitForFunction(
      ([dx, dy]) => {
        const el = document.querySelector('.graph-scroll');
        return (
          (!dx || Math.sign(el.scrollLeft - 500) === Math.sign(dx)) &&
          (!dy || Math.sign(el.scrollTop - 500) === Math.sign(dy))
        );
      },
      [dx, dy],
    );
    await page.mouse.up();
    const stopped = await scroll();
    const contentAfter = await page.locator(`.node[data-id="${nodeId}"]`).evaluate((el) => {
      const rect = el.getBoundingClientRect();
      return [rect.x, rect.y];
    });
    for (const [axis, direction] of [dx, dy].entries()) {
      assert.equal(Math.sign(stopped[axis] - 500), Math.sign(direction), 'view follows tilt');
      assert.equal(
        Math.sign(contentAfter[axis] - contentBefore[axis]),
        direction ? -Math.sign(direction) : 0,
        'visible content moves opposite the view',
      );
    }
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
    await page.keyboard.down('ArrowRight');
    await page.waitForFunction(() => document.querySelector('.graph-scroll').scrollLeft > 10);
    await page.keyboard.up('ArrowRight');
    assert.deepEqual(
      await page
        .locator(`.node[data-id="${nodeId}"]`)
        .evaluate((el) => [el.offsetLeft, el.offsetTop]),
      position,
    );
    assert.equal(await saved(), original, `${zoomStep} view-only keyboard pan`);
  }
  for (const input of ['pointer', 'keyboard']) {
    await viewport.evaluate((el) => el.scrollTo(500, 500));
    if (input === 'pointer') {
      const box = await stick.boundingBox();
      await page.mouse.move(box.x + box.width / 2 - 18, box.y + box.height / 2);
      await page.mouse.down();
    } else {
      await stick.focus();
      await page.keyboard.down('ArrowRight');
    }
    await page.waitForFunction(() => document.querySelector('.graph-scroll').scrollLeft !== 500);
    await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem', { name: 'Hide pan joystick', exact: true }).focus();
    await page.keyboard.press('Enter');
    assert.equal(await stick.isVisible(), false);
    assert.equal(await viewport.evaluate((el) => document.activeElement === el), true);
    const stopped = await scroll();
    await page.evaluate(async () => {
      await new Promise(requestAnimationFrame);
      await new Promise(requestAnimationFrame);
    });
    assert.deepEqual(await scroll(), stopped, 'hiding stops held input');
    if (input === 'pointer') await page.mouse.up();
    else await page.keyboard.up('ArrowRight');
    await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem', { name: 'Show pan joystick', exact: true }).focus();
    await page.keyboard.press('Enter');
    assert.equal(await stick.isVisible(), true);
    if (input === 'pointer') {
      const box = await stick.boundingBox();
      await page.mouse.move(box.x + box.width / 2 + 18, box.y + box.height / 2);
      await page.mouse.down();
    } else {
      await stick.focus();
      await page.keyboard.down('ArrowRight');
    }
    await page.waitForFunction(
      (x) => document.querySelector('.graph-scroll').scrollLeft > x,
      stopped[0],
    );
    if (input === 'pointer') await page.mouse.up();
    else await page.keyboard.up('ArrowRight');
    assert.equal(await saved(), original, 'Hide/show and resumed pan are document/history neutral');
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
    'Graph joystick: viewport follows tilt in all8directions; content moves opposite; release, zoom, geometry/history and placement passed',
  );
} finally {
  await browser.close();
}
