import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';

// Scoped headless browser, isolated persistent profile. Installed-font IPC is
// deterministic; this does not deliver physical IME or control native dialogs.
await fs.mkdir('test-results', { recursive: true });
const profile = await fs.mkdtemp(path.resolve('test-results/trial-picker-profile-'));
const executablePath =
  process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const base = process.env.BASE_URL || 'http://127.0.0.1:5184';
const launch = () =>
  chromium.launchPersistentContext(profile, {
    executablePath,
    headless: true,
    viewport: { width: 1800, height: 1100 },
  });
let browser = await launch();
const errors = [];
async function setup(context) {
  const page = context.pages()[0] || (await context.newPage());
  page.setDefaultTimeout(5000);
  page.on('pageerror', (error) => errors.push(error.message));
  await page.route('**/src/storage.js', (route) =>
    route.fulfill({
      contentType: 'text/javascript',
      body: `export { DocumentLifecycle } from './document-lifecycle.js';
      export const desktop = true;
      export const saveFile = async () => true, openFile = async () => null,
        writeProject = async () => null, releaseProject = async () => {},
        chooseUnsaved = async () => 'cancel', registerClose = async () => {};
      export const listSystemFonts = async () => [
        ['system-0', 'Japanese fixture', null], ['system-1', 'Latin fixture', null],
        ['system-2', 'Slow fixture', null], ['system-3', 'Unsupported fixture', 'Variable fonts unsupported'],
        ['system-4', 'Rejected fixture', null]
      ];
      export async function readSystemFont(id) {
        await new Promise(resolve => setTimeout(resolve, id === 'system-2' ? 350 : 40));
        if (id === 'system-4') throw Error('Font file was removed');
        return (await fetch(id === 'system-1' ? '/fonts/DejaVuSans.ttf' : '/tests/fixtures/fonts/NectJPFixture-Regular.otf')).arrayBuffer();
      }`,
    }),
  );
  await page.goto(base);
  await page.waitForSelector('.shape');
  return page;
}
const doc = (page) => page.evaluate(() => JSON.stringify(window.nect.snapshot()));
const canvas = (page) =>
  page
    .locator('.artboard .shape')
    .last()
    .evaluate((element) => element.outerHTML);
