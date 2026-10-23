import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('Layer name drags follow visual order and retain transaction boundaries', async (t) => {
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
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return { x: this.x, y: this.y };
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
    '\nwindow.orderingUI = () => ({activeLayer, selected, artworkTarget, selection: [...selection], characterEdit: characterEdit ? { ...characterEdit, selected: [...characterEdit.selected] } : null, selectedGuide, remapBatches: [...remapBatches].map(([id, batch]) => [id, { selected: [...batch.selected], delta: [...batch.delta] }]), past: history.past.length, future: history.future.length, drag: drag?.type, wiring: !!wiring, draft: penDraft?.layer, nameEdit, zoom, pan: [...pan], graphZoom});  window.testBeginName = beginNameEdit;';
  // These suites synchronously create/evaluate Text after startup. Await the
  // font condition inside the test module, rather than racing filesystem I/O.
  source +=
    '\nconst fontTestDeadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() >= fontTestDeadline) throw Error("Bundled test font did not finish loading"); await new Promise((r) => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  flush();
  const $ = (s) => document.querySelector(s),
    ui = () => win.orderingUI(),
    saved = () => serialize(win.nect.document),
    load = (doc) => {
      win.nect.load(serialize(doc));
      flush();
    },
    click = (selector) => {
      $(selector).click();
      flush();
    },
    key = (el, name, extra = {}) => {
      const e = new win.KeyboardEvent('keydown', {
        bubbles: true,
        cancelable: true,
        key: name,
        ...extra,
      });
      el.dispatchEvent(e);
      flush();
      return e.defaultPrevented;
    },
    fire = (el, type, extra = {}) => {
      el.dispatchEvent(
        new win.PointerEvent(type, {
          bubbles: true,
          cancelable: true,
          pointerId: 3,
          button: 0,
          clientX: 180,
          clientY: 180,
          ...extra,
        }),
      );
      flush();
    },
    select = (id) => click(`[data-select-layer="${id}"]`);
  const captures = new WeakMap();
  win.HTMLElement.prototype.setPointerCapture = function (id) {
    captures.set(this, id);
  };
  win.HTMLElement.prototype.hasPointerCapture = function (id) {
    return captures.get(this) === id;
  };
  win.HTMLElement.prototype.releasePointerCapture = function () {
    captures.delete(this);
  };
  const rows = () => [...document.querySelectorAll('.layers .layer[data-layer]')];
  const row = (id) => rows().find((el) => el.dataset.layer === id);
  const rect = (el) => {
    const i = rows().indexOf(el);
    return {
      top: 100 + i * 70,
      bottom: 170 + i * 70,
      height: 70,
      left: 300,
      right: 600,
      width: 300,
    };
  };
  const layout = () => {
    for (const el of rows()) el.getBoundingClientRect = () => rect(el);
  };
  document.elementFromPoint = (x, y) =>
    rows()
      .find((el) => y >= rect(el).top && y < rect(el).bottom)
      ?.querySelector('.name') || null;
  const seed = (doc = orderingDocument()) => {
    load(doc);
    layout();
    return doc.layers.map((l) => l.id);
  };
  const pointer = (el, type, y, extra = {}) =>
    fire(el, type, { clientX: 400, clientY: y, isPrimary: true, ...extra });
  const start = (id, target, edge = 'above', name = true) => {
    const origin = row(id),
      y = rect(row(target))[edge === 'above' ? 'top' : 'bottom'] + (edge === 'above' ? 10 : -10);
    pointer(name ? origin.querySelector('.name') : origin, 'pointerdown', rect(origin).top + 20);
    pointer(origin, 'pointermove', y);
    return { origin, y };
  };
  const move = (...args) => {
    const { origin, y } = start(...args);
    pointer(origin, 'pointerup', y);
    layout();
  };
  const ids = () => win.nect.document.layers.map((l) => l.id);
  await t.test('name drag moves adjacent rows both ways and is one exact Undo/Redo', () => {
    const [a, b, c] = seed();
    select(a);
    layout();
    const before = saved(),
      selected = ui().artworkTarget;
    move(b, c, 'above');
    assert.deepEqual(ids(), [a, c, b]);
    assert.equal(ui().past, 1);
    assert.equal(ui().artworkTarget, selected);
    row(b)
      .querySelector('.name')
      .dispatchEvent(new win.MouseEvent('click', { bubbles: true, cancelable: true, detail: 1 }));
    flush();
    assert.equal(ui().artworkTarget, selected, 'drag compatibility click does not select/rename');
    const expected = JSON.parse(before);
    expected.layers = [expected.layers[0], expected.layers[2], expected.layers[1]];
    assert.equal(saved(), serialize(expected));
    const after = saved();
    click('[data-action="undo"]');
    assert.equal(saved(), before);
    layout();
    click('[data-action="redo"]');
    assert.equal(saved(), after);
    layout();
    move(b, c, 'below');
    assert.equal(saved(), before);
  });
  await t.test('row padding supports both insertion edges and front/back extremes', () => {
    const [a, b, c] = seed();
    move(a, c, 'above', false);
    assert.deepEqual(ids(), [b, c, a]);
    move(a, b, 'below', false);
    assert.deepEqual(ids(), [a, b, c]);
    move(a, c, 'below', false);
    assert.deepEqual(ids(), [b, a, c]);
    const offsets = win.nect.document.layers.map((l) =>
      win.nect.export().indexOf('data-layer="' + l.name + '"'),
    );
    assert.ok(offsets.every((n) => n >= 0));
    assert.deepEqual(
      offsets,
      [...offsets].sort((a, b) => a - b),
    );
    assert.deepEqual(
      rows().map((el) => el.dataset.layer),
      [c, a, b],
    );
  });
  await t.test('a plain name click still selects; a no-op drop preserves Redo', () => {
    const [a, b, c] = seed();
    move(b, c, 'above');
    click('[data-action="undo"]');
    layout();
    const before = saved();
    assert.equal(ui().future, 1);
    move(a, b, 'below');
    assert.equal(saved(), before);
    assert.equal(ui().past, 0);
    assert.equal(ui().future, 1);
    const origin = row(b),
      y = rect(origin).top + 20;
    pointer(origin.querySelector('.name'), 'pointerdown', y);
    assert.equal(
      origin.hasPointerCapture(3),
      false,
      'capture starts only after crossing the drag threshold',
    );
    pointer(origin.querySelector('.name'), 'pointerup', y);
    origin
      .querySelector('.name')
      .dispatchEvent(new win.MouseEvent('click', { bubbles: true, cancelable: true, detail: 1 }));
    flush();
    assert.equal(ui().artworkTarget, b);
    assert.equal(saved(), before);
    assert.equal(ui().future, 1);
  });
  await t.test(
    'pointer cancellation, capture loss, Escape, blur and other pointers cannot commit',
    () => {
      for (const kind of [
        'pointercancel',
        'lostpointercapture',
        'Escape',
        'blur',
        'other-pointer',
      ]) {
        const [a, b, c] = seed(),
          before = saved(),
          held = start(a, c);
        if (kind === 'blur') window.dispatchEvent(new win.Event('blur'));
        else if (kind === 'Escape') key(held.origin, 'Escape');
        else if (kind === 'other-pointer')
          pointer(held.origin, 'pointerup', held.y, { pointerId: 9 });
        else pointer(held.origin, kind, held.y);
        if (kind === 'other-pointer') {
          assert.equal(saved(), before);
          pointer(held.origin, 'pointercancel', held.y);
        }
        pointer(document, 'pointerup', held.y);
        assert.equal(saved(), before, kind);
        assert.equal(ui().past, 0, kind);
      }
    },
  );
  await t.test('locked sources, action buttons and stale revisions do not reorder', () => {
    const doc = orderingDocument();
    doc.layers[0].locked = true;
    let [a, b, c] = seed(doc),
      before = saved();
    move(a, c);
    assert.equal(saved(), before);
    [a, b, c] = seed();
    before = saved();
    const origin = row(a);
    pointer(origin.querySelector('[data-visible]'), 'pointerdown', rect(origin).top + 20);
    pointer(origin, 'pointerup', 110);
    assert.equal(saved(), before);
    const held = start(a, c);
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'order-layer',
      payload: { layerId: b, direction: 'forward' },
    });
    flush();
    const changed = saved();
    pointer(document, 'pointerup', held.y);
    assert.equal(saved(), changed);
    assert.equal(ui().past, 1);
  });
  await win.happyDOM.close();
});
