import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } });
const errors = [];
const clickUndo = () => page.locator('[data-action="undo"]').click();
page.on('pageerror', (e) => errors.push(e.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5184');
  await page.waitForSelector('.shape');
  await page.locator('#add-node').selectOption('Math');
  const setup = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      layer = before.layers[0],
      math = layer.nodes.find((n) => n.type === 'Math');
    const headless = new CommandSession(before);
    headless.dispatch({
      ...headless.state,
      type: 'set-operation',
      payload: { layerId: layer.id, nodeId: math.id, operation: 'multiply' },
    });
    return {
      before,
      expected: headless.snapshot(),
      math: math.id,
      state: window.nect.commandState,
    };
  });
  await page.locator(`.graph [data-operation="${setup.math}"]`).selectOption('multiply');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), setup.expected);
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    setup.state.expectedRevision + 1,
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), setup.before);
  const refusal = await page.evaluate((state) => {
    try {
      window.nect.dispatch({ ...state, type: 'redo' });
      return null;
    } catch (e) {
      return e.message;
    }
  }, setup.state);
  assert.match(refusal, /STALE_REVISION/);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), setup.expected);
  const property = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      layer = before.layers[0],
      circle = layer.nodes.find((n) => n.type === 'Circle');
    const headless = new CommandSession(before);
    headless.dispatch({
      ...headless.state,
      type: 'set-parameter',
      payload: { layerId: layer.id, nodeId: circle.id, key: 'radius', value: 175 },
    });
    return {
      before,
      expected: headless.snapshot(),
      circle: circle.id,
      revision: window.nect.commandState.expectedRevision,
    };
  });
  const field = () =>
    page.locator(`.graph .field[data-node="${property.circle}"][data-key="radius"]`);
  await field().locator('[data-value]').fill('175');
  await field().locator('[data-value]').dispatchEvent('change');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), property.expected);
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    property.revision + 1,
  );
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), property.before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  const expressionButton = field().locator('[data-fx]');
  assert.equal(await expressionButton.locator('svg[data-icon="expression"]').count(), 1);
  assert.match(await expressionButton.getAttribute('aria-label'), /Expression and parameter references/);
  page.once('dialog', (dialog) => dialog.accept('20 * 9'));
  await field().locator('[data-fx]').click();
  assert.equal(await field().locator('[data-expression]').inputValue(), '20 * 9');
  const expressed = await page.evaluate(() => window.nect.snapshot());
  await field().locator('[data-expression]').fill('1 / 0');
  await field().locator('[data-expression]').dispatchEvent('change');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), expressed);
  await field().locator('[data-lock]').click();
  assert.equal(await field().locator('[data-expression]').isDisabled(), true);
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), expressed);
  const metadata = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      layer = before.layers[0],
      math = layer.nodes.find((n) => n.type === 'Math');
    const headless = new CommandSession(before);
    headless.dispatch({
      ...headless.state,
      type: 'rename',
      payload: { layerId: layer.id, nodeId: math.id, name: '計算 A' },
    });
    return { before, expected: headless.snapshot(), layerId: layer.id, math: math.id };
  });
  await page.locator(`[data-rename-node="${metadata.math}"]`).press('F2');
  await page.locator('[data-rename-name]').fill('計算 A');
  await page.locator('[data-rename-name]').press('Enter');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), metadata.expected);
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), metadata.before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  const beforeBypass = await page.evaluate(() => window.nect.snapshot());
  await page.locator(`.graph .node[data-id="${metadata.math}"] [data-bypass]`).click();
  assert.equal(
    (await page.evaluate(() => window.nect.snapshot())).layers[0].nodes.find(
      (n) => n.id === metadata.math,
    ).bypassed,
    true,
  );
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), beforeBypass);
  await page.locator(`[data-layer-lock="${metadata.layerId}"]`).click();
  await page.locator(`[data-visible="${metadata.layerId}"]`).click();
  const locked = await page.evaluate(() => window.nect.snapshot());
  assert.equal(locked.layers[0].locked, true);
  assert.equal(locked.layers[0].visible, false);
  await page.locator(`[data-layer-lock="${metadata.layerId}"]`).click();
  await page.locator(`[data-visible="${metadata.layerId}"]`).click();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), beforeBypass);
  await page.locator('#add-node').selectOption('Rectangle');
  const created = (await page.evaluate(() => window.nect.snapshot())).layers[0].nodes.at(-1);
  await page.locator(`.graph [data-preview="${created.id}"]`).click();
  const preview = await page.evaluate(() => window.nect.snapshot());
  assert.equal(preview.layers[0].nodes.at(-1).params.path.source, created.id);
  await clickUndo();
  await page.locator(`.graph [data-delete="${created.id}"]`).click();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), beforeBypass);
  await clickUndo();
  assert.equal(
    (await page.evaluate(() => window.nect.snapshot())).layers[0].nodes.at(-1).id,
    created.id,
  );
  await page.locator('[data-tool="Text"]').click();
  await page.locator('.artboard').click();
  await page
    .locator('.graph .node')
    .filter({ has: page.locator('[data-text]') })
    .locator('.node-head strong')
    .click();
  const textSetup = await page.evaluate(async () => {
    const { CommandSession } = await import('/src/core.js');
    const before = window.nect.snapshot(),
      layer = before.layers.at(-1),
      text = layer.nodes.find((n) => n.type === 'Text');
    const headless = new CommandSession(before);
    headless.dispatch({
      ...headless.state,
      type: 'set-text-content',
      payload: { layerId: layer.id, nodeId: text.id, text: '日本語 ABC' },
    });
    return {
      before,
      expected: headless.snapshot(),
      revision: window.nect.commandState.expectedRevision,
    };
  });
  const textInput = page.locator('.inspector-body textarea[data-text]');
  await textInput.fill('日本語 ABC');
  await textInput.dispatchEvent('change');
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), textSetup.expected);
  const composingDraft = await page.evaluate(() => {
    const snapshot = window.nect.snapshot(),
      layer = snapshot.layers.at(-1),
      text = layer.nodes.find((n) => n.type === 'Text');
    const draft = window.nect.dispatch({
      ...window.nect.commandState,
      type: 'begin-text-draft',
      payload: { layerId: layer.id, nodeId: text.id },
    });
    return window.nect.dispatch({
      ...window.nect.commandState,
      type: 'stage-text-draft',
      payload: {
        draftId: draft.draftId,
        expectedDraftRevision: draft.draftRevision,
        action: 'composition-start',
        previousRaw: draft.text,
        selectionStart: 0,
        selectionEnd: 1,
      },
    });
  });
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), textSetup.expected);
  await page.evaluate(
    (draftId) =>
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'cancel-text-draft',
        payload: { draftId },
      }),
    composingDraft.draftId,
  );
  assert.equal(
    (await page.evaluate(() => window.nect.commandState)).expectedRevision,
    textSetup.revision + 1,
  );
  await clickUndo();
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), textSetup.before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await page.evaluate(() => window.nect.snapshot()), textSetup.expected);
  assert.deepEqual(errors, []);
  console.log(
    'PASS GUI operation/property/Text controls equal headless commands; shared one-step Undo/API Redo, monotonic revision, stale refusal, expression validation and locks.',
  );
} finally {
  await browser.close();
}
