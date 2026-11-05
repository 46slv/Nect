import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';

test('main composition keeps Assets portal on hide and disposes only on panel close', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.localStorage = win.localStorage;
  t.after(() => delete globalThis.localStorage);
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
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let i = 0; i < 200 && !win.nect; i++) await tick();
  assert.ok(win.nect);
  openDOMPanel(document, 'layers');
  const width = document.querySelector('[data-dimension="width"]');
  assert.ok(width);
  assert.equal(width.closest('[hidden], [inert]'), null);
  width.value = String(Number(width.value) + 1);
  width.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  const changed = serialize(win.nect.document);
  document.querySelector('[data-action="undo"]').click();
  const before = serialize(win.nect.document);
  assert.notEqual(before, changed);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  const toggle = () => document.querySelector('[data-workspace-panel="assets"]');
  assert.ok(toggle());
  toggle().click();
  let panel = document.querySelector('[data-asset-library]');
  assert.ok(panel);
  assert.equal(panel.parentNode, document.body);
  const input = panel.querySelector('[data-asset-search]');
  input.value = 'synthetic search';
  toggle().click();
  assert.equal(panel.hidden, true);
  assert.equal(panel.isConnected, true);
  toggle().click();
  assert.equal(document.querySelector('[data-asset-library]'), panel);
  assert.equal(panel.querySelector('[data-asset-search]'), input);
  assert.equal(input.value, 'synthetic search');
  assert.equal(serialize(win.nect.document), before);
  input.focus();
  assert.equal(document.activeElement, input);
  const viewers = document.querySelector('.viewer-tabs [data-viewer-mode="viewers"]');
  assert.ok(viewers);
  assert.equal(viewers.disabled, false);
  viewers.click();
  assert.equal(document.querySelector('.canvas-wrap').dataset.viewerMode, 'viewers');
  assert.equal(panel.hidden, true);
  assert.equal(panel.inert, true);
  assert.equal(panel.parentNode, document.body);
  assert.equal(document.activeElement, viewers);
  assert.equal(document.querySelector('[data-asset-library]'), panel);
  assert.equal(panel.querySelector('[data-asset-search]'), input);
  assert.equal(input.value, 'synthetic search');
  assert.equal(serialize(win.nect.document), before);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  document.querySelector('.viewer-tabs [data-viewer-mode="artwork"]').click();
  assert.equal(document.querySelector('.canvas-wrap').dataset.viewerMode, 'artwork');
  assert.equal(panel.hidden, false);
  assert.equal(panel.inert, false);
  assert.equal(document.querySelector('[data-asset-library]'), panel);
  assert.equal(panel.querySelector('[data-asset-search]'), input);
  assert.equal(input.value, 'synthetic search');
  assert.equal(serialize(win.nect.document), before);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  openDOMPanel(document, 'layers');
  assert.equal(panel.hidden, true);
  assert.equal(panel.inert, true);
  assert.equal(panel.parentNode, document.body);
  assert.equal(serialize(win.nect.document), before);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  toggle().click();
  assert.equal(document.querySelector('[data-asset-library]'), panel);
  assert.equal(panel.querySelector('[data-asset-search]'), input);
  assert.equal(input.value, 'synthetic search');
  revealDOMBoundary(document, 'canvas');
  const oldWrap = document.querySelector('.canvas-wrap');
  const gridButton = document.querySelector('[data-action="grid-panel"]');
  assert.equal(gridButton.closest('[hidden], [inert]'), null);
  gridButton.click();
  assert.notEqual(document.querySelector('.canvas-wrap'), oldWrap);
  assert.equal(document.querySelector('[data-asset-library]'), panel);
  assert.equal(panel.parentNode, document.body);
  assert.equal(panel.hidden, false);
  assert.equal(panel.querySelector('[data-asset-search]'), input);
  assert.equal(input.value, 'synthetic search');
  assert.equal(serialize(win.nect.document), before);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  input.focus();
  const preference = win.localStorage.getItem('nect.workspace-drawers.v1');
  const expanded = toggle().getAttribute('aria-expanded');
  assert.equal(expanded, 'true');
  assert.ok(preference);
  for (const options of [{ isComposing: true }, { isComposing: false, keyCode: 229 }]) {
    const composingEscape = new win.KeyboardEvent('keydown', {
      key: 'Escape',
      bubbles: true,
      cancelable: true,
      ...options,
    });
    assert.equal(composingEscape.isComposing, options.isComposing);
    if (options.keyCode) assert.equal(composingEscape.keyCode, 229);
    input.dispatchEvent(composingEscape);
    assert.equal(composingEscape.defaultPrevented, false);
    assert.equal(document.querySelector('[data-asset-library]'), panel);
    assert.equal(panel.isConnected, true);
    assert.equal(panel.querySelector('[data-asset-search]'), input);
    assert.equal(input.value, 'synthetic search');
    assert.equal(document.activeElement, input);
    assert.equal(toggle().getAttribute('aria-expanded'), expanded);
    assert.equal(win.localStorage.getItem('nect.workspace-drawers.v1'), preference);
    assert.equal(serialize(win.nect.document), before);
    assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  }
  const escape = new win.KeyboardEvent('keydown', {
    key: 'Escape',
    bubbles: true,
    cancelable: true,
  });
  input.dispatchEvent(escape);
  assert.equal(escape.defaultPrevented, true);
  assert.equal(panel.isConnected, false);
  assert.equal(document.querySelector('[data-asset-library]'), null);
  assert.equal(serialize(win.nect.document), before);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  toggle().click();
  const afterEscape = document.querySelector('[data-asset-library]');
  assert.ok(afterEscape);
  assert.notEqual(afterEscape, panel);
  panel = afterEscape;
  panel.querySelector('[aria-label="Close assets"]').click();
  assert.equal(panel.isConnected, false);
  toggle().click();
  const replacement = document.querySelector('[data-asset-library]');
  assert.ok(replacement);
  assert.notEqual(replacement, panel);
  assert.equal(replacement.parentNode, document.body);
  assert.equal(serialize(win.nect.document), before);
  replacement.querySelector('[aria-label="Close assets"]').click();
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  document.querySelector('[data-action="redo"]').click();
  assert.equal(serialize(win.nect.document), changed);
  await win.happyDOM.close();
});
