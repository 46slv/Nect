import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import {
  document as makeDoc,
  node,
  point,
  connect,
  serialize,
  evaluate,
  pathBounds,
} from '../src/core.js';
import { appendArtwork } from '../src/artwork-creation.js';
import { isolatedDomScenarios } from './run-isolated-dom-scenarios.js';

test('Pen inserts selected source segments atomically and keeps gesture ownership', async (rawT) => {
  const t = isolatedDomScenarios(rawT, import.meta.url);
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  let pending = [];
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.requestAnimationFrame = (fn) => {
    pending.push(fn);
    return pending.length;
  };
  const flush = () => {
    const jobs = pending;
    pending = [];
    jobs.forEach((fn) => fn(0));
  };
  const matrix = {
    a: 2,
    b: 0,
    c: 0,
    d: 2,
    e: 80,
    f: 40,
    inverse: () => ({ a: 0.5, b: 0, c: 0, d: 0.5, e: -40, f: -20 }),
  };
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: this.x * m.a + this.y * m.c + m.e, y: this.x * m.b + this.y * m.d + m.f };
    }
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
    '\nwindow.artworkUI = () => ({activeLayer, selected, artworkTarget, selection: [...selection], past: history.past.length, future: history.future.length, drag: drag?.type, tool, draft: penDraft?.layer, pointIds: [...pointSelection.ids], pointFocus}); window.markPenSaved = () => { documents.saved = serialize(history.doc); };';
  // These suites synchronously create/evaluate Text after startup. Await the
  // font condition inside the test module, rather than racing filesystem I/O.
  source +=
    '\nconst fontTestDeadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() >= fontTestDeadline) throw Error("Bundled test font did not finish loading"); await new Promise((r) => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  flush();
  const $ = (s) => document.querySelector(s),
    stage = () => $('.canvas-stage');
  const saved = () => serialize(win.nect.document),
    ui = () => win.artworkUI();
  const load = (d) => {
    win.nect.load(serialize(d));
    flush();
  };
  const click = (s) => {
    $(s).click();
    flush();
  };
  const xy = ([x, y]) => [x * 2 + 80, y * 2 + 40];
  const fire = (el, type, p = [0, 0], extra = {}) => {
    const screen = xy(p);
    const event = new win.PointerEvent(type, {
      bubbles: true,
      cancelable: true,
      pointerId: 3,
      button: 0,
      clientX: screen[0],
      clientY: screen[1],
      ...extra,
    });
    el.dispatchEvent(event);
    flush();
    return event;
  };
  const key = (el, name, extra = {}) => {
    const e = new win.KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: name,
      ...extra,
    });
    el.dispatchEvent(e);
    flush();
    return e.defaultPrevented;
  };
  const selectNode = (id) => {
    fire($(`.node[data-id="${id}"] .node-head`), 'pointerdown');
    fire(document, 'pointerup');
  };
  const shape = (id) => $(`.shape[data-layer="${id}"]`);
  const seed = () => {
    const d = makeDoc();
    d.layers[0].name = 'Keep original';
    return d;
  };
  const start = (p = [100, 100]) => {
    click('[data-tool="pen"]');
    fire($('.artboard'), 'pointerdown', p);
    fire(document, 'pointerup', p);
  };
  const add = (p = [250, 200], handles) => {
    fire($('.artboard'), 'pointerdown', p);
    if (handles) fire(document, 'pointermove', handles);
    fire(document, 'pointerup', handles || p);
  };
  const curve = () => ui().draft.nodes[0];

  const setupPath = (mutate) => {
    const d = makeDoc(),
      l = d.layers[0],
      n = node('Curve');
    n.points = [point(100, 100), point(400, 100), point(400, 300)];
    n.params.closed.value = false;
    l.nodes.push(n);
    connect(l, n.id, l.output, 'path');
    mutate?.(d, l, n);
    load(d);
    selectNode(n.id);
    click('[data-tool="pen"]');
    return { n, l, live: () => win.nect.document.layers[0].nodes.find((p) => p.id === n.id) };
  };
  await t.test(
    'segment release selects the new ID; Undo prunes it and Redo retains identity',
    () => {
      const f = setupPath(),
        before = saved();
      fire($('.artboard'), 'pointerdown', [250, 100]);
      assert.equal(ui().drag, 'path-insert');
      assert.equal(saved(), before);
      fire(document, 'pointerup', [250, 100]);
      assert.equal(f.live().points.length, 4);
      assert.equal(win.nect.document.layers.length, 1);
      const id = f.live().points[1].id;
      assert.deepEqual(ui().pointIds, [id]);
      assert.equal(ui().pointFocus.pointId, id);
      assert.ok($(`[data-select-panel-point="${id}"]`));
      assert.equal(ui().past, 1);
      fire(document, 'pointerup', [250, 100]);
      assert.equal(ui().past, 1);
      key(stage(), 'z', { ctrlKey: true });
      assert.equal(saved(), before);
      assert.deepEqual(ui().pointIds, []);
      key(stage(), 'z', { ctrlKey: true, shiftKey: true });
      assert.equal(f.live().points[1].id, id);
    },
  );
  await t.test(
    'drag, Escape, capture loss, cancellation and blur discard without stale release',
    () => {
      for (const mode of ['move', 'Escape', 'pointercancel', 'lostpointercapture', 'blur']) {
        const f = setupPath(),
          before = saved();
        fire($('.artboard'), 'pointerdown', [250, 100]);
        assert.equal(ui().drag, 'path-insert');
        if (mode === 'move') fire(document, 'pointermove', [253, 100]);
        else if (mode === 'Escape') key(stage(), 'Escape');
        else if (mode === 'blur') win.dispatchEvent(new win.Event('blur'));
        else fire(mode === 'lostpointercapture' ? stage() : document, mode, [250, 100]);
        fire(document, 'pointerup', [250, 100]);
        assert.equal(saved(), before);
        assert.equal(ui().past, 0);
        assert.equal(f.live().points.length, 3);
      }
    },
  );
  await t.test(
    'anchors select, Shift toggles and Alt creates handles rather than a new path',
    () => {
      const f = setupPath(),
        before = saved();
      fire($('.artboard'), 'pointerdown', [100, 100]);
      fire(document, 'pointerup', [100, 100]);
      assert.deepEqual(ui().pointIds, [f.n.points[0].id]);
      assert.equal(saved(), before);
      fire($('.artboard'), 'pointerdown', [100, 100], { shiftKey: true });
      fire(document, 'pointerup', [100, 100]);
      assert.deepEqual(ui().pointIds, []);
      fire($('.artboard'), 'pointerdown', [100, 100], { altKey: true });
      fire(document, 'pointerup', [100, 100], { altKey: true });
      assert.equal(f.live().points.length, 3);
      assert.ok(Math.hypot(...f.live().points[0].handleB.value) > 0);
      assert.equal(win.nect.document.layers.length, 1);
      assert.equal(ui().draft, undefined);
    },
  );
  await t.test(
    'modifiers, locked target, generated target and ambiguous crossings never start independent paths',
    () => {
      for (const modifier of [{ altKey: true }, { shiftKey: true }]) {
        setupPath();
        const before = saved();
        fire($('.artboard'), 'pointerdown', [250, 100], modifier);
        fire(document, 'pointerup', [250, 100]);
        assert.equal(saved(), before);
        assert.equal(ui().draft, undefined);
      }
      setupPath((d, l, n) => {
        n.points.push(point(100, 300));
        n.params.closed.value = true;
        n.points[1].position.value = [400, 300];
        n.points[2].position.value = [400, 100];
      });
      let before = saved();
      fire($('.artboard'), 'pointerdown', [250, 200]);
      fire(document, 'pointerup', [250, 200]);
      assert.equal(saved(), before);
      assert.match($('.status .message').textContent, /ambiguous/);
      assert.equal(ui().draft, undefined);
      setupPath((d, l) => {
        l.locked = true;
      });
      before = saved();
      fire($('.artboard'), 'pointerdown', [250, 100]);
      fire(document, 'pointerup', [250, 100]);
      assert.equal(saved(), before);
      assert.equal(ui().draft, undefined);
      const f = setupPath((d, l, n) => {
        const r = node('Path Remap');
        l.nodes.push(r);
        connect(l, n.id, r.id, 'path');
      });
      selectNode(win.nect.document.layers[0].nodes.at(-1).id);
      click('[data-tool="pen"]');
      before = saved();
      fire($('.artboard'), 'pointerdown', [250, 100]);
      fire(document, 'pointerup', [250, 100]);
      assert.equal(saved(), before);
      assert.equal(ui().draft, undefined);
      assert.match($('.status .message').textContent, /Generated path/);
    },
  );
  await t.test(
    'blank canvas still starts an independent draft and a live draft never edits old segments',
    () => {
      const f = setupPath(),
        before = saved();
      fire($('.artboard'), 'pointerdown', [200, 400]);
      fire(document, 'pointerup', [200, 400]);
      assert.ok(ui().draft);
      assert.equal(saved(), before);
      fire($('.artboard'), 'pointerdown', [250, 100]);
      fire(document, 'pointerup', [250, 100]);
      assert.equal(ui().draft.nodes[0].points.length, 2);
      assert.equal(f.live().points.length, 3);
      key(stage(), 'Escape');
      assert.equal(win.nect.document.layers.length, 2);
      assert.equal(ui().past, 1);
    },
  );
  await t.test('Canvas Delete and Backspace delete points, preserve nodes and use one Undo', () => {
    for (const name of ['Delete', 'Backspace']) {
      const f = setupPath(),
        before = saved();
      for (const p of [
        [100, 100],
        [400, 100],
      ]) {
        fire($('.artboard'), 'pointerdown', p, { shiftKey: true });
        fire(document, 'pointerup', p, { shiftKey: true });
      }
      assert.equal(ui().pointIds.length, 2);
      assert.equal(key(stage(), name, { repeat: true }), true);
      assert.equal(saved(), before);
      assert.equal(key(stage(), name, { isComposing: true }), false);
      assert.equal(saved(), before);
      const input = document.createElement('input');
      stage().append(input);
      key(input, name);
      assert.equal(saved(), before);
      assert.equal(key(stage(), name), true);
      assert.ok(f.live());
      assert.equal(f.live().points.length, 1);
      assert.equal(ui().past, 1);
      const after = saved();
      key(stage(), name, { repeat: true });
      assert.equal(saved(), after);
      key(stage(), 'z', { ctrlKey: true });
      assert.equal(saved(), before);
    }
  });
  win.happyDOM.abort();
});
