import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';

// User feedback 2026-10-17: graph multi-selection already exists; batch deletion
// and Curve/Path Remap point multi-selection are the requested missing behavior.
test('Text outline conversion, contained units and Alt handles through DOM', async (t) => {
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
    '\nwindow.feedbackState = () => ({ past: history.past.length, selection: [...selection], points: selectedPointIds(selectedNode()), drag:drag?.type, message }); window.feedbackZoom = () => { graphZoom = 1; };';
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
  let a;
  const reset = (type = 'Text', locked = false) => {
    const doc = makeDocument();
    a = node(type, 100, 100);
    if (type === 'Text') a.text = 'BO';
    if (type === 'Curve')
      for (const p of a.points) {
        p.handleA.value = [0, 0];
        p.handleB.value = [0, 0];
      }
    const fill = node('Fill', 500, 100),
      output = node('Output', 800, 100);
    fill.params.path.source = a.id;
    output.params.path.source = fill.id;
    doc.layers[0].nodes = [a, fill, output];
    doc.layers[0].output = output.id;
    doc.layers[0].locked = locked;
    win.nect.load(serialize(doc));
    clickNode(a.id);
  };
  const live = () => win.nect.document.layers[0].nodes.find((n) => n.id === a.id);
  const snapshot = () => serialize(win.nect.document);
  const anchor = (p) => $(`.canvas-stage .point-hit[data-point="${p.id}"]:not([data-handle])`);
  const undo = () => key('z', $('.graph-scroll'), { ctrlKey: true });
  await t.test('conversion is explicit, replaces Text and one Undo restores it', () => {
    reset();
    const before = snapshot(),
      past = state().past;
    $('[data-bake-text-outline]').click();
    assert.equal(live().type, 'Multi Path');
    assert.equal(live().units.length, 2);
    assert.equal(state().past, past + 1);
    assert.equal($('[data-text]'), null);
    assert.ok($('[data-path-unit]'));
    assert.equal($('[data-add-point]'), null);
    undo();
    assert.equal(snapshot(), before);
  });
  await t.test('unit selection includes holes; unit button moves only that unit atomically', () => {
    reset();
    $('[data-bake-text-outline]').click();
    const n = live(),
      unit = n.units[0],
      ids = n.contours.filter((c) => unit.contours.includes(c.id)).flatMap((c) => c.points);
    assert.ok(unit.contours.length > 1);
    $(`[data-path-unit="${unit.id}"]`).click();
    assert.deepEqual(new Set(state().points), new Set(ids));
    const before = snapshot(),
      past = state().past;
    $(`[data-move-path-unit="${unit.id}"][data-delta="1,0"]`).click();
    for (const p of n.points)
      assert.deepEqual(live().points.find((q) => q.id === p.id).position.value, [
        p.position.value[0] + (ids.includes(p.id) ? 1 : 0),
        p.position.value[1],
      ]);
    assert.equal(state().past, past + 1);
    undo();
    assert.equal(snapshot(), before);
  });
  await t.test('locked conversion cannot mutate', () => {
    reset('Text', true);
    const before = snapshot();
    assert.equal($('[data-bake-text-outline]').disabled, true);
    $('[data-bake-text-outline]').click();
    assert.equal(snapshot(), before);
  });
  await t.test('Alt-click generates handles without anchor movement, one Undo', () => {
    reset('Curve');
    const p = live().points[1],
      before = snapshot(),
      past = state().past;
    fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
    fire(document, 'pointerup', ...p.position.value, { altKey: true });
    const updated = live().points[1];
    assert.deepEqual(updated.position.value, p.position.value);
    assert.ok(updated.handleA.value.some((v) => v !== 0));
    assert.ok(updated.handleB.value.some((v) => v !== 0));
    assert.equal(state().past, past + 1);
    undo();
    assert.equal(snapshot(), before);
  });
  await t.test('Alt-drag pulls symmetric handles; Escape and pointercancel roll back', () => {
    for (const cancel of [null, 'Escape', 'pointercancel']) {
      reset('Curve');
      const p = live().points[1],
        before = snapshot(),
        past = state().past;
      fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
      fire(document, 'pointermove', p.position.value[0] + 24, p.position.value[1] + 12, {
        altKey: true,
      });
      if (cancel === 'Escape') key('Escape', $('.canvas-stage'));
      else if (cancel) fire(document, cancel);
      fire(document, 'pointerup');
      if (cancel) {
        assert.equal(snapshot(), before);
        assert.equal(state().past, past);
      } else {
        assert.deepEqual(live().points[1].position.value, p.position.value);
        assert.deepEqual(live().points[1].handleB.value, [24, 12]);
        assert.deepEqual(live().points[1].handleA.value, [-24, -12]);
        assert.equal(state().past, past + 1);
      }
    }
  });
  await t.test('selected Multi Path unit drags all its anchors and holes only', () => {
    reset();
    $('[data-bake-text-outline]').click();
    const n = live(),
      unit = n.units[0],
      ids = n.contours.filter((c) => unit.contours.includes(c.id)).flatMap((c) => c.points);
    $(`[data-path-unit="${unit.id}"]`).click();
    const p = n.points.find((p) => p.id === ids[1]),
      past = state().past;
    fire(anchor(p), 'pointerdown', ...p.position.value);
    fire(document, 'pointermove', p.position.value[0] + 13, p.position.value[1] + 17, {
      altKey: true,
    });
    fire(document, 'pointerup');
    for (const p of n.points)
      assert.deepEqual(
        live().points.find((q) => q.id === p.id).position.value,
        p.position.value.map((v, i) => v + (ids.includes(p.id) ? [13, 17][i] : 0)),
      );
    assert.equal(state().past, past + 1);
  });
  await t.test('Alt-click keeps existing handles and creates no extra Undo', () => {
    reset('Curve');
    const p = live().points[1];
    fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
    fire(document, 'pointerup');
    const before = snapshot(),
      past = state().past;
    fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
    fire(document, 'pointerup');
    assert.equal(snapshot(), before);
    assert.equal(state().past, past);
  });
  await t.test('Alt handles respects driven or locked handle fields', () => {
    for (const driven of [false, true]) {
      reset('Curve');
      const doc = structuredClone(win.nect.document),
        n = doc.layers[0].nodes.find((n) => n.id === a.id);
      if (driven) n.points[1].handleA.expression = '0; 0';
      else n.points[1].handleA.locked = true;
      win.nect.load(serialize(doc));
      clickNode(a.id);
      const p = live().points[1],
        before = snapshot(),
        past = state().past;
      fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
      fire(document, 'pointermove', p.position.value[0] + 20, p.position.value[1] + 10, {
        altKey: true,
      });
      fire(document, 'pointerup');
      assert.equal(snapshot(), before);
      assert.equal(state().past, past);
    }
  });
  await t.test('Alt handles edits Path Remap without changing upstream Curve', () => {
    reset('Curve');
    const doc = structuredClone(win.nect.document),
      remap = node('Path Remap', 350, 100);
    remap.params.path.source = a.id;
    doc.layers[0].nodes.splice(1, 0, remap);
    win.nect.load(serialize(doc));
    clickNode(remap.id);
    const p = live().points[1],
      before = snapshot(),
      past = state().past;
    fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
    fire(document, 'pointermove', p.position.value[0] + 18, p.position.value[1] + 9, {
      altKey: true,
    });
    fire(document, 'pointerup');
    const edited = win.nect.document.layers[0].nodes.find((n) => n.id === remap.id);
    assert.ok(edited.edits.length);
    assert.deepEqual(live().points[1], p);
    assert.equal(state().past, past + 1);
    undo();
    assert.equal(snapshot(), before);
  });
  await t.test('dedicated unit marker drags one glyph with holes and cancels atomically', () => {
    for (const cancel of [false, true]) {
      reset();
      $('[data-bake-text-outline]').click();
      const n = live(),
        unit = n.units[0],
        ids = n.contours.filter((c) => unit.contours.includes(c.id)).flatMap((c) => c.points);
      $(`[data-path-unit="${unit.id}"]`).click();
      const marker = $(`[data-path-unit-move="${unit.id}"]`),
        before = snapshot(),
        past = state().past;
      assert.ok(marker);
      fire(marker, 'pointerdown', 200, 100);
      fire(document, 'pointermove', 220, 110, { altKey: true });
      if (cancel) fire(document, 'pointercancel');
      fire(document, 'pointerup');
      if (cancel) {
        assert.equal(snapshot(), before);
        assert.equal(state().past, past);
      } else {
        for (const p of n.points)
          assert.deepEqual(
            live().points.find((q) => q.id === p.id).position.value,
            p.position.value.map((v, i) => v + (ids.includes(p.id) ? [20, 10][i] : 0)),
          );
        assert.equal(state().past, past + 1);
        undo();
        assert.equal(snapshot(), before);
      }
    }
  });
  await t.test('Multi Path Alt-drag changes handles without moving selected glyph points', () => {
    reset();
    $('[data-bake-text-outline]').click();
    const n = live(),
      p = n.points[1],
      before = snapshot(),
      past = state().past;
    fire(anchor(p), 'pointerdown', ...p.position.value, { altKey: true });
    fire(document, 'pointermove', p.position.value[0] + 14, p.position.value[1] + 8, {
      altKey: true,
    });
    fire(document, 'pointerup');
    for (const old of n.points)
      assert.deepEqual(
        live().points.find((q) => q.id === old.id).position.value,
        old.position.value,
      );
    assert.deepEqual(live().points[1].handleB.value, [14, 8]);
    assert.equal(state().past, past + 1);
    undo();
    assert.equal(snapshot(), before);
  });
});
