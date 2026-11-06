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
    window.openStateForTest = () => ({project:session.projectSnapshot(), command:session.state,
      file:window.nect.fileState, past:history.past, future:history.future,
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
});
