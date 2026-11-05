import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { blankDocument, blankLayer, connect, node, serialize } from '../src/core.js';

const output = resolve('test-results/workspace-stack');
await mkdir(output, { recursive: true });
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
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.addInitScript(() => {
  if (!sessionStorage.getItem('stack-test-seeded')) {
    localStorage.setItem(
      'nect.workspace-drawers.v1',
      JSON.stringify({ left: null, right: 'layers', lastSide: 'right' }),
    );
    sessionStorage.setItem('stack-test-seeded', 'yes');
  }
});
const doc = blankDocument(),
  layer = doc.layers[0],
  text = node('Text'),
  fill = node('Fill');
text.text = 'A';
layer.nodes.unshift(text, fill);
connect(layer, text.id, fill.id, 'path');
connect(layer, fill.id, layer.output, 'path');
for (let i = 0; i < 8; i++) doc.layers.push(blankLayer(`Extra ${i + 1}`));
const layers = page.locator('[data-drawer-side="right"] .layers');
const inspector = page.locator('[data-drawer-side="right"] .inspector');
const layersRail = page.locator('[data-workspace-panel="layers"]');
const inspectorRail = page.locator('[data-workspace-panel="inspector"]');
const alignRail = page.locator('[data-workspace-panel="align"]');
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const command = () => page.evaluate(() => window.nect.commandState);
const visibleStack = async () => {
  assert.equal(await layers.isVisible(), true);
  assert.equal(await inspector.isVisible(), true);
  assert.equal(await layersRail.getAttribute('aria-expanded'), 'true');
  assert.equal(await inspectorRail.getAttribute('aria-expanded'), 'true');
  const [a, b] = await Promise.all([layers.boundingBox(), inspector.boundingBox()]);
  assert.ok(a.width > 100 && b.width > 100 && a.height > 40 && b.height > 40);
  assert.ok(a.y + a.height <= b.y + 1, 'Layers precedes Inspector without overlapping');
};
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((data) => window.nect.load(data), serialize(doc));
  await visibleStack();
  const before = await snapshot();
  const beforeCommand = await command();
  const separator = page.locator('[data-workspace-split="editing"]');
  assert.equal(await separator.getAttribute('role'), 'separator');
  assert.equal(await separator.getAttribute('aria-controls'), await layers.getAttribute('id'));
  const initialHeight = (await layers.boundingBox()).height;
  await separator.focus();
  await page.keyboard.press('ArrowDown');
  assert.ok((await layers.boundingBox()).height > initialHeight);
  const keyboardSize = Number(await separator.getAttribute('aria-valuenow'));
  const beforeCancelPreference = await page.evaluate(() =>
    localStorage.getItem('nect.workspace-drawers.v1'),
  );
  let splitBox = await separator.boundingBox();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2 + 45);
  assert.notEqual(Number(await separator.getAttribute('aria-valuenow')), keyboardSize);
  await page.keyboard.press('Escape');
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), keyboardSize);
  assert.equal(
    await page.evaluate(() => localStorage.getItem('nect.workspace-drawers.v1')),
    beforeCancelPreference,
  );
  await page.mouse.up();
  splitBox = await separator.boundingBox();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2 + 35);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), keyboardSize);
  await page.mouse.up();
  splitBox = await separator.boundingBox();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2 + 35);
  await page.mouse.up();
  let pointerSize = Number(await separator.getAttribute('aria-valuenow'));
  assert.ok(pointerSize > keyboardSize);
  assert.equal(
    await page.evaluate(
      () => JSON.parse(localStorage.getItem('nect.workspace-drawers.v1')).splits.editing,
    ),
    pointerSize,
  );
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await command(), beforeCommand);
  assert.equal(await layersRail.getAttribute('aria-pressed'), 'true');
  assert.equal(await inspectorRail.getAttribute('aria-pressed'), 'false');
  assert.equal(await layersRail.getAttribute('aria-controls'), await layers.getAttribute('id'));
  assert.equal(
    await inspectorRail.getAttribute('aria-controls'),
    await inspector.getAttribute('id'),
  );
  await page.screenshot({ path: resolve(output, 'initial.png'), animations: 'disabled' });
  await page.evaluate(() => {
    window.savedLayers = document.querySelector('[data-drawer-side="right"] .layers');
    window.savedInspector = document.querySelector('[data-drawer-side="right"] .inspector');
    window.savedLayers.scrollTop = 80;
    window.savedInspector.scrollTop = 90;
  });
  const scroll = await page.evaluate(() => [
    window.savedLayers.scrollTop,
    window.savedInspector.scrollTop,
  ]);
  assert.ok(scroll[0] > 0 && scroll[1] > 0, 'both panels independently scroll');
  await inspectorRail.click();
  await visibleStack();
  assert.equal(await inspectorRail.getAttribute('aria-pressed'), 'true');
  assert.equal(await layersRail.getAttribute('aria-pressed'), 'false');
  assert.deepEqual(
    await page.evaluate(() => [
      window.savedLayers === document.querySelector('[data-drawer-side="right"] .layers'),
      window.savedInspector === document.querySelector('[data-drawer-side="right"] .inspector'),
      window.savedLayers.scrollTop,
      window.savedInspector.scrollTop,
    ]),
    [true, true, ...scroll],
  );
  assert.equal(await inspector.evaluate((element) => element === document.activeElement), true);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await command(), beforeCommand);
  await page.screenshot({ path: resolve(output, 'wide.png'), animations: 'disabled' });
  await layers.evaluate((element) => {
    element.scrollTop = 0;
  });
  await inspector.evaluate((element) => {
    element.scrollTop = 0;
  });
  for (const [width, height, name] of [
    [620, 820, 'narrow'],
    [1200, 620, 'short'],
  ]) {
    await page.setViewportSize({ width, height });
    await visibleStack();
    await page.screenshot({ path: resolve(output, `${name}.png`), animations: 'disabled' });
  }
  await separator.focus();
  await page.keyboard.press('Home');
  assert.equal(
    await separator.getAttribute('aria-valuenow'),
    await separator.getAttribute('aria-valuemin'),
  );
  assert.ok((await layers.boundingBox()).height >= 79);
  assert.ok((await inspector.boundingBox()).height >= 79);
  await page.keyboard.press('End');
  assert.equal(
    await separator.getAttribute('aria-valuenow'),
    await separator.getAttribute('aria-valuemax'),
  );
  assert.ok((await layers.boundingBox()).height >= 79);
  assert.ok((await inspector.boundingBox()).height >= 79);
  pointerSize = Number(await separator.getAttribute('aria-valuenow'));
  const viewerOutput = inspector.locator('select[aria-label="Viewer public output"]');
  const viewerDetails = viewerOutput.locator('xpath=ancestor::details[1]');
  if (!(await viewerDetails.evaluate((element) => element.open)))
    await viewerDetails.locator('summary').click();
  for (const [port, control] of [
    [layers, layers.locator('[data-dimension="height"]')],
    [inspector, viewerOutput],
  ]) {
    await port.hover();
    await page.mouse.wheel(0, 10000);
    await page.waitForTimeout(100);
    const firstRect = await control.boundingBox();
    const portRect = await port.boundingBox();
    await port.hover();
    await page.mouse.wheel(
      0,
      firstRect.y + firstRect.height / 2 - (portRect.y + portRect.height / 2),
    );
    await page.waitForTimeout(100);
    const rect = await control.boundingBox();
    assert.equal(
      await control.evaluate((element) => {
        const r = element.getBoundingClientRect();
        return [0.1, 0.5, 0.9].every(
          (fraction) =>
            document.elementFromPoint(r.x + r.width * fraction, r.y + r.height / 2) === element,
        );
      }),
      true,
      'last control is not covered by another surface',
    );
    await page.mouse.click(rect.x + rect.width / 2, rect.y + rect.height / 2);
    assert.equal(await control.evaluate((element) => element === document.activeElement), true);
    if (await control.evaluate((element) => element.tagName === 'SELECT'))
      await page.keyboard.press('Escape');
  }
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await command(), beforeCommand);
  await page.setViewportSize({ width: 1440, height: 960 });
  await layers.evaluate((element) => {
    element.scrollTop = 0;
  });
  const layerRevision = (await command()).expectedRevision;
  await layers.locator('[data-visible]').first().click();
  assert.equal((await command()).expectedRevision, layerRevision + 1);
  assert.notDeepEqual(await snapshot(), before);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before);
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), pointerSize);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  const pendingRedoState = await command();
  await separator.focus();
  await page.keyboard.press('ArrowUp');
  pointerSize = Number(await separator.getAttribute('aria-valuenow'));
  assert.deepEqual(await command(), pendingRedoState);
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await alignRail.click();
  assert.equal(await page.locator('.alignment-panel').isVisible(), true);
  assert.equal(await layers.isVisible(), false);
  assert.equal(await inspector.isVisible(), false);
  await inspectorRail.click();
  await visibleStack();

  const textInput = page.locator(`.inspector textarea[data-text="${text.id}"]`);
  await textInput.focus();
  await textInput.evaluate((element) => {
    element.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
  });
  await textInput.press('ControlOrMeta+A');
  await page.keyboard.insertText('編集中');
  const beforeIme = await command();
  const addLayer = page.locator('.layers [data-action="add-blank-layer"]');
  await layers.evaluate((element) => {
    element.scrollTop = 0;
  });
  const viewers = page.locator('.viewer-tabs [data-viewer-mode="viewers"]');
  for (const key of ['Tab', 'Shift+Tab']) {
    await page.keyboard.press(key);
    assert.equal(await textInput.evaluate((element) => element === document.activeElement), true);
    assert.equal(await textInput.inputValue(), '編集中');
    assert.deepEqual(await command(), beforeIme);
    assert.deepEqual(await snapshot(), before);
  }
  for (const target of [layersRail, addLayer, separator, alignRail, viewers]) {
    await target.click();
    await target.evaluate((element) => element.click());
    assert.equal(await textInput.evaluate((element) => element === document.activeElement), true);
    assert.equal(await textInput.inputValue(), '編集中');
    assert.deepEqual(await command(), beforeIme);
    assert.deepEqual(await snapshot(), before);
  }
  await textInput.evaluate((element) =>
    element.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: '中' })),
  );
  await layersRail.click();
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((entry) => entry.id === id).text === '編集中',
    text.id,
  );
  assert.equal((await command()).expectedRevision, beforeIme.expectedRevision + 1);
  assert.equal(await layersRail.getAttribute('aria-pressed'), 'true');
  await visibleStack();
  const afterIme = await snapshot();
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), before);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await snapshot(), afterIme);
  await viewers.click();
  assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'viewers');
  await visibleStack();
  await page.locator('.viewer-tabs [data-viewer-mode="artwork"]').click();
  assert.equal(await page.locator('.canvas-wrap').getAttribute('data-viewer-mode'), 'artwork');
  await visibleStack();
  await inspectorRail.click();
  const numeric = page.locator('.inspector input[type="number"]:enabled').first();
  const numericBefore = await snapshot();
  const numericRevision = (await command()).expectedRevision;
  const number = Number(await numeric.inputValue());
  await numeric.fill(String(number + 1));
  assert.equal((await command()).expectedRevision, numericRevision);
  await layersRail.click();
  await page.waitForFunction(
    (revision) => window.nect.commandState.expectedRevision === revision + 1,
    numericRevision,
  );
  assert.equal(await layersRail.getAttribute('aria-pressed'), 'true');
  const numericAfter = await snapshot();
  assert.notDeepEqual(numericAfter, numericBefore);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), numericBefore);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await snapshot(), numericAfter);
  const beforeReattachPreference = await page.evaluate(() =>
    localStorage.getItem('nect.workspace-drawers.v1'),
  );
  splitBox = await separator.boundingBox();
  await separator.evaluate((element) => {
    window.oldWorkspaceSeparator = element;
  });
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2 + 30);
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: window.nect.document.width + 1 },
    }),
  );
  assert.equal(await page.evaluate(() => window.oldWorkspaceSeparator.isConnected), false);
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), pointerSize);
  await page.mouse.move(splitBox.x + splitBox.width / 2, splitBox.y + splitBox.height / 2 + 60);
  await page.mouse.up();
  assert.equal(
    await page.evaluate(() => localStorage.getItem('nect.workspace-drawers.v1')),
    beforeReattachPreference,
  );
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), pointerSize);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), numericAfter);
  await inspectorRail.click();
  await inspector.focus();
  await page.keyboard.press('ControlOrMeta+z');
  assert.deepEqual(await snapshot(), numericBefore);
  assert.equal(await inspector.evaluate((element) => element === document.activeElement), true);
  await page.keyboard.press('Escape');
  assert.equal(await inspectorRail.getAttribute('aria-expanded'), 'false');
  assert.equal(await layersRail.getAttribute('aria-expanded'), 'false');
  assert.equal(await inspectorRail.evaluate((element) => element === document.activeElement), true);
  await inspectorRail.click();
  await visibleStack();
  await page.reload();
  await page.waitForFunction(() => window.nect);
  await visibleStack();
  assert.equal(await inspectorRail.getAttribute('aria-pressed'), 'true');
  assert.equal(Number(await separator.getAttribute('aria-valuenow')), pointerSize);
  assert.deepEqual(errors, []);
  console.log(
    'Workspace stack: focus, scrolling, preferences, synthetic IME and one Undo/Redo passed',
  );
} finally {
  await browser.close();
}
