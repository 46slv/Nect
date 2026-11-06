import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand, revealCommandMenu } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';

const project = migrateLegacyDocumentToCompositionProject(blankDocument());
project.compositions[0].id = 'first';
project.compositions[0].name = 'Composition 1';
const second = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
second.id = 'second';
second.name = 'Composition 1';
project.compositions.push(second);
const serialized = serializeCompositionProject(project);
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
let page = await browser.newPage({
  viewport: { width: 1600, height: 1000 },
  acceptDownloads: true,
});
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const base = process.env.BASE_URL || 'http://127.0.0.1:5173';
const output = 'test-results/composition-rename';
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const dialog = () => page.locator('.composition-rename-dialog[open]');
const input = () => dialog().locator('[data-composition-name]');
try {
  await mkdir(output, { recursive: true });
  await page.goto(base);
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  const single = await snapshot();

  // Closing must finish its own focus/render work before accepting the next
  // tab navigation. Exercise the real handlers in one browser task so a queued
  // native close event cannot silently steal that newer focus afterward.
  await page.evaluate((text) => window.nect.load(text), serialized);
  for (const route of ['unchanged', 'cancel-button', 'cancel-event', 'rename']) {
    await activateCommand(page, 'rename-composition');
    if (route === 'rename') await input().fill('Closed synchronously');
    const closing = await page.evaluate(async (route) => {
      const dialog = document.querySelector('.composition-rename-dialog[open]');
      const closed = new Promise((resolve, reject) => {
        const deadline = setTimeout(() => reject(Error('Rename close event did not arrive')), 5000);
        dialog.addEventListener(
          'close',
          () => {
            clearTimeout(deadline);
            queueMicrotask(resolve);
          },
          { once: true },
        );
      });
      if (route === 'cancel-button') dialog.querySelector('[data-cancel]').click();
      else if (route === 'cancel-event')
        dialog.dispatchEvent(new Event('cancel', { cancelable: true }));
      else dialog.querySelector('button[type="submit"]').click();
      const closedAtReturn = !dialog.open;
      const restoredAtReturn = document.activeElement?.dataset.activeComposition;
      document.activeElement.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }),
      );
      const navigated = document.activeElement?.dataset.activeComposition;
      await closed;
      return {
        closedAtReturn,
        restoredAtReturn,
        navigated,
        settled: document.activeElement?.dataset.activeComposition,
      };
    }, route);
    assert.deepEqual(
      closing,
      { closedAtReturn: true, restoredAtReturn: 'first', navigated: 'second', settled: 'second' },
      route,
    );
    assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'first');
  }

  // Opening the Project menu must not finish an unrelated live Pen path.
  await page.evaluate((text) => window.nect.load(text), serialized);
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 961 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const beforePen = await snapshot(),
    beforePenState = await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    }));
  await page.locator('[data-tool="pen"]').click();
  for (const position of [
    [120, 100],
    [220, 180],
  ]) {
    const screen = await page.locator('.artboard').evaluate((svg, p) => {
      const transformed = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
      return [transformed.x, transformed.y];
    }, position);
    await page.mouse.click(...screen);
  }
  assert.equal(await page.locator('[data-pen-draft]').count(), 1);
  await activateCommand(page, 'rename-composition');
  assert.equal(await dialog().count(), 0);
  assert.equal(await page.locator('[data-pen-draft]').count(), 1);
  assert.deepEqual(await snapshot(), beforePen);
  assert.deepEqual(
    await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    })),
    beforePenState,
  );
  await page.locator('[role="tab"][aria-selected="true"]').focus();
  await page.keyboard.press('F2');
  assert.equal(await dialog().count(), 0);
  assert.equal(await page.locator('[data-pen-draft]').count(), 1);
  assert.deepEqual(await snapshot(), beforePen);
  await page.locator('[data-pen-cancel]').click();
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await snapshot()).compositions[0].width, 961);

  // The Shared Colors owner has an earlier pointer listener. A refused rename
  // must not commit its live color preview while the Project menu is opened.
  const colorProject = structuredClone(project);
  colorProject.schemaVersion = Math.max(colorProject.schemaVersion, 54);
  colorProject.colorSlots = [
    { id: 'rename-color', name: 'Brand', value: { hex: '#808080', alpha: 1 } },
  ];
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(colorProject));
  await ensureWorkspacePanel(page, 'colors');
  if (!(await page.locator('[data-color-advanced]').evaluate((el) => el.open)))
    await page.locator('[data-color-advanced] summary').click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 961 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const beforeColor = await snapshot(),
    beforeColorState = await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    }));
  const hex = page.locator('[data-shared-color-row="rename-color"] [data-shared-color-edit="hex"]');
  await hex.focus();
  await hex.evaluate((el) => {
    el.value = '#445566';
    el.dispatchEvent(new Event('input', { bubbles: true }));
  });
  const colorMenu = await revealCommandMenu(page, 'Project');
  assert.equal(await hex.inputValue(), '#445566');
  assert.equal(await hex.evaluate((el) => document.activeElement === el), true);
  assert.deepEqual(await snapshot(), beforeColor);
  await colorMenu.locator('[data-action="rename-composition"]').click();
  assert.equal(await dialog().count(), 0);
  assert.equal(await hex.inputValue(), '#445566');
  assert.equal(await hex.evaluate((el) => document.activeElement === el), true);
  assert.deepEqual(await snapshot(), beforeColor);
  assert.deepEqual(
    await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    })),
    beforeColorState,
  );
  await hex.press('Escape');
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await snapshot()).compositions[0].width, 961);
  const hexAgain = page.locator(
    '[data-shared-color-row="rename-color"] [data-shared-color-edit="hex"]',
  );
  await hexAgain.focus();
  await hexAgain.evaluate((el) => {
    el.value = '#445566';
    el.dispatchEvent(new Event('input', { bubbles: true }));
  });
  const otherColorMenu = await revealCommandMenu(page, 'Project');
  await otherColorMenu.locator('[data-action="add-composition"]').click();
  const colorCommitted = await snapshot();
  assert.equal(colorCommitted.compositions.length, 3);
  assert.equal(colorCommitted.colorSlots[0].value.hex, '#445566');

  // Text inputs normalize line breaks on assignment. Unchanged submission must
  // preserve the original model name, even when its displayed value differs.
  const unusual = structuredClone(project);
  unusual.compositions[0].name = '  Phone frame  ';
  unusual.compositions[1].name = 'Line\r\nBreak';
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(unusual));
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 961 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const unusualBefore = await snapshot(),
    unusualState = await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    }));
  await activateCommand(page, 'rename-composition');
  assert.equal(await input().inputValue(), '  Phone frame  ');
  await input().press('Enter');
  // `open` clears before the close event restores focus to the prior tab.
  await page.locator('.composition-rename-dialog').waitFor({ state: 'detached' });
  await page.locator('[data-active-composition="second"][role="tab"]').focus();
  assert.equal(
    await page
      .locator('[data-active-composition="second"][role="tab"]')
      .evaluate((tab) => tab === document.activeElement),
    true,
  );
  await page.keyboard.press('F2');
  assert.equal(await input().inputValue(), 'LineBreak');
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), unusualBefore);
  assert.deepEqual(
    await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    })),
    unusualState,
  );
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await activateCommand(page, 'rename-composition');
  await input().fill('  Renamed frame  ');
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  assert.equal((await snapshot()).compositions[0].name, '  Renamed frame  ');

  // Rename must retain the actual current Artwork selection across Undo/Redo.
  await page.evaluate((text) => window.nect.load(text), JSON.stringify(single));
  await page.locator('[data-tool="Circle"]').click();
  const circleId = (await snapshot()).compositions[0].layers.at(-1).id;
  await page.locator('[data-tool="Rectangle"]').click();
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${circleId}"]`).click();
  const artworkBefore = await snapshot();
  await activateCommand(page, 'rename-composition');
  await input().fill('Selection retained');
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  const artworkRenamed = await snapshot();
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), artworkBefore);
  assert.equal(
    await page.locator(`[data-select-layer="${circleId}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), artworkRenamed);
  assert.equal(
    await page.locator(`[data-select-layer="${circleId}"]`).getAttribute('aria-pressed'),
    'true',
  );

  // A refused rename must not blur and commit an unrelated inline name draft.
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 961 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await page.locator(`[data-select-layer="${circleId}"]`).press('F2');
  const layerName = page.locator(`[data-rename-name="${circleId}"]`);
  await layerName.fill('Uncommitted layer draft');
  const beforeDraft = await snapshot();
  const projectMenu = await revealCommandMenu(page, 'Project');
  assert.equal(await layerName.evaluate((el) => document.activeElement === el), true);
  await projectMenu.locator('[data-action="rename-composition"]').click();
  assert.equal(await dialog().count(), 0);
  assert.equal(await layerName.inputValue(), 'Uncommitted layer draft');
  assert.equal(await layerName.evaluate((el) => document.activeElement === el), true);
  assert.deepEqual(await snapshot(), beforeDraft);
  await layerName.press('Escape');
  await layerName.waitFor({ state: 'detached' });

  await page.locator(`[data-select-layer="${circleId}"]`).press('F2');
  const composingLayerName = page.locator(`[data-rename-name="${circleId}"]`);
  await composingLayerName.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await composingLayerName.fill('変換中');
  const composingMenu = await revealCommandMenu(page, 'Project');
  await composingMenu.locator('[data-action="rename-composition"]').click();
  assert.equal(await dialog().count(), 0);
  assert.equal(await composingLayerName.inputValue(), '変換中');
  assert.equal(await composingLayerName.evaluate((el) => document.activeElement === el), true);
  assert.deepEqual(await snapshot(), beforeDraft);
  await composingLayerName.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: el.value })),
  );
  await composingLayerName.press('Escape');
  await composingLayerName.waitFor({ state: 'detached' });
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await snapshot()).compositions[0].width, 961);

  // Other Project actions keep their normal blur/commit behavior.
  await page.locator(`[data-select-layer="${circleId}"]`).press('F2');
  await page.locator(`[data-rename-name="${circleId}"]`).fill('Committed layer name');
  const normalProjectMenu = await revealCommandMenu(page, 'Project');
  await normalProjectMenu.locator('[data-action="add-composition"]').click();
  const afterAdd = await snapshot();
  assert.equal(afterAdd.compositions.length, 2);
  assert.equal(
    afterAdd.compositions[0].layers.find((layer) => layer.id === circleId).name,
    'Committed layer name',
  );

  // Renaming A must preserve B's current selection when Undo/Redo occurs in B.
  await page.evaluate((text) => window.nect.load(text), serialized);
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await page.locator('[data-tool="Circle"]').click();
  const otherCircle = (await snapshot()).compositions[1].layers.at(-1).id;
  await page.locator('[data-tool="Rectangle"]').click();
  const otherRectangle = (await snapshot()).compositions[1].layers.at(-1).id;
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${otherCircle}"]`).click();
  await page.locator('[data-active-composition="first"][role="tab"]').click();
  const crossBefore = await snapshot();
  await activateCommand(page, 'rename-composition');
  await input().fill('Other composition renamed');
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  const crossAfter = await snapshot();
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${otherCircle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), crossBefore);
  assert.equal(
    await page.locator(`[data-select-layer="${otherCircle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), crossAfter);
  await page.locator(`[data-select-layer="${otherRectangle}"]`).click();
  await page.locator('[data-action="undo"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${otherRectangle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator('[data-action="redo"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${otherRectangle}"]`).getAttribute('aria-pressed'),
    'true',
  );

  // Replacing the redo branch with an ordinary Artwork edit must restore its
  // own selection history instead of inheriting the abandoned rename marker.
  await page.locator('[data-action="undo"]').click();
  await page.locator(`[data-select-layer="${otherCircle}"]`).click();
  await page.locator('[data-tool="Polygon"]').click();
  const polygon = (await snapshot()).compositions[1].layers.at(-1).id;
  assert.equal(await page.locator('[data-action="redo"]').isDisabled(), true);
  await page.locator('[data-action="undo"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${otherCircle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator('[data-action="redo"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${polygon}"]`).getAttribute('aria-pressed'),
    'true',
  );
  const beforeMachineRename = await snapshot();
  await page.evaluate(() => {
    const owner = window.nect;
    owner.dispatch({
      ...owner.commandState,
      type: 'rename-composition',
      payload: {
        compositionId: 'first',
        name: 'API rename',
        expectedName: owner.compositions.find((item) => item.id === 'first').name,
        expectedProjectRevision: owner.projectRevision,
      },
    });
  });
  const afterMachineRename = await snapshot();
  await page.locator(`[data-select-layer="${otherCircle}"]`).click();
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), beforeMachineRename);
  assert.equal(
    await page.locator(`[data-select-layer="${otherCircle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), afterMachineRename);
  assert.equal(
    await page.locator(`[data-select-layer="${otherCircle}"]`).getAttribute('aria-pressed'),
    'true',
  );

  await page.evaluate((text) => window.nect.load(text), JSON.stringify(single));
  await activateCommand(page, 'rename-composition');
  await input().fill('Single frame');
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  assert.equal((await snapshot()).compositions[0].name, 'Single frame');
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), single);
  await page.evaluate((text) => window.nect.load(text), serialized);
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 961 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const before = await snapshot(),
    svg = await page.evaluate(() => window.nect.export());
  const state = await page.evaluate(() => ({
    command: window.nect.commandState,
    revision: window.nect.projectRevision,
  }));
  await activateCommand(page, 'rename-composition');
  await dialog().waitFor();
  assert.equal(await dialog().getAttribute('aria-labelledby'), 'composition-rename-heading');
  assert.equal(await dialog().locator('label').innerText(), 'Name');
  await input().fill('   ');
  await dialog().locator('button[type="submit"]').click();
  assert.match(await dialog().locator('[role="alert"]').innerText(), /cannot be empty/i);
  await page.keyboard.press('Escape');
  await dialog().waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(
    await page.evaluate(() => ({
      command: window.nect.commandState,
      revision: window.nect.projectRevision,
    })),
    state,
  );
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await activateCommand(page, 'rename-composition');
  await dialog().locator('button[type="submit"]').click();
  await dialog().waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), before);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await page.locator('[data-active-composition="first"][role="tab"]').focus();
  await page.keyboard.press('ArrowRight');
  assert.equal(
    await page.evaluate(() => document.activeElement?.dataset.activeComposition),
    'second',
  );
  await page.keyboard.press('F2');
  await dialog().waitFor();
  await input().fill('横型 · 日本語');
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  await page.waitForFunction(() => window.nect.compositions[1].name === '横型 · 日本語');
  const renamed = await snapshot();
  assert.deepEqual(renamed.compositions[0], before.compositions[0]);
  assert.deepEqual(renamed.compositions[1], { ...before.compositions[1], name: '横型 · 日本語' });
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'first');
  assert.equal(
    await page.evaluate(() => document.activeElement?.dataset.activeComposition),
    'second',
  );
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before);
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), renamed);

  const machine = await browser.newPage();
  await machine.goto(base);
  await machine.waitForFunction(() => typeof window.nect?.load === 'function');
  await machine.evaluate(
    ({ text, name }) => {
      window.nect.load(text);
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'rename-composition',
        payload: {
          compositionId: 'second',
          name,
          expectedName: 'Composition 1',
          expectedProjectRevision: window.nect.projectRevision,
        },
      });
    },
    { text: serialized, name: '横型 · 日本語' },
  );
  assert.deepEqual(await machine.evaluate(() => window.nect.projectSnapshot()), renamed);
  await machine.close();

  await page.locator('[data-active-composition="second"][role="tab"]').focus();
  await page.keyboard.press('F2');
  await input().evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await input().fill('縦型');
  await input().press('Enter');
  assert.equal(await dialog().isVisible(), true);
  await input().press('Escape');
  assert.equal(await dialog().isVisible(), true);
  assert.deepEqual(await snapshot(), renamed);
  await input().evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: el.value })),
  );
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  await page.waitForFunction(() => window.nect.compositions[1].name === '縦型');
  const finalProject = await snapshot();

  await page.locator('[data-active-composition="second"][role="tab"]').focus();
  await page.keyboard.press('F2');
  await input().fill('Stale draft');
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 962 },
    }),
  );
  const changedElsewhere = await snapshot();
  await dialog().locator('button[type="submit"]').click();
  assert.match(await dialog().locator('[role="alert"]').innerText(), /Project changed/);
  assert.deepEqual(await snapshot(), changedElsewhere);
  assert.equal(await input().inputValue(), 'Stale draft');
  await page.keyboard.press('Escape');
  await dialog().waitFor({ state: 'detached' });
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), finalProject);

  await page.locator('[data-active-composition="second"][role="tab"]').focus();
  await page.keyboard.press('F2');
  await input().fill('Previous project draft');
  await page.evaluate((text) => window.nect.load(text), JSON.stringify(finalProject));
  await dialog().locator('button[type="submit"]').click();
  assert.match(await dialog().locator('[role="alert"]').innerText(), /Project changed/);
  assert.deepEqual(await snapshot(), finalProject);
  await page.keyboard.press('Escape');
  await dialog().waitFor({ state: 'detached' });

  await page.locator('[data-active-composition="second"][role="tab"]').focus();
  await page.keyboard.press('F2');
  const longName = `Variant ${'字'.repeat(200)}`;
  await input().fill(longName);
  await input().press('Enter');
  await dialog().waitFor({ state: 'detached' });
  await page.waitForFunction((name) => window.nect.compositions[1].name === name, longName);
  await page.waitForFunction(
    (name) =>
      document.querySelector('[data-active-composition="second"][role="tab"]')?.title === name,
    longName,
  );
  const longTab = page.locator('[data-active-composition="second"][role="tab"]');
  assert.equal(await longTab.getAttribute('title'), longName);
  assert.equal(await longTab.getAttribute('aria-keyshortcuts'), 'F2');
  assert.equal(await longTab.evaluate((el) => el.scrollWidth > el.clientWidth), true);
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), finalProject);

  await page.setViewportSize({ width: 1000, height: 650 });
  await page.locator('[data-active-composition="second"][role="tab"]').focus();
  await page.keyboard.press('F2');
  await page.screenshot({ path: `${output}/minimum.png` });
  await page.keyboard.press('Escape');
  const pending = page.waitForEvent('download');
  await activateCommand(page, 'save');
  await (await pending).saveAs(`${output}/project.json`);
  assert.deepEqual(JSON.parse(await readFile(`${output}/project.json`, 'utf8')), finalProject);
  await page.close();
  page = await browser.newPage({ viewport: { width: 1000, height: 650 } });
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto(base);
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  const discard = page.locator('dialog button[value="discard"]');
  if (await discard.isVisible()) await discard.click();
  await (await chooser).setFiles(`${output}/project.json`);
  await page.waitForFunction(
    () => window.nect.compositions.length === 2 && window.nect.compositions[1].name === '縦型',
  );
  assert.deepEqual(await snapshot(), finalProject);
  assert.deepEqual(errors, []);
  console.log('PASS: Composition rename, Session parity, draft/IME, history and save/reopen.');
} finally {
  await browser.close();
}
