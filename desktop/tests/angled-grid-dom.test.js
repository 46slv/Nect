import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, guideDefaults } from '../src/core.js';
import { MAX_GRID_LINES } from '../src/guides.js';

test('Grid angle control owns typed/scrub history, preview/cancel, JSON and bounded editor-only drawing', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  let next = 0;
  const frames = new Map();
  globalThis.requestAnimationFrame = win.requestAnimationFrame = (fn) => {
    frames.set(++next, fn);
    return next;
  };
  win.cancelAnimationFrame = (id) => frames.delete(id);
  const settle = () => {
    for (let i = 0; frames.size && i < 8; i++) {
      const batch = [...frames.values()];
      frames.clear();
      batch.forEach((fn) => fn(0));
    }
    assert.equal(frames.size, 0);
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return this;
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({ a: 1, b: 0, inverse: () => ({}) });
  globalThis.fetch = async () => ({ ok: false });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  settle();
  const $ = (s) => document.querySelector(s),
    get = () => win.nect.document,
    click = (s) => {
      $(s).click();
      settle();
    };
  const open = () => {
    if (!$('[data-guide-setting="gridAngle"]')) click('[data-action="grid-panel"]');
  };
  const edit = (value) => {
    open();
    const el = $('[data-guide-setting="gridAngle"]');
    el.value = String(value);
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
    settle();
  };
  const pointer = (target, type, x) => {
    target.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        button: 0,
        pointerId: 1,
        clientX: x,
        clientY: 100,
      }),
    );
    settle();
  };
  const doc = newDocument();
  doc.editor = { ...guideDefaults(), gridVisible: true, gridSize: 40 };
  delete doc.editor.gridAngle;
  win.nect.load(JSON.stringify(doc));
  settle();
  open();
  const svg = win.nect.export(),
    geometry = JSON.stringify(get().layers);
  assert.equal($('[data-guide-setting="gridAngle"]').value, '0');
  assert.equal($('#editing-grid').getAttribute('patternTransform'), 'translate(0 0)');
  for (const angle of [45, -30, 90, 22.5, 0]) {
    const before = get();
    edit(angle);
    assert.equal(get().editor.gridAngle, angle);
    assert.equal(get().schemaVersion, 49);
    assert.equal(JSON.stringify(get().layers), geometry);
    assert.equal(win.nect.export(), svg);
    if (angle) {
      assert.equal($('#editing-grid'), null);
      assert.equal($('[data-grid-overlay]').getAttribute('data-grid-angle'), String(angle));
      assert.ok(document.querySelectorAll('[data-grid-axis]').length > 0);
    } else assert.equal($('#editing-grid').getAttribute('patternTransform'), 'translate(0 0)');
    const saved = get();
    click('[data-action="undo"]');
    assert.deepEqual(get(), before);
    click('[data-action="redo"]');
    assert.deepEqual(get(), saved);
    win.nect.load(JSON.stringify(saved));
    settle();
    assert.deepEqual(get(), saved);
    assert.equal(win.nect.export(), svg);
  }
  for (const ending of ['Escape', 'pointercancel']) {
    open();
    const el = $('[data-guide-setting="gridAngle"]'),
      before = get(),
      file = win.nect.fileState,
      state = win.nect.commandState;
    el.focus();
    pointer(el, 'pointerdown', 100);
    pointer(document, 'pointermove', 124);
    assert.equal(Number(el.value), 20);
    assert.equal(get().editor.gridAngle, 0);
    assert.equal($('[data-grid-overlay]').getAttribute('data-grid-angle'), '20');
    assert.equal($('[data-guide-setting="gridAngle"]'), el);
    assert.equal(document.activeElement, el);
    assert.ok(win.nect.gestureState);
    if (ending === 'Escape') {
      el.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      settle();
    } else pointer(document, 'pointercancel', 124);
    assert.equal(win.nect.gestureState, null);
    pointer(document, 'pointerup', 124);
    assert.deepEqual(get(), before);
    assert.deepEqual(win.nect.fileState, file);
    assert.deepEqual(win.nect.commandState, state);
    assert.equal(win.nect.export(), svg);
    assert.ok($('#editing-grid'));
  }
  open();
  const before = get(),
    el = $('[data-guide-setting="gridAngle"]');
  pointer(el, 'pointerdown', 100);
  pointer(document, 'pointermove', 124);
  pointer(document, 'pointerup', 124);
  assert.equal(get().editor.gridAngle, 20);
  click('[data-action="undo"]');
  assert.deepEqual(get(), before);
  click('[data-action="redo"]');
  assert.equal(get().editor.gridAngle, 20);
  const saved = get();
  win.nect.load(JSON.stringify(saved));
  settle();
  edit(361);
  assert.deepEqual(get(), saved);
  assert.equal($('[data-action="undo"]').disabled, true);
  for (const [key, value] of [
    ['width', 799.5],
    ['height', 601.25],
  ]) {
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'set-document-settings',
      payload: { key, value },
    });
    settle();
  }
  assert.equal(JSON.stringify(get().layers), geometry);
  for (const path of document.querySelectorAll('[data-grid-axis]'))
    assert.ok(!/NaN|Infinity/.test(path.getAttribute('d')));
  const dense = get();
  dense.width = dense.height = 100000;
  dense.editor.gridSize = 1;
  dense.editor.gridAngle = 45;
  win.nect.load(JSON.stringify(dense));
  settle();
  open();
  assert.ok(document.querySelectorAll('[data-grid-axis]').length <= MAX_GRID_LINES);
  assert.match($('[data-grid-density]').textContent, /Display every \d+ lines/);
  assert.equal($('[data-grid-density]').hidden, false);
  assert.equal(get().editor.gridSize, 1);
});
