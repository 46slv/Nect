import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { activateCommand } from './command-menu-actions.js';
import { verifyProjectFileAtomicity } from './project-file-atomicity.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } }),
  errors = [];
page.on('pageerror', (e) => errors.push(e.message));
page.on('dialog', (d) => d.accept());
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
const load = (s) => page.evaluate((s) => window.nect.load(s), s);
const projectSnapshot = () => page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
const click = async (action, choice = 'discard') => {
  const sessionId =
    action === 'new' ? await page.evaluate(() => window.nect.commandState.sessionId) : null;
  await activateCommand(page, action);
  if (action === 'new') {
    // Ordinary New is dirty in these scenarios. Draft-blocked New is exercised
    // separately below and must never open this Save/Discard/Cancel dialog.
    await page.locator(`dialog[open] button[value="${choice}"]`).click();
    await page.waitForFunction(() => !document.querySelector('dialog[open]'));
    if (choice === 'discard')
      await page.waitForFunction(
        (before) => window.nect.commandState.sessionId !== before,
        sessionId,
      );
  }
};
const blockedNew = async () => {
  await activateCommand(page, 'new', { keyboard: true });
  await page.waitForFunction(() =>
    /Finish or cancel the active edit before changing documents/.test(
      document.querySelector('.status .message')?.textContent || '',
    ),
  );
  assert.equal(await page.locator('dialog[open]').count(), 0);
};
const xy = (p) =>
  page.locator('.artboard').evaluate((svg, p) => {
    const q = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
    return [q.x, q.y];
  }, p);
