import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('contour point list navigation uses stable membership without authored edits', async (t) => {
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

  await t.test(
    'scope-only changes preserve selection, geometry, closure, hole grouping and history',
    () => {
      const f = compoundFixture();
      win.nect.load(serialize(f.doc));
      inspect(f.path.id);
      const id = f.path.id,
        before = saved(),
        commandState = win.nect.commandState;
      assert.equal(rows(id).length, 0);
      change(unit(id), 'scope-unit-a');
      assert.equal(rows(id).length, 24);
      assert.equal(rowIds(id)[0], f.path.contours[0].points[0]);
      panel(id).querySelector('[data-point-panel-page][data-page="1"]').click();
      assert.equal(rowIds(id)[0], f.path.contours[0].points[24]);
      change(contour(id), 'scope-hole');
      assert.deepEqual(rowIds(id), f.path.contours[1].points);
      assert.match(selectedOption(contour(id)).textContent, /Contour 2.*closed.*10 points/);
      assert.equal(panel(id).querySelector('[data-point-panel-page]'), null);
      assert.deepEqual(
        rows(id, '.inspector').map((r) => r.dataset.pointRow),
        f.path.contours[1].points,
      );
      assert.deepEqual(state().points, []);
      assert.deepEqual(win.nect.commandState, commandState);
      assert.equal(saved(), before);
      const exportBefore = win.nect.export();
      change(unit(id), 'scope-unit-b');
      change(contour(id), 'scope-open');
      assert.match(selectedOption(contour(id)).textContent, /open.*7 points/);
      assert.equal(win.nect.export(), exportBefore);
      assert.equal(saved(), before);
    },
  );

  await t.test(
    'row selection and Canvas cross-contour selection follow stable IDs and active page',
    () => {
      const f = compoundFixture();
      win.nect.load(serialize(f.doc));
      inspect(f.path.id);
      const id = f.path.id,
        before = saved();
      change(contour(id), 'scope-outer');
      panel(id).querySelector('[data-point-panel-page][data-page="1"]').click();
      const target = f.path.contours[0].points[29];
      panel(id).querySelector(`[data-select-panel-point="${target}"]`).click();
      assert.deepEqual(state().points, [target]);
      assert.deepEqual(rowIds(id), [target]);
      assert.equal(panel(id).querySelector('.point-active').dataset.pointRow, target);
      assert.equal(selectedValue(contour(id)), 'scope-outer');
      const other = f.path.contours[2].points[3];
      pick(other, true);
      assert.deepEqual(new Set(state().points), new Set([target, other]));
      assert.equal(selectedValue(unit(id)), 'scope-unit-b');
      assert.equal(selectedValue(contour(id)), 'scope-other');
      assert.deepEqual(rowIds(id), [other]);
      assert.equal(panel(id).querySelector('.point-active').dataset.pointRow, other);
      change(unit(id), 'scope-unit-a');
      assert.equal(rows(id).length, 24);
      assert.deepEqual(new Set(state().points), new Set([target, other]));
      assert.equal(saved(), before);
      assert.match(panel(id).textContent, /2 Canvas selected.*1 selected in list/);
      pick(other);
      assert.equal(selectedValue(contour(id)), 'scope-other');
      pick(other, true);
      assert.equal(rows(id).length, 0);
      assert.deepEqual(state().points, [target]);
    },
  );

  await t.test(
    'unit movement and point numeric edits retain their existing distinct one-Undo semantics',
    async () => {
      const f = compoundFixture();
      win.nect.load(serialize(f.doc));
      inspect(f.path.id);
      const id = f.path.id,
        before = saved();
      change(contour(id), 'scope-hole');
      assert.deepEqual(state().points, []);
      $(`.graph [data-path-unit="scope-unit-a"]`).click();
      const ids = f.path.contours.slice(0, 2).flatMap((c) => c.points);
      assert.deepEqual(new Set(state().points), new Set(ids));
      $(`.graph [data-move-path-unit="scope-unit-a"][data-delta="1,0"]`).click();
      assert.equal(state().past, 1);
      const live = win.nect.document.layers[0].nodes.find((n) => n.id === id);
      assert.deepEqual(live.contours, f.path.contours);
      assert.deepEqual(live.units, f.path.units);
      for (const p of f.path.points)
        assert.deepEqual(live.points.find((q) => q.id === p.id).position.value, [
          p.position.value[0] + (ids.includes(p.id) ? 1 : 0),
          p.position.value[1],
        ]);
      undo();
      assert.equal(saved(), before);
      change(contour(id), 'scope-hole');
      const target = f.path.contours[1].points[2];
      panel(id).querySelector(`[data-select-panel-point="${target}"]`).click();
      const input = panel(id).querySelector(
        `[data-key="${target}.position"] [data-value][data-axis="0"]`,
      );
      change(input, String(Number(input.value) + 4));
      await tick();
      assert.equal(state().past, 1);
      undo();
      assert.equal(saved(), before);
    },
  );

  await t.test(
    'large scoped lists stay paged and transient scopes reset on document reload',
    () => {
      const f = compoundFixture([1000, 80, 150, 7]);
      win.nect.load(serialize(f.doc));
      inspect(f.path.id);
      const id = f.path.id,
        before = saved();
      assert.equal(rows(id).length, 0);
      change(contour(id), 'scope-outer');
      assert.equal(rows(id).length, 24);
      panel(id).querySelector('[data-point-panel-page][data-page="1"]').click();
      assert.equal(rows(id).length, 24);
      change(contour(id), 'scope-open');
      assert.equal(rows(id).length, 7);
      assert.equal(saved(), before);
      win.nect.load(before);
      inspect(id);
      assert.equal(selectedValue(contour(id)), '');
      assert.equal(selectedValue(unit(id)), '');
      assert.equal(rows(id).length, 0);
    },
  );

  await t.test(
    'outlined Text uses real units while generated Remap uses actual contours and independent batch state',
    async () => {
      const d = makeDocument(),
        l = d.layers[0],
        text = node('Text', 40, 40),
        convert = node('Text to Path', 320, 40),
        remap = node('Path Remap', 600, 40),
        fill = node('Fill', 880, 40),
        out = node('Output', 1160, 40);
      text.text = 'BO';
      convert.params.path.source = text.id;
      remap.params.path.source = convert.id;
      fill.params.path.source = remap.id;
      out.params.path.source = fill.id;
      l.nodes = [text, convert, remap, fill, out];
      l.output = out.id;
      win.nect.load(serialize(d));
      inspect(remap.id);
      const path = win.nect.evaluate().run(convert.id),
        selectedContour = path.contours.at(-1),
        before = saved();
      assert.equal(unit(remap.id), null);
      change(contour(remap.id), selectedContour.id);
      assert.deepEqual(
        rowIds(remap.id),
        selectedContour.points.slice(0, 24).map((p) => p.id),
      );
      const checkbox = panel(remap.id).querySelector('[data-batch-point]');
      checkbox.checked = true;
      checkbox.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
      assert.deepEqual(state().points, []);
      change(contour(remap.id), path.contours[0].id);
      assert.match(
        $(`.graph .node[data-id="${remap.id}"] .batch-edit`).textContent,
        /1 batch checked/,
      );
      assert.equal(saved(), before);
      change(contour(remap.id), selectedContour.id);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'set-text-content',
        payload: { layerId: l.id, nodeId: text.id, text: 'I' },
      });
      await tick();
      assert.equal(
        selectedValue(contour(remap.id)),
        '',
        'stale contour ID falls back to the actual source',
      );
      const changed = win.nect.document.layers[0].nodes.find((n) => n.id === remap.id);
      assert.deepEqual(changed.edits, [], 'filtering never bakes or rebinds generated edits');
      win.nect.load(serialize(d));
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'bake-text-outline',
        payload: { layerId: l.id, nodeId: text.id },
      });
      inspect(text.id);
      const outlined = win.nect.document.layers[0].nodes.find((n) => n.id === text.id),
        baked = saved();
      assert.ok(outlined.units.length >= 2);
      change(unit(text.id), outlined.units[0].id);
      const c = outlined.contours.find((c) => outlined.units[0].contours.includes(c.id));
      change(contour(text.id), c.id);
      assert.deepEqual(rowIds(text.id), c.points.slice(0, 24));
      assert.equal(saved(), baked);
    },
  );
});
