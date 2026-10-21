import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('scoped point selection is view-only, complete and stable across paging', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
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
  win.SVGElement.prototype.getScreenCTM = () => ({ inverse: () => ({}) });
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
    '\nwindow.scopeState = () => ({points: [...pointSelection.ids], focus: pointFocus, past: history.past.length});';
  source += '\nwhile (!bundledFont) await new Promise(r => setTimeout(r, 10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const tick = () => new Promise((r) => setTimeout(r, 25));
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
  const change = (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const inspect = (id) => {
    fire($(`.graph .node[data-id="${id}"] .node-head .type-tag`), 'pointerdown');
    fire(document, 'pointerup');
  };
  const panel = (id, region = '.graph') => $(`${region} [data-point-panel="${id}"]`);
  const rows = (id, region) => [...panel(id, region).querySelectorAll('[data-point-row]')];
  const rowIds = (id) => rows(id).map((row) => row.dataset.pointRow);
  const unit = (id) => panel(id).querySelector('[data-point-panel-unit]');
  const contour = (id) => panel(id).querySelector('[data-point-panel-contour]');
  // happy-dom's parsed select.value can lag its selected attribute; native checks read the real control value.
  const selectedOption = (select) => select.querySelector('option[selected]');
  const selectedValue = (select) => selectedOption(select)?.value || '';
  const saved = () => serialize(win.nect.document);
  const state = () => win.scopeState();
  const undo = () => $('[data-action="undo"]').click();
  const pick = (id, shiftKey = false) => {
    const el = $(`.canvas-stage .point-hit[data-point="${id}"]:not([data-handle])`);
    assert.ok(el, `Canvas point ${id}`);
    const x = Number(el.getAttribute('cx')),
      y = Number(el.getAttribute('cy'));
    fire(el, 'pointerdown', x, y, { shiftKey });
    fire(document, 'pointerup', x, y);
  };

  const action = (id, kind, region = '.graph') =>
    panel(id, region).querySelector(`[data-point-scope-action="${kind}"]`);
  const act = (id, kind, region) => action(id, kind, region).click();
  const checkpoint = () => ({
    saved: saved(),
    command: win.nect.commandState,
    file: win.nect.fileState,
    past: state().past,
  });
  const neutral = (before) => assert.deepEqual(checkpoint(), before);
  const load = (f = compoundFixture()) => {
    win.nect.load(serialize(f.doc));
    inspect(f.path.id);
    return f;
  };
  const oldClick = (button) => {
    document.querySelector('#app').append(button);
    button.click();
    button.remove();
  };

  await t.test(
    'all spans every page and preserves active ID; selected-only inversion keeps a fixed full contour target',
    () => {
      const f = load(),
        id = f.path.id,
        before = checkpoint();
      change(contour(id), 'scope-outer');
      panel(id).querySelector('[data-point-panel-page][data-page="1"]').click();
      const active = f.path.contours[0].points[29];
      panel(id).querySelector(`[data-select-panel-point="${active}"]`).click();
      assert.equal(rows(id).length, 1);
      assert.match(action(id, 'all').getAttribute('aria-label'), /contour 1: 40 anchors/);
      act(id, 'all');
      assert.deepEqual(state().points, f.path.contours[0].points);
      assert.equal(state().focus.pointId, active);
      assert.equal(panel(id).querySelector('.point-active').dataset.pointRow, active);
      assert.equal(
        rows(id).length,
        16,
        'active ID stays visible on page 2, without expanding 40 rows',
      );
      assert.equal(rows(id, '.inspector').length, 16);
      act(id, 'invert');
      assert.deepEqual(state().points, []);
      assert.equal(state().focus, null);
      assert.equal(rows(id).length, 0);
      assert.match(
        action(id, 'invert').textContent,
        /40/,
        'zero selected rows do not redefine target',
      );
      act(id, 'invert', '.inspector');
      assert.deepEqual(state().points, f.path.contours[0].points);
      assert.equal(state().focus.pointId, f.path.contours[0].points[0]);
      assert.equal(rows(id).length, 24);
      neutral(before);
    },
  );
  await t.test(
    'unit includes its hole; contour complement drops existing outside-scope selection',
    () => {
      const f = load(),
        id = f.path.id,
        before = checkpoint();
      const inside = f.path.contours[0].points[1],
        outside = f.path.contours[2].points[2];
      pick(inside);
      pick(outside, true);
      change(contour(id), 'scope-outer');
      act(id, 'invert');
      assert.deepEqual(
        state().points,
        f.path.contours[0].points.filter((id) => id !== inside),
      );
      assert.ok(!state().points.includes(outside));
      change(unit(id), 'scope-unit-a');
      act(id, 'all');
      assert.deepEqual(
        state().points,
        f.path.contours.slice(0, 2).flatMap((c) => c.points),
      );
      assert.match(action(id, 'all').getAttribute('aria-label'), /unit 1: 50 anchors/);
      change(contour(id), 'scope-hole');
      act(id, 'all');
      assert.deepEqual(state().points, f.path.contours[1].points);
      assert.equal(rows(id).length, 10);
      neutral(before);
    },
  );
  await t.test(
    'Curve is bounded to this source path and empty sources have disabled clear controls',
    () => {
      const f = compoundFixture(),
        curve = node('Curve');
      f.layer.nodes.unshift(curve);
      win.nect.load(serialize(f.doc));
      inspect(curve.id);
      const before = checkpoint();
      assert.match(
        action(curve.id, 'all').getAttribute('aria-label'),
        new RegExp(`this path: ${curve.points.length} anchors`),
      );
      act(curve.id, 'all');
      assert.deepEqual(
        state().points,
        curve.points.map((p) => p.id),
      );
      neutral(before);
      const empty = node('Multi Path');
      empty.points = [];
      empty.contours = [];
      empty.units = [];
      f.layer.nodes.unshift(empty);
      win.nect.load(serialize(f.doc));
      inspect(empty.id);
      const blank = checkpoint();
      assert.equal(action(empty.id, 'all').disabled, true);
      assert.equal(action(empty.id, 'invert').disabled, true);
      act(empty.id, 'all');
      assert.deepEqual(state().points, []);
      neutral(blank);
    },
  );
  await t.test(
    'locked/driven anchors remain selected and existing edits reject the complete group',
    () => {
      const f = compoundFixture();
      f.doc.schemaVersion = 42;
      f.path.points.find((p) => p.id === f.path.contours[0].points[1]).position.locked = true;
      f.path.points.find((p) => p.id === f.path.contours[0].points[2]).position.expression =
        'value; value';
      load(f);
      const before = checkpoint(),
        id = f.path.id;
      change(contour(id), 'scope-outer');
      act(id, 'all');
      assert.deepEqual(state().points, f.path.contours[0].points);
      assert.equal(panel(id).querySelector('[data-apply-point-delta]').disabled, true);
      assert.equal(panel(id).querySelector('[data-point-bounds]').dataset.state, 'unavailable');
      assert.throws(
        () =>
          win.nect.dispatch({
            ...win.nect.commandState,
            type: 'translate-points',
            payload: { layerId: f.layer.id, nodeId: id, pointIds: state().points, delta: [1, 0] },
          }),
        /locked|driven/i,
      );
      neutral(before);
    },
  );
  await t.test(
    'stale scope and stale revision tokens reject without changing selection or document',
    () => {
      const f = load(),
        id = f.path.id;
      change(contour(id), 'scope-outer');
      const staleScope = action(id, 'all').cloneNode(true);
      change(contour(id), 'scope-hole');
      act(id, 'all');
      const before = checkpoint(),
        points = state().points;
      oldClick(staleScope);
      neutral(before);
      assert.deepEqual(state().points, points);
      const staleRevision = action(id, 'invert').cloneNode(true);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'rename',
        payload: { layerId: f.layer.id, nodeId: id, name: 'Renamed compound' },
      });
      const renamed = checkpoint();
      oldClick(staleRevision);
      neutral(renamed);
      assert.deepEqual(state().points, points);
      assert.match(document.body.textContent, /scope or revision changed/);
    },
  );
  await t.test(
    'selection preserves redo and full authored geometry, IDs, units and contour relationships',
    () => {
      const f = load(),
        id = f.path.id;
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'translate-points',
        payload: {
          layerId: f.layer.id,
          nodeId: id,
          pointIds: [f.path.contours[0].points[0]],
          delta: [1, 0],
        },
      });
      const moved = saved();
      undo();
      const before = checkpoint();
      assert.equal($('[data-action="redo"]').disabled, false);
      change(unit(id), 'scope-unit-b');
      act(id, 'all');
      act(id, 'invert');
      act(id, 'invert');
      neutral(before);
      assert.equal($('[data-action="redo"]').disabled, false);
      $('[data-action="redo"]').click();
      assert.equal(saved(), moved);
    },
  );
  await t.test(
    'generated text contours use current IDs and keep independent batch/held edit data unchanged',
    async () => {
      const d = makeDocument(),
        l = d.layers[0],
        text = node('Text'),
        convert = node('Text to Path'),
        remap = node('Path Remap'),
        fill = node('Fill'),
        out = node('Output');
      text.text = 'BO';
      convert.params.path.source = text.id;
      remap.params.path.source = convert.id;
      fill.params.path.source = remap.id;
      out.params.path.source = fill.id;
      l.nodes = [text, convert, remap, fill, out];
      l.output = out.id;
      win.nect.load(serialize(d));
      inspect(remap.id);
      const source = win.nect.evaluate().run(convert.id),
        c = source.contours.at(-1),
        id = remap.id;
      change(contour(id), c.id);
      const checkbox = panel(id).querySelector('[data-batch-point]');
      checkbox.checked = true;
      checkbox.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
      const before = checkpoint();
      act(id, 'all');
      assert.deepEqual(
        state().points,
        c.points.map((p) => p.id),
      );
      assert.equal(state().focus.pointId, c.points[0].id);
      assert.match($(`.graph .node[data-id="${id}"] .batch-edit`).textContent, /1 batch checked/);
      act(id, 'invert');
      assert.deepEqual(state().points, []);
      act(id, 'invert');
      assert.deepEqual(
        state().points,
        c.points.map((p) => p.id),
      );
      neutral(before);
      const old = action(id, 'all').cloneNode(true);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'set-text-content',
        payload: { layerId: l.id, nodeId: text.id, text: 'I' },
      });
      await tick();
      const changed = checkpoint(),
        priorSelection = state().points;
      oldClick(old);
      neutral(changed);
      assert.deepEqual(state().points, priorSelection);
      act(id, 'all');
      const current = win.nect.evaluate().run(convert.id);
      assert.deepEqual(
        state().points,
        current.contours.flatMap((c) => c.points.map((p) => p.id)),
      );
      assert.deepEqual(win.nect.document.layers[0].nodes.find((n) => n.id === id).edits, []);
      neutral(changed);
    },
  );
  await t.test(
    'explicitly outlined text uses actual unit membership without another bake or Undo entry',
    () => {
      const d = makeDocument(),
        l = d.layers[0],
        text = node('Text');
      text.text = 'BO';
      l.nodes.push(text);
      win.nect.load(serialize(d));
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'bake-text-outline',
        payload: { layerId: l.id, nodeId: text.id },
      });
      inspect(text.id);
      const owned = win.nect.document.layers[0].nodes.find((n) => n.id === text.id),
        before = checkpoint();
      change(unit(text.id), owned.units[0].id);
      act(text.id, 'all');
      assert.deepEqual(
        state().points,
        owned.contours
          .filter((c) => owned.units[0].contours.includes(c.id))
          .flatMap((c) => c.points),
      );
      assert.ok(rows(text.id).length <= 24);
      neutral(before);
    },
  );
});
