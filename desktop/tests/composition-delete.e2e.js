import assert from 'node:assert/strict';
import { mkdir, readFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { blankDocument, node } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { activateCommand, revealCommandMenu } from './command-menu-actions.js';
import { ensureWorkspacePanel } from './helpers/workspace-panels.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

const a = blankDocument(),
  b = blankDocument();
a.schemaVersion = 54;
a.colorSlots = [{ id: 'brand', name: 'Brand', value: { hex: '#808080', alpha: 1 } }];
a.layers[0].id = 'art-a';
b.layers[0].id = 'art-b';
const scalar = node('Value'),
  point = node('Value'),
  fill = node('Fill'),
  driver = node('Value'),
  color = node('Color'),
  curve = node('Curve');
scalar.id = 'scalar-sink';
point.id = 'point-sink';
fill.id = 'color-sink';
scalar.position = [40, 120];
point.position = [350, 120];
fill.position = [660, 120];
a.layers[0].nodes[0].position = [970, 120];
driver.id = 'driver';
driver.params.value.value = 7;
color.id = 'source-color';
color.params.value.value = { hex: '#2468ac', alpha: 0.25 };
curve.id = 'source-curve';
curve.points[0].id = 'point';
curve.points[0].handleA.value = [3, 4];
a.layers[0].nodes.push(scalar, point, fill);
b.layers[0].nodes.push(driver, color, curve);
const project = migrateLegacyDocumentToCompositionProject(a);
project.compositions[0].id = 'first';
project.compositions[0].name = 'Controls';
project.compositions.push({
  id: 'second',
  name: 'Source · 日本語',
  width: 720,
  height: 540,
  layers: b.layers,
  editor: b.editor,
});
const owner = createCompositionSessionOwner(project);
for (const [nodeId, key, expression] of [
  ['scalar-sink', 'value', 'ref(composition, "second", "art-b", "driver", "value")'],
  [
    'point-sink',
    'value',
    'ref(componentComposition, "second", "art-b", "source-curve", "point.handleA", "length")',
  ],
  ['color-sink', 'color', 'ref(colorComposition, "second", "art-b", "source-color", "value")'],
])
  owner.dispatch({
    ...owner.state,
    type: 'set-parameter-expression',
    payload: { layerId: 'art-a', nodeId, key, expression },
  });
const serialized = serializeCompositionProject(owner.projectSnapshot());

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage({
  viewport: { width: 1600, height: 1000 },
  acceptDownloads: true,
});
const errors = [];
page.on('pageerror', (error) => errors.push(error.message));
const base = process.env.BASE_URL || 'http://127.0.0.1:5173';
const output = 'test-results/composition-delete';
const snapshot = () => page.evaluate(() => window.nect.projectSnapshot());
const state = () =>
  page.evaluate(() => ({
    command: window.nect.commandState,
    revision: window.nect.projectRevision,
  }));
const dialog = () => page.locator('.composition-delete-dialog[open]');
const values = (target = page) =>
  target.evaluate(() => {
    const result = window.nect.evaluate();
    return [
      ['scalar-sink', 'value'],
      ['point-sink', 'value'],
      ['color-sink', 'color'],
    ].map(([id, key]) => {
      try {
        return result.parameterValue(id, key);
      } catch (error) {
        return error.message;
      }
    });
  });
const stagePoint = async (position) => {
  const screen = await page.locator('.artboard').evaluate((svg, p) => {
    const transformed = new DOMPoint(...p).matrixTransform(svg.getScreenCTM());
    return [transformed.x, transformed.y];
  }, position);
  await page.mouse.click(...screen);
};

try {
  await mkdir(output, { recursive: true });
  await page.goto(base);
  await page.waitForFunction(() => typeof window.nect?.load === 'function');
  const onlyMenu = await revealCommandMenu(page, 'Project');
  assert.equal(await onlyMenu.locator('[data-action="delete-composition"]').isDisabled(), true);
  await page.evaluate((text) => window.nect.load(text), serialized);
  assert.deepEqual(await values(), [7, 5, { hex: '#2468ac', alpha: 0.25 }]);
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await page.locator('[data-tool="Circle"]').click();
  const circle = (await snapshot()).compositions[1].layers.at(-1).id;
  await page.locator('[data-tool="Rectangle"]').click();
  const rectangle = (await snapshot()).compositions[1].layers.at(-1).id;
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${circle}"]`).click();
  await page.locator('[data-active-composition="first"][role="tab"]').click();
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 721 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const before = await snapshot(),
    beforeState = await state();
  assert.equal(
    await page.locator(`[data-select-layer="${circle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  const malformed = await page.evaluate(() => {
    const fields = {
      compositionId: 'second',
      expectedName: 'Source · 日本語',
      expectedProjectRevision: window.nect.projectRevision,
    };
    let getterCalls = 0;
    const array = Object.assign([], fields);
    Object.defineProperty(array, 'compositionId', {
      get() {
        getterCalls++;
        return 'second';
      },
    });
    const errors = [array, Object.assign(() => {}, fields), null].map((payload) => {
      let error = '';
      try {
        window.nect.dispatch({ ...window.nect.commandState, type: 'delete-composition', payload });
      } catch (cause) {
        error = cause.message;
      }
      return error;
    });
    return { errors, getterCalls };
  });
  for (const error of malformed.errors) assert.match(error, /Invalid command payload/);
  assert.equal(malformed.getterCalls, 0);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().locator('[data-composition-name]').textContent(), 'Source · 日本語');
  assert.equal(await dialog().locator('[data-composition-references]').isVisible(), true);
  assert.equal(
    await dialog()
      .locator('[data-cancel]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  const dialogApiError = await page.evaluate(() => {
    try {
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'delete-composition',
        payload: {
          compositionId: 'second',
          expectedName: 'Source · 日本語',
          expectedProjectRevision: window.nect.projectRevision,
        },
      });
    } catch (error) {
      return error.message;
    }
    return '';
  });
  assert.match(dialogApiError, /finish|active/i);
  assert.equal(await dialog().count(), 1);
  assert.deepEqual(await snapshot(), before);
  await page.keyboard.press('Escape');
  await dialog().waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await activateCommand(page, 'delete-composition');
  await page.keyboard.press('Enter');
  await dialog().waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  await activateCommand(page, 'delete-composition', { keyboard: true });
  assert.equal(
    await dialog()
      .locator('[data-cancel]')
      .evaluate((el) => document.activeElement === el),
    true,
  );
  await page.keyboard.press('Escape');
  await dialog().waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), beforeState);

  await activateCommand(page, 'delete-composition');
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 722 },
    }),
  );
  const changedElsewhere = await snapshot(),
    changedState = await state();
  await dialog().locator('button[type="submit"]').click();
  assert.match(await dialog().locator('[role="alert"]').innerText(), /Project changed/);
  assert.deepEqual(await snapshot(), changedElsewhere);
  assert.deepEqual(await state(), changedState);
  await dialog().locator('[data-cancel]').click();
  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before);
  await page.locator(`[data-select-layer="${circle}"]`).click();

  await page.setViewportSize({ width: 1000, height: 650 });
  await activateCommand(page, 'delete-composition');
  await page.screenshot({ path: `${output}/minimum-confirmation.png` });
  await dialog().locator('button[type="submit"]').click();
  await dialog().waitFor({ state: 'detached' });
  const deleted = await snapshot();
  assert.equal(deleted.compositions.length, 1);
  assert.deepEqual(deleted.compositions[0], before.compositions[0]);
  assert.deepEqual(deleted.colorSlots, before.colorSlots);
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'first');
  for (const error of await values()) assert.match(error, /missing/i);
  const save = page.waitForEvent('download');
  await activateCommand(page, 'save');
  await (await save).saveAs(`${output}/deleted.json`);
  assert.deepEqual(JSON.parse(await readFile(`${output}/deleted.json`, 'utf8')), deleted);

  await page.locator('[data-action="undo"]').click();
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await values(), [7, 5, { hex: '#2468ac', alpha: 0.25 }]);
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${circle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.locator(`[data-select-layer="${rectangle}"]`).click();
  await page.locator('[data-action="redo"]').click();
  assert.deepEqual(await snapshot(), deleted);
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'first');
  await page.locator('[data-action="undo"]').click();
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${rectangle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  await page.waitForFunction(() => !document.querySelector('[data-action="redo"]')?.disabled);
  const width = page.locator('[data-dimension="width"]');
  await width.fill('730');
  await width.press('Tab');
  await page.waitForFunction(() => window.nect.document.width === 730);
  // The Session commits before the scheduled render updates the toolbar.
  await page.waitForFunction(() => document.querySelector('[data-action="redo"]')?.disabled);
  assert.equal(await page.locator('[data-action="redo"]').isDisabled(), true);
  const branched = await snapshot(),
    branchedState = await state();
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }));
  assert.deepEqual(await snapshot(), branched);
  assert.deepEqual(await state(), branchedState);
  await page.locator('[data-action="undo"]').click();
  assert.equal((await snapshot()).compositions[1].width, 720);
  assert.equal((await snapshot()).compositions.length, 2);

  // Machine dispatch goes through the same deletion adapter and project History.
  await page.evaluate((text) => window.nect.load(text), JSON.stringify(before));
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await page.locator(`[data-select-layer="${rectangle}"]`).click();
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'delete-composition',
      payload: {
        compositionId: 'second',
        expectedName: 'Source · 日本語',
        expectedProjectRevision: window.nect.projectRevision,
      },
    }),
  );
  assert.deepEqual(await snapshot(), deleted);
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'first');
  const undoResult = await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }),
  );
  assert.equal(undoResult.changed, true);
  assert.deepEqual(await snapshot(), before);
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${rectangle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  const undoState = await state();
  const staleRedo = await page.evaluate(() => {
    try {
      window.nect.dispatch({ ...window.nect.commandState, expectedRevision: -1, type: 'redo' });
    } catch (error) {
      return error.message;
    }
    return '';
  });
  assert.match(staleRedo, /STALE/);
  assert.deepEqual(await snapshot(), before);
  assert.deepEqual(await state(), undoState);
  assert.equal(
    await page.locator(`[data-select-layer="${rectangle}"]`).getAttribute('aria-pressed'),
    'true',
  );
  const redoResult = await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'redo' }),
  );
  assert.equal(redoResult.changed, true);
  assert.deepEqual(await snapshot(), deleted);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${rectangle}"]`).getAttribute('aria-pressed'),
    'true',
  );

  await page.evaluate((text) => window.nect.load(text), JSON.stringify(before));
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await ensureWorkspacePanel(page, 'layers');
  await page.locator(`[data-select-layer="${circle}"]`).click();
  await page.locator('[data-active-composition="first"][role="tab"]').click();
  await page.locator('.graph .node[data-id="scalar-sink"] .node-head strong').click();
  await page.evaluate(() =>
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'delete-composition',
      payload: {
        compositionId: 'second',
        expectedName: 'Source · 日本語',
        expectedProjectRevision: window.nect.projectRevision,
      },
    }),
  );
  assert.equal(await page.evaluate(() => window.nect.activeCompositionId), 'first');
  const inactiveUndo = await page.evaluate(() =>
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }),
  );
  assert.equal(inactiveUndo.changed, true);
  assert.deepEqual(await snapshot(), before);
  assert.equal(
    await page
      .locator('.graph .node[data-id="scalar-sink"]')
      .evaluate((el) => el.classList.contains('selected')),
    true,
  );
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  assert.equal(
    await page.locator(`[data-select-layer="${circle}"]`).getAttribute('aria-pressed'),
    'true',
  );

  // A payload accessor may replace the project; an old owner must not alter the new view.
  const reentry = await page.evaluate(
    ({ original, rectangle }) => {
      const oldState = window.nect.commandState,
        revision = window.nect.projectRevision;
      let nested,
        nestedSelection,
        error = '';
      try {
        window.nect.dispatch({
          ...oldState,
          type: 'delete-composition',
          payload: {
            compositionId: 'second',
            expectedProjectRevision: revision,
            get expectedName() {
              window.nect.load(JSON.stringify(original));
              document.querySelector('[data-active-composition="second"][role="tab"]').click();
              document.querySelector(`[data-select-layer="${rectangle}"]`).click();
              nested = window.nect.projectSnapshot();
              nestedSelection = document
                .querySelector(`[data-select-layer="${rectangle}"]`)
                .getAttribute('aria-pressed');
              return 'Source · 日本語';
            },
          },
        });
      } catch (cause) {
        error = cause.message;
      }
      return {
        error,
        nested,
        nestedSelection,
        after: window.nect.projectSnapshot(),
        afterSelection: document
          .querySelector(`[data-select-layer="${rectangle}"]`)
          .getAttribute('aria-pressed'),
      };
    },
    { original: before, rectangle },
  );
  assert.match(reentry.error, /STALE/);
  assert.deepEqual(reentry.after, reentry.nested);
  assert.equal(reentry.nestedSelection, 'true');
  assert.equal(reentry.afterSelection, 'true');
  const typeReentry = await page.evaluate(
    ({ original, rectangle }) => {
      const oldState = window.nect.commandState,
        revision = window.nect.projectRevision;
      let nested,
        nestedSelection,
        error = '';
      try {
        window.nect.dispatch({
          ...oldState,
          get type() {
            window.nect.load(JSON.stringify(original));
            document.querySelector('[data-active-composition="second"][role="tab"]').click();
            document.querySelector(`[data-select-layer="${rectangle}"]`).click();
            nested = window.nect.projectSnapshot();
            nestedSelection = document
              .querySelector(`[data-select-layer="${rectangle}"]`)
              .getAttribute('aria-pressed');
            return 'delete-composition';
          },
          payload: {
            compositionId: 'second',
            expectedName: 'Source · 日本語',
            expectedProjectRevision: revision,
          },
        });
      } catch (cause) {
        error = cause.message;
      }
      return {
        error,
        nested,
        nestedSelection,
        after: window.nect.projectSnapshot(),
        afterSelection: document
          .querySelector(`[data-select-layer="${rectangle}"]`)
          .getAttribute('aria-pressed'),
      };
    },
    { original: before, rectangle },
  );
  assert.match(typeReentry.error, /STALE/);
  assert.deepEqual(typeReentry.after, typeReentry.nested);
  assert.equal(typeReentry.nestedSelection, 'true');
  assert.equal(typeReentry.afterSelection, 'true');

  // Pen and live Shared Colors drafts block the action before blur or commit.
  await page.evaluate((text) => window.nect.load(text), JSON.stringify(before));
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 721 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const draftBefore = await snapshot(),
    draftState = await state();
  await page.locator('[data-tool="pen"]').click();
  await stagePoint([100, 100]);
  await stagePoint([220, 180]);
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 0);
  assert.equal(await page.locator('[data-pen-draft]').count(), 1);
  assert.deepEqual(await snapshot(), draftBefore);
  assert.deepEqual(await state(), draftState);
  await page.locator('[data-pen-cancel]').click();
  await ensureWorkspacePanel(page, 'colors');
  if (!(await page.locator('[data-color-advanced]').evaluate((el) => el.open)))
    await page.locator('[data-color-advanced] summary').click();
  const hex = page.locator('[data-shared-color-row="brand"] [data-shared-color-edit="hex"]');
  await hex.focus();
  await hex.evaluate((el) => {
    el.value = '#445566';
    el.dispatchEvent(new Event('input', { bubbles: true }));
  });
  const draftApiError = await page.evaluate(() => {
    try {
      window.nect.dispatch({
        ...window.nect.commandState,
        type: 'delete-composition',
        payload: {
          compositionId: 'second',
          expectedName: 'Source · 日本語',
          expectedProjectRevision: window.nect.projectRevision,
        },
      });
    } catch (error) {
      return error.message;
    }
    return '';
  });
  assert.match(draftApiError, /finish|active/i);
  assert.equal(await hex.inputValue(), '#445566');
  assert.deepEqual(await snapshot(), draftBefore);
  assert.deepEqual(await state(), draftState);
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 0);
  assert.equal(await hex.inputValue(), '#445566');
  assert.equal(await hex.evaluate((el) => document.activeElement === el), true);
  assert.deepEqual(await snapshot(), draftBefore);
  assert.deepEqual(await state(), draftState);
  await hex.press('Escape');
  const colorName = page.locator('[data-shared-color-row="brand"] [data-shared-color-edit="name"]');
  await colorName.fill('編集中');
  await colorName.evaluate((el) =>
    el.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true })),
  );
  await page.keyboard.press('Tab');
  assert.equal(await colorName.evaluate((el) => document.activeElement === el), true);
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 0);
  assert.equal(await colorName.inputValue(), '編集中');
  assert.equal(await colorName.evaluate((el) => document.activeElement === el), true);
  assert.deepEqual(await snapshot(), draftBefore);
  assert.deepEqual(await state(), draftState);
  await colorName.evaluate((el) => {
    el.value = 'Brand';
    el.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: 'Brand' }));
    el.dispatchEvent(new InputEvent('input', { bubbles: true, data: 'Brand' }));
  });
  await colorName.press('Escape');
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('[data-action="redo"]').click();
  assert.equal((await snapshot()).compositions[1].width, 721);

  // Long duplicate names remain identifiable by the exact stable ID.
  const sameNames = structuredClone(owner.projectSnapshot());
  const longName = `Variant ${'字'.repeat(500)}`;
  for (const item of sameNames.compositions) item.name = longName;
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(sameNames));
  await page.locator('[data-active-composition="second"][role="tab"]').click();
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 721 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  const sameBefore = await snapshot(),
    sameState = await state();
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().locator('[data-composition-name]').textContent(), longName);
  assert.equal(await dialog().locator('[data-composition-identity]').textContent(), 'ID · second');
  assert.equal(await dialog().locator('[data-composition-identity]').isVisible(), true);
  const fits = await dialog().evaluate((el) => {
    const rect = el.getBoundingClientRect();
    return (
      rect.left >= 0 && rect.top >= 0 && rect.right <= innerWidth && rect.bottom <= innerHeight
    );
  });
  assert.equal(fits, true);
  await page.screenshot({ path: `${output}/long-duplicate-name.png` });
  await page.keyboard.press('Escape');
  assert.deepEqual(await snapshot(), sameBefore);
  assert.deepEqual(await state(), sameState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);

  // A point move stays a draft after blur and must not disappear on deletion.
  const pointFixture = compoundFixture([8, 5, 7, 4]);
  pointFixture.doc.schemaVersion = 42;
  pointFixture.path.collapsed = true;
  const pointProject = migrateLegacyDocumentToCompositionProject(pointFixture.doc),
    pointOther = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  pointProject.compositions[0].id = 'point-first';
  pointOther.id = 'point-second';
  pointProject.compositions.push(pointOther);
  await page.setViewportSize({ width: 1600, height: 1000 });
  await page.evaluate((text) => window.nect.load(text), serializeCompositionProject(pointProject));
  await page.evaluate(() => {
    window.nect.dispatch({
      ...window.nect.commandState,
      type: 'set-document-settings',
      payload: { key: 'width', value: 721 },
    });
    window.nect.dispatch({ ...window.nect.commandState, type: 'undo' });
  });
  await ensureWorkspacePanel(page, 'inspector');
  await page.locator(`.graph .node[data-id="${pointFixture.path.id}"] .node-head strong`).click();
  await page
    .locator(
      `.canvas-stage .point-hit[data-point="${pointFixture.path.contours[0].points[1]}"]:not([data-handle])`,
    )
    .click();
  await page
    .locator(
      `.canvas-stage .point-hit[data-point="${pointFixture.path.contours[3].points[2]}"]:not([data-handle])`,
    )
    .click({ modifiers: ['Shift'] });
  const pointDelta = page.locator('.inspector-body [data-point-delta-axis="0"]');
  await pointDelta.fill('12.5');
  await pointDelta.press('Tab');
  await page.keyboard.press('Tab');
  const pointBefore = await snapshot(),
    pointState = await state();
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 0);
  assert.equal(await pointDelta.inputValue(), '12.5');
  assert.deepEqual(await snapshot(), pointBefore);
  assert.deepEqual(await state(), pointState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  await page.locator('.inspector-body [data-cancel-point-delta]').click();
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 1);
  await dialog().locator('[data-cancel]').click();
  await page.locator('.composition-delete-dialog').waitFor({ state: 'detached' });
  await pointDelta.fill('9.75');
  await pointDelta.press('Tab');
  await page.keyboard.press('Tab');
  await page.locator('.canvas-stage').click({ position: { x: 3, y: 3 } });
  assert.equal(await pointDelta.count(), 0);
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 1);
  assert.deepEqual(await snapshot(), pointBefore);
  assert.deepEqual(await state(), pointState);
  await dialog().locator('[data-cancel]').click();
  await page.locator('.composition-delete-dialog').waitFor({ state: 'detached' });
  await page.locator(`.graph .node[data-id="${pointFixture.path.id}"] .node-head strong`).click();
  await page
    .locator(
      `.canvas-stage .point-hit[data-point="${pointFixture.path.contours[0].points[1]}"]:not([data-handle])`,
    )
    .click();
  await page
    .locator(
      `.canvas-stage .point-hit[data-point="${pointFixture.path.contours[3].points[2]}"]:not([data-handle])`,
    )
    .click({ modifiers: ['Shift'] });
  assert.equal(await pointDelta.inputValue(), '0');
  await pointDelta.fill('12.5');
  await pointDelta.press('Tab');
  await page.keyboard.press('Tab');
  await page
    .locator(`.graph .node[data-id="${pointFixture.fill.id}"] .node-body`)
    .click({ position: { x: 4, y: 4 } });
  assert.equal(await pointDelta.count(), 0);
  await activateCommand(page, 'rename-composition');
  assert.equal(await page.locator('.composition-rename-dialog[open]').count(), 1);
  await page.locator('.composition-rename-dialog [data-cancel]').click();
  await page.locator('.composition-rename-dialog').waitFor({ state: 'detached' });
  await activateCommand(page, 'delete-composition');
  assert.equal(await dialog().count(), 1);
  await dialog().locator('[data-cancel]').click();
  await page.locator('.composition-delete-dialog').waitFor({ state: 'detached' });
  assert.deepEqual(await snapshot(), pointBefore);
  assert.deepEqual(await state(), pointState);
  assert.equal(await page.locator('[data-action="redo"]').isEnabled(), true);
  const pointDelete = await page.evaluate(() => {
    const project = window.nect.projectSnapshot();
    return window.nect.dispatch({
      ...window.nect.commandState,
      type: 'delete-composition',
      payload: {
        compositionId: project.compositions[0].id,
        expectedName: project.compositions[0].name,
        expectedProjectRevision: window.nect.projectRevision,
      },
    });
  });
  assert.equal(pointDelete.changed, true);
  await page.evaluate(() => window.nect.dispatch({ ...window.nect.commandState, type: 'undo' }));
  assert.deepEqual(await snapshot(), pointBefore);

  const reopened = await browser.newPage({ viewport: { width: 1000, height: 650 } });
  reopened.on('pageerror', (error) => errors.push(error.message));
  await reopened.goto(base);
  await reopened.waitForFunction(() => typeof window.nect?.load === 'function');
  const chooser = reopened.waitForEvent('filechooser');
  await activateCommand(reopened, 'open');
  const discard = reopened.locator('dialog button[value="discard"]');
  if (await discard.isVisible()) await discard.click();
  await (await chooser).setFiles(`${output}/deleted.json`);
  await reopened.waitForFunction(() => window.nect.document.schemaVersion === 65);
  assert.deepEqual(await reopened.evaluate(() => window.nect.projectSnapshot()), deleted);
  for (const error of await values(reopened)) assert.match(error, /missing/i);
  assert.deepEqual(errors, []);
  await reopened.close();
  console.log('PASS: Composition deletion, dependency diagnostics, draft protection and History.');
} finally {
  await browser.close();
}
