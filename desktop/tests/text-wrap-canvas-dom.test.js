import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize, blankDocument } from '../src/core.js';
import { textWrapCanvasFixture } from './fixtures/text-wrap-canvas.js';

test('Canvas wrap width owns one coalesced gesture without changing Text or selection', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  let scale = 1,
    pan = [30, 40],
    next = 0;
  const frames = new Map();
  globalThis.requestAnimationFrame = win.requestAnimationFrame = (fn) => {
    frames.set(++next, fn);
    return next;
  };
  win.cancelAnimationFrame = (id) => frames.delete(id);
  const flush = () => {
    const jobs = [...frames.values()];
    frames.clear();
    jobs.forEach((fn) => fn(0));
  };
  const settle = () => {
    for (let i = 0; frames.size && i < 5; i++) flush();
    assert.equal(frames.size, 0);
  };
  const ctm = () => ({
    a: scale,
    b: 0,
    c: 0,
    d: scale,
    e: pan[0],
    f: pan[1],
    inverse() {
      return { a: 1 / scale, b: 0, c: 0, d: 1 / scale, e: -pan[0] / scale, f: -pan[1] / scale };
    },
  });
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: m.a * this.x + m.c * this.y + m.e, y: m.b * this.x + m.d * this.y + m.f };
    }
  };
  win.SVGElement.prototype.getScreenCTM = ctm;
  win.SVGSVGElement.prototype.getScreenCTM = ctm;
  win.HTMLElement.prototype.getBoundingClientRect = () => ({
    x: 0,
    y: 0,
    left: 0,
    top: 0,
    width: 1200,
    height: 800,
    right: 1200,
    bottom: 800,
  });
  for (const key of ['clientWidth', 'clientHeight'])
    Object.defineProperty(win.HTMLElement.prototype, key, {
      configurable: true,
      get() {
        return key === 'clientWidth' ? 1200 : 800;
      },
    });
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
  win.wrapWork = { render: 0, wires: 0, bounds: 0, updates: 0, canvas: 0 };
  for (const [name, key] of [
    ['render', 'render'],
    ['drawWires', 'wires'],
    ['updateGraphBounds', 'bounds'],
    ['updateGraphGesture', 'updates'],
    ['drawCanvas', 'canvas'],
  ]) {
    const pattern = new RegExp(`function ${name}\\([^)]*\\) \\{`);
    assert.match(source, pattern);
    source = source.replace(pattern, (m) => m + ` window.wrapWork.${key}++;`);
  }
  source +=
    '\nwindow.wrapTestLoad=(s)=>{replaceDocument(deserializeProject(s),{saved:true});render(true);};window.wrapTestContext=wrapResizeContext;window.wrapTestState=()=>({past:history.past.length,future:history.future.length,gesture:session.hasGesture,drag:drag?.type,selection:wrapResizeSelection()});';
  source +=
    '\nconst deadline=Date.now()+5000;while(!bundledFont){if(Date.now()>deadline)throw Error("Font unavailable");await new Promise(r=>setTimeout(r,10));}';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s),
    handle = () => $('[data-text-wrap-resize]');
  const fire = (el, type, x, y, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 7,
        button: 0,
        clientX: x,
        clientY: y,
        ...extra,
      }),
    );
  const key = (k) =>
    $('.canvas-stage').dispatchEvent(new win.KeyboardEvent('keydown', { bubbles: true, key: k }));
  const resetCounts = () => Object.keys(win.wrapWork).forEach((k) => (win.wrapWork[k] = 0));
  let fixture;
  const selectText = () => {
    const head = $(`.node[data-id="${fixture.textId}"] .node-head`);
    fire(head, 'pointerdown', 100, 100);
    fire(document, 'pointerup', 100, 100);
    settle();
  };
  const load = (transformed = false) => {
    fixture = textWrapCanvasFixture({ transformed });
    win.wrapTestLoad(serialize(fixture.doc));
    if ($('.alignment-panel')) $('.canvas-toolbar [data-action="align-panel"]').click();
    selectText();
    resetCounts();
  };
  const state = () => [
    serialize(win.nect.document),
    win.nect.export(),
    win.nect.fileState,
    win.wrapTestState().past,
    win.wrapTestState().future,
  ];
  const text = () => win.nect.document.layers[0].nodes.find((n) => n.id === fixture.textId);
  const press = () => {
    const c = win.wrapTestContext();
    assert.ok(c && handle());
    const p = c.handle.map((v, i) => v * scale + pan[i]);
    fire(handle(), 'pointerdown', ...p);
    return p;
  };
  await t.test(
    'coalesces moves/reflow, leaves graph untouched and commits one width-only Undo',
    () => {
      load();
      assert.equal(win.nect.fileState.dirty, false);
      const before = state(),
        identity = win.wrapTestState().selection,
        svg = $('.artboard').innerHTML,
        p = press();
      for (const dx of [-20, -40, -60, -100]) fire(document, 'pointermove', p[0] + dx, p[1]);
      assert.deepEqual(state(), before);
      assert.equal(win.wrapWork.updates, 0);
      assert.equal(win.wrapTestState().drag, 'text-wrap-resize');
      flush();
      assert.equal(win.wrapWork.updates, 1);
      assert.equal(win.wrapWork.canvas, 1);
      assert.equal(win.wrapWork.render, 0);
      assert.equal(win.wrapWork.wires, 0);
      assert.equal(win.wrapWork.bounds, 0);
      assert.deepEqual(state(), before);
      assert.notEqual($('.artboard').innerHTML, svg);
      assert.equal($('[data-text-wrap-width]').value, '220');
      fire(document, 'pointerup', p[0] - 100, p[1]);
      settle();
      const committed = win.nect.document,
        expected = structuredClone(fixture.doc);
      expected.layers[0].nodes[0].textWrap.width = 220;
      assert.deepEqual(committed, expected);
      assert.equal(win.wrapTestState().past, 1);
      assert.equal(win.wrapTestState().selection, identity);
      assert.equal(win.nect.fileState.dirty, true);
      assert.notEqual(win.nect.export(), before[1]);
      $('[data-action="undo"]').click();
      settle();
      assert.equal(serialize(win.nect.document), before[0]);
      assert.equal(win.nect.export(), before[1]);
      $('[data-action="redo"]').click();
      settle();
      assert.equal(text().textWrap.width, 220);
    },
  );
  await t.test(
    'Escape/pointercancel/blur discard queued or visible previews and preserve clean lifecycle/selection',
    () => {
      for (const [route, preview] of [
        ['Escape', false],
        ['Escape', true],
        ['pointercancel', true],
        ['blur', true],
      ]) {
        load();
        const before = state(),
          identity = win.wrapTestState().selection,
          svg = $('.artboard').innerHTML,
          p = press();
        fire(document, 'pointermove', p[0] - 120, p[1]);
        if (preview) flush();
        if (route === 'Escape') key('Escape');
        else if (route === 'blur') win.dispatchEvent(new win.Event('blur'));
        else fire(document, 'pointercancel', ...p);
        settle();
        assert.deepEqual(state(), before);
        assert.equal($('.artboard').innerHTML, svg);
        assert.equal(win.wrapTestState().selection, identity);
        assert.equal(win.wrapTestState().gesture, false);
      }
    },
  );
  await t.test(
    'pending release flushes latest width, while clicks and returned width stay no-op',
    () => {
      load();
      let p = press();
      fire(document, 'pointermove', p[0] - 80, p[1]);
      fire(document, 'pointerup', p[0] - 80, p[1]);
      settle();
      assert.equal(text().textWrap.width, 240);
      assert.equal(win.wrapTestState().past, 1);
      load();
      const before = state();
      p = press();
      fire(document, 'pointerup', ...p);
      settle();
      assert.deepEqual(state(), before);
      p = press();
      fire(document, 'pointermove', p[0] - 50, p[1]);
      flush();
      fire(document, 'pointermove', ...p);
      fire(document, 'pointerup', ...p);
      settle();
      assert.deepEqual(state(), before);
    },
  );
  await t.test(
    'whole-artwork transform and Canvas CTM inverse map to the same local width without changing selection',
    () => {
      scale = 2;
      pan = [90, -20];
      load(true);
      fire($('.shape[data-layer]'), 'pointerdown', 400, 300);
      fire(document, 'pointerup', 400, 300);
      settle();
      resetCounts();
      const before = state(),
        identity = win.wrapTestState().selection,
        p = press();
      assert.deepEqual(win.wrapTestContext().matrix, [1.5, 0, -0, 1.5, 20, 30]);
      fire(document, 'pointermove', p[0] - 180, p[1]);
      flush();
      fire(document, 'pointerup', p[0] - 180, p[1]);
      settle();
      assert.equal(text().textWrap.width, 260);
      assert.equal(win.wrapTestState().selection, identity);
      const expected = JSON.parse(before[0]);
      expected.layers[0].nodes[0].textWrap.width = 260;
      assert.deepEqual(win.nect.document, expected);
      $('[data-action="undo"]').click();
      settle();
      assert.equal(win.nect.export(), before[1]);
      scale = 1;
      pan = [30, 40];
    },
  );
  await t.test(
    'stale document and unsupported targets cannot accept an old handle or preview',
    () => {
      load();
      let p = press();
      fire(document, 'pointermove', p[0] - 100, p[1]);
      const replacement = blankDocument();
      win.nect.load(serialize(replacement));
      const current = JSON.stringify(win.nect.document);
      settle();
      fire(document, 'pointerup', p[0] - 100, p[1]);
      settle();
      assert.equal(JSON.stringify(win.nect.document), current);
      assert.equal(win.wrapTestState().gesture, false);
      for (const change of [
        (d) => (d.layers[0].locked = true),
        (d) => (d.layers[0].nodes[0].textWrap = undefined),
        (d) => (d.layers[0].nodes[0].alignment = 'end'),
      ]) {
        load();
        change(fixture.doc);
        win.nect.load(serialize(fixture.doc));
        selectText();
        assert.equal(handle(), null);
      }
    },
  );
});