const dialog = (page) => page.getByRole('dialog');
async function ready(page) {
  await page.waitForFunction(() => {
    const panel = document.querySelector('.search-panel');
    return (
      panel &&
      !panel.querySelector('.picker-apply').disabled &&
      panel.querySelector('output').textContent !== 'Loading font…'
    );
  });
}
async function createText(page) {
  await page.locator('[data-tool="Text"]').click();
  await page
    .locator('.graph .node')
    .filter({ has: page.locator('[data-text]') })
    .locator('.node-head strong')
    .click();
  const text = page.locator('.inspector-body [data-text]');
  await text.fill('ABA');
  await text.dispatchEvent('change');
  await page.waitForTimeout(80);
}
async function exerciseAlignment(page) {
  const ids = await page.evaluate(async () => {
    const { blankDocument } = await import('/src/core.js');
    const { appendArtwork } = await import('/src/artwork-creation.js');
    const doc = blankDocument();
    const ids = [100, 280, 650].map((x, i) => {
      const created = appendArtwork(doc, 'Rectangle', { at: [x, 100 + i * 80] });
      doc.layers.at(-1).nodes[0].params.size.value = [40 + i * 20, 30 + i * 10];
      return created.layerId;
    });
    doc.layers.shift();
    window.nect.load(JSON.stringify(doc));
    return ids;
  });
  const select = async () => {
    for (const [index, id] of ids.entries())
      await page
        .locator(`[data-select-layer="${id}"]`)
        .click({ modifiers: index ? ['Shift'] : [] });
  };
  const bounds = () =>
    page.locator('.artboard .shape').evaluateAll((elements) =>
      elements.map((element) => {
        const b = element.getBBox();
        return { x: b.x, y: b.y, width: b.width, height: b.height };
      }),
    );
  await select();
  const before = await doc(page);
  assert.equal(await page.locator('[data-align-artwork]').count(), 6);
  assert.equal(await page.locator('[data-distribute-artwork]').count(), 6);
  await page.locator('[data-align-artwork="left"]').click();
  const aligned = await bounds();
  assert.ok(aligned.every((b) => Math.abs(b.x - aligned[0].x) < 1e-6));
  await page.locator('[data-action="undo"]').click();
  assert.equal(await doc(page), before);
  const gap = page.getByRole('spinbutton', { name: 'Artwork gap spacing' });
  await gap.fill('12');
  // Real pointer click, without Playwright's retry of a detached target. A blur
  // redraw used to consume this first click after typing the spacing value.
  const gapButton = await page.locator('[data-distribute-gap="gap-x"]').boundingBox();
  await page.mouse.click(gapButton.x + gapButton.width / 2, gapButton.y + gapButton.height / 2);
  const spaced = await bounds();
  for (let i = 1; i < spaced.length; i++)
    assert.ok(Math.abs(spaced[i].x - spaced[i - 1].x - spaced[i - 1].width - 12) < 1e-6);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await doc(page), before);
  await page.locator(`[data-select-layer="${ids[0]}"]`).click();
  await page.getByRole('combobox', { name: 'Alignment reference' }).selectOption('artboard');
  await page.locator('[data-align-artwork="left"]').click();
  assert.ok(Math.abs((await bounds())[0].x) < 1e-6);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await doc(page), before);
  await page.locator('.graph .node').first().locator('.node-head strong').click();
  const panel = page.getByRole('region', { name: 'Alignment panel' });
  const top = (await panel.boundingBox()).y;
  await page.locator('.inspector').evaluate((element) => {
    element.scrollTop = element.scrollHeight;
  });
  assert.equal((await panel.boundingBox()).y, top);
  assert.ok(await panel.isVisible());
  await page.screenshot({ path: 'test-results/trial-feedback-alignment.png' });
  await page.locator('.canvas-toolbar [data-action="align-panel"]').click();
  assert.equal(await panel.count(), 0);
}
try {
  const page = await setup(browser);
  const beforeNode = await doc(page);
  await page.locator('.graph-scroll').focus();
  await page.keyboard.press('Control+Space');
  await page.getByRole('combobox', { name: 'ノードを追加 search' }).fill('vector math');
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('Enter');
  await page.waitForFunction(() =>
    window.nect.document.layers[0].nodes.some((node) => node.type === 'Vector Math'),
  );
  assert.equal(await dialog(page).count(), 0);
  await page.keyboard.press('Control+z');
  assert.equal(await doc(page), beforeNode);
  await page.locator('#add-node').click();
  await page.getByRole('combobox', { name: 'ノードを追加 search' }).fill('no-such-node');
  assert.equal(await dialog(page).getByRole('option').count(), 0);
  await page.keyboard.press('Enter');
  assert.equal(await dialog(page).count(), 1);
  await page.keyboard.press('Escape');
  assert.equal(await doc(page), beforeNode);
  await page
    .locator('.graph-scroll')
    .dispatchEvent('keydown', { key: ' ', code: 'Space', ctrlKey: true, isComposing: true });
  assert.equal(await dialog(page).count(), 0);
  await createText(page);
  await page.locator('.inspector-body [data-text]').focus();
  await page.keyboard.press('Control+Space');
  assert.equal(await dialog(page).count(), 0, 'text input retains Ctrl+Space for its own IME');
  const baseline = await doc(page),
    baselineCanvas = await canvas(page);
  const baselineSVG = await page.evaluate(() => window.nect.export());
  await page.locator('.inspector-body [data-font]').click();
  const search = page.getByRole('combobox', { name: 'Font search' });
  await search.fill('Japanese');
  await ready(page);
  const japaneseCanvas = await canvas(page);
  assert.notEqual(japaneseCanvas, baselineCanvas, 'candidate changes real canvas before commit');
  assert.equal(await doc(page), baseline);
  assert.equal(await page.evaluate(() => window.nect.export()), baselineSVG);
  await search.fill('fixture');
  await page.keyboard.press('ArrowDown');
  await ready(page);
  const latinCanvas = await canvas(page);
  assert.notEqual(latinCanvas, japaneseCanvas);
  await page.keyboard.press('ArrowDown'); // deliberately slow load
  await page.keyboard.press('ArrowUp'); // latest, fast Latin load
  await ready(page);
  await page.waitForTimeout(450);
  assert.equal(
    await canvas(page),
    latinCanvas,
    'old async response cannot overwrite newer preview',
  );
  await search.fill('Rejected');
  await page.waitForFunction(() =>
    document.querySelector('.picker-status').textContent.includes('Font file was removed'),
  );
  assert.equal(await doc(page), baseline);
  await page.keyboard.press('Escape');
  assert.equal(await doc(page), baseline);
  assert.equal(await canvas(page), baselineCanvas);

  await page.locator('.inspector-body [data-font]').click();
  await search.fill('Japanese');
  await ready(page);
  await page.getByRole('button', { name: 'Favorite Japanese fixture', exact: true }).click();
  await page.getByRole('button', { name: 'Show favorite fonts' }).click();
  assert.equal(await dialog(page).getByRole('option').count(), 1);
  assert.equal(
    await page
      .getByRole('button', { name: 'Favorite Japanese fixture', exact: true })
      .getAttribute('aria-pressed'),
    'true',
  );
  await page.keyboard.press('Enter');
  await page.waitForFunction(() => !document.querySelector('.search-panel'));
  const committed = await doc(page);
  assert.notEqual(committed, baseline);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await doc(page), baseline, 'all browsing commits as a single Undo');
  await page.locator('[data-action="redo"]').click();
  assert.equal(await doc(page), committed);
  // Long explanations are optional Help; actionable errors/status remain visible.
  assert.ok(await page.locator('.inspector-body .control-help').count());
  assert.equal(
    await page
      .locator('.inspector-body .font-note:visible')
      .filter({ hasText: 'Position reference holds each line' })
      .count(),
    0,
  );
  await page.screenshot({ path: 'test-results/trial-feedback-editor.png' });
  await page.locator('.inspector-body [data-font]').click();
  await page.screenshot({ path: 'test-results/trial-feedback-font-picker.png' });
  await page.keyboard.press('Escape');
  await exerciseAlignment(page);
  await browser.close();

  browser = await launch(); // actual process restart, no storageState injection
  const restarted = await setup(browser);
  await createText(restarted);
  await restarted.locator('.inspector-body [data-font]').click();
  await restarted.getByRole('button', { name: 'Show favorite fonts' }).click();
  assert.equal(await dialog(restarted).getByRole('option').count(), 1);
  assert.equal(await dialog(restarted).getByRole('option').textContent(), 'Japanese fixture★');
  await restarted.keyboard.press('Escape');
  assert.equal(await restarted.locator('.alignment-panel').count(), 0);
  await restarted.locator('.canvas-toolbar [data-action="align-panel"]').click();
  assert.equal(
    await restarted.getByRole('combobox', { name: 'Alignment reference' }).inputValue(),
    'artboard',
  );
  assert.equal(
    await restarted.getByRole('spinbutton', { name: 'Artwork gap spacing' }).inputValue(),
    '12',
  );
  assert.deepEqual(errors, []);
  console.log(
    'PASS: Ctrl+Space/search/IME exclusion; live font browsing/stale-load/cancel/one Undo; persistent favorites; pinned align/spacing/artboard/Undo and restart preferences; optional Help.',
  );
} finally {
  await browser.close();
}
