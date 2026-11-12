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
  const search = page.getByRole('searchbox', { name: 'Search effects' });
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
