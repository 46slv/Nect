import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { document as makeDocument, serialize } from '../src/core.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import {
  revealBoundary,
  revealNodePresets,
  activateBoundaryCommand,
} from './helpers/boundary-controls.js';

const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1500, height: 1000 } });
const errors = [];
const cases = [];
page.on('pageerror', (error) => errors.push(error.message));
const inspectorTrigger = page.locator('[data-workspace-panel="inspector"]');
const graphTrigger = page.getByRole('button', { name: 'Toggle node graph controls', exact: true });
const viewTrigger = page.getByRole('button', {
  name: 'Toggle artboard view controls',
  exact: true,
});
const focused = (locator) => locator.evaluate((el) => el === document.activeElement);
const settingsOpen = async () => {
  assert.equal(await graphTrigger.getAttribute('aria-expanded'), 'true');
  assert.equal(await viewTrigger.getAttribute('aria-expanded'), 'true');
};
const snapshot = () =>
  page.evaluate(() => ({
    document: JSON.stringify(window.nect.document),
    svg: window.nect.export(),
    file: window.nect.fileState,
    command: window.nect.commandState,
    gesture: window.nect.gestureState,
    undo: document.querySelector('[data-action="undo"]').disabled,
    redo: document.querySelector('[data-action="redo"]').disabled,
    selected: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
    canvas: document.querySelector('.artboard').getAttribute('style'),
    graph: document.querySelector('.graph').getAttribute('style'),
    scroll: [
      document.querySelector('.graph-scroll').scrollLeft,
      document.querySelector('.graph-scroll').scrollTop,
    ],
  }));
try {
  await mkdir('test-results', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate(() => document.fonts.ready);
  for (const name of ['First loaded document', 'Replacement document']) {
    const doc = makeDocument();
    doc.name = name;
    await page.evaluate((text) => window.nect.load(text), serialize(doc));
    await revealBoundary(page, 'graph');
    await revealBoundary(page, 'canvas');
    if ((await inspectorTrigger.getAttribute('aria-expanded')) === 'true')
      await inspectorTrigger.click();
    const before = await snapshot();
    for (const target of ['panel', 'button', 'rail']) {
      await ensureWorkspacePanel(page, 'inspector');
      if (target === 'button') await page.locator('.inspector button:enabled').first().focus();
      else if (target === 'rail') await inspectorTrigger.focus();
      else assert.equal(await focused(page.locator('.inspector')), true);
      if (name === 'First loaded document' && target === 'panel')
        await page.screenshot({
          path: 'test-results/workflow-inspector-escape-before.png',
          animations: 'disabled',
        });
      await page.keyboard.press('Escape');
      assert.equal(
        await inspectorTrigger.getAttribute('aria-expanded'),
        'false',
        `${name}: ${target}`,
      );
      assert.equal(await focused(inspectorTrigger), true);
      await settingsOpen();
      await page.keyboard.down('Escape');
      await page.keyboard.down('Escape');
      await page.keyboard.up('Escape');
      assert.equal(await focused(inspectorTrigger), true, 'repeated Escape keeps the rail focus');
      assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'false');
      await settingsOpen();
      assert.deepEqual(await snapshot(), before);
      cases.push(`${name}: ${target} dismisses Inspector only and retains focus on repeat`);
      if (name === 'First loaded document' && target === 'panel')
        await page.screenshot({
          path: 'test-results/workflow-inspector-escape-after.png',
          animations: 'disabled',
        });
    }
    await ensureWorkspacePanel(page, 'inspector');
    const input = page.locator('.inspector-body input[type="number"]').first();
    await input.focus();
    await page.keyboard.press('Escape');
    assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'true');
    assert.equal(await focused(input), true);
    await settingsOpen();
    assert.deepEqual(await snapshot(), before);
    cases.push(`${name}: Inspector number input retains Escape ownership`);
  }

  const before = await snapshot();
  for (const [field, trigger, other] of [
    [page.locator('[data-action="graph-fit"]'), graphTrigger, viewTrigger],
    [page.locator('[data-action="fit"]'), viewTrigger, graphTrigger],
  ]) {
    await field.focus();
    await page.keyboard.press('Escape');
    assert.equal(await trigger.getAttribute('aria-expanded'), 'false');
    assert.equal(await other.getAttribute('aria-expanded'), 'true');
    assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'true');
    assert.equal(await focused(trigger), true);
    await page.keyboard.press('Escape');
    assert.equal(await focused(trigger), true);
    assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'true');
    await trigger.press('Enter');
  }
  cases.push('View and Graph keep independent Escape dismissal and focus');
  await revealNodePresets(page);
  const presetInput = page.getByRole('textbox', { name: 'Preset name', exact: true });
  await presetInput.focus();
  await page.keyboard.press('Escape');
  assert.equal(await focused(presetInput), true);
  const presetSummary = page
    .locator('.boundary-popover > summary')
    .filter({ hasText: /^Presets$/ });
  await presetSummary.press('Escape');
  assert.equal(await presetSummary.locator('..').evaluate((el) => el.open), false);
  assert.equal(await focused(presetSummary), true);
  await settingsOpen();
  assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'true');
  cases.push('Preset input and nested popup precede Graph dismissal');

  await page.evaluate(() => {
    const dialog = document.createElement('dialog');
    dialog.id = 'workspace-escape-modal';
    dialog.innerHTML = '<button>Modal owner</button>';
    document.body.append(dialog);
    dialog.showModal();
  });
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('#workspace-escape-modal').evaluate((el) => el.open), false);
  assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'true');
  await settingsOpen();
  await page.locator('#workspace-escape-modal').evaluate((el) => el.remove());
  cases.push('Native HTML modal retains Escape ownership');

  await activateBoundaryCommand(page, 'grid-panel');
  const grid = page.locator('.guides-panel[aria-label="Grid"]');
  await grid.evaluate((el) => {
    el.tabIndex = -1;
    el.focus();
  });
  await page.keyboard.press('Escape');
  assert.equal(await grid.count(), 0);
  await settingsOpen();
  assert.equal(await inspectorTrigger.getAttribute('aria-expanded'), 'true');
  cases.push('Inner Grid dismisses before View, Graph and Inspector');
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(errors, []);
  await writeFile(
    'test-results/workflow-workspace-escape.json',
    JSON.stringify({ ok: true, browser: browser.version(), cases, errors }, null, 2),
  );
  console.log(`workspace Escape PASS: ${cases.length} load, focus and ownership cases`);
} catch (error) {
  await page
    .screenshot({ path: 'test-results/workflow-workspace-escape-failure.png' })
    .catch(() => {});
  await writeFile(
    'test-results/workflow-workspace-escape.json',
    JSON.stringify({ ok: false, cases, errors, failure: String(error) }, null, 2),
  );
  throw error;
} finally {
  await browser.close();
}
