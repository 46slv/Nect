import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { activateCommand } from './command-menu-actions.js';
import { openMoreTypography } from './helpers/text-inspector.js';
import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1800, height: 1100 } }),
  errors = [];
page.on('pageerror', (e) => errors.push(e.message));
page.on('dialog', (d) => d.accept());
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  // This scenario exercises Inspector behavior; opening it is explicit in the new default-closed UI.
  await ensureWorkspacePanel(page, 'inspector');
  await page.waitForFunction(() => window.nect);
  await fs.mkdir('test-results', { recursive: true });
  // Regression for a render crash after committing an unconnected Mask.
  // Use real keyboard and pointer events, not a direct create-node command.
  await page.evaluate(async () => {
    const { blankDocument } = await import('/src/core.js');
    window.nect.load(JSON.stringify(blankDocument()));
  });
  const beforeMask = await page.evaluate(() => JSON.stringify(window.nect.document));
  const addByEnter = async (type) => {
    await page.locator('#add-node').click();
    const search = page.locator('.search-panel input').first();
    await search.fill(type);
    await page.locator(`.picker-option[data-id="${type}"]`).click();
    await search.press('Enter');
    await page.waitForFunction(() => !document.querySelector('.search-panel'));
  };
  await addByEnter('Mask');
  await page.waitForFunction(() => document.querySelector('[data-mask-luminance]'));
  assert.equal(
    await page.evaluate(
      () => window.nect.document.layers[0].nodes.filter((n) => n.type === 'Mask').length,
    ),
    1,
  );
  assert.equal(await page.locator('[data-mask-luminance]').first().isDisabled(), true);
  const withMask = await page.evaluate(() => JSON.stringify(window.nect.document));
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), beforeMask);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), withMask);
  await page.waitForFunction(() => document.querySelector('[data-mask-luminance]'));
  await addByEnter('Circle');
  assert.equal(
    await page.evaluate(
      () => window.nect.document.layers[0].nodes.filter((n) => n.type === 'Circle').length,
    ),
    1,
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => JSON.stringify(window.nect.document)), withMask);
  await page.screenshot({ path: 'test-results/workflow-mask-enter.png', fullPage: true });
  assert.deepEqual(errors, []);
  const ids = await page.evaluate(async () => {
    const { document, node, connect, layer } = await import('/src/core.js');
    const d = document();
    d.name = 'Radial mark and anchored typography';
    d.width = 900;
    d.height = 540;
    const l = d.layers[0],
      r = node('Rectangle', 40, 40),
      rep = node('Geometry Repeater', 340, 40),
      fill = node('Fill', 650, 40),
      out = node('Output', 960, 40);
    r.params.center.value = [450, 125];
    r.params.size.value = [20, 80];
    rep.operation = 'radial-fit';
    rep.params.count.value = 12;
    rep.params.center.value = [450, 250];
    fill.params.color.value = { hex: '#286b82', alpha: 1 };
    l.nodes = [r, rep, fill, out];
    l.output = out.id;
    connect(l, r.id, rep.id, 'path');
    connect(l, rep.id, fill.id, 'path');
    connect(l, fill.id, out.id, 'path');
    const t = layer('Typography'),
      text = node('Text', 40, 40),
      tf = node('Fill', 380, 40),
      to = node('Output', 680, 40);
    text.text = 'NECT\nGeometry + type';
    text.params.position.value = [450, 440];
    text.params.size.value = 36;
    text.alignment = 'center';
    t.nodes = [text, tf, to];
    t.output = to.id;
    connect(t, text.id, tf.id, 'path');
    connect(t, tf.id, to.id, 'path');
    d.layers.push(t);
    window.nect.load(JSON.stringify(d));
    return { rep: rep.id, text: text.id, layer: t.id };
  });
  const inspect = async (id) =>
    page.locator(`.graph .node[data-id="${id}"] .node-head strong`).click();
  await inspect(ids.rep);
  const field = (key) =>
    page.locator(`.inspector-body .field[data-node="${ids.rep}"][data-key="${key}"]`);
  const count = field('count').locator('[data-value]');
  await count.fill('8');
  await count.press('Tab');
  assert.equal(await page.evaluate(() => window.nect.evaluate().path.contours.length), 8);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await page.evaluate(() => window.nect.evaluate().path.contours.length), 12);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await page.evaluate(() => window.nect.evaluate().path.contours.length), 8);
  await page.locator(`.inspector-body [data-operation="${ids.rep}"]`).selectOption('linear');
  assert.equal(await field('angle').count(), 0);
  assert.equal(await field('step').count(), 1);
  await page.locator(`.inspector-body [data-operation="${ids.rep}"]`).selectOption('radial-fit');
  assert.equal(await field('angle').locator('[data-value]').inputValue(), '360');
  await count.fill('0');
  await count.press('Tab');
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[0].nodes.find((n) => n.id === id).params.count.value === 8 &&
      document.querySelector(
        `.inspector-body .field[data-node="${id}"][data-key="count"] [data-value]`,
      )?.value === '8',
    ids.rep,
  );
  assert.equal(await count.inputValue(), '8');
  await page.evaluate(({ layer }) => {
    document.querySelector(`[data-select-layer="${layer}"]`).click();
  }, ids);
  await inspect(ids.text);
  const align = () => page.locator(`.inspector-body [data-text-alignment="${ids.text}"]`);
  assert.equal(await align().inputValue(), 'center');
  await page
    .locator('.inspector-body [data-text-symbol="textAlignment"][data-text-symbol-value="end"]')
    .click();
  await page.waitForFunction(
    (id) =>
      window.nect.document.layers[1].nodes.find((n) => n.id === id).alignment === 'end' &&
      document.querySelector(`.graph [data-text-alignment="${id}"]`)?.value === 'end',
    ids.text,
  );
  assert.equal(
    await page.locator(`.graph [data-text-alignment="${ids.text}"]`).inputValue(),
    'end',
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await align().inputValue(), 'center');
  await openMoreTypography(page);
  await page.locator(`.inspector-body [data-text-kerning="${ids.text}"]`).uncheck();
  assert.equal(
    await page.evaluate(
      (id) => window.nect.document.layers[1].nodes.find((n) => n.id === id).kerning,
      ids.text,
    ),
    false,
  );
  for (const [action, ext] of [
    ['save', 'json'],
    ['export', 'svg'],
    ['export-png', 'png'],
  ]) {
    const wait = page.waitForEvent('download');
    await activateCommand(page, action);
    await (await wait).saveAs(`test-results/workflow-creation.${ext}`);
  }
  const saved = await fs.readFile('test-results/workflow-creation.json', 'utf8');
  await page.evaluate((s) => window.nect.load(s), saved);
  assert.equal(
    await page.evaluate(() => window.nect.document.layers[1].nodes[0].alignment),
    'center',
  );
  assert.equal(
    await page.evaluate(() => window.nect.document.layers[0].nodes[1].params.count.value),
    8,
  );
  await page.screenshot({ path: 'test-results/workflow-creation-editor.png', fullPage: true });
  assert.deepEqual(errors, []);
  console.log(
    'PASS creation: radial/linear UI, history, invalid count, text anchor+kerning parity, JSON/SVG/PNG artifacts',
  );
} finally {
  await browser.close();
}
