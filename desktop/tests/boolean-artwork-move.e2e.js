import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
let page = await browser.newPage({
  viewport: { width: 1600, height: 1000 },
  acceptDownloads: true,
});
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const base = process.env.BASE_URL || 'http://127.0.0.1:5173';
const dir = 'test-results/boolean-artwork-move';
const snapshot = () => page.evaluate(() => window.nect.snapshot());
const state = () => page.evaluate(() => window.nect.commandState);
const screen = (point) =>
  page.locator('.artboard').evaluate((svg, xy) => {
    const p = new DOMPoint(...xy).matrixTransform(svg.getScreenCTM());
    return [p.x, p.y];
  }, point);
const drag = async (start, end, finish = 'up') => {
  const a = await screen(start),
    b = await screen(end);
  await page.keyboard.down('Alt');
  await page.mouse.move(...a);
  await page.mouse.down();
  await page.mouse.move(...b, { steps: 4 });
  if (finish === 'cancel') await page.keyboard.press('Escape');
  else if (finish === 'return') await page.mouse.move(...a, { steps: 4 });
  await page.mouse.up();
  await page.keyboard.up('Alt');
};
const download = async (action, name) => {
  const pending = page.waitForEvent('download');
  await activateCommand(page, action);
  const file = await pending;
  await file.saveAs(`${dir}/${name}`);
};
try {
  await fs.mkdir(dir, { recursive: true });
  await page.goto(base);
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  await page.evaluate(async () => {
    const { blankDocument, blankLayer, node, connect } = await import('/src/core.js');
    const doc = blankDocument(),
      layer = doc.layers[0],
      output = layer.nodes[0];
    layer.id = 'boolean-layer';
    layer.name = 'Boolean mark';
    output.id = 'boolean-output';
    layer.output = output.id;
    const a = node('Circle'),
      b = node('Circle'),
      bool = node('Boolean');
    a.id = 'outer';
    b.id = 'inner';
    bool.id = 'boolean';
    a.params.center.value = [245, 260];
    a.params.radius.value = 110;
    b.params.center.value = [245, 260];
    b.params.radius.value = 55;
    bool.operation = 'subtract';
    const transform = node('Transform'),
      fill = node('Fill');
    transform.id = 'boolean-transform';
    fill.id = 'boolean-fill';
    fill.params.color.value = { hex: '#183744', alpha: 1 };
    layer.nodes = [a, b, bool, transform, fill, output];
    connect(layer, a.id, bool.id, 'a');
    connect(layer, b.id, bool.id, 'b');
    connect(layer, bool.id, transform.id, 'path');
    connect(layer, transform.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    const other = blankLayer('Other shape'),
      rect = node('Rectangle'),
      paint = node('Fill');
    other.id = 'other-layer';
    rect.id = 'other-rect';
    paint.id = 'other-fill';
    rect.params.center.value = [640, 260];
    rect.params.size.value = [120, 80];
    other.nodes.unshift(rect, paint);
    connect(other, rect.id, paint.id, 'path');
    connect(other, paint.id, other.output, 'path');
    doc.layers.push(other);
    window.nect.load(JSON.stringify(doc));
  });
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-select-layer="boolean-layer"]').click();
  await ensureWorkspacePanel(page, 'inspector');
  assert.match(await page.locator('.artwork-move-status').innerText(), /Drag artwork/);
  assert.equal(await page.locator('[data-duplicate-artwork]').isDisabled(), true);
  assert.match(
    await page.locator('.artwork-duplicate-status').innerText(),
    /unavailable for this artwork/,
  );
  const hit = await screen([320, 260]);
  assert.equal(
    await page.evaluate(
      ([x, y]) => document.elementFromPoint(x, y)?.closest('.shape')?.dataset.layer,
      hit,
    ),
    'boolean-layer',
  );
  const before = await snapshot(),
    beforeState = await state();
  await drag([320, 260], [345, 275]);
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset
        .value[0] !== 0,
  );
  const after = await snapshot();
  const moved = after.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset.value;
  assert.ok(Math.abs(moved[0] - 25) < 0.01 && Math.abs(moved[1] - 15) < 0.01, String(moved));
  const expected = await page.evaluate(
    async ({ before, moved }) => {
      const { CommandSession } = await import('/src/core.js');
      const session = new CommandSession(before);
      session.dispatch({
        ...session.state,
        type: 'translate-artworks',
        payload: { layerIds: ['boolean-layer'], delta: moved },
      });
      return session.snapshot();
    },
    { before, moved },
  );
  assert.deepEqual(after, expected);
  assert.equal((await state()).expectedRevision, beforeState.expectedRevision + 1);
  assert.deepEqual(
    after.layers[0].nodes.filter((n) => n.id !== 'boolean-transform'),
    before.layers[0].nodes.filter((n) => n.id !== 'boolean-transform'),
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before);
  const undone = await state();
  await drag([320, 260], [345, 275], 'cancel');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), undone);
  await drag([320, 260], [345, 275], 'return');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), undone);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), after);
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('ArrowRight');
  await page.waitForFunction(
    (x) =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset
        .value[0] ===
      x + 1,
    moved[0],
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), after);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-select-layer="other-layer"]').click({ modifiers: ['Shift'] });
  assert.match(
    await page.locator('.artwork-duplicate-status').innerText(),
    /unavailable for this selection/,
  );
  const beforeMixed = await snapshot();
  await drag([345, 275], [365, 265]);
  await page.waitForFunction(
    (x) =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset
        .value[0] >
      x + 19,
    moved[0],
  );
  const mixed = await snapshot();
  const movedAgain = mixed.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset
    .value;
  const other = mixed.layers[1].nodes.find((n) => n.id === 'other-rect').params.center.value;
  assert.ok(
    Math.abs(movedAgain[0] - moved[0] - 20) < 0.01 &&
      Math.abs(movedAgain[1] - moved[1] + 10) < 0.01,
  );
  assert.ok(Math.abs(other[0] - 660) < 0.01 && Math.abs(other[1] - 250) < 0.01);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), beforeMixed);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), mixed);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), beforeMixed);
  await page.screenshot({ path: `${dir}/moved.png` });
  await download('save', 'project.json');
  await download('export', 'mark.svg');
  await download('export-png', 'mark.png');
  assert.match(await fs.readFile(`${dir}/mark.svg`, 'utf8'), /<svg/);
  const png = await fs.readFile(`${dir}/mark.png`);
  assert.deepEqual([...png.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
  const pixels = await page.evaluate(
    async ({ encoded, moved }) => {
      const image = new Image();
      image.src = `data:image/png;base64,${encoded}`;
      await image.decode();
      const canvas = document.createElement('canvas');
      canvas.width = image.width;
      canvas.height = image.height;
      const context = canvas.getContext('2d');
      context.drawImage(image, 0, 0);
      const alpha = (x, y) => context.getImageData(Math.round(x), Math.round(y), 1, 1).data[3];
      return {
        hole: alpha(245 + moved[0], 260 + moved[1]),
        ring: alpha(320 + moved[0], 260 + moved[1]),
      };
    },
    { encoded: png.toString('base64'), moved },
  );
  assert.equal(pixels.hole, 0, 'the Boolean counter remains transparent');
  assert.equal(pixels.ring, 255, 'the translated ring remains painted');
  const project = await page.evaluate(() => window.nect.projectSnapshot());
  await page.close();
  page = await browser.newPage({ viewport: { width: 1600, height: 1000 }, acceptDownloads: true });
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto(base);
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  const opening = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  const discard = page.locator('dialog button[value="discard"]');
  if (await discard.isVisible()) await discard.click();
  await (await opening).setFiles(`${dir}/project.json`);
  await page.waitForFunction(() =>
    window.nect.document.layers.some((l) => l.id === 'boolean-layer'),
  );
  assert.deepEqual(await page.evaluate(() => window.nect.projectSnapshot()), project);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-select-layer="boolean-layer"]').click();
  const radius = page.locator('.graph .field[data-node="outer"][data-key="radius"] [data-value]');
  await radius.fill('120');
  await radius.press('Tab');
  await page.waitForFunction(
    () =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'outer').params.radius.value ===
      120,
  );
  const edited = await snapshot();
  assert.equal(edited.layers[0].nodes.find((n) => n.id === 'boolean').params.a.source, 'outer');
  assert.deepEqual(
    edited.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset.value,
    moved,
  );
  await page.setViewportSize({ width: 1000, height: 650 });
  await page.waitForFunction(() => innerWidth === 1000 && innerHeight === 650);
  await ensureWorkspacePanel(page, 'inspector');
  assert.match(await page.locator('.artwork-move-status').innerText(), /Drag artwork/);
  const narrowHit = await screen([345, 275]);
  assert.equal(
    await page.evaluate(
      ([x, y]) => document.elementFromPoint(x, y)?.closest('.shape')?.dataset.layer,
      narrowHit,
    ),
    'boolean-layer',
  );
  await page.screenshot({ path: `${dir}/minimum-size.png` });
  await drag([345, 275], [355, 280]);
  await page.waitForFunction(
    (x) =>
      window.nect.document.layers[0].nodes.find((n) => n.id === 'boolean-transform').params.offset
        .value[0] >
      x + 9,
    moved[0],
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), edited);
  await page.evaluate(() => {
    const send = (type, payload) =>
      window.nect.dispatch({ ...window.nect.commandState, type, payload });
    const target = { layerId: 'boolean-layer', nodeId: 'boolean-transform' };
    send('set-transform-control', { ...target, key: 'uniformScale', value: false });
    send('set-transform-control', { ...target, key: 'scaleWidth', value: 0 });
    send('set-transform-control', { ...target, key: 'rotation', value: 45 });
    send('set-document-settings', { key: 'width', value: 961 });
    send('undo', {});
  });
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-select-layer="boolean-layer"]').click();
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() =>
    document.querySelector('.artwork-move-status')?.textContent.includes('nonzero area'),
  );
  const collapsed = await snapshot(),
    collapsedState = await state();
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('ArrowRight');
  assert.deepEqual(await snapshot(), collapsed);
  assert.deepEqual(await state(), collapsedState);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await snapshot()).width, 961);
  assert.deepEqual(errors, []);
  console.log(
    'PASS: Boolean Canvas move, Session parity, cancellation, history, strict duplication, exports and reopen.',
  );
} finally {
  await browser.close();
}
