import { activateBoundaryCommand } from './helpers/boundary-controls.js';
// One dedicated native Tauri/WebView2 before/after case; CDP is synthetic input.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import { chromium } from 'playwright-core';
import { document, guideDefaults, serialize } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';

const mode = process.env.NECT_GUIDE_ESCAPE_EXPECT || 'cancelled';
assert.ok(['retained', 'cancelled'].includes(mode));
const output = path.resolve(
  process.env.NECT_GUIDE_ESCAPE_OUTPUT || `test-results/guides-dom-maintenance/native-${mode}`,
);
await fs.mkdir(output, { recursive: true });
const browser = await chromium.connectOverCDP('http://127.0.0.1:9473');
try {
  const pages = browser
    .contexts()[0]
    .pages()
    .filter((p) => /tauri\.localhost/.test(p.url()));
  assert.equal(pages.length, 1);
  const [page] = pages,
    errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.waitForFunction(() => window.nect && window.__TAURI_INTERNALS__);
  const doc = document();
  doc.name = `NECT_GUIDE_ESCAPE_${mode}`;
  doc.editor = {
    ...guideDefaults(),
    guides: [{ id: 'focused-guide', axis: 'x', position: 600, locked: false }],
  };
  await page.evaluate((text) => window.nect.load(text), serialize(doc));
  await activateBoundaryCommand(page, 'guides-panel');
  const input = page.locator('[data-guide-position="focused-guide"]');
  await input.focus();
  const read = () =>
    page.evaluate(() => ({
      json: JSON.stringify(window.nect.document),
      svg: window.nect.export(),
      file: window.nect.fileState,
      command: window.nect.commandState,
      gesture: window.nect.gestureState,
      guide: window.nect.document.editor.guides[0].position,
      undoDisabled: document.querySelector('[data-action="undo"]').disabled,
      redoDisabled: document.querySelector('[data-action="redo"]').disabled,
      selected: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
      focusedGuide: document.activeElement?.dataset.guidePosition || null,
      inputConnected: document.activeElement?.isConnected || false,
    }));
  const before = await read();
  const point = (x) =>
    page.locator('.artboard').evaluate((el, x) => {
      const p = new DOMPoint(x, 270).matrixTransform(el.getScreenCTM());
      return [p.x, p.y];
    }, x);
  const start = await point(600);
  assert.equal(
    await page.evaluate(
      ([x, y]) => document.elementFromPoint(x, y)?.closest('[data-guide]')?.dataset.guide,
      start,
    ),
    'focused-guide',
  );
  await page.mouse.move(...start);
  await page.mouse.down();
  await page.mouse.move(...(await point(640)), { steps: 3 });
  const preview = await read();
  assert.equal(
    preview.focusedGuide,
    'focused-guide',
    'actual native pointer drag must naturally retain number-input focus',
  );
  assert.equal(preview.inputConnected, true);
  assert.equal(preview.gesture?.kind, 'guide-position');
  await page.keyboard.press('Escape');
  const escaped = await read();
  await page.mouse.up();
  const released = await read();
  if (mode === 'retained') {
    assert.ok(escaped.gesture, 'baseline diagnostic expects the unreleased guide gesture');
    assert.ok(
      Math.abs(released.guide - 640) < 1,
      'baseline late pointerup commits the preview (client-pixel quantization)',
    );
    assert.notEqual(released.json, before.json);
    assert.equal(released.undoDisabled, false);
  } else {
    assert.equal(escaped.gesture, null);
    assert.equal(released.gesture, null);
    for (const field of [
      'json',
      'svg',
      'file',
      'command',
      'guide',
      'undoDisabled',
      'redoDisabled',
      'selected',
    ])
      assert.deepEqual(released[field], before[field], field);
    assert.equal(escaped.json, before.json);
    await activateBoundaryCommand(page, 'guides-panel');
    const field = page.locator('[data-guide-position="focused-guide"]');
    await field.fill('610');
    await field.press('Tab');
    assert.equal((await read()).guide, 610);
    await page.locator('[data-action="undo"]').click();
    assert.deepEqual(JSON.parse((await read()).json), JSON.parse(before.json));
  }
  assert.deepEqual(errors, []);
  await page.screenshot({ path: path.join(output, 'renderer.png') });
  const receipt = {
    mode,
    input:
      'Native Tauri/WebView2 CDP mouse and keyboard; naturally retained connected number-input focus',
    before,
    preview,
    escaped,
    released,
    pageErrors: errors,
    notRun: ['OS/physical input', 'OS Save/Open/clean cold reopen'],
  };
  await fs.writeFile(path.join(output, 'receipt.json'), JSON.stringify(receipt, null, 2), {
    flag: 'wx',
  });
  const own = await page.evaluate(() => ({
    name: window.nect.document.name,
    binding: window.nect.fileState.binding,
    gesture: window.nect.gestureState,
    project: window.nect.projectSnapshot(),
  }));
  assert.ok(own.name.startsWith('NECT_GUIDE_ESCAPE_'));
  assert.equal(own.binding, null);
  assert.equal(own.gesture, null);
  await fs.writeFile(
    path.join(output, 'closed-fixture.json'),
    serializeCompositionProject(own.project),
    { flag: 'wx' },
  );
  try {
    await page.evaluate(() => window.__TAURI_INTERNALS__.invoke('finish_close'));
  } catch (e) {
    if (!/closed/i.test(String(e))) throw e;
  }
  console.log(
    JSON.stringify(
      {
        mode,
        naturallyFocused: preview.focusedGuide,
        gestureAfterEscape: escaped.gesture,
        guideBefore: before.guide,
        guideAfterRelease: released.guide,
        pageErrors: errors,
        directTestClose: true,
      },
      null,
      2,
    ),
  );
} finally {
  await browser.close();
}
