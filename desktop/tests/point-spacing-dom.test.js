import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { node, connect, serialize } from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('numeric anchor spacing controls use stable snapshots and atomic Session commands', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
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
    '\nwindow.pointIntervalUI=()=>({selected,selection:[...selection],points:[...pointSelection.ids],focus:pointFocus,past:history.past.length,future:history.future.length});';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  source += '\n//# sourceURL=main-point-spacing-test.js';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    ui = () => structuredClone(win.pointIntervalUI()),
    snapshot = () => ({
      doc: win.nect.document,
      svg: win.nect.export(),
      file: win.nect.fileState,
      command: win.nect.commandState,
      past: ui().past,
      future: ui().future,
    }),
    fire = (el, type, p = [0, 0], extra = {}) =>
      el.dispatchEvent(
        new win.PointerEvent(type, {
          bubbles: true,
          cancelable: true,
          pointerId: 18,
          button: 0,
          clientX: p[0],
          clientY: p[1],
          ...extra,
        }),
      ),
    pick = (id, shiftKey = false) => {
      const el = $(`.point-hit[data-point="${id}"]:not([data-handle])`);
      assert.ok(el, id);
      const p = [Number(el.getAttribute('cx')), Number(el.getAttribute('cy'))];
      fire(el, 'pointerdown', p, { shiftKey });
      fire(document, 'pointerup', p, { shiftKey });
    },
    button = (action, region = '.inspector-body') =>
      $(`${region} [data-point-set-spacing="${action}"]`),
    act = (action, region) => {
      const b = button(action, region);
      assert.ok(b);
      assert.equal(b.disabled, false);
      b.click();
    },
    tick = () => new Promise((r) => setTimeout(r, 20)),
    load = ({ transformed = false, mutate } = {}) => {
      const f = directMarqueeFixture({ transformed });
      f.curve.collapsed = false;
      mutate?.(f);
      win.nect.load(serialize(f.doc));
      $('[data-tool="direct"]').click();
      return f;
    },
    selectThree = (f) => f.curve.points.slice(0, 3).forEach((p, i) => pick(p.id, i > 0)),
    cloneClick = (b) => {
      const host = document.createElement('div');
      host.append(b);
      $('#app').append(host);
      b.click();
      host.remove();
    };

  const input = (region = '.inspector-body') => $(`${region} [data-point-spacing]`),
    stage = (value, region) => {
      const el = input(region);
      el.value = value;
      el.dispatchEvent(new win.Event('input', { bubbles: true }));
      el.dispatchEvent(new win.Event('change', { bubbles: true }));
      return el;
    },
    clonePanel = (panel) => {
      const host = document.createElement('div');
      host.append(panel);
      $('#app').append(host);
      panel.querySelector('[data-point-set-spacing="x"]').click();
      host.remove();
    };

  await t.test(
    'two-anchor minimum; input and blur are transient; fractional apply moves maximum with one Undo/Redo',
    () => {
      const f = load();
      pick('anchor-0');
      assert.equal(button('x').disabled, true);
      pick('anchor-1', true);
      assert.equal(button('x').disabled, false);
      assert.equal($('.inspector-body [data-point-distribute="x"]').disabled, true);
      assert.match($('.inspector-body [data-point-spacing-panel]').textContent, /document units/);
      assert.match(
        $('.inspector-body [data-point-spacing-panel]').textContent,
        /Min fixed.*max moves/,
      );
      const before = snapshot(),
        selection = ui();
      stage('12.5');
      assert.equal(input('.graph').value, '12.5');
      assert.deepEqual(snapshot(), before);
      input().dispatchEvent(
        new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true, cancelable: true }),
      );
      assert.deepEqual(snapshot(), before);
      act('x');
      const after = snapshot(),
        live = after.doc.layers[0].nodes.find((n) => n.id === f.curve.id);
      assert.deepEqual(
        live.points.slice(0, 2).map((p) => p.position.value[0]),
        [100, 112.5],
      );
      assert.deepEqual(live.points.slice(2), f.curve.points.slice(2));
      assert.equal(after.past, before.past + 1);
      assert.equal(after.file.dirty, true);
      assert.deepEqual(ui().points, selection.points);
      assert.deepEqual(ui().focus, selection.focus);
      assert.equal(input().value, '12.5');
      const noop = snapshot();
      act('x');
      assert.deepEqual(snapshot(), noop);
      $('[data-action="undo"]').click();
      assert.deepEqual(win.nect.document, before.doc);
      assert.equal(win.nect.export(), before.svg);
      assert.deepEqual(ui().points, selection.points);
      $('[data-action="redo"]').click();
      assert.deepEqual(win.nect.document, after.doc);
      assert.equal(win.nect.export(), after.svg);
      assert.deepEqual(ui().focus, selection.focus);
    },
  );
  await t.test(
    'zero interval and Escape draft cancellation do not add unintended document changes',
    () => {
      const f = load();
      selectThree(f);
      const before = snapshot();
      const el = stage('7.25');
      el.dispatchEvent(
        new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
      );
      assert.equal(input().value, '10');
      assert.equal(input('.graph').value, '10');
      assert.deepEqual(snapshot(), before);
      stage('0');
      act('y');
      const changed = win.nect.document.layers[0].nodes.find((n) => n.id === f.curve.id);
      assert.deepEqual(
        changed.points.slice(0, 3).map((p) => p.position.value[1]),
        [100, 100, 100],
      );
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'rename',
        payload: { layerId: f.layer.id, nodeId: f.curve.id, name: 'Temporary' },
      });
      $('[data-action="undo"]').click();
      $('[data-tool="direct"]').click();
      // Undo returns to the already selected spacing result. Re-Shift-clicking
      // those members would intentionally toggle them off.
      stage('0');
      const noop = snapshot();
      assert.ok(noop.future > 0);
      assert.deepEqual(
        ui().points,
        f.curve.points.slice(0, 3).map((p) => p.id),
      );
      act('y');
      assert.deepEqual(snapshot(), noop);
    },
  );
  await t.test(
    'negative, empty, nonfinite and overflowing drafts reject without history or geometry changes',
    () => {
      for (const value of ['-1', '', 'NaN', 'Infinity', '1e308']) {
        const f = load();
        selectThree(f);
        stage(value);
        const before = snapshot();
        act('x');
        assert.deepEqual(snapshot(), before);
        assert.match($('.status').textContent, /spacing|finite|range/i);
      }
    },
  );
  await t.test(
    'decimal key stays numeric; source-order ties and document-unit label are explicit',
    () => {
      const f = load({
        mutate: (f) => {
          f.curve.points[1].position.value = [100, 140];
        },
      });
      pick('anchor-1');
      pick('anchor-0', true);
      pick('anchor-2', true);
      const el = stage('2.5');
      el.dispatchEvent(
        new win.KeyboardEvent('keydown', { key: '.', bubbles: true, cancelable: true }),
      );
      assert.equal(el.type, 'number');
      act('x', '.graph');
      assert.deepEqual(
        win.nect.document.layers[0].nodes
          .find((n) => n.id === f.curve.id)
          .points.slice(0, 3)
          .map((p) => p.position.value[0]),
        [100, 102.5, 105],
      );
    },
  );
  await t.test(
    'held selection/revision/Session panels and active gestures reject without partial edits',
    async () => {
      for (const change of ['selection', 'revision', 'session']) {
        const f = load();
        selectThree(f);
        stage('5');
        const old = $('.inspector-body [data-point-spacing-panel]').cloneNode(true);
        if (change === 'selection') pick('anchor-3');
        if (change === 'revision')
          win.nect.dispatch({
            ...win.nect.commandState,
            type: 'rename',
            payload: { layerId: f.layer.id, nodeId: f.curve.id, name: 'Renamed' },
          });
        if (change === 'session') {
          win.nect.load(serialize(f.doc));
          $('[data-tool="direct"]').click();
          selectThree(f);
        }
        const before = snapshot(),
          selection = ui();
        clonePanel(old);
        assert.deepEqual(snapshot(), before);
        assert.deepEqual(ui(), selection);
        assert.match($('.status').textContent, /spacing cancelled/i);
      }
      const f = load();
      selectThree(f);
      stage('5');
      const old = $('.inspector-body [data-point-spacing-panel]').cloneNode(true),
        before = snapshot();
      fire($('.artboard'), 'pointerdown', [40, 40]);
      fire(document, 'pointermove', [250, 250]);
      clonePanel(old);
      assert.deepEqual(snapshot(), before);
      fire(document, 'pointercancel');
      await tick();
      assert.deepEqual(snapshot(), before);
    },
  );
  await t.test(
    'locked/driven members disable the numeric operation and stale editable panel cannot mutate them',
    () => {
      for (const mutate of [
        (f) => {
          f.curve.points[0].position.locked = true;
        },
        (f) => {
          f.curve.points[1].position.expression = 'value; value';
        },
      ]) {
        const f = load();
        selectThree(f);
        const old = $('.inspector-body [data-point-spacing-panel]').cloneNode(true);
        const changed = directMarqueeFixture();
        mutate(changed);
        win.nect.load(serialize(changed.doc));
        $('[data-tool="direct"]').click();
        selectThree(changed);
        assert.equal(button('x').disabled, true);
        assert.equal(input().disabled, true);
        const before = snapshot();
        clonePanel(old);
        assert.deepEqual(snapshot(), before);
      }
    },
  );
  await t.test(
    'Remap Graph numeric interval uses actual IDs and preserves immutable source',
    () => {
      const f = compoundFixture([4, 4, 4, 2]),
        remap = node('Path Remap');
      f.layer.nodes.splice(1, 0, remap);
      f.fill.params.path.source = null;
      connect(f.layer, f.path.id, remap.id, 'path');
      connect(f.layer, remap.id, f.fill.id, 'path');
      win.nect.load(serialize(f.doc));
      $('[data-tool="direct"]').click();
      f.path.contours[0].points.slice(0, 3).forEach((id, i) => pick(id, i > 0));
      stage('3.125', '.graph');
      act('y', '.graph');
      const doc = win.nect.document;
      assert.deepEqual(
        doc.layers[0].nodes.find((n) => n.id === f.path.id),
        f.path,
      );
      assert.equal(doc.layers[0].nodes.find((n) => n.id === remap.id).edits.length, 2);
    },
  );
});
