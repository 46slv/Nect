import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, connect, node, point, serialize } from '../src/core.js';
import { activateCommand } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { openMoreTypography } from './helpers/text-inspector.js';

const doc = blankDocument(),
  target = doc.layers[0],
  rectangle = node('Rectangle', 30, 70),
  transform = node('Transform', 350, 70),
  fill = node('Fill', 700, 70),
  circle = node('Circle', 1080, 550),
  textPath = node('Curve', 40, 550),
  text = node('Text', 400, 550),
  output = target.nodes[0];
target.id = 'target-artwork';
target.name = 'Target';
output.position = [1050, 70];
transform.id = 'target-transform';
fill.id = 'target-fill';
circle.id = 'target-circle';
circle.params.center.value = [12, 25];
textPath.id = 'text-path';
textPath.points = [point(50, 100), point(300, 100)];
textPath.points[0].id = 'text-anchor-a';
textPath.points[1].id = 'text-anchor-b';
textPath.params.closed.value = false;
text.id = 'text-node';
text.text = 'Type on path';
text.params.size.value = 20;
target.nodes.push(rectangle, transform, fill, circle, textPath, text);
connect(target, rectangle.id, transform.id, 'path');
connect(target, transform.id, fill.id, 'path');
connect(target, fill.id, output.id, 'path');
for (const [suffix, x] of [
  ['a', 10],
  ['b', 30],
]) {
  const artwork = blankLayer('Controller'),
    curve = node('Curve', 60, 80);
  artwork.id = `source-${suffix}`;
  curve.id = `curve-${suffix}`;
  curve.name = 'Outline';
  curve.points[0].id = `anchor-${suffix}`;
  curve.points[0].position.value = [x, 20];
  curve.points[0].handleA.value = [3, 4];
  artwork.nodes.push(curve);
  if (suffix === 'b') {
    const driver = node('Value', 400, 80);
    driver.id = 'source-b-value';
    driver.name = 'Driver';
    artwork.nodes.push(driver);
  }
  doc.layers.push(artwork);
}

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 960 } });
page.setDefaultTimeout(15000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const rotation = () =>
  page.locator('.inspector-body .field[data-node="target-transform"][data-key="rotation"]');
const snapshot = () => page.evaluate(() => window.nect.snapshot());
const value = () =>
  page.evaluate(async () => {
    const { evaluate } = await import('/src/core.js');
    const current = window.nect.snapshot(),
      layer = current.layers.find((entry) => entry.id === 'target-artwork');
    return evaluate(layer, {}, current.imageAssets, current).parameterValue(
      'target-transform',
      'rotation',
    );
  });
const colorAlpha = () =>
  page.evaluate(async () => {
    const { evaluate } = await import('/src/core.js');
    const current = window.nect.snapshot(),
      layer = current.layers.find((entry) => entry.id === 'target-artwork');
    return evaluate(layer, {}, current.imageAssets, current).parameterValue('target-fill', 'color')
      .alpha;
  });
async function applyColorReference() {
  await page.locator('.node[data-id="target-fill"] .node-head strong').click();
  await ensureWorkspacePanel(page, 'inspector');
  const color = page.locator('.inspector-body .field[data-node="target-fill"][data-key="color"]');
  await color.locator('[data-fx]').click();
  const colorDialog = page
      .locator('dialog[open]')
      .filter({ has: page.locator('[data-insert-reference]') }),
    colorDraft = colorDialog.locator('[aria-label="Parameter expression"]');
  const prefix = 'alpha(value, ';
  await colorDraft.fill(`${prefix} / 10)`);
  await colorDraft.evaluate(
    (input, position) => input.setSelectionRange(position, position),
    prefix.length,
  );
  await colorDialog.locator('[data-insert-reference]').click();
  const colorPicker = page.getByRole('dialog', { name: 'Parameter references' });
  await colorPicker.getByRole('combobox').fill('source-a anchor-a.handleA length');
  await colorPicker.locator('.picker-option').filter({ hasText: 'Incoming · Length' }).click();
  await colorPicker.locator('.picker-apply').click();
  const colorExpression =
    'alpha(value, ref(artwork, "source-a", "curve-a", "anchor-a.handleA", "length") / 10)';
  assert.equal(await colorDraft.inputValue(), colorExpression);
  await colorDialog.locator('[data-confirm]').click();
  await colorDialog.waitFor({ state: 'detached' });
}

try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  await ensureWorkspacePanel(page, 'inspector');

  // Copy and Paste through the graph retains an external Artwork source.
  await page.evaluate(() => {
    const send = (type, payload) =>
      window.nect.dispatch({ ...window.nect.commandState, type, payload });
    send('set-parameter', {
      layerId: 'source-b',
      nodeId: 'source-b-value',
      key: 'value',
      value: 7,
    });
    send('set-parameter-expression', {
      layerId: 'target-artwork',
      nodeId: 'target-transform',
      key: 'rotation',
      expression: 'ref(artwork, "source-b", "source-b-value", "value")',
    });
  });
  await page.locator('.node[data-id="target-transform"] .node-head strong').click();
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+c');
  assert.match(await page.locator('.status').innerText(), /external input/i);
  const beforePaste = await snapshot();
  await page.keyboard.press('Control+v');
  await page.waitForFunction(
    () => window.nect.document.layers[0].nodes.filter((n) => n.type === 'Transform').length === 2,
  );
  const pastedSnapshot = await snapshot();
  const pasted = pastedSnapshot.layers[0].nodes.find(
    (n) => n.type === 'Transform' && n.id !== 'target-transform',
  );
  assert.ok(pasted);
  assert.deepEqual(
    await page
      .locator('.graph .node.selected')
      .evaluateAll((nodes) => nodes.map((n) => n.dataset.id)),
    [pasted.id],
  );
  assert.equal(pasted.params.path.source, rectangle.id);
  assert.equal(
    pasted.params.rotation.expression,
    'ref(artwork, "source-b", "source-b-value", "value")',
  );
  const pastedValue = () =>
    page.evaluate(async (id) => {
      const { evaluate } = await import('/src/core.js');
      const current = window.nect.snapshot();
      return evaluate(current.layers[0], {}, current.imageAssets, current).parameterValue(
        id,
        'rotation',
      );
    }, pasted.id);
  assert.equal(await pastedValue(), 7);
  await activateCommand(page, 'undo');
  assert.deepEqual(await snapshot(), beforePaste);
  await activateCommand(page, 'redo');
  assert.deepEqual(await snapshot(), pastedSnapshot);
  await mkdir('test-results/cross-artwork-reference', { recursive: true });
  const clipboardDownload = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const clipboardFile = resolve('test-results/cross-artwork-reference/clipboard-project.json');
  await (await clipboardDownload).saveAs(clipboardFile);
  const clipboardBytes = await readFile(clipboardFile);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page
    .locator('#open-file')
    .setInputFiles({ name: 'clipboard-project.json', buffer: clipboardBytes });
  await page.waitForFunction(
    () => window.nect.document.layers[0].nodes.filter((n) => n.type === 'Transform').length === 2,
  );
  assert.deepEqual(await snapshot(), pastedSnapshot);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter',
      payload: { layerId: 'source-b', nodeId: 'source-b-value', key: 'value', value: 9 },
    }),
  );
  assert.equal(await pastedValue(), 9);

  // A removed source makes the copied graph snapshot unusable immediately.
  await page.locator('.node[data-id="target-transform"] .node-head strong').click();
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+c');
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'delete-node',
      payload: { layerId: 'source-b', nodeId: 'source-b-value' },
    }),
  );
  const missingSource = await snapshot(),
    missingState = await page.evaluate(() => window.nect.commandState);
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+v');
  assert.match(await page.locator('.status').innerText(), /source.*copy again/i);
  assert.deepEqual(await snapshot(), missingSource);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), missingState);
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  await ensureWorkspacePanel(page, 'inspector');
  await page.locator('.node[data-id="target-transform"] .node-head strong').click();
  await rotation().locator('[data-fx]').click();
  const staleExpression = page
    .locator('dialog[open]')
    .filter({ has: page.locator('[data-insert-reference]') });
  await staleExpression.locator('[data-insert-reference]').click();
  const stalePicker = page.getByRole('dialog', { name: 'Parameter references' });
  await stalePicker.getByRole('combobox').fill('source-a anchor-a.position');
  await stalePicker.locator('.picker-option').filter({ hasText: 'Point 1 · X' }).click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const beforeStale = await snapshot(),
    beforeStaleState = await page.evaluate(() => window.nect.commandState);
  await stalePicker.locator('.picker-apply').click();
  assert.equal(
    await staleExpression.locator('[aria-label="Parameter expression"]').inputValue(),
    '',
  );
  assert.match(await staleExpression.getByRole('alert').innerText(), /Close and reopen/);
  assert.deepEqual(await snapshot(), beforeStale);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeStaleState);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.equal((await snapshot()).width, beforeStale.width + 1);
  await staleExpression.locator('[data-cancel]').click();
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  // Explicitly qualifying the current Artwork is invalid in both preview and Apply.
  await page.locator('.node[data-id="target-fill"] .node-head strong').click();
  await page
    .locator('.inspector-body .field[data-node="target-fill"][data-key="color"] [data-fx]')
    .click();
  const invalidColorDialog = page
      .locator('dialog[open]')
      .filter({ has: page.locator('[data-insert-reference]') }),
    invalidColorDraft = invalidColorDialog.locator('[aria-label="Parameter expression"]');
  const beforeInvalidColor = await snapshot(),
    beforeInvalidState = await page.evaluate(() => window.nect.commandState);
  await invalidColorDraft.fill(
    'alpha(value, ref(artwork, "target-artwork", "target-circle", "radius") / 200)',
  );
  assert.equal(await invalidColorDraft.getAttribute('aria-invalid'), 'true');
  await invalidColorDialog.locator('[data-confirm]').click();
  assert.deepEqual(await snapshot(), beforeInvalidColor);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeInvalidState);
  await invalidColorDialog.locator('[data-cancel]').click();
  // Color must be able to introduce the first qualified reference into an old document.
  await applyColorReference();
  assert.equal((await snapshot()).schemaVersion, 62);
  assert.equal(await colorAlpha(), 0.5);
  await activateCommand(page, 'undo');
  assert.equal((await snapshot()).schemaVersion, doc.schemaVersion);
  assert.equal(await colorAlpha(), 1);
  await activateCommand(page, 'redo');
  assert.equal(await colorAlpha(), 0.5);
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  await page.locator('.node[data-id="target-transform"] .node-head strong').click();
  await rotation().locator('[data-fx]').click();
  const dialog = page
    .locator('dialog[open]')
    .filter({ has: page.locator('[data-insert-reference]') });
  const draft = dialog.locator('[aria-label="Parameter expression"]');
  await draft.fill('source');
  const suggestion = dialog.locator('.expression-completions [role="option"]').first();
  assert.match(await suggestion.innerText(), /Controller/);
  await suggestion.click();
  assert.match(await draft.inputValue(), /^ref\(artwork,/);
  await draft.fill('');
  await dialog.locator('[data-insert-reference]').click();
  const picker = page.getByRole('dialog', { name: 'Parameter references' }),
    search = picker.getByRole('combobox');
  await search.fill('Controller Point 1');
  const xRows = picker.locator('.picker-option').filter({ hasText: 'Point 1 · X' });
  assert.equal(await xRows.count(), 2);
  assert.notEqual(
    (await xRows.nth(0).innerText()).normalize('NFC'),
    (await xRows.nth(1).innerText()).normalize('NFC'),
  );
  await page.setViewportSize({ width: 1000, height: 650 });
  await mkdir('test-results/cross-artwork-reference', { recursive: true });
  await page.screenshot({ path: 'test-results/cross-artwork-reference/picker-1000x650.png' });
  await search.fill('source-a anchor-a.position');
  await picker.locator('.picker-option').filter({ hasText: 'Point 1 · X' }).click();
  await picker.locator('.picker-apply').click();
  const expression = 'ref(artwork, "source-a", "curve-a", "anchor-a.position", "x")';
  assert.equal(
    await dialog.locator('[aria-label="Parameter expression"]').inputValue(),
    expression,
  );
  await dialog.locator('[data-confirm]').click();
  await dialog.waitFor({ state: 'detached' });
  assert.equal((await snapshot()).schemaVersion, 62);
  assert.equal(await value(), 10);

  // Edit the source point with real UI controls, then return to the consumer.
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-select-layer="source-a"]').click();
  await page.locator('.node[data-id="curve-a"] .node-head strong').click();
  await page.locator('.graph [data-point-panel-mode="curve-a"]').click();
  const pointX = page.locator(
    '.graph .field[data-node="curve-a"][data-key="anchor-a.position"] [data-value][data-axis="0"]',
  );
  await pointX.fill('17');
  await pointX.press('Tab');
  assert.equal(await value(), 17);
  await activateCommand(page, 'undo');
  assert.equal(await value(), 10);
  await activateCommand(page, 'redo');
  assert.equal(await value(), 17);
  await ensureWorkspacePanel(page, 'layers');
  await page.locator('[data-select-layer="target-artwork"]').click();
  await page.locator('.node[data-id="target-transform"] .node-head strong').click();
  await ensureWorkspacePanel(page, 'inspector');
  assert.match(await rotation().locator('output').innerText(), /17/);

  const beforeSave = await snapshot(),
    download = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const file = resolve('test-results/cross-artwork-reference/project.json');
  await (await download).saveAs(file);
  const bytes = await readFile(file);
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await page.locator('#open-file').setInputFiles({ name: 'project.json', buffer: bytes });
  await page.waitForFunction(() => window.nect.document.schemaVersion === 62);
  assert.deepEqual(await snapshot(), beforeSave);
  assert.equal(await value(), 17);

  // The same Color expression works after a qualified Scalar reference was reopened.
  await applyColorReference();
  assert.equal(await colorAlpha(), 0.5);
  await activateCommand(page, 'undo');
  assert.equal(await colorAlpha(), 1);
  await activateCommand(page, 'redo');
  assert.equal(await colorAlpha(), 0.5);

  // A local Text path may itself depend on an external Artwork parameter.
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter-expression',
      payload: {
        layerId: 'target-artwork',
        nodeId: 'text-path',
        key: 'text-anchor-a.position',
        expression: 'ref(artwork, "source-a", "curve-a", "anchor-a.position", "x"); value',
      },
    }),
  );
  await page.locator('.node[data-id="text-node"] .node-head strong').click();
  await openMoreTypography(page);
  const pathSelect = page
    .locator('.inspector-body')
    .getByRole('combobox', { name: 'Type on Path contour', exact: true });
  const beforeAttachment = (await snapshot()).layers[0].nodes.find(
    (entry) => entry.id === 'text-node',
  ).pathAttachment;
  await pathSelect.selectOption(JSON.stringify(['text-path', 'text-path']));
  const pathAfter = (await snapshot()).layers[0].nodes.find((entry) => entry.id === 'text-node');
  assert.ok(pathAfter.pathAttachment, await page.locator('.status .message').innerText());
  assert.equal(pathAfter.pathAttachment.nodeId, 'text-path');
  await activateCommand(page, 'undo');
  assert.equal(
    (await snapshot()).layers[0].nodes.find((entry) => entry.id === 'text-node').pathAttachment,
    beforeAttachment,
  );
  await activateCommand(page, 'redo');
  assert.equal(
    (await snapshot()).layers[0].nodes.find((entry) => entry.id === 'text-node').pathAttachment
      .nodeId,
    'text-path',
  );

  // A foreign value depending on center remains a valid source for radius.
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-parameter-expression',
      payload: {
        layerId: 'source-b',
        nodeId: 'source-b-value',
        key: 'value',
        expression: 'ref(artwork, "target-artwork", "target-circle", "center", "x")',
      },
    }),
  );
  await page.locator('.node[data-id="target-circle"] .node-head strong').click();
  const radius = page.locator(
    '.inspector-body .field[data-node="target-circle"][data-key="radius"]',
  );
  await radius.locator('[data-fx]').click();
  const radiusDialog = page
    .locator('dialog[open]')
    .filter({ has: page.locator('[data-insert-reference]') });
  await radiusDialog.locator('[data-insert-reference]').click();
  const radiusPicker = page.getByRole('dialog', { name: 'Parameter references' });
  await radiusPicker.getByRole('combobox').fill('source-b source-b-value');
  await radiusPicker.locator('.picker-option').filter({ hasText: 'Driver · value' }).click();
  await radiusPicker.locator('.picker-apply').click();
  await radiusDialog.locator('[data-confirm]').click();
  await radiusDialog.waitFor({ state: 'detached' });
  assert.match(await radius.locator('output').innerText(), /12/);

  // Activating a formerly dormant property must reject its nonfinite value.
  const modeDoc = structuredClone(doc),
    modeLayer = modeDoc.layers.find((entry) => entry.id === 'target-artwork'),
    zeroLayer = modeDoc.layers.find((entry) => entry.id === 'source-a'),
    pucker = node('Pucker & Bloat', 1500, 600),
    zero = node('Value', 400, 350);
  modeDoc.schemaVersion = 62;
  pucker.id = 'mode-pucker';
  zero.id = 'mode-zero';
  zero.params.value.value = 0;
  pucker.params.center.expression = '1 / ref(artwork, "source-a", "mode-zero", "value"); 0';
  modeLayer.nodes.push(pucker);
  zeroLayer.nodes.push(zero);
  await page.evaluate((text) => window.nect.load(text), serialize(modeDoc));
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await page.locator('.node[data-id="mode-pucker"] .node-head strong').click();
  await ensureWorkspacePanel(page, 'inspector');
  const beforeModeChange = await snapshot(),
    beforeModeState = await page.evaluate(() => window.nect.commandState),
    autoCenter = page.locator(
      '.inspector-body .field[data-node="mode-pucker"][data-key="autoCenter"] [data-value]',
    );
  assert.equal(await autoCenter.isChecked(), true);
  const oldAutoCenter = await autoCenter.elementHandle();
  await autoCenter.click();
  assert.deepEqual(await snapshot(), beforeModeChange);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeModeState);
  await page.waitForFunction((element) => !element.isConnected, oldAutoCenter);
  assert.equal(await autoCenter.isChecked(), true);
  assert.match(await page.locator('.status .message').textContent(), /finite/i);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.equal((await snapshot()).width, beforeModeChange.width + 1);

  // The same activation guard applies when the mode is driven by a Toggle.
  const connectedMode = structuredClone(modeDoc),
    connectedLayer = connectedMode.layers.find((entry) => entry.id === 'target-artwork'),
    toggle = node('Toggle', 1850, 600);
  toggle.id = 'mode-toggle';
  connectedLayer.nodes.push(toggle);
  connect(connectedLayer, 'mode-toggle', 'mode-pucker', 'autoCenter');
  await page.evaluate((text) => window.nect.load(text), serialize(connectedMode));
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await page.locator('.node[data-id="mode-toggle"] .node-head strong').click();
  await ensureWorkspacePanel(page, 'inspector');
  const beforeToggle = await snapshot(),
    beforeToggleState = await page.evaluate(() => window.nect.commandState),
    toggleValue = page.locator(
      '.inspector-body .field[data-node="mode-toggle"][data-key="value"] [data-value]',
    );
  assert.equal(await toggleValue.isChecked(), true);
  const oldToggleValue = await toggleValue.elementHandle();
  await toggleValue.click();
  assert.deepEqual(await snapshot(), beforeToggle);
  assert.deepEqual(await page.evaluate(() => window.nect.commandState), beforeToggleState);
  // The rejected change schedules a structural render after pointer release.
  // Assert the actual replacement rather than the transient old checkbox.
  await page.waitForFunction((element) => !element.isConnected, oldToggleValue);
  assert.equal(await toggleValue.isChecked(), true);
  assert.match(await page.locator('.status .message').textContent(), /finite/i);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.equal((await snapshot()).width, beforeToggle.width + 1);

  // Long source names and the maximum Curve size remain reachable together.
  const large = structuredClone(doc),
    largeLayer = large.layers.find((entry) => entry.id === 'source-a'),
    largeCurve = largeLayer.nodes.find((entry) => entry.id === 'curve-a'),
    longLayerName = `${'Artwork '.repeat(12498)}LayerTail`,
    longNodeName = `${'Contour '.repeat(12498)}NodeTail`;
  largeLayer.name = longLayerName;
  largeCurve.name = longNodeName;
  largeCurve.points = Array.from({ length: 10000 }, (_, index) => {
    const point = structuredClone(largeCurve.points[0]);
    point.id = `large-point-${index + 1}`;
    return point;
  });
  await page.evaluate((text) => window.nect.load(text), serialize(large));
  await page.locator('.node[data-id="target-transform"] .node-head strong').click();
  await ensureWorkspacePanel(page, 'inspector');
  await rotation().locator('[data-fx]').click();
  const largeDialog = page
    .locator('dialog[open]')
    .filter({ has: page.locator('[data-insert-reference]') });
  const largeOpenStart = Date.now();
  await largeDialog.locator('[data-insert-reference]').click();
  const largePicker = page.getByRole('dialog', { name: 'Parameter references' });
  assert.equal(await largePicker.locator('.picker-option').count(), 100);
  const largeOpenMs = Date.now() - largeOpenStart,
    largeSearchStart = Date.now();
  await largePicker.getByRole('combobox').fill('LayerTail NodeTail large-point-10000.position');
  assert.equal(await largePicker.locator('.picker-option').count(), 2);
  const largeRow = largePicker.locator('.picker-option').first();
  assert.ok((await largeRow.innerText()).length < 200);
  assert.equal(
    await largeRow.getAttribute('aria-description'),
    `${longLayerName} · ${longNodeName}`,
  );
  await page.screenshot({ path: 'test-results/cross-artwork-reference/long-foreign-picker.png' });
  console.log(
    JSON.stringify({ points: 10000, largeOpenMs, largeSearchMs: Date.now() - largeSearchStart }),
  );
  await largeRow.click();
  await largePicker.locator('.picker-apply').click();
  assert.match(
    await largeDialog.locator('[aria-label="Parameter expression"]').inputValue(),
    /large-point-10000\.position/,
  );
  await largeDialog.locator('[data-confirm]').click();
  await largeDialog.waitFor({ state: 'detached' });
  assert.deepEqual(errors, []);
  console.log('PASS cross-Artwork picker, graph clipboard, source edit, Undo/Redo and JSON reopen');
} finally {
  await browser.close();
}
