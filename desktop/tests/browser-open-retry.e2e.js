import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { join, resolve } from 'node:path';
import { activateCommand } from './command-menu-actions.js';
import { preloadVariableFontDependency } from './helpers/browser-fixture-setup.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
await fs.mkdir('test-results', { recursive: true });
const directory = await fs.mkdtemp(resolve('test-results/browser-open-retry-'));
const errors = [];
try {
  for (const scenario of ['json-malformed', 'json-dimensions-cancel', 'svg-malformed']) {
    const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
    page.on('pageerror', (error) => errors.push(error.message));
    const svg = scenario.startsWith('svg'),
      action = svg ? 'import-svg' : 'open',
      inputId = svg ? 'import-svg-file' : 'open-file',
      path = join(directory, svg ? 'project.svg' : 'project.json');
    const state = () =>
      page.evaluate(() => ({
        project: window.nect.projectSnapshot(),
        command: window.nect.commandState,
        file: window.nect.fileState,
        composition: window.nect.activeCompositionId,
        nodes: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
        layers: [...document.querySelectorAll('.layer.selected')].map((el) => el.dataset.layer),
        undoDisabled: document.querySelector('[data-action="undo"]').disabled,
        redoDisabled: document.querySelector('[data-action="redo"]').disabled,
        svg: window.nect.export(),
      }));
    const choose = async () => {
      const dirty = await page.evaluate(() => window.nect.fileState.dirty),
        choosing = page.waitForEvent('filechooser');
      await activateCommand(page, action);
      if (dirty) await page.locator('dialog[open] button[value="discard"]').click();
      return choosing;
    };
    try {
      await preloadVariableFontDependency(page, process.env.BASE_URL || 'http://127.0.0.1:5173');
      await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
      await page.waitForFunction(() => window.nect);
      await page.locator('[data-tool="Rectangle"]').click();
      await page.locator('[data-tool="Circle"]').click();
      await activateCommand(page, 'undo');
      const baseline = await state();
      assert.equal(baseline.file.dirty, true);
      assert.equal(baseline.undoDisabled, false);
      assert.equal(baseline.redoDisabled, false);
      const repaired = structuredClone(baseline.project);
      repaired.compositions[0].width = 317;
      const invalid = structuredClone(repaired);
      invalid.compositions[0].width = 0;
      await fs.writeFile(
        path,
        svg ? '<svg' : scenario === 'json-malformed' ? '{' : JSON.stringify(invalid),
      );
      await page.evaluate((id) => {
        window.retryEvents = [];
        for (const type of ['change', 'cancel'])
          document.addEventListener(
            type,
            (event) => {
              if (event.target.id === id) window.retryEvents.push(type);
            },
            true,
          );
      }, inputId);
      await (await choose()).setFiles(path);
      await page.waitForFunction(
        (prefix) => document.querySelector('.status .message')?.textContent.startsWith(prefix),
        svg ? 'SVG import rejected;' : 'Open rejected;',
      );
      if (!svg)
        assert.match(
          await page.locator('.status .message').textContent(),
          scenario === 'json-malformed' ? /JSON|property name/i : /Invalid Composition width/,
        );
      assert.deepEqual(await page.evaluate(() => window.retryEvents), ['change']);
      assert.deepEqual(await state(), baseline, `${scenario}: rejected Open is atomic`);
      assert.notEqual(await page.locator(`#${inputId}`).inputValue(), '');

      // Correct the actual disk file, keeping its exact absolute path. Do not
      // replace the input, refresh the page or use an in-memory File payload.
      await fs.writeFile(
        path,
        svg
          ? '<svg xmlns="http://www.w3.org/2000/svg" width="317" height="211"><rect width="20" height="30" fill="#123456"/></svg>'
          : JSON.stringify(repaired),
      );
      if (scenario.endsWith('-cancel')) {
        const cancelled = await choose();
        assert.equal(await page.locator(`#${inputId}`).inputValue(), '');
        // Chromium empty selection on an already re-armed input emits cancel.
        // This is browser automation, not physical OS-dialog acceptance.
        await cancelled.setFiles([]);
        await page.waitForFunction(() => window.retryEvents.includes('cancel'));
        assert.deepEqual(await state(), baseline, 'chooser cancellation preserves the document');
      }
      const retry = await choose();
      assert.equal(await page.locator(`#${inputId}`).inputValue(), '');
      await retry.setFiles(path);
      await page.waitForFunction(
        ({ svg, sessionId }) =>
          svg
            ? window.nect.document.width === 317 &&
              document.querySelector('.status .message')?.textContent === 'SVG document imported'
            : window.nect.commandState.sessionId !== sessionId && !window.nect.fileState.dirty,
        { svg, sessionId: baseline.command.sessionId },
      );
      const events = await page.evaluate(() => window.retryEvents);
      assert.equal(events.filter((event) => event === 'change').length, 2);
      if (svg) {
        const imported = await state();
        assert.equal(imported.project.compositions[0].width, 317);
        assert.equal(imported.project.compositions[0].height, 211);
        assert.match(imported.svg, /#123456/);
        assert.equal(imported.file.dirty, true);
        await activateCommand(page, 'undo');
        assert.deepEqual((await state()).project, baseline.project);
        assert.equal((await state()).svg, baseline.svg);
        await activateCommand(page, 'redo');
        assert.deepEqual((await state()).project, imported.project);
      } else {
        const opened = await state();
        assert.deepEqual(opened.project, repaired, 'fresh corrected contents are opened');
        assert.equal(opened.undoDisabled, true);
        assert.equal(opened.redoDisabled, true);
        assert.equal(opened.file.dirty, false);
      }
      console.log(`PASS ${scenario}: same-path rejection, repair and retry`);
    } finally {
      await page.close();
    }
  }
  assert.deepEqual(errors, []);
} finally {
  await browser.close();
  await fs.rm(directory, { recursive: true, force: true });
}
