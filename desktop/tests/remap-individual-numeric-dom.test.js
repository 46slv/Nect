import { revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';

test('raw remap fields share numeric behavior and retain sparse Session edits', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
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
  globalThis.fetch = async () => ({ ok: false });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source = source.replace(
    'const binding = numericBehaviorBinding(numeric),',
    'const binding = numericBehaviorBinding(numeric), __record = (globalThis.__remapBinding = !!binding),',
  );
  source += '\nwindow.remapTestSelection = (ids) => { selection = new Set(ids); };';
  source +=
    "\nwindow.remapTestDisconnect = (id) => commandChange('disconnect', { nodeId: id, key: 'path' });";
  source +=
    '\nwindow.remapNumericHistory = () => ({revision:session.state.expectedRevision,past:history.past.length,future:history.future.length,gesture:session.hasGesture});';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let i = 0; i < 200 && !win.nect; i++) await tick();
  assert.ok(win.nect);
  const doc = makeDocument(),
    layer = doc.layers[0],
    remap = layer.nodes[3],
    curve = node('Curve');
  layer.nodes.push(curve);
  remap.params.path.source = curve.id;
  const pointId = curve.points[0].id;
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  const pointer = (el, type, x = 100) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 7,
        clientX: x,
        clientY: 20,
      }),
    );
  const head = document.querySelector(`.graph .node[data-id="${remap.id}"] .node-head .type-tag`);
  assert.ok(head);
  pointer(head, 'pointerdown');
  pointer(document, 'pointerup');
  const browse = document.querySelector(`.graph [data-point-panel-mode="${remap.id}"]`);
  assert.ok(browse, 'point browse control is rendered');
  browse.click();
  const field = (component = 'position', axis = 0) => {
    const selector = `.graph [data-remap="${pointId}"][data-component="${component}"][data-axis="${axis}"]`;
    let input = document.querySelector(selector);
    if (!input) {
      const nodeHead = document.querySelector(
        `.graph .node[data-id="${remap.id}"] .node-head .type-tag`,
      );
      pointer(nodeHead, 'pointerdown');
      pointer(document, 'pointerup');
      input = document.querySelector(selector);
      if (!input) document.querySelector(`.graph [data-point-panel-mode="${remap.id}"]`).click();
      input = document.querySelector(selector);
    }
    return input;
  };
  assert.ok(field(), 'raw remap field is rendered');
  const saved = () => JSON.stringify(win.nect.document),
    edits = () => win.nect.document.layers[0].nodes.find((n) => n.id === remap.id).edits,
    hist = () => structuredClone(win.remapNumericHistory());
  const undo = () => {
      document.activeElement?.blur();
      document.querySelector('[data-action="undo"]').click();
    },
    redo = () => {
      document.activeElement?.blur();
      document.querySelector('[data-action="redo"]').click();
    };
  const start = (el) => {
      el.focus();
      globalThis.__remapBinding = false;
      pointer(el, 'pointerdown');
      assert.equal(globalThis.__remapBinding, true);
    },
    move = (x = 114) => pointer(document, 'pointermove', x),
    end = (x = 114) => pointer(document, 'pointerup', x);
  const before = saved();
  assert.equal(edits().length, 0);
  start(field());
  move();
  assert.equal(saved(), before, 'preview is detached');
  end();
  assert.equal(edits()[0].position[0], 10);
  undo();
  assert.equal(saved(), before, 'one undo restores sparse state');
  const redoState = hist();
  start(field());
  move();
  pointer(document, 'pointercancel', 114);
  assert.equal(saved(), before);
  assert.deepEqual(hist(), redoState);
  start(field());
  move();
  move(100);
  end(100);
  assert.equal(saved(), before, 'return to origin retains sparse state');
  assert.deepEqual(hist(), redoState);
  redo();
  assert.equal(edits()[0].position[0], 10);
  for (const [component, axis] of [
    ['handleA', 1],
    ['handleB', 0],
  ]) {
    const snapshot = saved();
    start(field(component, axis));
    move();
    end();
    assert.equal(edits()[0][component][axis], 10);
    undo();
    assert.equal(saved(), snapshot);
  }
  const draft = field();
  draft.value = '7';
  draft.focus();
  start(draft);
  move();
  end();
  assert.equal(edits()[0].position[0], 17);
  undo();
  assert.equal(edits()[0].position[0], 7);
  undo();
  assert.equal(edits()[0].position[0], 10);
  const beforeReadonly = saved(),
    readonly = field();
  start(readonly);
  move();
  readonly.readOnly = true;
  end();
  assert.equal(saved(), beforeReadonly, 'readonly interruption cancels');
  const beforeSelection = saved(),
    selectionHistory = hist();
  const selectionInput = field();
  start(selectionInput);
  move();
  win.remapTestSelection([remap.id, curve.id]);
  end();
  selectionInput.blur();
  assert.equal(saved(), beforeSelection, 'selection-set change cancels preview');
  assert.deepEqual(hist(), selectionHistory);
  win.remapTestSelection([remap.id]);
  const beforeMetadata = saved(),
    metadataHistory = hist();
  const changedControl = field();
  start(changedControl);
  move();
  changedControl.dataset.component = 'handleB';
  changedControl.value = '77';
  end();
  assert.equal(changedControl.value, '77', 'old gesture cannot overwrite new control value');
  assert.equal(saved(), beforeMetadata);
  assert.deepEqual(hist(), metadataHistory);
  const pointControl = field();
  start(pointControl);
  move();
  pointControl.dataset.remap = curve.points[1].id;
  pointControl.value = '99';
  end();
  assert.equal(pointControl.value, '99', 'old gesture cannot overwrite another point control');
  assert.equal(saved(), beforeMetadata);
  assert.deepEqual(hist(), metadataHistory);
  const newer = field();
  start(newer);
  move();
  newer.value = '23';
  newer.dispatchEvent(new win.Event('input', { bubbles: true }));
  end();
  assert.equal(newer.value, '23', 'new typed value survives cancellation');
  assert.equal(saved(), beforeMetadata);
  win.nect.load(beforeMetadata);
  const staleSource = field();
  start(staleSource);
  win.remapTestDisconnect(remap.id);
  const afterDisconnect = saved(),
    disconnectedHistory = hist();
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.id === remap.id).params.path.source,
    undefined,
  );
  move();
  end();
  staleSource.blur();
  assert.equal(saved(), afterDisconnect, 'late pointer cannot edit disconnected source');
  assert.deepEqual(hist(), disconnectedHistory);
  undo();
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.id === remap.id).params.path.source,
    curve.id,
  );
  const restored = field();
  start(restored);
  move();
  pointer(document, 'pointercancel', 114);
  const preciseDoc = JSON.parse(saved());
  preciseDoc.layers[0].nodes.find((n) => n.id === remap.id).edits[0].position[0] =
    0.12345678901234566;
  win.nect.load(JSON.stringify(preciseDoc));
  const preciseBefore = saved();
  start(field());
  move();
  move(100);
  end(100);
  assert.equal(saved(), preciseBefore, 'origin preserves exact high-precision authored value');
  const enable = document.querySelector(`.graph [data-enable-remap="${pointId}"]`);
  assert.ok(enable);
  enable.click();
  assert.ok(edits()[0].params, 'typed parameter owner remains available');
  assert.equal(document.querySelector(`.graph [data-remap="${pointId}"]`), null);
  await win.happyDOM.close();
});
