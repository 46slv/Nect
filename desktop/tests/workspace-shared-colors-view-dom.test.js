import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { revealDOMBoundary } from './helpers/dom-workspace-controls.js';

test('Shared Colors controller view mounts through adapter and uses Session through rerenders', async (t) => {
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
  const colorsToggle = () => document.querySelector('[data-workspace-panel="colors"]');
  const colors = () => document.querySelector('.shared-colors-panel');
  assert.ok(colorsToggle());
  colorsToggle().click();
  assert.ok(colors());
  const host = document.querySelector('[data-drawer-side="left"]'),
    slot = host.querySelector('[data-workspace-asset-slot]'),
    effects = host.querySelector('.effects-panel');
  assert.equal(colors().parentNode, host);
  assert.equal(slot.nextElementSibling, colors());
  assert.equal(effects.parentNode, host);
  assert.equal(effects.hidden, true);
  const before = (win.nect.projectSnapshot().colorSlots || []).length;
  const beforeAddRevision = win.nect.commandState.expectedRevision;
  colors().querySelector('[data-shared-color-add]').click();
  assert.equal(win.nect.projectSnapshot().colorSlots.length, before + 1);
  assert.equal(win.nect.commandState.expectedRevision, beforeAddRevision + 1);
  const created = win.nect.projectSnapshot().colorSlots.at(-1),
    id = created.id;
  const nameInput = () =>
    colors().querySelector(`[data-shared-color-edit="name"][data-shared-color-id="${id}"]`);
  assert.ok(nameInput());
  const beforeEditRevision = win.nect.commandState.expectedRevision;
  nameInput().value = 'Studio';
  nameInput().dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(win.nect.commandState.expectedRevision, beforeEditRevision + 1);
  assert.equal(win.nect.projectSnapshot().colorSlots.at(-1).id, id);
  assert.equal(win.nect.projectSnapshot().colorSlots.at(-1).name, 'Studio');
  document.querySelector('[data-action="undo"]').click();
  assert.notEqual(win.nect.projectSnapshot().colorSlots.at(-1).name, 'Studio');
  document.querySelector('[data-action="redo"]').click();
  assert.equal(win.nect.projectSnapshot().colorSlots.at(-1).name, 'Studio');
  assert.equal(win.nect.projectSnapshot().colorSlots.at(-1).id, id);
  revealDOMBoundary(document, 'canvas');
  const previous = colors();
  colors().scrollTop = 31;
  nameInput().focus();
  assert.equal(document.activeElement, nameInput());
  document.querySelector('[data-action="grid-panel"]').click();
  assert.notEqual(colors(), previous);
  assert.equal(colors().parentNode, document.querySelector('[data-drawer-side="left"]'));
  assert.equal(colors().scrollTop, 31);
  assert.equal(document.activeElement, nameInput());
  assert.equal(nameInput().value, 'Studio');
  const snapshot = JSON.stringify(win.nect.projectSnapshot());
  document.querySelector('.viewer-tabs [data-viewer-mode="viewers"]').click();
  assert.equal(colors().closest('[data-drawer-side]').hidden, true);
  document.querySelector('.viewer-tabs [data-viewer-mode="artwork"]').click();
  assert.equal(colors().closest('[data-drawer-side]').hidden, false);
  document.querySelector('[data-workspace-panel="assets"]').click();
  assert.equal(colors().closest('[data-drawer-side]').dataset.activePanel, 'assets');
  colorsToggle().click();
  assert.equal(colors().closest('[data-drawer-side]').dataset.activePanel, 'colors');
  colors().querySelector('[data-shared-color-close]').click();
  assert.equal(colorsToggle().getAttribute('aria-expanded'), 'false');
  colorsToggle().click();
  assert.equal(colorsToggle().getAttribute('aria-expanded'), 'true');
  assert.equal(JSON.stringify(win.nect.projectSnapshot()), snapshot);

  const composing = nameInput();
  composing.focus();
  composing.value = '編集中';
  composing.dispatchEvent(new win.Event('compositionstart', { bubbles: true }));
  const compositionState = { ...win.nect.commandState };
  for (const selector of [
    '[data-workspace-panel="assets"]',
    '[data-workspace-panel="effects"]',
    '[data-workspace-panel="layers"]',
    '[data-workspace-panel="colors"]',
    '[data-shared-color-add]',
    '[data-shared-color-close]',
    '.viewer-tabs [data-viewer-mode="viewers"]',
    '[data-key-color-recipe-open]',
    '.canvas-toolbar [data-action="align-panel"]',
  ]) {
    const target = document.querySelector(selector);
    assert.ok(target, selector);
    target.click();
    assert.equal(colors().closest('[data-drawer-side]').dataset.activePanel, 'colors', selector);
    assert.equal(colors().closest('[data-drawer-side]').hidden, false, selector);
    assert.equal(document.activeElement, composing, selector);
    assert.equal(composing.value, '編集中', selector);
    const down = new win.PointerEvent('pointerdown', {
      bubbles: true,
      cancelable: true,
      button: 0,
      isPrimary: true,
      pointerId: 1,
    });
    target.dispatchEvent(down);
    assert.equal(down.defaultPrevented, true, `${selector} preserves focus before pointer blur`);
    target.dispatchEvent(new win.PointerEvent('pointerup', { bubbles: true, pointerId: 1 }));
    assert.deepEqual(win.nect.commandState, compositionState, selector);
  }
  composing.dispatchEvent(new win.Event('compositionend', { bubbles: true }));
  composing.dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(win.nect.projectSnapshot().colorSlots.find((slot) => slot.id === id).name, '編集中');
  assert.equal(win.nect.commandState.expectedRevision, compositionState.expectedRevision + 1);
  win.nect.dispatch({ ...win.nect.commandState, type: 'undo', payload: {} });
  assert.equal(win.nect.projectSnapshot().colorSlots.find((slot) => slot.id === id).name, 'Studio');
  win.nect.dispatch({ ...win.nect.commandState, type: 'redo', payload: {} });
  assert.equal(win.nect.projectSnapshot().colorSlots.find((slot) => slot.id === id).name, '編集中');
  document.querySelector('[data-workspace-panel="assets"]').click();
  assert.equal(colors().closest('[data-drawer-side]').dataset.activePanel, 'assets');
  document.querySelector('[aria-label="Close assets"]')?.click();
  await win.happyDOM.close();
});
