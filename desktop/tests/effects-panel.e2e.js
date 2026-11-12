import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({ viewport: { width: 1400, height: 900 } });
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
page.on('dialog', (dialog) => dialog.accept());
const project = () => page.evaluate(() => JSON.stringify(window.nect.projectSnapshot()));
const shape = () =>
  page
    .locator('.artboard .shape')
    .first()
    .evaluate((element) => element.outerHTML);
const activeNode = (id) =>
  page.evaluate(
    (nodeId) => window.nect.document.layers.flatMap((l) => l.nodes).find((n) => n.id === nodeId),
    id,
  );

try {
  await fs.mkdir('test-results', { recursive: true });
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.locator('[data-tool="Circle"]').click();
  const before = await project(),
    beforeShape = await shape();

  await page.getByRole('button', { name: 'Toggle Effects panel', exact: true }).click();
  assert.equal(await page.locator('[data-effects-target]').innerText(), 'Circle');
  const favorite = page.locator('[data-effects-favorite]');
  await favorite.focus();
  await page.keyboard.press('Space');
  assert.equal(await favorite.getAttribute('aria-pressed'), 'true');
  assert.deepEqual(
    await page.evaluate(() => JSON.parse(localStorage.getItem('nect.effect-favorites.v1'))),
    ['builtin.transform.v1'],
  );
  assert.equal(await project(), before);
  const search = page.getByRole('searchbox', { name: 'Search effects' });
  const circleId = await page.evaluate(
    () =>
      window.nect.document.layers
        .flatMap((layer) => layer.nodes)
        .find((node) => node.type === 'Circle').id,
  );
  await page
    .locator(`.graph .node[data-id="${circleId}"]`)
    .getByRole('spinbutton', { name: 'Circle Position X', exact: true })
    .click();
  assert.equal(await page.locator('[data-effects-apply]').isDisabled(), true);
  assert.equal(await page.locator('[data-effects-status]').innerText(), 'Select artwork');
  await page.locator('.artboard .shape').first().click();
  assert.equal(await page.locator('[data-effects-target]').innerText(), 'Circle');

  await page.evaluate(
    () => (window.hiddenEffectApply = document.querySelector('[data-effects-apply]')),
  );
  await page.locator('.viewer-tabs [data-viewer-mode="viewers"]').click();
  await page.evaluate(() => window.hiddenEffectApply.click());
  assert.equal(await project(), before);
  await page.locator('.viewer-tabs [data-viewer-mode="artwork"]').click();

  // A press begun while IME is composing must not become an Apply on release.
  const cdp = await page.context().newCDPSession(page);
  await search.focus();
  await cdp.send('Input.imeSetComposition', { text: '移動', selectionStart: 0, selectionEnd: 2 });
  const applyBox = await page.locator('[data-effects-apply]').boundingBox();
  assert.equal(await page.locator('[data-effects-apply]').isDisabled(), true);
  await page.mouse.click(applyBox.x + applyBox.width / 2, applyBox.y + applyBox.height / 2);
  assert.equal(await project(), before);
  assert.equal(await search.evaluate((element) => element === document.activeElement), true);
  assert.equal(await page.locator('[data-effects-apply]').isDisabled(), true);
  await cdp.send('Input.insertText', { text: '移動' });

  // Committing by Tab keeps the result row alive as the browser moves focus.
  await search.fill('');
  await search.focus();
  await page.evaluate(() => (window.effectSearchRow = document.querySelector('[data-effect-id]')));
  await cdp.send('Input.imeSetComposition', { text: '移動', selectionStart: 0, selectionEnd: 2 });
  await page.keyboard.press('Tab');
  assert.equal(await page.evaluate(() => document.activeElement === window.effectSearchRow), true);
  assert.equal(await project(), before);
  await search.fill('移動');
  assert.equal(await page.getByRole('button', { name: 'Transform', exact: true }).count(), 1);
  await page.screenshot({ path: 'test-results/effects-panel-search.png', animations: 'disabled' });
  await page.locator('[data-effects-apply]').click();
  const after = await project();
  assert.notEqual(after, before);
  assert.equal(await shape(), beforeShape, 'identity effect preserves the painted artwork');
  const transform = await page.evaluate(() =>
    window.nect.document.layers
      .flatMap((layer) => layer.nodes)
      .find((node) => node.type === 'Transform'),
  );
  assert.ok(transform?.id);
  assert.equal(await page.locator('.inspector-body h3').first().innerText(), 'Transform');
  const graphRects = await page.locator('.graph .node').evaluateAll((cards) =>
    cards.map((card) => ({
      id: card.dataset.id,
      x: card.getBoundingClientRect().x,
      width: card.getBoundingClientRect().width,
    })),
  );
  const inserted = graphRects.find((card) => card.id === transform.id);
  assert.ok(inserted);
  assert.ok(
    graphRects
      .filter((card) => card.id !== transform.id)
      .every((card) => inserted.x + inserted.width <= card.x || card.x + card.width <= inserted.x),
  );

  const x = page
    .locator('.inspector-body')
    .getByRole('spinbutton', { name: 'Transform Position X', exact: true });
  await x.fill('25');
  await x.press('Tab');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers.flatMap((layer) => layer.nodes).find((node) => node.id === id)
        ?.params.position?.value[0] === 25,
    transform.id,
  );
  await page.waitForFunction(
    (before) => document.querySelector('.artboard .shape')?.outerHTML !== before,
    beforeShape,
  );
  const edited = await project();
  assert.notEqual(await shape(), beforeShape);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await project(), after);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await project(), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await project(), after);
  assert.ok(await activeNode(transform.id));
  await page.locator('[data-action="redo"]').click();
  assert.equal(await project(), edited);

  await page.locator('.artboard .shape').first().click();
  assert.equal(await page.locator('.artwork-selection').count(), 1);
  await page.locator('.canvas-stage').focus();
  await page.keyboard.press('ArrowRight');
  assert.deepEqual((await activeNode(transform.id)).params.position.value, [26, 0]);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await project(), edited);

  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save');
  const file = 'test-results/effects-panel-project.json';
  await (await downloading).saveAs(file);
  assert.deepEqual(JSON.parse(await fs.readFile(file, 'utf8')), JSON.parse(edited));
  await page.reload();
  await page.waitForFunction(() => window.nect);
  const choosing = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await page.locator('dialog[open] button[value="discard"]').click();
  await (await choosing).setFiles(file);
  await page.waitForFunction(() => !window.nect.fileState.dirty);
  assert.equal(await project(), edited);
  assert.ok(await activeNode(transform.id));

  // Detached old controls cannot apply to a newly selected artwork.
  await page.locator('[data-tool="Rectangle"]').click();
  await page.getByRole('button', { name: 'Toggle Effects panel', exact: true }).click();
  assert.equal(await page.locator('[data-effects-favorite]').getAttribute('aria-pressed'), 'true');
  await page.evaluate(
    () => (window.oldEffectApply = document.querySelector('[data-effects-apply]')),
  );
  await page.locator('[data-tool="Star"]').click();
  const replacement = await project();
  await page.evaluate(() => window.oldEffectApply.click());
  assert.equal(await project(), replacement);
  assert.equal(await page.locator('[data-effects-target]').innerText(), 'Star');
  assert.deepEqual(errors, []);
  console.log(
    'PASS Effects search/apply, identity paint, Inspector, canvas move, Undo/Redo and Save/Open',
  );
} finally {
  await browser.close();
}
