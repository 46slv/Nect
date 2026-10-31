import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { chromium } from 'playwright-core';
import { openArtworkPanel } from './helpers/artwork-panels.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(5000);
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
const snapshot = () => page.evaluate(() => JSON.stringify(window.nect.document));
const drawing = () =>
  page.locator('.artboard .shape').evaluateAll((nodes) => nodes.map((n) => n.outerHTML).join(''));
const revision = () => page.evaluate(() => window.nect.commandState.expectedRevision);
const input = () => page.locator('.graph textarea[data-text]').first();
async function selectText() {
  await page
    .locator('.graph .node')
    .filter({ has: page.locator('textarea[data-text]') })
    .locator('.node-head .type-tag')
    .click();
}
async function history(type) {
  await page.evaluate((type) => window.nect.dispatch({ ...window.nect.commandState, type }), type);
}
async function reset() {
  await page.evaluate(async () => {
    const { document } = await import('/src/core.js');
    window.nect.load(JSON.stringify(document()));
  });
  await page.locator('[data-tool="Text"]').click();
  await page.locator('.artboard').click();
  await selectText();
  await page.waitForTimeout(50);
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await reset();
  const before = await snapshot(),
    beforeDrawing = await drawing(),
    rev = await revision();
  await input().fill('NectAAAA');
  await page.waitForTimeout(80);
  assert.equal(await input().evaluate((el) => el === document.activeElement), true);
  assert.notEqual(await drawing(), beforeDrawing, 'drawing updates before blur');
  assert.equal(await snapshot(), before, 'draft is private');
  assert.equal(await revision(), rev);
  assert.equal(await page.locator('.inspector textarea[data-text]').inputValue(), 'NectAAAA');
  await page.screenshot({ path: 'test-results/text-live-focused.png' });
  await page.keyboard.press('Escape');
  assert.equal(await snapshot(), before);
  assert.equal(await drawing(), beforeDrawing);
  await input().fill('NectAAAA');
  await input().press('Tab');
  await page.waitForTimeout(50);
  const typed = await snapshot();
  await history('undo');
  assert.equal(await snapshot(), before, 'whole edit is one Undo');
  await history('redo');
  assert.equal(await snapshot(), typed);

  // Composition displays only the last accepted draft, then updates once at end.
  await input().focus();
  const compositionDrawing = await drawing();
  await input().dispatchEvent('compositionstart');
  // Native insertion marks the textarea dirty so Tab emits its real change
  // event. Assigning el.value alone leaves a private draft after synthetic IME.
  await input().press('End');
  await page.keyboard.insertText('A');
  assert.equal(await drawing(), compositionDrawing);
  await input().dispatchEvent('compositionend');
  assert.notEqual(await drawing(), compositionDrawing);
  assert.equal(await input().evaluate((el) => el === document.activeElement), true);
  await input().press('Tab');
  await page.waitForFunction(() =>
    window.nect.document.layers
      .flatMap((l) => l.nodes)
      .some((n) => n.type === 'Text' && n.text === 'NectAAAAA'),
  );

  // Selected source resolves the same final output once; graph box stays put.
  await page.locator('[data-tool="select"]').click();
  await selectText();
  const graphPositions = await page.evaluate(() =>
    window.nect.document.layers.map((l) => l.nodes.map((n) => [n.id, n.position])),
  );
  const unaligned = await snapshot();
  await openArtworkPanel(page);
  await page.locator('select[data-align-reference]').selectOption('artboard');
  const left = page.locator('[data-align-artwork="left"]');
  await page.waitForFunction(
    () =>
      document.querySelector('select[data-align-reference]')?.value === 'artboard' &&
      document.querySelector('[data-align-artwork="left"]')?.disabled === false,
  );
  assert.equal(await left.isEnabled(), true);
  await left.click();
  assert.equal(
    await page.evaluate(async () => {
      const { pathBounds } = await import('/src/core.js');
      return Math.abs(pathBounds(window.nect.evaluate().path)[0][0]) < 0.001;
    }),
    true,
  );
  assert.deepEqual(
    await page.evaluate(() =>
      window.nect.document.layers.map((l) => l.nodes.map((n) => [n.id, n.position])),
    ),
    graphPositions,
  );
  await history('undo');
  assert.equal(await snapshot(), unaligned);
  await history('redo');
  const aligned = await snapshot();
  // Selecting the source plus its downstream Fill does not move twice.
  await page.keyboard.down('Shift');
  await page
    .locator('.graph .node')
    .filter({ has: page.locator('.node-head strong', { hasText: /^Fill$/ }) })
    .locator('.node-head .type-tag')
    .click();
  await page.keyboard.up('Shift');
  await openArtworkPanel(page);
  await page.locator('[data-align-artwork="left"]').click();
  assert.equal(await snapshot(), aligned);
  await selectText();
  await ensureWorkspacePanel(page, 'inspector');
  assert.equal(await page.locator('.text-symbol-buttons button[aria-pressed="true"]').count(), 4);
  await page.getByRole('button', { name: 'Writing direction: Vertical', exact: false }).click();
  assert.equal(
    await page.evaluate(
      () =>
        window.nect.document.layers.flatMap((l) => l.nodes).find((n) => n.type === 'Text')
          .writingMode,
    ),
    'vertical-rl',
  );
  await history('undo');
  assert.equal(await snapshot(), aligned);
  await page.getByRole('button', { name: 'Text alignment: Center', exact: true }).click();
  await history('undo');
  assert.equal(await snapshot(), aligned);

  await page.setViewportSize({ width: 1280, height: 900 });
  await page.locator('.text-symbol-buttons').first().scrollIntoViewIfNeeded();
  await page.screenshot({ path: 'test-results/text-symbolic-inspector.png' });
  // Actual double pointer click resolves the visible Text source after selection renders.
  const shape = page.locator('.artboard .shape').last();
  const clickText = async () => {
    await shape.waitFor({ state: 'visible' });
    const box = await shape.evaluate((el) => {
      const rect = el.getBoundingClientRect();
      return { x: rect.x, y: rect.y, width: rect.width, height: rect.height };
    });
    await page.mouse.dblclick(box.x + 4, box.y + box.height / 2);
  };
  await clickText();
  const editor = page.getByRole('textbox', { name: 'Edit canvas text', exact: true });
  await editor.waitFor();
  await editor.fill('Canvas edited');
  assert.equal(await input().inputValue(), 'Canvas edited');
  const live = await drawing();
  await page.screenshot({ path: 'test-results/text-canvas-edit.png' });
  await editor.press('Escape');
  assert.equal(await editor.count(), 0);
  assert.equal(await snapshot(), aligned);
  await clickText();
  await editor.fill('Canvas edited');
  await editor.press('Control+Enter');
  await page.waitForTimeout(60);
  assert.equal(await editor.count(), 0);
  assert.equal(await drawing(), live);
  const edited = await snapshot();
  await history('undo');
  assert.equal(await snapshot(), aligned);
  await history('redo');
  assert.equal(await snapshot(), edited);
  const project = await snapshot();
  const svg = await page.evaluate(() => window.nect.export());
  await page.evaluate((project) => window.nect.load(project), project);
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  assert.deepEqual(errors, []);
  await fs.writeFile(
    'test-results/text-feedback-after.json',
    JSON.stringify(
      {
        liveInputFocus: true,
        oneUndo: true,
        composition: 'synthetic browser sequence only',
        canvasDoubleClick: true,
        sourceOutputAlignment: true,
        graphBoxesUnchanged: true,
        symbolicControls: true,
        saveReloadSVG: true,
      },
      null,
      2,
    ),
  );
  console.log(
    'PASS: focused live text, private draft/cancel/one Undo, composition boundary, node-output align/dedup/graph preservation, icon controls, canvas double-click editing/sync/Undo/reload/SVG.',
  );
} catch (error) {
  console.error(
    JSON.stringify({
      pageErrors: errors,
      canvas: await page.evaluate(() => ({
        editor: document.querySelector('.canvas-text-editor')?.outerHTML.slice(0, 500),
        caret: document.querySelector('.canvas-text-caret')?.outerHTML,
        focus: document.activeElement?.tagName,
        status: document.querySelector('.message')?.textContent,
        gesture: window.nect.gestureState,
        alignment: document.querySelector('.alignment-panel')?.innerText,
        nodes: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
      })),
    }),
  );
  await page.screenshot({ path: 'test-results/text-feedback-failure.png' });
  throw error;
} finally {
  await browser.close();
}
