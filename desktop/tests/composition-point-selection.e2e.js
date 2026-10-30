import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
const result = { ok: false, cases: [], pageErrors: [] };
page.on('pageerror', (error) => result.pageErrors.push(error.message));
const checkpoint = () =>
  page.evaluate(() => ({
    project: window.nect.projectSnapshot(),
    command: window.nect.commandState,
    file: window.nect.fileState,
    undo: document.querySelector('[data-action="undo"]').disabled,
    redo: document.querySelector('[data-action="redo"]').disabled,
  }));
const tab = (id) => page.locator(`[role="tab"][data-active-composition="${id}"]`);
const panel = (id) => page.locator(`.graph [data-point-panel="${id}"]`);
const selected = (id) =>
  panel(id)
    .locator('.point-selected')
    .evaluateAll((rows) => rows.map((row) => row.dataset.pointRow));
const focus = (id) => panel(id).locator('.point-active').getAttribute('data-point-row');
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const fixture = await page.evaluate(async () => {
    const { blankDocument, node } = await import('/src/core.js');
    const { migrateLegacyDocumentToCompositionProject } = await import(
      '/src/composition-project-model.js'
    );
    const doc = blankDocument(),
      layer = doc.layers[0],
      curve = node('Curve', 40, 50),
      remap = node('Path Remap', 360, 50),
      fill = node('Fill', 680, 50),
      out = node('Output', 1000, 50);
    remap.params.path.source = curve.id;
    fill.params.path.source = remap.id;
    out.params.path.source = fill.id;
    layer.nodes = [curve, remap, fill, out];
    layer.output = out.id;
    const project = migrateLegacyDocumentToCompositionProject(doc),
      other = migrateLegacyDocumentToCompositionProject(blankDocument());
    project.compositions[0].id = 'composition-a';
    project.compositions[0].name = 'A';
    other.compositions[0].id = 'composition-b';
    other.compositions[0].name = 'B';
    project.compositions.push(other.compositions[0]);
    window.nect.load(JSON.stringify(project));
    const initial = JSON.stringify(window.nect.projectSnapshot());
    // A real existing Undo entry must survive tab navigation, not just an empty stack.
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: layer.id, nodeId: curve.id, key: 'closed', value: false },
    });
    return {
      curve: curve.id,
      remap: remap.id,
      points: curve.points.map((point) => point.id),
      initial,
    };
  });
  for (const [kind, id] of [
    ['Remap', fixture.remap],
    ['Curve', fixture.curve],
  ]) {
    await page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
    const browse = panel(id).getByRole('button', { name: 'Browse all points', exact: true });
    if (await browse.count()) await browse.click();
    await panel(id).locator(`[data-select-panel-point="${fixture.points[0]}"]`).click();
    await panel(id).getByRole('button', { name: 'Browse all points', exact: true }).click();
    await panel(id)
      .locator(`[data-select-panel-point="${fixture.points[1]}"]`)
      .click({ modifiers: ['Shift'] });
    assert.deepEqual(await selected(id), fixture.points.slice(0, 2));
    assert.equal(await focus(id), fixture.points[1]);
    const before = await checkpoint();
    // Repeat pointer and keyboard activation. Tab focus is distinct from active point focus.
    await tab('composition-b').click();
    await page.waitForFunction(() => window.nect.activeCompositionId === 'composition-b');
    await tab('composition-a').click();
    await page.waitForFunction(() => window.nect.activeCompositionId === 'composition-a');
    assert.deepEqual(await selected(id), fixture.points.slice(0, 2));
    assert.equal(await focus(id), fixture.points[1]);
    assert.deepEqual(await checkpoint(), before);
    await tab('composition-a').focus();
    await page.keyboard.press('ArrowRight');
    await page.keyboard.press('Enter');
    await page.waitForFunction(() => window.nect.activeCompositionId === 'composition-b');
    await page.keyboard.press('ArrowLeft');
    await page.keyboard.press('Space');
    await page.waitForFunction(() => window.nect.activeCompositionId === 'composition-a');
    assert.deepEqual(await selected(id), fixture.points.slice(0, 2));
    assert.equal(await focus(id), fixture.points[1]);
    assert.deepEqual(await checkpoint(), before);
    if (kind === 'Remap') {
      await page.locator('.canvas-stage').focus();
      await page.keyboard.press('ArrowRight');
      const moved = await checkpoint();
      assert.equal(moved.command.expectedRevision, before.command.expectedRevision + 1);
      assert.notDeepEqual(moved.project, before.project);
      await page.locator('[data-action="undo"]').click();
      assert.deepEqual((await checkpoint()).project, before.project);
      assert.deepEqual(await selected(id), fixture.points.slice(0, 2));
      assert.equal(await focus(id), fixture.points[1]);
      await page.locator('[data-action="redo"]').click();
      assert.deepEqual((await checkpoint()).project, moved.project);
      assert.deepEqual(await selected(id), fixture.points.slice(0, 2));
      assert.equal(await focus(id), fixture.points[1]);
      await page.locator('[data-action="undo"]').click();
      assert.deepEqual((await checkpoint()).project, before.project);
      result.remapUndoRedoSelection = true;
    }
    result.cases.push({
      kind,
      pointIds: await selected(id),
      focus: await focus(id),
      neutral: true,
    });
  }
  const authoredRemap = await page.evaluate(
    (id) => window.nect.document.layers[0].nodes.find((node) => node.id === id),
    fixture.remap,
  );
  assert.deepEqual(authoredRemap.points, []);
  assert.deepEqual(authoredRemap.edits, []);
  const after = await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
  await page.locator('[data-action="undo"]').click();
  assert.equal(
    await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())),
    fixture.initial,
  );
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.projectSnapshot())), after);
  result.historyUndoRedo = true;
  assert.deepEqual(result.pageErrors, []);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/workflow-composition-point-selection.png' });
  result.ok = true;
} catch (error) {
  result.failure = error.stack || String(error);
  throw error;
} finally {
  await fs.mkdir('test-results', { recursive: true });
  await fs.writeFile(
    'test-results/workflow-composition-point-selection.json',
    JSON.stringify(result, null, 2),
  );
  await browser.close();
}
