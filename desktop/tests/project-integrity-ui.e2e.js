import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { activateCommand } from './command-menu-actions.js';

const fixture = await fs.readFile('tests/fixtures/project-integrity-native.json', 'utf8'),
  svg = await fs.readFile('tests/fixtures/project-integrity-import.svg');
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const page = await browser.newPage({ viewport: { width: 1600, height: 1100 } }),
  errors = [];
page.on('pageerror', (error) => errors.push(error.message));
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  await page.evaluate((text) => window.nect.load(text), fixture);
  const snapshot = () => page.evaluate(() => window.nect.projectSnapshot()),
    saved = async () => JSON.stringify(await snapshot());
  const original = await saved();
  await activateCommand(page, 'import-svg');
  await page.locator('dialog[open] [value="cancel"]').click();
  await page.locator('dialog[open]').waitFor({ state: 'detached' });
  assert.equal(await saved(), original, 'Cancel keeps the complete project');

  // Real graph selection and preset controls exercise the schema-50 save path.
  await page.locator('[data-action="graph-fit"]').click();
  const sourceIds = ['example-image', 'example-keyer', 'example-mask-shape', 'example-mask'];
  for (const [index, id] of sourceIds.entries())
    await page.locator(`.node[data-id="${id}"] .node-head strong`).click({
      modifiers: index ? ['Shift'] : [],
    });
  await page.locator('#node-preset-name').fill('Feather UI preset');
  await page.locator('[data-action="save-node-preset"]').click();
  await page.waitForFunction(() => window.nect.projectSnapshot().nodePresets.length === 2);
  const presetSaved = await saved(),
    preset = (await snapshot()).nodePresets.find((p) => p.name === 'Feather UI preset');
  assert.equal(preset.nodes.find((n) => n.type === 'Mask').params.feather.value, 4);
  assert.equal((await snapshot()).schemaVersion, 50);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), original);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), presetSaved);
  await page.locator('#node-preset').selectOption(preset.id);
  await page.locator('[data-action="apply-node-preset"]').click();
  await page.waitForFunction(() => window.nect.document.layers[0].nodes.length === 10);
  const applied = await saved(),
    appliedLayer = (await snapshot()).compositions[0].layers[0],
    maskCopies = appliedLayer.nodes.filter((n) => n.type === 'Mask');
  assert.equal(maskCopies.length, 2);
  assert.ok(maskCopies.every((n) => n.params.feather.value === 4));
  assert.notEqual(maskCopies[0].id, maskCopies[1].id);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), presetSaved);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), applied);
  console.log('PASS Feather preset UI save/apply and exact Undo/Redo');

  const importSvg = async (buffer, name) => {
    const dirty = await page.evaluate(() => window.nect.fileState.dirty),
      chooser = page.waitForEvent('filechooser');
    await activateCommand(page, 'import-svg');
    if (dirty) await page.locator('dialog[open] [value="discard"]').click();
    await (await chooser).setFiles({ name, mimeType: 'image/svg+xml', buffer });
  };
  await importSvg(Buffer.from('<svg><script>alert(1)</script></svg>'), 'invalid.svg');
  await page.waitForFunction(() =>
    document.querySelector('.status').textContent.includes('SVG import rejected'),
  );
  assert.equal(await saved(), applied, 'invalid SVG preserves artwork, registries and presets');
  const beforeImport = await snapshot();
  await importSvg(svg, 'replacement.svg');
  await page.waitForFunction(() =>
    document.querySelector('.status').textContent.includes('SVG document imported'),
  );
  const afterImport = await snapshot(),
    imported = await saved();
  for (const key of ['name', 'schemaVersion', 'imageAssets', 'fontAssets', 'nodePresets'])
    assert.deepEqual(afterImport[key], beforeImport[key], `${key} survives SVG replacement`);
  assert.deepEqual(afterImport.compositions[1], beforeImport.compositions[1]);
  assert.notDeepEqual(afterImport.compositions[0].layers, beforeImport.compositions[0].layers);
  await page.locator('[data-action="undo"]').click();
  assert.equal(await saved(), applied);
  await page.locator('[data-action="redo"]').click();
  assert.equal(await saved(), imported);

  await page.locator('[role="tab"][data-active-composition="integrity-sibling"]').click();
  await page.waitForFunction(() => window.nect.activeCompositionId === 'integrity-sibling');
  const siblingSvg = await page.evaluate(() => window.nect.export());
  assert.match(siblingSvg, /data:image\/png;base64,/);
  assert.match(siblingSvg, /<path/);
  assert.deepEqual((await snapshot()).compositions[1], beforeImport.compositions[1]);
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/project-integrity-sibling.png', fullPage: true });
  const downloading = page.waitForEvent('download');
  await activateCommand(page, 'save-as');
  const download = await downloading,
    savePath = 'test-results/project-integrity-saved.json';
  await download.saveAs(savePath);
  const savedText = await fs.readFile(savePath, 'utf8');
  assert.deepEqual(JSON.parse(savedText), afterImport);
  const chooser = page.waitForEvent('filechooser');
  await activateCommand(page, 'open');
  await (await chooser).setFiles(savePath);
  await page.waitForFunction(() =>
    document.querySelector('.status').textContent.includes('Project restored'),
  );
  assert.equal(await saved(), imported);
  await page.locator('[role="tab"][data-active-composition="integrity-sibling"]').click();
  assert.equal(await page.evaluate(() => window.nect.export()), siblingSvg);
  assert.deepEqual(errors, []);
  console.log('PASS SVG UI refusal/import, sibling assets/schema, Undo/Redo, download and reopen');
  // Keep the Escape acceptance last so a guard regression does not hide the
  // independent save/import evidence above. Loading is test setup only.
  await page.evaluate((text) => window.nect.load(text), fixture);
  const beforeEscape = await saved();
  await activateCommand(page, 'import-svg');
  const guard = page.locator('dialog[open]');
  await guard.locator('[value="cancel"]').focus();
  await page.keyboard.press('Escape');
  await guard.waitFor({ state: 'detached', timeout: 5000 });
  assert.equal(await saved(), beforeEscape, 'Escape keeps the complete project');
  console.log(
    'PASS project integrity UI: Feather preset save/apply, SVG cancel/Escape/refusal/import, sibling image/font/presets/schema, Undo/Redo, downloaded JSON reopen',
  );
} catch (error) {
  await fs.mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/project-integrity-failure.png', fullPage: true });
  throw error;
} finally {
  await browser.close();
}