const point = async (p, handle) => {
  await page.mouse.move(...(await xy(p)));
  await page.mouse.down();
  if (handle) await page.mouse.move(...(await xy(handle)), { steps: 4 });
  await page.mouse.up();
};
const download = async (action, ext) => {
  const pending = page.waitForEvent('download');
  await click(action);
  const path = `test-results/workflow-blank-start.${ext}`;
  await (await pending).saveAs(path);
  return fs.readFile(path);
};
try {
  await fs.mkdir('test-results', { recursive: true });
  await preloadVariableFontDependency(page, process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.waitForSelector('.node');
  const launch = JSON.parse(await snapshot());
  assert.equal(launch.layers[0].nodes.length, 1);
  assert.equal(launch.layers[0].nodes[0].type, 'Output');
  assert.equal(await page.locator('.shape').count(), 0);
  assert.equal(await page.locator('[data-action="add-layer"]').count(), 0);
  // Existing demo-shaped documents remain valid user files and explicit fixtures.
  const demoFixture = await page.evaluate(async () => {
    const { document, serialize } = await import('/src/core.js');
    return serialize(document());
  });
  await load(demoFixture);
  const demo = await snapshot();
  assert.deepEqual(JSON.parse(demo), JSON.parse(demoFixture));
  // Give Cancel a real redo branch to preserve, rather than only an empty stack.
  await page.locator('[data-tool="Circle"]').click();
  const editedDemo = await snapshot();
  await click('undo');
  assert.equal(await snapshot(), demo);
  const demoProject = await projectSnapshot(),
    cancelledState = await page.evaluate(() => window.nect.commandState);
  await click('new', 'cancel');
  assert.equal(await snapshot(), demo);
  assert.equal(await projectSnapshot(), demoProject);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), cancelledState);
  assert.equal(await page.locator('[data-action="redo"]').isDisabled(), false);
  await click('redo');
  assert.equal(await snapshot(), editedDemo);
  await click('undo');
  assert.equal(await snapshot(), demo);
  await click('new');
  const blank = await snapshot(),
    empty = JSON.parse(blank),
    emptyLayer = empty.layers[0];
  assert.equal(empty.layers.length, 1);
  assert.equal(emptyLayer.nodes.length, 1);
  assert.equal(emptyLayer.nodes[0].type, 'Output');
  assert.equal(emptyLayer.allowEmptyOutput, true);
  assert.equal(
    await page
      .locator('.shape,.artwork-selection,.vertex,[data-object-resize],.node-stage-move')
      .count(),
    0,
  );
  assert.equal(await page.locator('.node.selected').count(), 0);
  assert.equal(await page.locator('.warning').count(), 0);
  assert.equal(await page.locator('.inspector-body h3').innerText(), 'Empty composition');
  assert.equal(await page.evaluate(() => document.activeElement.matches('.canvas-stage')), true);
  await point([300, 260]);
  await page.locator('.canvas-stage').press('ArrowRight');
  assert.equal(await snapshot(), blank);
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  await page.keyboard.press('Control+z');
  assert.equal(await snapshot(), blank);
  assert.equal(await page.locator('.node.selected').count(), 0);
  await page.keyboard.press('Control+Shift+z');
  assert.equal(await snapshot(), blank);
  assert.equal(await page.locator('.shape').count(), 0);
  const blankProject = await projectSnapshot(),
    beforeSaveExport = await page.evaluate(() => window.nect.commandState);
  assert.equal(await page.evaluate(() => window.nect.fileState.dirty), true);
  const saved = await download('save', 'json'),
    svg = await download('export', 'svg'),
    png = await download('export-png', 'png');
  // Files contain the entire Composition project, not just its active view.
  assert.deepEqual(JSON.parse(saved.toString()), JSON.parse(blankProject));
  assert.equal(JSON.parse(saved.toString()).compositions.length, 1);
  assert.deepEqual(JSON.parse(saved.toString()).compositions[0].layers, empty.layers);
  await page.waitForFunction(() => !window.nect.fileState.dirty);
  assert.equal(await projectSnapshot(), blankProject, 'Save/export cannot author artwork');
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeSaveExport);
  assert.equal(
    svg.toString(),
    '<svg xmlns="http://www.w3.org/2000/svg" width="960" height="540" viewBox="0 0 960 540"></svg>',
  );
  const pixels = await page.evaluate(async (data) => {
    const i = new Image();
    i.src = 'data:image/png;base64,' + data;
    await i.decode();
    const c = document.createElement('canvas');
    c.width = i.width;
    c.height = i.height;
    const x = c.getContext('2d');
    x.drawImage(i, 0, 0);
    const bytes = x.getImageData(0, 0, c.width, c.height).data;
    return { width: c.width, height: c.height, nonzero: bytes.some((v) => v !== 0) };
  }, png.toString('base64'));
  assert.deepEqual(pixels, { width: 960, height: 540, nonzero: false });
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page.waitForSelector('.node');
  assert.equal(await page.locator('.shape').count(), 0);
  // Reopen the downloaded project through the real browser file chooser after a
  // cold start. Public load alone would not exercise Open's lifecycle boundary.
  const opening = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await page.locator('dialog[open] button[value="discard"]').click();
  await (await opening).setFiles('test-results/workflow-blank-start.json');
  await page.waitForFunction(() => !window.nect.fileState.dirty);
  assert.equal(await snapshot(), blank);
  assert.equal(await projectSnapshot(), blankProject);
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  assert.equal(await page.locator('.shape,.warning').count(), 0);
  await verifyProjectFileAtomicity(page);
  for (const tool of ['Circle', 'Rectangle', 'Polygon', 'Star', 'Text', 'pen']) {
    await load(blank);
    await page.locator(`[data-tool="${tool}"]`).click();
    if (tool === 'Text') await point([300, 260]);
    if (tool === 'pen') {
      await point([250, 110]);
      await point([430, 220], [460, 250]);
      await point([300, 330]);
      await page.keyboard.press('Enter');
    }
    await page.waitForSelector('.shape');
    const after = await snapshot(),
      d = JSON.parse(after);
    assert.equal(d.layers.length, 2);
    assert.deepEqual(d.layers[0], emptyLayer);
    assert.equal(await page.locator('.shape').count(), 1);
    await click('undo');
    assert.equal(await snapshot(), blank);
    assert.equal(await page.locator('.shape').count(), 0);
    await click('redo');
    assert.equal(await snapshot(), after);
    await load(after);
    assert.equal(await snapshot(), after);
  }
  // New is blocked before any unsaved dialog while a released or pressed Pen
  // draft exists. Menu activation must not finish, discard or change that draft.
  for (const pressed of [false, true]) {
    await load(blank);
    await page.locator('[data-tool="pen"]').click();
    await point([200, 100]);
    await point([360, 180], [390, 210]);
    if (pressed) {
      await page.mouse.move(...(await xy([440, 300])));
      await page.mouse.down();
      await page.mouse.move(...(await xy([470, 330])), { steps: 3 });
    }
    const before = await snapshot(),
      project = await projectSnapshot(),
      command = await page.evaluate(() => window.nect.commandState),
      gesture = await page.evaluate(() => window.nect.gestureState),
      drawing = await page.locator('.artboard').innerHTML();
    const projectMenu = page.locator('header details.command-menu').filter({
      has: page.locator('summary', { hasText: /^Project$/ }),
    });
    assert.equal(await projectMenu.getAttribute('open'), null);
    await projectMenu.locator(':scope > summary').press('Enter');
    assert.equal(await projectMenu.evaluate((menu) => menu.open), true);
    assert.equal(await projectSnapshot(), project);
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), command);
    assert.deepEqual(await page.evaluate(() => window.nect.gestureState), gesture);
    assert.equal(await snapshot(), before);
    assert.equal(await page.locator('.artboard').innerHTML(), drawing);
    assert.equal(await page.locator('[data-pen-draft]').count(), 1);
    await blockedNew();
    assert.equal(await projectSnapshot(), project);
    assert.deepEqual(await page.evaluate(() => window.nect.commandState), command);
    assert.deepEqual(await page.evaluate(() => window.nect.gestureState), gesture);
    assert.equal(await snapshot(), before);
    assert.equal(await page.locator('.artboard').innerHTML(), drawing);
    assert.equal(await page.locator('[data-pen-draft]').count(), 1);
    if (pressed) await page.mouse.up();
    await page.locator('[data-pen-cancel]').click();
    assert.equal(await snapshot(), before);
  }
  await page.locator('[data-tool="pen"]').click();
  await point([200, 100]);
  await point([360, 180]);
  await blockedNew();
  assert.equal(await page.locator('[data-pen-draft]').count(), 1);
  await page.locator('.canvas-stage').press('Enter');
  assert.equal(JSON.parse(await snapshot()).layers.length, 2);
  await click('new');
  assert.equal(JSON.parse(await snapshot()).layers[0].nodes.length, 1);
  assert.equal(await page.locator('[data-pen-draft]').count(), 0);
  await page.keyboard.press('Control+z');
  assert.equal(JSON.parse(await snapshot()).layers.length, 1);
  assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
  await page.keyboard.press('Control+Shift+z');
  assert.equal(await page.locator('.shape').count(), 0);
  // +Blank appends from hidden/locked old work; demo artwork is fixture-only.
  await load(demo);
  const oldId = JSON.parse(demo).layers[0].id;
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-layer-lock="${oldId}"]`).click();
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-visible="${oldId}"]`).click();
  const prior = await snapshot();
  await click('add-blank-layer');
  const appended = await snapshot(),
    d = JSON.parse(appended);
  assert.equal(d.layers.length, 2);
  assert.deepEqual(d.layers[0], JSON.parse(prior).layers[0]);
  assert.equal(d.layers[1].nodes.length, 1);
  assert.equal(await page.locator('.shape').count(), 0);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${d.layers[1].id}"]`).click();
  assert.equal(await page.locator('.artwork-selection').count(), 0);
  await click('undo');
  assert.equal(await snapshot(), prior);
  await click('redo');
  assert.equal(await snapshot(), appended);
  assert.equal(await page.locator('[data-action="add-layer"]').count(), 0);
  // A keyboard-activated +Blank cannot carry a pressed old-layer drag into its new graph.
  await load(demo);
  const handle = page.locator('.node-stage-move rect'),
    rect = await handle.boundingBox();
  await page.mouse.move(rect.x + rect.width / 2, rect.y + rect.height / 2);
  await page.mouse.down();
  await page.mouse.move(rect.x + rect.width / 2 + 25, rect.y + rect.height / 2 + 15, { steps: 3 });
  assert.equal(await snapshot(), demo, 'pressed drag keeps committed artwork unchanged');
  assert.notEqual(
    await page.evaluate(() =>
      JSON.stringify(window.nect.previewSnapshot(window.nect.gestureState.gestureId).document),
    ),
    demo,
    'the pressed drag must have a real preview to roll back',
  );
  await page.locator('[data-action="add-blank-layer"]').press('Enter');
  assert.deepEqual(JSON.parse(await snapshot()).layers[0], JSON.parse(demo).layers[0]);
  assert.equal(await page.evaluate(() => window.nect.gestureState), null);
  const afterInterrupted = await snapshot();
  await page.mouse.move(rect.x + 90, rect.y + 70);
  await page.mouse.up();
  assert.equal(await snapshot(), afterInterrupted);
  await click('undo');
  assert.equal(await snapshot(), demo);
  const capacity = await page.evaluate(async () => {
    const { blankDocument, blankLayer } = await import('/src/core.js');
    const d = blankDocument();
    while (d.layers.length < 100) d.layers.push(blankLayer('Blank ' + d.layers.length));
    window.nect.load(JSON.stringify(d));
    return JSON.stringify(window.nect.document);
  });
  await click('add-blank-layer');
  assert.equal(await snapshot(), capacity);
  assert.match(await page.locator('.status .message').innerText(), /100/);
  for (const size of [
    { width: 1280, height: 800 },
    { width: 1600, height: 1000 },
  ]) {
    await page.setViewportSize(size);
    await load(blank);
    await click('new');
    await click('fit');
    await ensureWorkspacePanel(page, 'inspector');
    assert.equal(await page.locator('.inspector-body h3').innerText(), 'Empty composition');
    assert.equal(await page.locator('.node.selected').count(), 0);
    await ensureWorkspacePanel(page, 'layers');
    const button = page.locator('[data-action="add-blank-layer"]'),
      box = await button.boundingBox();
    assert.ok(box.x >= 0 && box.x + box.width <= size.width && box.y + box.height <= size.height);
    assert.equal(
      await button.evaluate((el) => {
        const r = el.getBoundingClientRect();
        return document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2) === el;
      }),
      true,
    );
    await page.screenshot({
      path: `test-results/blank-start-${size.width}x${size.height}.png`,
      fullPage: true,
    });
  }
  assert.deepEqual(errors, []);
  console.log(
    'PASS blank start: confirmed/cancelled New, exact Undo/Redo, empty selection and no phantom hits, Save/reload/transparent SVG+PNG, every independent tool/Pen, draft preservation, +Blank atomicity/capacity, fixture-only demo compatibility and 1280/1600 layout',
  );
} finally {
  await browser.close();
}
