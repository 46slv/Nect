import { chromium } from 'playwright-core';
import assert from 'node:assert/strict';
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
page.setDefaultTimeout(5000);
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const read = () => page.evaluate(() => JSON.stringify(window.nect.document));
const get = (id) =>
  page.evaluate((id) => window.nect.document.layers[0].nodes.find((n) => n.id === id), id);
let ids;
async function select(names) {
  const box = await page.locator('.graph-scroll').boundingBox();
  await page.mouse.click(box.x + 6, box.y + 8);
  for (const [i, name] of names.entries()) {
    if (i) await page.keyboard.down('Shift');
    await page.locator(`.graph .node[data-id="${ids[name]}"] .node-head .type-tag`).click();
    if (i) await page.keyboard.up('Shift');
  }
}
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.locator('.shape').waitFor();
  ids = await page.evaluate(async () => {
    const { document, node, connect } = await import('/src/core.js');
    const doc = document(),
      l = doc.layers[0];
    const nodes = {
      Text: node('Text', 80, 50),
      Fill: node('Fill', 390, 50),
      Output: node('Output', 700, 50),
      Vector: node('Vector', 80, 140),
      Null: node('Null', 390, 140),
      Value: node('Value', 700, 140),
      Curve: node('Curve', 80, 230),
    };
    nodes.Text.text = 'Text';
    nodes.Vector.params.value.value = [650, 430];
    l.nodes = Object.values(nodes);
    l.nodes.forEach((n) => (n.collapsed = true));
    l.output = nodes.Output.id;
    connect(l, nodes.Text.id, nodes.Fill.id, 'path');
    connect(l, nodes.Fill.id, nodes.Output.id, 'path');
    window.nect.load(JSON.stringify(doc));
    return Object.fromEntries(Object.entries(nodes).map(([name, n]) => [name, n.id]));
  });
  await page.locator('select[data-align-reference]').selectOption('artboard');
  await select(['Vector']);
  const before = await read();
  await page.locator('[data-align-artwork="center-x"]').click();
  assert.equal((await get(ids.Vector)).params.value.value[0], 480);
  const centered = await read();
  await page.locator('[data-action="undo"]').click();
  assert.equal(await read(), before);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await read(), centered);
  await select(['Text', 'Fill', 'Output', 'Vector']);
  assert.equal(
    await page.locator('[aria-label="Alignment selection"]').textContent(),
    '2 selected',
  );
  await page.locator('select[data-align-reference]').selectOption('selection');
  const mixedBefore = await read();
  await page.locator('[data-align-artwork="right"]').click();
  const bounds = await page.evaluate(async (id) => {
    const { pathBounds } = await import('/src/core.js');
    return { text: pathBounds(window.nect.evaluate().run(id))[0][2] };
  }, ids.Text);
  assert.ok(Math.abs(bounds.text - (await get(ids.Vector)).params.value.value[0]) < 0.00002);
  const mixed = await read();
  assert.deepEqual(
    JSON.parse(mixed).layers[0].nodes.map((n) => n.position),
    JSON.parse(mixedBefore).layers[0].nodes.map((n) => n.position),
  );
  await page.locator('[data-action="undo"]').click();
  assert.equal(await read(), mixedBefore);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await read(), mixed);
  await select(['Vector', 'Value']);
  const refused = await read();
  assert.equal(await page.locator('[data-align-artwork="left"]').isDisabled(), true);
  assert.match(
    await page.locator('.alignment-error').textContent(),
    /Value.*No selected output moved/,
  );
  assert.equal(await read(), refused);
  await select(['Null']);
  await page.locator('select[data-align-reference]').selectOption('artboard');
  await page.locator('[data-align-artwork="bottom"]').click();
  assert.equal((await get(ids.Null)).params.position.value[1], 540);
  await select(['Curve']);
  const oldCurve = await get(ids.Curve);
  await page.locator('[data-align-artwork="right"]').click();
  const curve = await get(ids.Curve);
  assert.deepEqual(
    curve.points.map((p) => [p.handleA, p.handleB]),
    oldCurve.points.map((p) => [p.handleA, p.handleB]),
  );
  await page.screenshot({ path: 'test-results/node-output-alignment.png' });
  assert.deepEqual(errors, []);
  console.log(
    'PASS: Vector/Null position alignment, Text+Vector mixed and route dedup, Artboard/Selection, atomic unsupported refusal, Curve handles, Undo/Redo and unchanged graph boxes.',
  );
} finally {
  await browser.close();
}
