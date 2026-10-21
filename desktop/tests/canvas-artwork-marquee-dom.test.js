import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { marqueeFixture } from './fixtures/canvas-artwork-marquee.js';

test('Canvas whole-artwork marquee stays detached until release', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  let view = { scale: 2, x: 80, y: 40 },
    measures = 0;
  const rect = win.HTMLElement.prototype.getBoundingClientRect;
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    if (this.matches('.canvas-stage')) {
      measures++;
      return { left: 0, top: 0, right: 1100, bottom: 800, width: 1100, height: 800 };
    }
    return rect.call(this);
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: this.x * m.a + m.e, y: this.y * m.d + m.f };
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({
    inverse: () => ({
      a: 1 / view.scale,
      d: 1 / view.scale,
      e: -view.x / view.scale,
      f: -view.y / view.scale,
    }),
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
    '\nwindow.marqueeUI=()=>({activeLayer, selected, selection:[...selection], artworkTarget, artwork:[...artworkSelection], points:[...pointSelection.ids], focus:pointFocus, past:history.past.length, future:history.future.length, drag:drag?.type});';
  source += '\nwindow.marqueeRefresh=()=>updateCanvas(true);';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    stage = () => $('.canvas-stage'),
    ui = () => win.marqueeUI(),
    saved = () => serialize(win.nect.document),
    tick = () => new Promise((r) => setTimeout(r, 20));
  const fire = (el, type, p = [0, 0], extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 9,
        button: 0,
        clientX: p[0] * view.scale + view.x,
        clientY: p[1] * view.scale + view.y,
        ...extra,
      }),
    );
  const key = (k) =>
    stage().dispatchEvent(
      new win.KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true }),
    );
  const snapshot = () => ({
    saved: saved(),
    command: win.nect.commandState,
    file: win.nect.fileState,
    past: ui().past,
    future: ui().future,
  });
  const clickLayer = (id, shift = false) => {
    $(`[data-select-layer="${id}"]`).dispatchEvent(
      new win.MouseEvent('click', { bubbles: true, shiftKey: shift }),
    );
  };
  const drag = (a, b, extra = {}) => {
    fire($('.artboard'), 'pointerdown', a, extra);
    fire(document, 'pointermove', b, extra);
    fire(document, 'pointerup', b, extra);
  };
  const load = () => {
    view = { scale: 2, x: 80, y: 40 };
    const f = marqueeFixture();
    win.nect.load(serialize(f.doc));
    return f;
  };
  await t.test(
    'reverse overlap selects complete outer layers and synchronizes Layers/Graph/Inspector once',
    () => {
      const f = load(),
        before = snapshot(),
        graph = $('.graph'),
        inspector = $('.inspector-body');
      fire($('.artboard'), 'pointerdown', [140, 75]);
      const downMeasures = measures;
      fire(document, 'pointermove', [45, 45]);
      assert.equal(ui().drag, 'canvas-marquee');
      assert.deepEqual(ui().artwork, []);
      assert.equal($('.graph'), graph);
      assert.equal($('.inspector-body'), inspector);
      assert.equal(measures, downMeasures);
      assert.match($('.canvas-marquee').textContent, /Bounds overlap/);
      assert.equal(
        document.querySelectorAll('.canvas-marquee-guides rect:not([style*="display: none"])')
          .length,
        2,
      );
      fire(document, 'pointerup', [45, 45]);
      assert.deepEqual(ui().artwork, [f.a.layer.id, f.b.layer.id]);
      assert.equal(ui().activeLayer, f.b.layer.id);
      assert.equal(ui().selected, null);
      assert.deepEqual(ui().selection, []);
      assert.deepEqual(ui().points, []);
      assert.equal(document.querySelectorAll('[data-artwork-selection]').length, 2);
      assert.equal(document.querySelectorAll('.layers .layer.selected').length, 2);
      assert.equal(document.querySelectorAll('.graph .node.selected').length, 0);
      assert.ok($('.graph .node[data-id="' + f.b.shape.id + '"]'));
      assert.match($('.hint').textContent, /2 selected/);
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    'Shift unions the starting artwork set, replacements/empty/click threshold stay view-only',
    () => {
      const f = load();
      clickLayer(f.path.layer.id);
      const before = snapshot();
      fire($('.artboard'), 'pointerdown', [10, 10], { shiftKey: true });
      fire(document, 'pointermove', [130, 80]);
      fire(document, 'pointermove', [70, 80]);
      fire(document, 'pointerup', [70, 80]);
      assert.deepEqual(ui().artwork, [f.path.layer.id, f.a.layer.id]);
      assert.deepEqual(snapshot(), before);
      drag([90, 10], [135, 80]);
      assert.deepEqual(ui().artwork, [f.b.layer.id]);
      drag([300, 180], [350, 220]);
      assert.deepEqual(ui().artwork, []);
      clickLayer(f.a.layer.id);
      drag([10, 10], [10.5, 10.5], { shiftKey: true });
      assert.deepEqual(ui().artwork, []);
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    'transformed zoom/pan coordinates, paths/generated/reference and hidden/solo eligibility',
    () => {
      const f = load();
      view = { scale: 0.75, x: 190, y: 110 };
      const before = snapshot();
      drag([150, 25], [290, 180]);
      assert.deepEqual(ui().artwork, [f.path.layer.id, f.generated.layer.id, f.reference.id]);
      assert.ok(!ui().artwork.includes(f.hidden.layer.id));
      assert.deepEqual(snapshot(), before);
      const d = marqueeFixture();
      d.a.layer.solo = true;
      win.nect.load(serialize(d.doc));
      drag([10, 10], [390, 230]);
      assert.deepEqual(ui().artwork, [d.a.layer.id]);
    },
  );
  await t.test(
    'Escape, pointercancel and capture loss restore starting selection with no history',
    () => {
      const f = load();
      clickLayer(f.a.layer.id);
      const start = ui(),
        before = snapshot();
      for (const cancel of ['Escape', 'pointercancel', 'lostpointercapture']) {
        fire($('.artboard'), 'pointerdown', [10, 10]);
        fire(document, 'pointermove', [290, 180]);
        if (cancel === 'Escape') key(cancel);
        else fire(cancel === 'lostpointercapture' ? stage() : document, cancel, [290, 180]);
        assert.deepEqual(ui(), start);
        assert.deepEqual(snapshot(), before);
        assert.equal($('.canvas-marquee'), null);
      }
    },
  );
  await t.test(
    'anchor selection survives Cancel and is cleared only by a committed whole-artwork marquee',
    () => {
      const f = load();
      clickLayer(f.path.layer.id);
      fire($(`.graph .node[data-id="${f.path.shape.id}"] .type-tag`), 'pointerdown');
      fire(document, 'pointerup');
      const id = f.path.shape.points[0].id;
      fire($(`[data-point="${id}"]:not([data-handle])`), 'pointerdown', [160, 40]);
      fire(document, 'pointerup', [160, 40]);
      assert.deepEqual(ui().points, [id]);
      const before = ui(),
        doc = snapshot();
      fire($('.artboard'), 'pointerdown', [10, 10]);
      fire(document, 'pointermove', [125, 75]);
      key('Escape');
      assert.deepEqual(ui(), before);
      assert.deepEqual(snapshot(), doc);
      drag([20, 20], [70, 80]);
      assert.deepEqual(ui().artwork, [f.a.layer.id]);
      assert.deepEqual(ui().points, []);
      assert.equal(ui().focus, null);
      assert.deepEqual(snapshot(), doc);
    },
  );
  await t.test(
    'busy commands reject and replaced documents discard previews without restoring newer selection',
    async () => {
      let f = load();
      clickLayer(f.a.layer.id);
      fire($('.artboard'), 'pointerdown', [10, 10]);
      fire(document, 'pointermove', [130, 80]);
      const before = snapshot();
      assert.throws(
        () =>
          win.nect.dispatch({
            ...win.nect.commandState,
            type: 'rename',
            payload: { layerId: f.a.layer.id, name: 'Changed' },
          }),
        /GESTURE_ACTIVE/,
      );
      assert.deepEqual(snapshot(), before);
      key('Escape');
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'rename',
        payload: { layerId: f.a.layer.id, name: 'Changed' },
      });
      const newer = snapshot();
      fire(document, 'pointerup', [130, 80]);
      assert.deepEqual(snapshot(), newer);
      assert.deepEqual(ui().artwork, [f.a.layer.id]);
      fire($('.artboard'), 'pointerdown', [10, 10]);
      fire(document, 'pointermove', [130, 80]);
      win.marqueeRefresh();
      fire(document, 'pointerup', [130, 80]);
      assert.deepEqual(snapshot(), newer);
      assert.deepEqual(ui().artwork, [f.a.layer.id]);
      fire($('.artboard'), 'pointerdown', [10, 10]);
      fire(document, 'pointermove', [130, 80]);
      f = marqueeFixture();
      win.nect.load(serialize(f.doc));
      const next = snapshot();
      fire(document, 'pointerup', [130, 80]);
      assert.deepEqual(snapshot(), next);
      assert.deepEqual(ui().artwork, []);
      await tick();
    },
  );
  await t.test(
    'target drag, pan and direct anchor editing keep their existing gesture routes',
    () => {
      const f = load(),
        before = snapshot();
      fire($(`.shape[data-layer="${f.a.layer.id}"]`), 'pointerdown', [50, 50]);
      assert.equal(ui().drag, 'artwork');
      fire(document, 'pointercancel');
      assert.equal($('.canvas-marquee'), null);
      assert.deepEqual(snapshot(), before);
      fire($('.artboard'), 'pointerdown', [10, 10], { button: 1 });
      assert.equal(ui().drag, 'pan');
      fire(document, 'pointercancel');
      $('[data-tool="direct"]').click();
      fire($(`.shape[data-layer="${f.path.layer.id}"]`), 'pointerdown', [160, 40]);
      assert.notEqual(ui().drag, 'canvas-marquee');
      fire(document, 'pointercancel');
      assert.deepEqual(snapshot(), before);
    },
  );
});
