import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, blankDocument } from '../src/core.js';
import { appendArtwork } from '../src/artwork-creation.js';

test('Artwork gap shared numeric behavior preserves auto, local state and document history', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver', 'localStorage'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = win.requestAnimationFrame = () => 0;
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
  source += '\nwindow.artworkGapRender = () => render();';
  source +=
    '\nwindow.artworkGapSelection = () => ({ target: artworkTarget, ids: [...artworkSelection], layer: activeLayer });';
  source +=
    '\nwindow.artworkGapTest = () => ({ spacing: alignmentPreferences.spacing, revision: session.state.expectedRevision, past: history.past.length, future: history.future.length });';
  source = source.replace(
    'const binding = numericBehaviorBinding(numeric),',
    'const binding = numericBehaviorBinding(numeric), __record = (globalThis.__gapBinding = !!binding),',
  );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const wait = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let i = 0; i < 200 && !win.nect; i++) await wait();
  assert.ok(win.nect);
  win.nect.load(JSON.stringify(newDocument()));
  revealDOMBoundary(document, 'canvas');
  document.querySelector('[data-action="grid-panel"]').click();
  const grid = document.querySelector('[data-guide-setting="gridSize"]');
  grid.value = '42';
  grid.dispatchEvent(new win.Event('change', { bubbles: true }));
  document.querySelector('[data-action="undo"]').click();
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  if (!document.querySelector('[data-align-spacing]'))
    document.querySelector('[data-action="align-panel"]').click();
  const field = () => document.querySelector('[data-align-spacing]');
  assert.ok(field());
  const storage = () => win.localStorage.getItem('nect.alignment-panel.v1');
  const history = () => {
    const { revision, past, future } = win.artworkGapTest();
    return { revision, past, future };
  };
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
  const edit = (value) => {
    const el = field();
    el.value = value;
    el.dispatchEvent(new win.Event('input', { bubbles: true }));
  };
  const before = history(),
    beforeDoc = JSON.stringify(win.nect.document),
    emptyStorage = storage();
  assert.equal(win.artworkGapTest().spacing, null);
  const arrow = new win.KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    key: 'ArrowUp',
  });
  field().dispatchEvent(arrow);
  assert.equal(arrow.defaultPrevented, false, 'native keyboard remains available');
  let el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 103);
  pointer(document, 'pointerup', 103);
  assert.equal(win.artworkGapTest().spacing, null, 'click and jitter keep Auto');
  el.focus();
  pointer(el, 'pointerdown');
  assert.equal(globalThis.__gapBinding, true);
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointermove', 100);
  pointer(document, 'pointerup', 100);
  assert.equal(win.artworkGapTest().spacing, null);
  assert.equal(field().value, '');
  assert.equal(storage(), emptyStorage);
  edit('-25');
  assert.equal(win.artworkGapTest().spacing, -25);
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  assert.equal(win.artworkGapTest().spacing, -15);
  const prior = storage();
  el = field();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointercancel', 114);
  assert.equal(win.artworkGapTest().spacing, -15);
  assert.equal(storage(), prior);
  edit('0');
  assert.equal(win.artworkGapTest().spacing, 0);
  edit('100000');
  el = field();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  assert.equal(win.artworkGapTest().spacing, 100000);
  edit('0.123456789012345');
  const precise = storage();
  el = field();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointercancel', 114);
  assert.equal(storage(), precise);
  edit('-100000');
  el = field();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 90);
  pointer(document, 'pointerup', 90);
  assert.equal(win.artworkGapTest().spacing, -100000);
  edit('17');
  for (const ending of ['Escape', 'lostpointercapture', 'blur']) {
    const saved = storage();
    el = field();
    el.focus();
    pointer(el, 'pointerdown');
    pointer(document, 'pointermove', 114);
    if (ending === 'Escape')
      el.dispatchEvent(
        new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: 'Escape' }),
      );
    else if (ending === 'blur') win.dispatchEvent(new win.Event('blur'));
    else pointer(el, 'lostpointercapture', 114);
    pointer(document, 'pointerup', 114);
    assert.equal(win.artworkGapTest().spacing, 17, ending);
    assert.equal(storage(), saved, ending);
  }
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  edit('22');
  pointer(document, 'pointerup', 114);
  assert.equal(win.artworkGapTest().spacing, 22, 'new typing survives old gesture');
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  win.artworkGapRender();
  pointer(document, 'pointerup', 114);
  assert.equal(win.artworkGapTest().spacing, 22, 'rerender cancels preview');
  assert.equal(field().value, '22');
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  const reference = document.querySelector('[data-align-reference]');
  reference.value = 'artboard';
  reference.dispatchEvent(new win.Event('change', { bubbles: true }));
  pointer(document, 'pointerup', 114);
  assert.equal(win.artworkGapTest().spacing, 22, 'reference change cancels spacing preview');
  assert.equal(JSON.parse(storage()).reference, 'artboard', 'new reference is preserved');
  for (const button of document.querySelectorAll('[data-distribute-gap]')) {
    assert.equal(button.dataset.distributeSpacing, '22');
    assert.equal(button.disabled, true);
  }
  edit('');
  assert.equal(win.artworkGapTest().spacing, null);
  assert.deepEqual(history(), before);
  assert.equal(JSON.stringify(win.nect.document), beforeDoc);

  // Exercise the layer selection click handler with a synthetic document.
  const multiDoc = blankDocument();
  const artworkIds = [100, 280, 650].map(
    (x) => appendArtwork(multiDoc, 'Rectangle', { at: [x, 100] }).layerId,
  );
  multiDoc.layers.shift();
  win.nect.load(JSON.stringify(multiDoc));
  document.querySelector('[data-action="grid-panel"]').click();
  const nextGrid = document.querySelector('[data-guide-setting="gridSize"]');
  nextGrid.value = '43';
  nextGrid.dispatchEvent(new win.Event('change', { bubbles: true }));
  document.querySelector('[data-action="undo"]').click();
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  if (!field()) document.querySelector('[data-action="align-panel"]').click();
  const selectLayer = (id, shiftKey = false) => {
    const button = document.querySelector(`[data-select-layer="${id}"]`);
    assert.ok(button, 'layer selection control exists');
    button.dispatchEvent(new win.MouseEvent('click', { bubbles: true, shiftKey }));
  };
  openDOMPanel(document, 'layers');
  selectLayer(artworkIds[0]);
  selectLayer(artworkIds[1], true);
  assert.equal(
    document.querySelectorAll('[data-select-layer][aria-pressed="true"]').length,
    2,
    JSON.stringify(win.artworkGapSelection()),
  );
  openDOMPanel(document, 'align');
  const gapButtons = () => [...document.querySelectorAll('[data-distribute-gap]')];
  const checkGap = (spacing, eligible, title, busy = false) => {
    assert.equal(gapButtons().length, 2);
    for (const button of gapButtons()) {
      assert.equal(button.dataset.distributeSpacing, spacing);
      assert.equal(button.dataset.eligible, String(eligible));
      assert.equal(button.disabled, !eligible || busy);
      assert.match(button.title, title);
    }
  };
  const multiBefore = history(),
    multiDocument = JSON.stringify(win.nect.document);
  edit('0');
  checkGap('0', true, /Fixed gap 0 px/);
  edit('-7');
  checkGap('-7', true, /Fixed gap -7 px/);
  const explicitStorage = storage();
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  checkGap('3', true, /Fixed gap 3 px/, true);
  pointer(document, 'pointercancel', 114);
  checkGap('-7', true, /Fixed gap -7 px/);
  assert.equal(storage(), explicitStorage, 'cancel restores explicit preference');
  edit('');
  checkGap('', false, /Equal gaps/);
  openDOMPanel(document, 'layers');
  selectLayer(artworkIds[2], true);
  openDOMPanel(document, 'align');
  assert.equal(document.querySelectorAll('[data-select-layer][aria-pressed="true"]').length, 3);
  checkGap('', true, /Equal gaps/);
  const multiStorage = storage();
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  checkGap('10', true, /Fixed gap 10 px/, true);
  pointer(document, 'pointermove', 100);
  pointer(document, 'pointerup', 100);
  checkGap('', true, /Equal gaps/);
  assert.equal(storage(), multiStorage, 'origin restores exact local storage');
  edit('-7');
  const beforeSelectionChange = storage();
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  openDOMPanel(document, 'layers');
  selectLayer(artworkIds[2], true);
  openDOMPanel(document, 'align');
  pointer(document, 'pointerup', 114);
  el.blur();
  assert.equal(win.artworkGapTest().spacing, -7, 'selection change cancels preview');
  assert.equal(storage(), beforeSelectionChange);
  checkGap('-7', true, /Fixed gap -7 px/);
  el = field();
  el.focus();
  pointer(el, 'pointerdown');
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  checkGap('3', true, /Fixed gap 3 px/);
  assert.deepEqual(history(), multiBefore);
  assert.equal(JSON.stringify(win.nect.document), multiDocument);
  await win.happyDOM.close();
});
