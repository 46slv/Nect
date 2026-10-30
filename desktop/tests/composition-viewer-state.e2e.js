import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const result = { ok: false, pageErrors: [] };
page.on('pageerror', (error) => result.pageErrors.push(error.message));
const tab = (id) => page.locator(`[role="tab"][data-active-composition="${id}"]`);
const status = () => page.locator('[data-viewer-status]').allTextContents();
const checkpoint = () =>
  page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    file: window.nect.fileState,
    undo: document.querySelector('[data-action="undo"]').disabled,
    redo: document.querySelector('[data-action="redo"]').disabled,
  }));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const fixture = await page.evaluate(async () => {
    const { blankDocument, node } = await import('/src/core.js');
    const { migrateLegacyDocumentToCompositionProject } = await import(
      '/src/composition-project-model.js'
    );
    const document = blankDocument(),
      rectangle = node('Rectangle', 40, 40),
      separate = node('Separate Vector', 350, 40),
      math = node('Math', 40, 40);
    document.layers[0].nodes.push(rectangle, separate);
    const project = migrateLegacyDocumentToCompositionProject(document),
      other = migrateLegacyDocumentToCompositionProject(blankDocument());
    project.compositions[0].id = 'composition-a';
    project.compositions[0].name = 'A';
    other.compositions[0].id = 'composition-b';
    other.compositions[0].name = 'B';
    other.compositions[0].layers[0].nodes.push(math);
    project.compositions.push(other.compositions[0]);
    window.nect.load(JSON.stringify(project));
    const initial = window.nect.projectSnapshot();
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: {
        layerId: document.layers[0].id,
        nodeId: rectangle.id,
        key: 'size',
        value: [310, 170],
      },
    });
    return { rectangle: rectangle.id, separate: separate.id, math: math.id, initial };
  });
  const select = async (id) => {
    await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  };
  await select(fixture.rectangle);
  await page.locator('[data-assign-viewer="0"]').click();
  await select(fixture.separate);
  assert.equal(await page.locator('[data-assign-viewer="1"]').isDisabled(), true);
  await page.locator('[aria-label="Viewer public output"]').selectOption('1');
  await page.locator('[data-assign-viewer="1"]').click();
  assert.deepEqual(await status(), ['Live', 'Live']);
  const labels = await page.locator('[data-viewer-label]').allTextContents(),
    body = page.locator('.node-viewer-body').first();
  const fittedViewBox = await body.locator('svg').getAttribute('viewBox');
  await body.hover();
  await page.mouse.wheel(0, 40);
  await page.waitForFunction(
    (before) => document.querySelector('.node-viewer-body svg')?.getAttribute('viewBox') !== before,
    fittedViewBox,
  );
  const viewBox = await body.locator('svg').getAttribute('viewBox'),
    before = await checkpoint();
  await tab('composition-b').click();
  await select(fixture.math);
  await page.locator('[data-assign-viewer="0"]').click();
  assert.deepEqual(await status(), ['Live', 'Unassigned']);
  const bLabels = await page.locator('[data-viewer-label]').allTextContents();
  await tab('composition-a').click();
  result.firstReturn = {
    statuses: await status(),
    labels: await page.locator('[data-viewer-label]').allTextContents(),
  };
  assert.deepEqual(await status(), ['Live', 'Live']);
  assert.deepEqual(await page.locator('[data-viewer-label]').allTextContents(), labels);
  assert.equal(await body.locator('svg').getAttribute('viewBox'), viewBox);
  assert.deepEqual(await checkpoint(), before);
  await tab('composition-a').focus();
  await page.keyboard.press('ArrowRight');
  await page.keyboard.press('Enter');
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'composition-b');
  assert.deepEqual(await status(), ['Live', 'Unassigned']);
  assert.deepEqual(await page.locator('[data-viewer-label]').allTextContents(), bLabels);
  await page.keyboard.press('ArrowLeft');
  await page.keyboard.press('Space');
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'composition-a');
  assert.deepEqual(await status(), ['Live', 'Live']);
  assert.deepEqual(await page.locator('[data-viewer-label]').allTextContents(), labels);
  assert.equal(await body.locator('svg').getAttribute('viewBox'), viewBox);
  assert.deepEqual(await checkpoint(), before);
  result.pointerAndKeyboardNeutral = true;
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual((await checkpoint()).project, fixture.initial);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual((await checkpoint()).project, before.project);
  assert.deepEqual(await status(), ['Live', 'Live']);
  result.originalHistoryPreserved = true;
  const afterHistory = await checkpoint();
  await page.locator('[data-viewer-clear="0"]').click();
  await tab('composition-b').click();
  await tab('composition-a').click();
  assert.deepEqual(await status(), ['Unassigned', 'Live']);
  assert.deepEqual(await checkpoint(), afterHistory);
  result.clearRetained = true;
  await select(fixture.rectangle);
  await page.locator('[data-assign-viewer="0"]').click();
  await page.locator('[data-viewer-mode="artwork"]').click();
  await tab('composition-b').click();
  await tab('composition-a').click();
  assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'artwork');
  await page.locator('[data-viewer-mode="viewers"]').click();
  assert.deepEqual(await status(), ['Live', 'Live']);
  result.artworkModeRetained = true;
  assert.deepEqual(result.pageErrors, []);
  result.ok = true;
} catch (error) {
  result.failure = error.stack || String(error);
  throw error;
} finally {
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-composition-viewer-state.png' });
  await fs.writeFile(
    'test-results/workflow-composition-viewer-state.json',
    JSON.stringify(result, null, 2),
  );
  await browser.close();
}
