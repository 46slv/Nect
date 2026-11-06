import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Worker } from 'node:worker_threads';
import { Window } from 'happy-dom';
import { blankDocument, node, createVariableDocumentPreparer } from '../src/core.js';
import { makeVariableFontAsset } from '../src/fonts.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { openDOMPanel } from './helpers/dom-workspace-controls.js';

test('rejected variable Open retains live fonts and retires only abandoned/old document resources', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return this;
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({ inverse: () => ({}) });
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    },
  });
  const records = [],
    terminated = [];
  let nextPreparationGate = null;
  const gateNextPreparation = () => {
    let release, ready;
    const gate = {
      ready: new Promise((resolve) => (ready = resolve)),
      wait: new Promise((resolve) => (release = resolve)),
      release: () => release(),
      markReady: () => ready(),
    };
    nextPreparationGate = gate;
    return gate;
  };
  win.makeVariablePreparerForTest = () => {
    const bridge = createVariableDocumentPreparer({
      workerFactory: () => {
        const worker = new Worker(new URL('./variable-font-worker-node.mjs', import.meta.url));
        const transport = {
          postMessage: (data) => worker.postMessage(data),
          terminate: () => {
            const ended = worker.terminate();
            terminated.push(ended);
            return ended;
          },
        };
        worker.on('message', (data) => transport.onmessage?.({ data }));
        worker.on('error', (error) => transport.onerror?.(error));
        return transport;
      },
    });
    const record = { bridge, disposals: 0 },
      dispose = bridge.dispose.bind(bridge);
    const prepareDocument = bridge.prepareDocument.bind(bridge);
    bridge.prepareDocument = async (...args) => {
      const gate = nextPreparationGate;
      nextPreparationGate = null;
      const ticket = await prepareDocument(...args);
      if (gate) {
        gate.markReady();
        await gate.wait;
      }
      return ticket;
    };
    bridge.dispose = () => {
      record.disposals++;
      dispose();
    };
    records.push(record);
    return bridge;
  };
  t.after(async () => {
    for (const record of records) if (!record.disposals) record.bridge.dispose();
    await Promise.allSettled(terminated);
    await win.happyDOM.abort();
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    )
    .replaceAll('createVariableDocumentPreparer()', 'window.makeVariablePreparerForTest()');
  // Test-local adapters observe real main.js ownership and use its actual async
  // Open/edit paths with the existing Node worker transport. No production hook.
  source += `
    window.openForTest = async project => { await prepareOpenDocument(project, {saved:true}); render(true); };
    window.editForTest = (type, payload = {}) => preparedChange(type, payload, 'Text updated');
    window.liveBridgeForTest = () => variableBridge;
    window.variableBusyForTest = () => !!variableAction;
    window.textDraftForTest = id => session.textDraftSnapshot(id);
    window.openColorUsesForTest = id => openDirectColorUses(id, null);
    window.openStateForTest = () => ({project:session.projectSnapshot(), command:session.state,
      file:window.nect.fileState, past:history.past, future:history.future,
      historyState:session.historyState,
      activeLayer, selected, selection:[...selection], artworkTarget,
      artworkSelection:[...artworkSelection], pointSelection:{nodeId:pointSelection.nodeId, ids:[...pointSelection.ids]}});
    while (!bundledFont) await new Promise(resolve => setTimeout(resolve,10));
  `;
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const bytes = await fs.readFile(
    new URL('./fixtures/variable-font/Roboto-variable.ttf', import.meta.url),
  );
  const asset = makeVariableFontAsset(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
    'Roboto.ttf',
  );
  const document = blankDocument(),
    text = node('Text', 400, 40);
  document.fontAssets = { [asset.id]: asset.asset };
  Object.assign(text, {
    font: asset.id,
    text: 'AV',
    variation: { version: 1, fontId: asset.id, axes: {} },
  });
  document.layers[0].nodes.push(text);
  document.layers[0].nodes[0].params.path.source = text.id;
  await win.openForTest(migrateLegacyDocumentToCompositionProject(document));
  const old = records[0];
  const edit = async (value) => {
    assert.ok(await win.editForTest('set-text-content', { nodeId: text.id, text: value }));
    assert.equal(win.nect.document.layers[0].nodes.find((n) => n.id === text.id).text, value);
  };
  const head = win.document.querySelector(`.node[data-id="${text.id}"] .node-head strong`);
  head.dispatchEvent(
    new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 1 }),
  );
  win.document.dispatchEvent(
    new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 1 }),
  );
  await edit('AVX');
  await edit('AVY');
  assert.ok(await win.editForTest('undo'));
  const before = win.openStateForTest(),
    svg = win.nect.export();
  assert.equal(before.file.dirty, true);
  assert.equal(before.past.length, 1);
  assert.equal(before.future.length, 1);
  assert.equal(before.selected, text.id);
  const invalid = structuredClone(before.project);
  invalid.compositions[0].layers[0].nodes.find((n) => n.id === text.id).text = 'AW';
  const later = migrateLegacyDocumentToCompositionProject(blankDocument()).compositions[0];
  later.id = 'invalid-later';
  later.layers[0].nodes[0].type = 'Not a Nect node';
  invalid.compositions.push(later);
  await assert.rejects(win.openForTest(invalid), /Invalid node/);
  assert.deepEqual(win.openStateForTest(), before);
  assert.equal(win.nect.export(), svg);
  assert.equal(win.liveBridgeForTest(), old.bridge, 'rejection retains the original live bridge');
  assert.equal(old.disposals, 0);
  assert.equal(records[1].disposals, 1);
  assert.deepEqual(records[1].bridge.fonts(), {});
  assert.equal(records[1].bridge.stats().active.runs, 0);
  // No Open/reset between rejection and this edit of the retained document.
  await edit('AVZ');
  assert.notEqual(win.nect.export(), svg);
  assert.ok(await win.editForTest('undo'));
  assert.deepEqual(win.nect.projectSnapshot(), before.project);
  const valid = structuredClone(invalid);
  valid.compositions.pop();
  await win.openForTest(valid);
  assert.equal(old.disposals, 1, 'successful replacement retires the old bridge once');
  assert.equal(records[1].disposals, 1, 'abandoned candidate is not retired twice');
  assert.equal(win.liveBridgeForTest(), records[2].bridge);
  assert.equal(records[2].disposals, 0);
  assert.equal(win.nect.fileState.dirty, false);
  assert.equal(win.openStateForTest().past.length, 0);
  assert.equal(win.openStateForTest().future.length, 0);
  await edit('AWX');
  await win.openForTest(migrateLegacyDocumentToCompositionProject(blankDocument()));
  assert.equal(win.liveBridgeForTest(), null);
  assert.equal(records[2].disposals, 1, 'static replacement releases the last variable bank');

  await t.test(
    'prepared rename Undo keeps a newer selection made while fonts are loading',
    async () => {
      const document = blankDocument(),
        texts = Array.from({ length: 8 }, (_, i) => {
          const text = node('Text', 100 + i * 20, 40);
          Object.assign(text, {
            font: asset.id,
            text: String.fromCharCode(65 + i),
            variation: { version: 1, fontId: asset.id, axes: {} },
          });
          return text;
        });
      document.fontAssets = { [asset.id]: asset.asset };
      document.layers[0].nodes.push(...texts);
      document.layers[0].nodes[0].params.path.source = texts[0].id;
      await win.openForTest(migrateLegacyDocumentToCompositionProject(document));
      assert.equal(win.liveBridgeForTest().stats().active.runs, 8);
      const root = win.document,
        click = (selector) => {
          const element = root.querySelector(selector);
          assert.ok(element, selector);
          element.click();
        },
        waitFor = async (predicate) => {
          const deadline = Date.now() + 15000;
          while (!predicate()) {
            if (Date.now() >= deadline) throw Error('Variable-font test timed out');
            await new Promise((resolve) => setTimeout(resolve, 10));
          }
        };
      click('[data-tool="Circle"]');
      const circle = win.nect.document.layers.at(-1).id;
      click('[data-tool="Rectangle"]');
      const rectangle = win.nect.document.layers.at(-1).id;
      openDOMPanel(root, 'layers');
      click(`[data-select-layer="${circle}"]`);
      const originalName = win.nect.compositions[0].name;
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'rename-composition',
        payload: {
          compositionId: win.nect.activeCompositionId,
          name: 'Variable rename',
          expectedName: originalName,
          expectedProjectRevision: win.nect.projectRevision,
        },
      });
      click(`[data-select-layer="${document.layers[0].id}"]`);
      const title = root.querySelector(`.node[data-id="${texts[0].id}"] .node-head strong`);
      title.dispatchEvent(
        new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 1 }),
      );
      root.dispatchEvent(
        new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 1 }),
      );
      openDOMPanel(root, 'inspector');
      const axis = root.querySelector('.inspector-body [data-variable-axis="wght"]');
      assert.ok(axis);
      axis.value = '500';
      axis.dispatchEvent(new win.Event('input', { bubbles: true }));
      await waitFor(() => !!win.liveBridgeForTest().stats().candidate);
      axis.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      await waitFor(() => !win.variableBusyForTest());
      assert.equal(win.nect.compositions[0].name, 'Variable rename');

      openDOMPanel(root, 'layers');
      click(`[data-select-layer="${circle}"]`);
      const revision = win.nect.projectRevision;
      click('[data-action="undo"]');
      assert.equal(win.variableBusyForTest(), true, 'Undo is still preparing');
      click(`[data-select-layer="${rectangle}"]`);
      assert.equal(win.openStateForTest().activeLayer, rectangle);
      await waitFor(
        () => !win.variableBusyForTest() && win.nect.compositions[0].name === originalName,
      );
      assert.equal(win.openStateForTest().activeLayer, rectangle);
      assert.equal(win.nect.projectRevision, revision + 1);
      click('[data-action="redo"]');
      await waitFor(
        () => !win.variableBusyForTest() && win.nect.compositions[0].name === 'Variable rename',
      );
      assert.equal(win.openStateForTest().activeLayer, rectangle);
      click(`[data-select-layer="${circle}"]`);
      click('[data-action="undo"]');
      await waitFor(
        () => !win.variableBusyForTest() && win.nect.compositions[0].name === originalName,
      );
      assert.equal(win.openStateForTest().activeLayer, circle);
    },
  );

  await t.test('inline name and focus survive a delayed Composition switch', async () => {
    const first = blankDocument(),
      second = blankDocument(),
      text = node('Text');
    Object.assign(text, {
      font: asset.id,
      text: 'AV',
      variation: { version: 1, fontId: asset.id, axes: {} },
    });
    second.layers[0].nodes.push(text);
    first.fontAssets = { [asset.id]: asset.asset };
    const project = migrateLegacyDocumentToCompositionProject(first);
    const target = migrateLegacyDocumentToCompositionProject(second).compositions[0];
    target.id = 'name-race-second';
    project.compositions.push(target);
    await win.openForTest(project);
    const before = win.openStateForTest();
    const gate = gateNextPreparation();
    win.document.querySelector('[role="tab"][data-active-composition="name-race-second"]').click();
    await gate.ready;
    const title = win.document.querySelector(`[data-rename-node="${first.layers[0].nodes[0].id}"]`);
    title.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'F2', bubbles: true }));
    const input = win.document.querySelector('[data-rename-name]');
    input.value = 'AVX';
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
    input.setSelectionRange(2, 2);
    gate.release();
    while (win.variableBusyForTest()) await new Promise((resolve) => setTimeout(resolve, 10));
    await new Promise((resolve) => setTimeout(resolve, 20));
    assert.equal(win.nect.activeCompositionId, project.compositions[0].id);
    assert.equal(win.document.querySelector('[data-rename-name]'), input);
    assert.equal(input.value, 'AVX');
    assert.equal(win.document.activeElement, input);
    assert.equal(input.selectionStart, 2);
    assert.deepEqual(win.openStateForTest().project, before.project);
    assert.deepEqual(win.openStateForTest().file, before.file);
    assert.deepEqual(win.openStateForTest().historyState, before.historyState);
    input.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));

    const again = gateNextPreparation();
    win.document.querySelector('[role="tab"][data-active-composition="name-race-second"]').click();
    await again.ready;
    const otherInput = win.document.createElement('textarea');
    otherInput.value = 'Uncommitted';
    win.document.body.append(otherInput);
    otherInput.focus();
    otherInput.setSelectionRange(3, 3);
    otherInput.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
    again.release();
    while (win.variableBusyForTest()) await new Promise((resolve) => setTimeout(resolve, 10));
    await new Promise((resolve) => setTimeout(resolve, 20));
    assert.equal(win.nect.activeCompositionId, project.compositions[0].id);
    assert.equal(win.document.activeElement, otherInput);
    assert.equal(otherInput.selectionStart, 3);
    assert.equal(otherInput.value, 'Uncommitted');
    otherInput.remove();
  });

  await t.test('direct Color use waits for the variable target before selecting it', async () => {
    const first = blankDocument(),
      second = blankDocument(),
      text = node('Text'),
      color = node('Color');
    Object.assign(text, {
      font: asset.id,
      text: 'AV',
      variation: { version: 1, fontId: asset.id, axes: {} },
    });
    second.layers[0].nodes.push(text, color);
    first.schemaVersion = 54;
    first.fontAssets = { [asset.id]: asset.asset };
    first.colorSlots = [{ id: 'brand', name: 'Brand', value: { hex: '#112233', alpha: 1 } }];
    const project = migrateLegacyDocumentToCompositionProject(first);
    const target = migrateLegacyDocumentToCompositionProject(second).compositions[0];
    target.id = 'color-race-second';
    target.layers[0].nodes.find((entry) => entry.id === color.id).params.value.colorSlotId =
      'brand';
    project.compositions.push(target);
    await win.openForTest(project);
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'set-color-slot',
      payload: { slotId: 'brand', value: { hex: '#445566', alpha: 1 } },
    });
    win.nect.dispatch({ ...win.nect.commandState, type: 'undo' });
    win.openColorUsesForTest('brand');
    const gate = gateNextPreparation(),
      before = win.openStateForTest();
    win.document.querySelector('[data-color-uses-list] button').click();
    await gate.ready;
    const pending = win.openStateForTest();
    gate.release();
    while (win.variableBusyForTest()) await new Promise((resolve) => setTimeout(resolve, 10));
    await new Promise((resolve) => setTimeout(resolve, 20));
    assert.equal(pending.activeLayer, before.activeLayer);
    assert.equal(pending.selected, before.selected);
    assert.equal(win.nect.activeCompositionId, target.id);
    assert.equal(win.openStateForTest().selected, color.id);
    assert.deepEqual(win.openStateForTest().project, before.project);
    assert.deepEqual(win.openStateForTest().file, before.file);
    assert.deepEqual(win.openStateForTest().historyState, before.historyState);

    assert.ok(await win.editForTest('redo'));
    assert.equal(win.nect.projectSnapshot().colorSlots[0].value.hex, '#445566');
    assert.ok(await win.editForTest('undo'));
    assert.deepEqual(win.nect.projectSnapshot(), before.project);

    const invalid = structuredClone(project);
    invalid.compositions[1].layers[0].nodes.find((entry) => entry.id === text.id).text = 'ffi';
    await win.openForTest(invalid);
    win.openColorUsesForTest('brand');
    const failed = win.openStateForTest(),
      failedSvg = win.nect.export(),
      failedBank = win.liveBridgeForTest().stats().active;
    win.document.querySelector('[data-color-uses-list] button').click();
    while (win.variableBusyForTest()) await new Promise((resolve) => setTimeout(resolve, 10));
    await new Promise((resolve) => setTimeout(resolve, 20));
    assert.equal(win.document.querySelector('.shared-color-uses-dialog').open, true);
    assert.match(
      win.document.querySelector('[data-color-uses-status]').textContent,
      /supports simple/,
    );
    assert.deepEqual(win.openStateForTest(), failed);
    assert.equal(win.nect.export(), failedSvg);
    assert.deepEqual(win.liveBridgeForTest().stats().active, failedBank);
    win.document.querySelector('[data-color-uses-close]').click();

    await win.openForTest(project);
    win.openColorUsesForTest('brand');
    const canceled = win.openStateForTest(),
      canceledSvg = win.nect.export(),
      canceledBank = win.liveBridgeForTest().stats().active;
    const canceledGate = gateNextPreparation();
    win.document.querySelector('[data-color-uses-list] button').click();
    await canceledGate.ready;
    win.document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
    canceledGate.release();
    await new Promise((resolve) => setTimeout(resolve, 20));
    assert.equal(win.document.querySelector('.shared-color-uses-dialog').open, true);
    assert.deepEqual(win.openStateForTest(), canceled);
    assert.equal(win.nect.export(), canceledSvg);
    assert.deepEqual(win.liveBridgeForTest().stats().active, canceledBank);
    win.document.querySelector('[data-color-uses-close]').click();

    await win.openForTest(project);
    win.openColorUsesForTest('brand');
    const changedGate = gateNextPreparation();
    win.document.querySelector('[data-color-uses-list] button').click();
    await changedGate.ready;
    const name = win.nect.compositions[0].name;
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'rename-composition',
      payload: {
        compositionId: project.compositions[0].id,
        name: 'New name',
        expectedName: name,
        expectedProjectRevision: win.nect.projectRevision,
      },
    });
    const changed = win.openStateForTest();
    changedGate.release();
    while (win.variableBusyForTest()) await new Promise((resolve) => setTimeout(resolve, 10));
    await new Promise((resolve) => setTimeout(resolve, 20));
    assert.equal(win.nect.activeCompositionId, project.compositions[0].id);
    assert.deepEqual(win.openStateForTest(), changed);
    assert.match(win.document.querySelector('[data-color-uses-status]').textContent, /changed/i);
    win.document.querySelector('[data-color-uses-close]').click();

    const staticProject = structuredClone(project);
    staticProject.fontAssets = {};
    staticProject.compositions[1].layers[0].nodes =
      staticProject.compositions[1].layers[0].nodes.filter((entry) => entry.id !== text.id);
    await win.openForTest(staticProject);
    win.openColorUsesForTest('brand');
    win.document.querySelector('[data-color-uses-list] button').click();
    assert.equal(win.nect.activeCompositionId, target.id);
    assert.equal(win.openStateForTest().selected, color.id);
  });

  await t.test(
    'prepared Composition deletion retains the latest per-Composition selection',
    async () => {
      const document = blankDocument(),
        texts = Array.from({ length: 8 }, (_, i) => {
          const text = node('Text', 100 + i * 20, 40);
          Object.assign(text, {
            font: asset.id,
            text: String.fromCharCode(65 + i),
            variation: { version: 1, fontId: asset.id, axes: {} },
          });
          return text;
        });
      document.fontAssets = { [asset.id]: asset.asset };
      document.layers[0].nodes.push(...texts);
      document.layers[0].nodes[0].params.path.source = texts[0].id;
      await win.openForTest(migrateLegacyDocumentToCompositionProject(document));
      const root = win.document,
        click = (selector) => {
          const element = root.querySelector(selector);
          assert.ok(element, selector);
          element.click();
        },
        waitFor = async (predicate) => {
          const deadline = Date.now() + 15000;
          while (!predicate()) {
            if (Date.now() >= deadline) throw Error('Variable-font test timed out');
            await new Promise((resolve) => setTimeout(resolve, 10));
          }
        };
      const first = win.nect.activeCompositionId;
      click('[data-tool="Circle"]');
      const firstCircle = win.nect.document.layers.at(-1).id;
      click('[data-tool="Rectangle"]');
      const firstRectangle = win.nect.document.layers.at(-1).id;
      click('[data-action="add-composition"]');
      const second = win.nect.activeCompositionId;
      click('[data-tool="Circle"]');
      const secondCircle = win.nect.document.layers.at(-1).id;
      click('[data-tool="Rectangle"]');
      const secondRectangle = win.nect.document.layers.at(-1).id;
      openDOMPanel(root, 'layers');
      click(`[data-select-layer="${secondCircle}"]`);
      click(`[role="tab"][data-active-composition="${first}"]`);
      click(`[data-select-layer="${firstCircle}"]`);
      const targetName = win.nect.compositions.find((item) => item.id === second).name;
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'delete-composition',
        payload: {
          compositionId: second,
          expectedName: targetName,
          expectedProjectRevision: win.nect.projectRevision,
        },
      });
      assert.equal(win.nect.compositions.length, 1);
      click('[data-action="undo"]');
      assert.equal(win.variableBusyForTest(), true);
      click(`[data-select-layer="${firstRectangle}"]`);
      await waitFor(() => !win.variableBusyForTest() && win.nect.compositions.length === 2);
      assert.equal(win.openStateForTest().activeLayer, firstRectangle);
      click(`[role="tab"][data-active-composition="${second}"]`);
      assert.equal(win.openStateForTest().activeLayer, secondCircle);
      click('[data-action="redo"]');
      assert.equal(win.variableBusyForTest(), true);
      click(`[data-select-layer="${secondRectangle}"]`);
      await waitFor(() => !win.variableBusyForTest() && win.nect.compositions.length === 1);
      click('[data-action="undo"]');
      await waitFor(() => !win.variableBusyForTest() && win.nect.compositions.length === 2);
      click(`[role="tab"][data-active-composition="${second}"]`);
      assert.equal(win.openStateForTest().activeLayer, secondRectangle);
    },
  );

  await t.test(
    'Composition tabs prepare a saved variable target without losing the active view or bank',
    async () => {
      const buildProject = (secondText = 'AV') => {
        const first = blankDocument(),
          second = blankDocument();
        const firstText = node('Text'),
          targetText = node('Text');
        for (const [doc, text, value] of [
          [first, firstText, 'AA'],
          [second, targetText, secondText],
        ]) {
          Object.assign(text, {
            font: asset.id,
            text: value,
            variation: { version: 1, fontId: asset.id, axes: {} },
          });
          doc.layers[0].nodes.push(text);
          doc.layers[0].nodes[0].params.path.source = text.id;
        }
        first.fontAssets = { [asset.id]: asset.asset };
        const project = migrateLegacyDocumentToCompositionProject(first);
        const other = migrateLegacyDocumentToCompositionProject(second).compositions[0];
        other.id = 'variable-tab-second';
        project.compositions.push(other);
        return {
          project,
          firstId: project.compositions[0].id,
          secondId: other.id,
          firstLayer: first.layers[0].id,
          firstText: firstText.id,
          targetText: targetText.id,
        };
      };
      const clickTab = (id) => {
        const tab = win.document.querySelector(`[role="tab"][data-active-composition="${id}"]`);
        assert.ok(tab);
        tab.click();
      };
      const waitFor = async (predicate) => {
        const deadline = Date.now() + 15000;
        while (!predicate()) {
          if (Date.now() >= deadline) throw Error('Variable Composition test timed out');
          await new Promise((resolve) => setTimeout(resolve, 10));
        }
      };
      const current = () => ({
        view: win.openStateForTest(),
        svg: win.nect.export(),
        bank: win.liveBridgeForTest().stats().active,
      });

      const invalid = buildProject('ffi');
      await win.openForTest(invalid.project);
      const beforeFailure = current();
      clickTab(invalid.secondId);
      await waitFor(() => !win.variableBusyForTest());
      assert.deepEqual(current(), beforeFailure);
      assert.equal(win.nect.activeCompositionId, invalid.firstId);

      const fixture = buildProject();
      await win.openForTest(fixture.project);
      const beforeCancel = current();
      let gate = gateNextPreparation();
      clickTab(fixture.secondId);
      await gate.ready;
      win.document.dispatchEvent(
        new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }),
      );
      gate.release();
      await waitFor(() => !win.variableBusyForTest());
      assert.deepEqual(current(), beforeCancel);
      assert.equal(win.nect.activeCompositionId, fixture.firstId);

      gate = gateNextPreparation();
      clickTab(fixture.secondId);
      await gate.ready;
      clickTab(fixture.secondId);
      assert.equal(win.variableBusyForTest(), true);
      assert.deepEqual(current(), beforeCancel);
      gate.release();
      await waitFor(() => !win.variableBusyForTest());
      assert.deepEqual(current(), beforeCancel);
      clickTab(fixture.secondId);
      await waitFor(
        () => !win.variableBusyForTest() && win.nect.activeCompositionId === fixture.secondId,
      );
      assert.doesNotMatch(win.document.querySelector('.status .message').textContent, /Preparing/);
      assert.deepEqual(win.nect.projectSnapshot(), beforeCancel.view.project);
      assert.deepEqual(win.nect.fileState, beforeCancel.view.file);
      assert.deepEqual(win.openStateForTest().past, beforeCancel.view.past);
      assert.deepEqual(win.openStateForTest().future, beforeCancel.view.future);
      const originalSvg = win.nect.export();
      assert.ok(
        await win.editForTest('set-text-content', { nodeId: fixture.targetText, text: 'AW' }),
      );
      const editedSvg = win.nect.export();
      assert.notEqual(editedSvg, originalSvg);
      assert.ok(await win.editForTest('undo'));
      assert.equal(win.nect.export(), originalSvg);
      assert.ok(await win.editForTest('redo'));
      assert.equal(win.nect.export(), editedSvg);
      clickTab(fixture.firstId);
      await waitFor(
        () => !win.variableBusyForTest() && win.nect.activeCompositionId === fixture.firstId,
      );
      clickTab(fixture.secondId);
      await waitFor(
        () => !win.variableBusyForTest() && win.nect.activeCompositionId === fixture.secondId,
      );
      assert.equal(win.nect.export(), editedSvg);

      await win.openForTest(fixture.project);
      gate = gateNextPreparation();
      clickTab(fixture.secondId);
      await gate.ready;
      let draft = win.nect.dispatch({
        ...win.nect.commandState,
        type: 'begin-text-draft',
        payload: { layerId: fixture.firstLayer, nodeId: fixture.firstText },
      });
      draft = win.nect.dispatch({
        ...win.nect.commandState,
        type: 'stage-text-draft',
        payload: {
          draftId: draft.draftId,
          expectedDraftRevision: draft.draftRevision,
          action: 'edit',
          previousRaw: 'AA',
          nextRaw: 'AAA',
          selectionStart: 3,
          selectionEnd: 3,
          inputType: 'insertText',
        },
      });
      const draftBefore = win.textDraftForTest(draft.draftId),
        beforeDraft = current();
      gate.release();
      await waitFor(() => !win.variableBusyForTest());
      assert.equal(win.nect.activeCompositionId, fixture.firstId);
      assert.deepEqual(win.textDraftForTest(draft.draftId), draftBefore);
      assert.deepEqual(current(), beforeDraft);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'cancel-text-draft',
        payload: { draftId: draft.draftId },
      });

      gate = gateNextPreparation();
      clickTab(fixture.secondId);
      await gate.ready;
      await win.openForTest(migrateLegacyDocumentToCompositionProject(blankDocument()));
      const replacement = win.openStateForTest();
      gate.release();
      await new Promise((resolve) => setTimeout(resolve, 20));
      assert.deepEqual(win.openStateForTest(), replacement);
      assert.equal(win.liveBridgeForTest(), null);
    },
  );
});
