import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand } from './command-menu-actions.js';

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
