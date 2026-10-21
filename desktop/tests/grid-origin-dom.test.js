import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, guideDefaults } from '../src/core.js';

test('Grid origin selector renders all nine phases, one Undo/Redo, reload and resize without geometry edits', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'requestAnimationFrame'])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
  globalThis.MutationObserver = win.MutationObserver;
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
  source = source.replace("import './style.css';", '');
  source = source.replace(
    /from '(\.\/[^']+)'/g,
    (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
  );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (q) => document.querySelector(q),
    click = (q) => $(q).click(),
    get = () => win.nect.document;
  const pointer = (target, type, x, y) =>
    target.dispatchEvent(
      new win.PointerEvent(type, { bubbles: true, clientX: x, clientY: y, button: 0 }),
    );
  const key = (target, value) =>
    target.dispatchEvent(new win.KeyboardEvent('keydown', { bubbles: true, key: value }));
  const edit = (q, value) => {
    const el = $(q);
    el.value = String(value);
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
  };

  t.after(() => win.happyDOM.abort());
  const doc = newDocument();
  doc.editor = { ...guideDefaults(), gridSize: 40, gridVisible: true };
  delete doc.editor.gridAnchor;
  win.nect.load(JSON.stringify(doc));
  click('[data-action="grid-panel"]');
  assert.equal($('[data-guide-setting="gridAnchor"]').value, 'top-left');
  assert.equal($('#editing-grid').getAttribute('patternTransform'), 'translate(0 0)');
  const gridSvg = win.nect.export();
  edit('[data-guide-setting="gridAnchor"]', 'center');
  assert.equal(get().schemaVersion, 27);
  // The selector covers the whole grid phase, not artwork/guide coordinates.
  assert.equal($('[data-guide-setting="gridAnchor"]').options.length, 9);
  const coordinates = [
    ['top-left', 0, 0],
    ['top-center', 480, 0],
    ['top-right', 960, 0],
    ['middle-left', 0, 270],
    ['center', 480, 270],
    ['middle-right', 960, 270],
    ['bottom-left', 0, 540],
    ['bottom-center', 480, 540],
    ['bottom-right', 960, 540],
  ];
  const geometry = JSON.stringify(get().layers),
    guides = JSON.stringify(get().editor.guides);
  for (const [anchor, x, y] of coordinates) {
    if (!$('[data-guide-setting="gridAnchor"]')) click('[data-action="grid-panel"]');
    const before = JSON.stringify(get());
    edit('[data-guide-setting="gridAnchor"]', anchor);
    assert.equal(get().editor.gridAnchor, anchor);
    // Happy DOM 20.8.9 reuses a prior selectedIndex across innerHTML replacement.
    // Assert authored state and the selected option; native WebView2 checks .value.
    assert.equal($('[data-guide-setting="gridAnchor"] option[selected]').value, anchor);
    assert.equal($('#editing-grid').getAttribute('patternTransform'), `translate(${x} ${y})`);
    assert.equal(JSON.stringify(get().layers), geometry);
    assert.equal(JSON.stringify(get().editor.guides), guides);
    assert.equal(win.nect.export(), gridSvg);
    const saved = JSON.stringify(get());
    click('[data-action="undo"]');
    assert.equal(JSON.stringify(get()), before);
    click('[data-action="redo"]');
    assert.equal(JSON.stringify(get()), saved);
    win.nect.load(saved);
    assert.equal(JSON.stringify(get()), saved);
    assert.equal($('#editing-grid').getAttribute('patternTransform'), `translate(${x} ${y})`);
  }
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-document-settings',
    payload: { key: 'width', value: 1023.5 },
  });
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-document-settings',
    payload: { key: 'height', value: 601.25 },
  });
  assert.equal($('#editing-grid').getAttribute('patternTransform'), 'translate(1023.5 601.25)');
  assert.equal(get().editor.gridAnchor, 'bottom-right');
  assert.equal(JSON.stringify(get().layers), geometry);
  assert.equal(JSON.stringify(get().editor.guides), guides);
});
