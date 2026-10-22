import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { keyerPickFixture } from './fixtures/keyer-input-pick.js';

test('Keyer input picker document-neutral lifecycle', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: m.a * this.x + m.c * this.y + m.e, y: m.b * this.x + m.d * this.y + m.f };
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({
    inverse: () => ({ a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }),
  });
  win.SVGSVGElement.prototype.getScreenCTM = win.SVGElement.prototype.getScreenCTM;
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
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.pickUI=()=>({selected,picking:!!keyerInputPicker,drag:!!drag,wiring:!!wiring,past:history.past.length,future:history.future.length});';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  source += '\n//# sourceURL=main-keyer-input-pick-test.js';
  source += '\nwindow.pickSelect=id=>{selected=id;selection=new Set([id]);render();};';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const artwork = () => {
    try {
      return win.nect.export();
    } catch (error) {
      return { error: error.message };
    }
  };
  const $ = (s) => document.querySelector(s),
    ui = () => structuredClone(win.pickUI()),
    state = () => ({
      doc: win.nect.document,
      svg: artwork(),
      file: win.nect.fileState,
      command: win.nect.commandState,
      past: ui().past,
      future: ui().future,
    }),
    load = (mutate) => {
      const f = keyerPickFixture();
      mutate?.(f);
      win.nect.load(serialize(f.doc));
      fire($(`.graph .node[data-id="${f.keyer.id}"] .node-head strong`), 'pointerdown');
      fire(document, 'pointerup');
      return f;
    },
    open = () => {
      $('.inspector-body [data-keyer-pick]').click();
      assert.ok($('[data-keyer-input-picker]'));
      return $('[data-keyer-input-preview]');
    },
    fire = (el, type, p = [0, 0]) =>
      el.dispatchEvent(
        new win.PointerEvent(type, {
          bubbles: true,
          cancelable: true,
          button: 0,
          pointerId: 31,
          clientX: p[0],
          clientY: p[1],
        }),
      );

  await t.test(
    'start/hover/outside/alpha0 do not change document or history; partial straight RGB commits once',
    () => {
      const f = load(),
        before = state(),
        svg = open();
      assert.deepEqual(state(), before);
      assert.match($('[data-keyer-input-identity]').textContent, /4.*2 input pixels/);
      for (const p of [
        [0.5, 0.5],
        [2.5, 0.5],
        [-0.01, 0.5],
        [4, 0.5],
      ]) {
        fire(svg, 'pointermove', p);
        fire(svg, 'click', p);
        assert.deepEqual(state(), before);
        assert.ok(ui().picking);
      }
      fire(svg, 'pointermove', [1.5, 0.5]);
      assert.match($('dialog output').textContent, /#0c2238/);
      assert.deepEqual(state(), before);
      fire(svg, 'click', [1.5, 0.5]);
      assert.equal(ui().picking, false);
      assert.equal($('[data-keyer-input-picker]'), null);
      const after = state();
      assert.equal(after.past, before.past + 1);
      assert.deepEqual(after.doc.imageAssets, before.doc.imageAssets);
      assert.equal(
        after.doc.layers[0].nodes.find((n) => n.id === f.keyer.id).params.keyColor.value.hex,
        '#0c2238',
      );
      $('[data-action="undo"]').click();
      assert.deepEqual(win.nect.document, before.doc);
      assert.equal(win.nect.export(), before.svg);
      $('[data-action="redo"]').click();
      assert.deepEqual(win.nect.document, after.doc);
      assert.equal(win.nect.export(), after.svg);
      const noop = state();
      fire(open(), 'click', [1.5, 0.5]);
      assert.deepEqual(state(), noop);
    },
  );
  await t.test(
    'Escape, Cancel, blur and pointercancel restore the untouched working display',
    () => {
      for (const action of ['escape', 'cancel', 'blur', 'pointercancel']) {
        load();
        const before = state(),
          canvas = $('.artboard').outerHTML,
          svg = open();
        if (action === 'escape')
          svg.dispatchEvent(
            new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
          );
        if (action === 'cancel') $('[data-keyer-pick-cancel]').click();
        if (action === 'blur') win.dispatchEvent(new win.Event('blur'));
        if (action === 'pointercancel') fire(svg, 'pointercancel');
        assert.equal(ui().picking, false);
        assert.deepEqual(state(), before);
        assert.equal($('.artboard').outerHTML, canvas);
        fire(svg, 'click', [1.5, 0.5]);
        assert.deepEqual(state(), before);
      }
    },
  );
  await t.test(
    'modal owns pointer starts; synthetic outside drag cannot start Canvas/path/Graph gestures',
    () => {
      load();
      const before = state(),
        svg = open();
      fire($('.artboard'), 'pointerdown', [40, 40]);
      fire(document, 'pointermove', [120, 120]);
      fire(document, 'pointerup', [120, 120]);
      assert.equal(ui().drag, false);
      assert.equal(ui().wiring, false);
      assert.deepEqual(state(), before);
      fire(svg, 'pointercancel');
    },
  );
  await t.test(
    'revision, selection/target and Session/image reload discard stale preview and late clicks',
    () => {
      for (const change of ['revision', 'selection', 'session', 'image']) {
        const f = load(),
          svg = open();
        if (change === 'revision')
          win.nect.dispatch({
            ...win.nect.commandState,
            type: 'rename',
            payload: { layerId: f.layer.id, nodeId: f.keyer.id, name: 'Renamed' },
          });
        if (change === 'selection') win.pickSelect(f.image.id);
        if (change === 'session' || change === 'image') win.nect.load(serialize(f.doc));
        const after = state();
        fire(svg, 'click', [1.5, 0.5]);
        assert.equal(ui().picking, false);
        assert.deepEqual(state(), after);
      }
    },
  );
  await t.test(
    'locked and missing inputs do not open; driven picking edits only saved base',
    () => {
      load((f) => {
        f.keyer.params.keyColor.locked = true;
      });
      assert.equal($('.inspector-body [data-keyer-pick]').disabled, true);
      load((f) => {
        delete f.keyer.params.path.source;
      });
      const before = state();
      assert.match(before.svg.error, /Connect an Image/);
      $('.inspector-body [data-keyer-pick]').click();
      assert.equal(ui().picking, false);
      assert.deepEqual(state(), before);
      const f = load((f) => {
        f.keyer.params.keyColor.expression = 'value';
        f.keyer.params.keyColor.value.alpha = 0.4;
      });
      assert.match($('.inspector-body [data-keyer-pick]').textContent, /Pick base/);
      fire(open(), 'click', [1.5, 0.5]);
      const q = win.nect.document.layers[0].nodes.find((n) => n.id === f.keyer.id).params.keyColor;
      assert.equal(q.expression, 'value');
      assert.equal(q.value.alpha, 0.4);
      assert.equal(q.value.hex, '#0c2238');
    },
  );
});
