import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, blankLayer, node, serialize } from '../src/core.js';

// Synthetic DOM integration: not browser/WebView visual acceptance.
test('Text Tool workspace selection and Canvas creation through production handlers', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver', 'Event'])
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
  const matrix = {
    a: 1,
    b: 0,
    c: 0,
    d: 1,
    e: 0,
    f: 0,
    inverse() {
      return this;
    },
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
  win.SVGSVGElement.prototype.getScreenCTM = () => matrix;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.fixtureClean = () => documents.replace({saved:true}); window.feedbackState = () => ({ past: history.past.length, dirty: documents.dirty, selection: [...selection] }); window.feedbackZoom = () => { graphZoom = 1; };';
  source +=
    '\nconst deadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() > deadline) throw Error("Font timeout"); await new Promise(r => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (selector) => document.querySelector(selector);
  const fire = (el, type, x = 0, y = 0, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 7,
        button: 0,
        isPrimary: true,
        clientX: x,
        clientY: y,
        ...extra,
      }),
    );
  const key = (key, target = document.activeElement, extra = {}) =>
    target.dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key, ...extra }),
    );
  const snapshot = () => serialize(win.nect.document);
  const state = () => ({ ...win.nect.commandState, ...win.nect.fileState, ...win.feedbackState() });
  const textButton = () => $('[data-tool="Text"]');
  const menu = () => $('.text-tool-menu');
  const pause = () => new Promise((r) => setTimeout(r, 480));
  win.HTMLElement.prototype.getBoundingClientRect = () => ({
    left: 0,
    top: 0,
    right: 800,
    bottom: 600,
    width: 800,
    height: 600,
  });
  const canvasClick = () => {
    fire($('.artboard'), 'pointerdown', 120, 90);
    fire(document, 'pointerup', 120, 90);
  };
  const open = () => {
    textButton().focus();
    key('ArrowDown');
    assert.ok(menu());
  };
  const choose = (id) => {
    open();
    menu().querySelector(`[data-text-variant="${id}"]`).click();
  };
  let doc, existing;
  const reset = () => {
    doc = makeDocument();
    existing = node('Text', 40, 40);
    existing.text = 'Existing AB';
    existing.params.size.value = 38;
    existing.params.letterSpacing.value = 3;
    const fill = node('Fill'),
      out = node('Output');
    fill.params.path.source = existing.id;
    out.params.path.source = fill.id;
    doc.layers[0].nodes = [existing, fill, out];
    doc.layers[0].output = out.id;
    win.nect.load(serialize(doc));
    win.fixtureClean();
    fire($(`.node[data-id="${existing.id}"] .node-head strong`), 'pointerdown');
    fire(document, 'pointerup');
  };
  reset();
  await t.test(
    'normal click, hold, variant selection and Escape preserve authored state, dirty, revision and history',
    async () => {
      const before = snapshot(),
        initial = state();
      assert.equal(initial.dirty, false);
      fire(textButton(), 'pointerdown');
      fire(document, 'pointerup');
      textButton().click();
      assert.equal(textButton().getAttribute('aria-pressed'), 'true');
      assert.equal(snapshot(), before);
      assert.deepEqual(state(), initial);
      $('[data-tool="select"]').click();
      const holdTarget = textButton();
      fire(holdTarget, 'pointerdown');
      await pause();
      fire(holdTarget, 'pointerup');
      holdTarget.click();
      assert.ok(menu());
      key('Escape');
      assert.equal(menu(), null);
      assert.equal($('[data-tool="select"]').classList.contains('active'), true);
      choose('vertical-rl');
      assert.equal(textButton().dataset.textVariant, 'vertical-rl');
      assert.equal(textButton().querySelector('svg').dataset.icon, 'Text-vertical');
      assert.equal(snapshot(), before);
      assert.deepEqual(state(), initial);
      $('[data-tool="select"]').click();
      textButton().click();
      assert.equal(textButton().dataset.textVariant, 'vertical-rl');
      assert.equal(snapshot(), before);
      assert.deepEqual(state(), initial);
    },
  );
  await t.test(
    'Canvas creates both directions at the explicit point with one Undo/Redo and exact existing Text preservation',
    () => {
      for (const variant of ['horizontal-tb', 'vertical-rl']) {
        reset();
        const before = snapshot(),
          old = structuredClone(win.nect.document.layers[0]),
          past = state().past;
        choose(variant);
        canvasClick();
        assert.equal(state().past, past + 1);
        const text = win.nect.document.layers.at(-1).nodes[0];
        assert.equal(text.writingMode, variant);
        assert.deepEqual(text.params.position.value, [120, 90]);
        assert.deepEqual(win.nect.document.layers[0], old);
        assert.equal(textButton().getAttribute('aria-pressed'), 'true');
        const after = snapshot();
        $('[data-action="undo"]').click();
        assert.equal(snapshot(), before);
        assert.equal(state().dirty, false);
        const undone = state();
        choose(variant === 'horizontal-tb' ? 'vertical-rl' : 'horizontal-tb');
        assert.deepEqual(state(), undone, 'Tool selection preserves the pending Redo');
        $('[data-action="redo"]').click();
        assert.equal(snapshot(), after);
      }
    },
  );
  await t.test(
    'pointer cancellation, movement, blur, Escape and outside menu dismissal cannot create',
    async () => {
      reset();
      choose('horizontal-tb');
      const before = snapshot(),
        initial = state();
      for (const cancel of [
        () => fire(document, 'pointercancel', 120, 90),
        () => fire($('.canvas-stage'), 'lostpointercapture', 120, 90),
        () => win.dispatchEvent(new win.Event('blur')),
        () => key('Escape'),
        () => fire(document, 'pointermove', 150, 90),
      ]) {
        textButton().click();
        fire($('.artboard'), 'pointerdown', 120, 90);
        cancel();
        fire(document, 'pointerup', 120, 90);
        assert.equal(snapshot(), before);
        assert.deepEqual(state(), initial);
      }
      open();
      canvasClick();
      assert.equal(menu(), null);
      assert.equal(snapshot(), before);
      open();
      win.dispatchEvent(new win.Event('blur'));
      assert.equal(menu(), null);
      assert.equal(snapshot(), before);
      for (const cancel of [
        () => fire(document, 'pointercancel'),
        () => win.dispatchEvent(new win.Event('blur')),
        () => key('Escape'),
      ]) {
        fire(textButton(), 'pointerdown');
        cancel();
        await pause();
        fire(textButton(), 'pointerup');
        textButton().click();
        assert.equal(menu(), null);
        assert.equal(snapshot(), before);
        assert.deepEqual(state(), initial);
      }
    },
  );
  await t.test(
    'keyboard menu navigation, IME guards, input focus and existing Inspector controls',
    () => {
      reset();
      const before = snapshot();
      textButton().focus();
      key('ArrowDown', textButton(), { isComposing: true });
      key('ArrowDown', textButton(), { keyCode: 229 });
      assert.equal(menu(), null);
      open();
      key('End');
      assert.equal(document.activeElement.dataset.textVariant, 'vertical-rl');
      key('Home');
      assert.equal(document.activeElement.dataset.textVariant, 'horizontal-tb');
      key('ArrowDown');
      assert.equal(document.activeElement.dataset.textVariant, 'vertical-rl');
      key('Escape');
      assert.equal(document.activeElement, textButton());
      const field = $('textarea[data-text]');
      field.focus();
      key('ArrowDown', field);
      key('Escape', field, { isComposing: true });
      assert.equal(menu(), null);
      assert.equal(snapshot(), before);
      field.value += 'draft';
      field.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
      fire(textButton(), 'pointerdown');
      textButton().click();
      assert.equal(document.activeElement, field);
      assert.equal(snapshot(), before);
      field.value = existing.text;
      field.dispatchEvent(new win.CompositionEvent('compositionend', { bubbles: true }));
      reset();
      const old = structuredClone(win.nect.document.layers[0].nodes[0]);
      $(
        '.inspector [data-text-symbol="writingMode"][data-text-symbol-value="vertical-rl"]',
      ).click();
      const changed = win.nect.document.layers[0].nodes[0];
      assert.equal(changed.writingMode, 'vertical-rl');
      changed.writingMode = old.writingMode;
      assert.deepEqual(changed, old);
    },
  );
});
