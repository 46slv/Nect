import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';

// User feedback 2026-10-17: graph multi-selection already exists; batch deletion
// and Curve/Path Remap point multi-selection are the requested missing behavior.
test('multi-selection user feedback through real DOM handlers', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
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
    '\nwindow.feedbackState = () => ({ past: history.past.length, selection: [...selection] }); window.feedbackZoom = () => { graphZoom = 1; };';
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
        clientX: x,
        clientY: y,
        ...extra,
      }),
    );
  const key = (name, target = $('.graph-scroll'), extra = {}) =>
    target.dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name, ...extra }),
    );
  const card = (id) => $(`.graph .node[data-id="${id}"]`);
  const clickNode = (id, shiftKey = false) => {
    fire(card(id).querySelector('.node-head .type-tag'), 'pointerdown', 0, 0, { shiftKey });
    fire(document, 'pointerup');
  };
  const state = () => win.feedbackState();
  let doc, a, b, fill, output;
  const reset = (locked = false, curve = false, remap = false) => {
    doc = makeDocument();
    a = node(curve ? 'Curve' : 'Circle', 100, 100);
    if (curve) {
      a.points[0].handleA.value = [-20, 0];
      a.points[0].handleB.value = [20, 0];
    }
    b = node(remap ? 'Path Remap' : 'Rectangle', 500, 100);
    if (remap) b.params.path.source = a.id;
    fill = node('Fill', 900, 100);
    fill.params.path.source = a.id;
    output = node('Output', 1300, 100);
    output.params.path.source = fill.id;
    doc.layers[0].nodes = [a, b, fill, output];
    doc.layers[0].output = output.id;
    doc.layers[0].locked = locked;
    win.nect.load(serialize(doc));
    win.feedbackZoom();
    clickNode(a.id);
    clickNode(b.id, true);
  };
  const nodes = () => win.nect.document.layers[0].nodes;
  const snapshot = () => serialize(win.nect.document);

  await t.test('existing graph Shift-click adds and toggles membership', () => {
    reset();
    assert.deepEqual(new Set(state().selection), new Set([a.id, b.id]));
    clickNode(b.id, true);
    assert.deepEqual(state().selection, [a.id]);
  });
  await t.test('existing graph marquee selects intersecting cards', () => {
    reset();
    const viewport = $('.graph-scroll');
    Object.defineProperties(viewport, {
      clientWidth: { value: 2000 },
      clientHeight: { value: 800 },
    });
    const rect = (x, y, w, h) => ({
      x,
      y,
      left: x,
      top: y,
      right: x + w,
      bottom: y + h,
      width: w,
      height: h,
    });
    viewport.getBoundingClientRect = () => rect(0, 0, 2000, 800);
    $('.graph').getBoundingClientRect = () => rect(0, 0, 2000, 800);
    for (const n of [a, b, fill, output])
      card(n.id).getBoundingClientRect = () => rect(...n.position, 200, 100);
    fire(viewport, 'pointerdown', 50, 50);
    fire(document, 'pointermove', 750, 250);
    fire(document, 'pointerup', 750, 250);
    assert.deepEqual(new Set(state().selection), new Set([a.id, b.id]));
  });
  for (const name of ['Delete', 'Backspace']) {
    await t.test(
      `${name} deletes selected nodes atomically, cleans wires, and one Undo restores both`,
      () => {
        reset();
        const before = snapshot(),
          past = state().past;
        key(name);
        assert.equal(
          nodes().some((n) => n.id === a.id || n.id === b.id),
          false,
          'both selected nodes must be removed',
        );
        assert.ok(
          !nodes().find((n) => n.id === fill.id).params.path.source,
          'surviving input must not dangle',
        );
        assert.equal(state().past, past + 1, 'batch deletion is exactly one history entry');
        key('z', $('.graph-scroll'), { ctrlKey: true });
        assert.equal(snapshot(), before);
      },
    );
    await t.test(
      `${name} preserves protected Output while deleting eligible selected nodes`,
      () => {
        reset();
        clickNode(output.id, true);
        key(name);
        assert.ok(nodes().some((n) => n.id === output.id));
        assert.equal(
          nodes().some((n) => n.id === a.id || n.id === b.id),
          false,
        );
      },
    );
    await t.test(`${name} cannot mutate locked composition`, () => {
      reset(true);
      const before = snapshot(),
        past = state().past;
      key(name);
      assert.equal(snapshot(), before);
      assert.equal(state().past, past);
    });
    await t.test(`${name} leaves focused parameter text and IME composition safe`, () => {
      reset();
      const before = snapshot(),
        past = state().past;
      const input = card(b.id).querySelector('input');
      input.focus();
      key(name, input);
      $('.graph-scroll').focus();
      key(name, $('.graph-scroll'), { isComposing: true });
      $('#app').dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
      key(name);
      $('#app').dispatchEvent(new win.CompositionEvent('compositionend', { bubbles: true }));
      assert.equal(snapshot(), before);
      assert.equal(state().past, past);
    });
  }
  for (const remap of [false, true]) {
    await t.test(`${remap ? 'Path Remap' : 'Curve'} Shift-click toggles anchor membership`, () => {
      reset(false, true, remap);
      clickNode(remap ? b.id : a.id);
      const pick = (p, shiftKey = false) => {
        const hit = $(`.canvas-stage .point-hit[data-point="${p.id}"]:not([data-handle])`);
        fire(hit, 'pointerdown', ...p.position.value, { shiftKey });
        fire(document, 'pointerup', ...p.position.value, { shiftKey });
      };
      const selectedAnchors = () =>
        [...document.querySelectorAll('.canvas-stage .vertex.point-marker.active')].map(
          (e) => e.dataset.point,
        );
      pick(a.points[0]);
      pick(a.points[1], true);
      assert.deepEqual(new Set(selectedAnchors()), new Set([a.points[0].id, a.points[1].id]));
      pick(a.points[1], true);
      assert.deepEqual(selectedAnchors(), [a.points[0].id]);
    });

    await t.test(
      `${remap ? 'Path Remap' : 'Curve'} Shift-select points then drag moves both with one Undo`,
      () => {
        reset(false, true, remap);
        clickNode(remap ? b.id : a.id);
        const target = remap ? b : a;
        const anchor = (p) =>
          $(`.canvas-stage .point-hit[data-point="${p.id}"]:not([data-handle])`);
        const pick = (p, shiftKey = false) => {
          fire(anchor(p), 'pointerdown', ...p.position.value, { shiftKey });
          fire(document, 'pointerup', ...p.position.value, { shiftKey });
        };
        pick(a.points[0]);
        pick(a.points[1], true);
        const before = snapshot(),
          past = state().past;
        fire(anchor(a.points[1]), 'pointerdown', 450, 150);
        fire(document, 'pointermove', 470, 180, { altKey: true });
        fire(document, 'pointerup', 470, 180, { altKey: true });
        const changed = nodes().find((n) => n.id === target.id);
        if (remap) {
          for (const p of a.points.slice(0, 2)) {
            const edit = changed.edits.find((e) => e.pointId === p.id);
            assert.ok(edit, 'each selected point needs an edit');
            assert.deepEqual(edit.params?.position?.value ?? edit.position, [20, 30]);
          }
        } else {
          assert.deepEqual(changed.points[0].position.value, [270, 180]);
          assert.deepEqual(changed.points[1].position.value, [470, 180]);
          assert.deepEqual(changed.points[2].position.value, [350, 350]);
          assert.deepEqual(changed.points[0].handleA.value, [-20, 0]);
          assert.deepEqual(changed.points[0].handleB.value, [20, 0]);
        }
        assert.equal(state().past, past + 1);
        key('z', $('.canvas-stage'), { ctrlKey: true });
        assert.equal(snapshot(), before);
      },
    );
  }
  await t.test('Curve group movement rejects a locked selected anchor atomically', () => {
    reset(false, true);
    a.points[0].position.locked = true;
    win.nect.load(serialize(doc));
    clickNode(a.id);
    const hit = (p) => $(`.canvas-stage .point-hit[data-point="${p.id}"]:not([data-handle])`);
    for (const [i, p] of a.points.slice(0, 2).entries()) {
      fire(hit(p), 'pointerdown', ...p.position.value, { shiftKey: i > 0 });
      fire(document, 'pointerup', ...p.position.value, { shiftKey: i > 0 });
    }
    const before = snapshot(),
      past = state().past;
    fire(hit(a.points[1]), 'pointerdown', 450, 150);
    fire(document, 'pointermove', 470, 180, { altKey: true });
    fire(document, 'pointerup', 470, 180, { altKey: true });
    assert.equal(snapshot(), before);
    assert.equal(state().past, past);
  });
});
