import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { blankDocument, node, serialize } from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { compoundFixture } from './fixtures/contour-navigation.js';
import { activateCommand } from './command-menu-actions.js';
import {
  observeProjectRejection,
  projectFileRejections,
} from './helpers/project-file-rejections.js';

// Called by the standard blank-artwork browser case. Synthetic files go through
// the visible menus and browser chooser; this does not exercise native dialogs.
export async function verifyProjectFileAtomicity(page) {
  const f = compoundFixture([4, 4, 4, 4]);
  const project = () => page.evaluate(() => window.nect.projectSnapshot());
  const state = () =>
    page.evaluate(() => ({
      document: window.nect.document,
      project: window.nect.projectSnapshot(),
      command: window.nect.commandState,
      file: window.nect.fileState,
      composition: window.nect.activeCompositionId,
      nodes: [...document.querySelectorAll('.node.selected')].map((el) => el.dataset.id),
      layers: [...document.querySelectorAll('.layer.selected')].map((el) => el.dataset.layer),
      points: [...document.querySelectorAll('.inspector [data-point-row].point-selected')].map(
        (el) => el.dataset.pointRow,
      ),
      pointFocus:
        document.querySelector('.inspector [data-point-row].point-active')?.dataset.pointRow ??
        null,
      undoDisabled: document.querySelector('[data-action="undo"]').disabled,
      redoDisabled: document.querySelector('[data-action="redo"]').disabled,
    }));
  const chooseFile = async (action, text, name) => {
    const dirty = await page.evaluate(() => window.nect.fileState.dirty);
    const choosing = page.waitForEvent('filechooser');
    await activateCommand(page, action);
    if (action === 'open' && dirty)
      await page.locator('dialog[open] button[value="discard"]').click();
    await (
      await choosing
    ).setFiles({ name, mimeType: 'application/json', buffer: Buffer.from(text) });
  };
  const rejectFile = async (action, text, name, diagnostic, needsImport = false) => {
    const prefix = action === 'open' ? 'Open rejected;' : 'Composition import rejected;';
    const observation = await page.evaluateHandle(observeProjectRejection, { name, prefix });
    try {
      await chooseFile(action, text, name);
      if (action === 'import-composition' && needsImport)
        await page.locator('dialog[open] button[value="import"]').click();
      await page.waitForFunction((observed) => observed.text !== null, observation);
      assert.equal(
        await page.locator('.status .message').textContent(),
        await observation.evaluate((observed) => observed.text),
      );
      assert.match(await page.locator('.status .message').textContent(), diagnostic);
    } finally {
      await observation.evaluate((observed) => observed.dispose());
      await observation.dispose();
    }
  };
  const open = async (text) => {
    const previous = await page.evaluate(() => window.nect.commandState.sessionId);
    await chooseFile('open', text, 'valid-project.json');
    await page.waitForFunction(
      (id) => window.nect.commandState.sessionId !== id && !window.nect.fileState.dirty,
      previous,
    );
    assert.equal(await page.locator('[data-action="undo"]').isDisabled(), true);
    assert.equal(await page.locator('[data-action="redo"]').isDisabled(), true);
  };
  const refresh = async () => {
    await activateCommand(page, 'guides-panel');
    await activateCommand(page, 'guides-panel');
  };
  const originalText = serialize(f.doc);
  for (const dirty of [false, true]) {
    await open(originalText);
    const initial = await project();
    await page.locator('[data-tool="Rectangle"]').click();
    const saving = page.waitForEvent('download');
    await activateCommand(page, 'save');
    // Await actual browser download completion before observing the saved marker.
    assert.equal(await (await saving).failure(), null);
    await page.waitForFunction(() => !window.nect.fileState.dirty);
    const saved = await project();
    if (dirty) await page.locator('[data-tool="Star"]').click();
    const before = await project();
    await page.locator('[data-tool="Circle"]').click();
    const future = await project();
    await activateCommand(page, 'undo');
    assert.deepEqual(await project(), before);
    await page.locator(`[data-select-layer="${f.layer.id}"]`).click();
    await page.locator(`.node[data-id="${f.path.id}"] .node-head strong`).click();
    await page.locator('[data-tool="direct"]').click();
    await page
      .locator(`.inspector [data-point-panel="${f.path.id}"] [data-point-scope-action="all"]`)
      .click();
    const baseline = await state();
    assert.equal(baseline.file.dirty, dirty);
    assert.equal(baseline.undoDisabled, false);
    assert.equal(baseline.redoDisabled, false);
    assert.equal(baseline.points.length, 16);
    await refresh();
    assert.deepEqual(await state(), baseline, 'normal panel refresh preserves selection');
    const rejections = projectFileRejections();
    for (const action of ['open', 'import-composition']) {
      for (const [name, text, diagnostic, needsImport] of rejections) {
        await rejectFile(action, text, `${name}.json`, diagnostic, needsImport);
        assert.deepEqual(await state(), baseline, `${action}/${name}: immediate atomicity`);
        // A failed Open can leave stale selected rows on screen even after its
        // internal point selection was cleared. Force a normal non-editing render.
        await refresh();
        assert.deepEqual(await state(), baseline, `${action}/${name}: refreshed atomicity`);
      }
    }
    // Prove both existing branches still contain their exact authored snapshots.
    await activateCommand(page, 'redo');
    assert.deepEqual(await project(), future);
    await activateCommand(page, 'undo');
    assert.deepEqual(await project(), before);
    await activateCommand(page, 'undo');
    assert.deepEqual(await project(), dirty ? saved : initial);
    await activateCommand(page, 'redo');
    assert.deepEqual(await project(), before);
    await open(originalText);
    assert.deepEqual(await project(), initial, 'valid Open still succeeds after rejected files');
  }
  // A prepared candidate can still fail validation in an inactive Composition.
  // Rejecting it must retain the current variable-font bridge, not just JSON.
  const bytes = await fs.readFile(
    new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
  );
  const asset = makeVariableFontAsset(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    'Roboto.ttf',
  );
  const variable = blankDocument(),
    text = node('Text', 400, 40);
  variable.fontAssets = { [asset.id]: asset.asset };
  text.font = asset.id;
  text.text = 'AV';
  text.variation = { version: 1, fontId: asset.id, axes: {} };
  variable.layers[0].nodes.push(text);
  variable.layers[0].nodes[0].params.path.source = text.id;
  const variableText = serialize(variable);
  const selectText = () =>
    page.locator(`.graph .node[data-id="${text.id}"] .node-head strong`).click();
  const editText = async (value) => {
    const input = page.locator(`.inspector-body [data-text="${text.id}"]`);
    await input.fill(value);
    await input.press('Tab');
    await page.waitForFunction(
      ({ id, value }) =>
        window.nect.document.layers[0].nodes.find((n) => n.id === id).text === value ||
        document
          .querySelector('.status .message')
          ?.textContent.startsWith('Variable font edit rejected;'),
      { id: text.id, value },
    );
    assert.equal(
      await page.evaluate(
        (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).text,
        text.id,
      ),
      value,
    );
    assert.equal(await page.locator('.status .message').textContent(), 'Text updated');
  };
  await open(variableText);
  await selectText();
  await editText('AVX');
  await activateCommand(page, 'undo');
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).text === 'AV',
    text.id,
  );
  await selectText();
  const baseline = await state(),
    svg = await page.evaluate(() => window.nect.export());
  assert.equal(baseline.file.dirty, false);
  assert.equal(baseline.redoDisabled, false);
  assert.match(svg, /<path/);
  const inactive = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  inactive.id = 'invalid-variable-inactive';
  inactive.layers[0].nodes.push({ ...node('Rectangle', 0, 0), type: 'Not a Nect node' });
  const invalid = structuredClone(baseline.project);
  invalid.compositions.push(inactive);
  invalid.compositions[0].layers[0].nodes.find((n) => n.id === text.id).text = 'AW';
  await rejectFile(
    'open',
    JSON.stringify(invalid),
    'invalid-variable-project.json',
    /Invalid node/,
  );
  assert.deepEqual(
    await state(),
    baseline,
    'rejected prepared candidate preserves the active project',
  );
  assert.equal(
    await page.evaluate(() => window.nect.export()),
    svg,
    'the previous prepared fonts remain usable',
  );
  await refresh();
  assert.deepEqual(await state(), baseline);
  // Edit the retained document before any Open/reset can replace a broken bank.
  await editText('AVZ');
  assert.notEqual(await page.evaluate(() => window.nect.export()), svg);
  await activateCommand(page, 'undo');
  await page.waitForFunction(
    (id) => window.nect.document.layers[0].nodes.find((n) => n.id === id).text === 'AV',
    text.id,
  );
  assert.deepEqual(await project(), baseline.project);
  await open(variableText);
  assert.deepEqual(await project(), baseline.project);
  assert.equal(await page.evaluate(() => window.nect.export()), svg);
  const followOn = structuredClone(baseline.project);
  followOn.compositions[0].layers[0].nodes.find((n) => n.id === text.id).text = 'AW';
  await open(JSON.stringify(followOn));
  assert.deepEqual(await project(), followOn);
  assert.notEqual(await page.evaluate(() => window.nect.export()), svg);
  await selectText();
  await editText('AWX');
  console.log(
    'PASS project file atomicity: 17 rejected Open/import cases; exact state/history/selection and subsequent valid Open, including prepared variable fonts',
  );
}
