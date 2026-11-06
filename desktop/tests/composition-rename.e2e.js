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
  await dialog().waitFor({ state: 'detached' });
  await page.locator('[data-active-composition="second"][role="tab"]').focus();
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
