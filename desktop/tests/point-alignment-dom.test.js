import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { node, connect, serialize } from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('selected anchor alignment controls use stable snapshots and atomic Session commands', async (t) => {
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
    '\nwindow.pointAlignUI=()=>({selected,selection:[...selection],points:[...pointSelection.ids],focus:pointFocus,past:history.past.length,future:history.future.length});';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  source += '\n//# sourceURL=main-point-alignment-test.js';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    ui = () => structuredClone(win.pointAlignUI()),
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
    button = (action, region = '.inspector-body') => $(`${region} [data-point-align="${action}"]`),
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

  await t.test(
    'six compact Artboard controls share tokens across panel copies and preserve focus/selection through one Undo/Redo',
    () => {
      const f = load();
      pick('anchor-0');
      assert.equal(button('center-x').disabled, true);
      assert.match($('[data-point-align-status]').textContent, /2-10000/);
      pick('anchor-1', true);
      pick('anchor-2', true);
      assert.equal(document.querySelectorAll('.inspector-body [data-point-align]').length, 6);
      assert.match($('.inspector-body [data-point-align-panel]').textContent, /Artboard/);
      assert.equal(
        button('left').dataset.pointAlignToken,
        button('left', '.graph').dataset.pointAlignToken,
      );
      const before = snapshot(),
        selected = ui();
      act('center-x');
      const after = snapshot();
      assert.equal(ui().past, before.past + 1);
      assert.deepEqual(ui().points, selected.points);
      assert.deepEqual(ui().focus, selected.focus);
      assert.ok(after.file.dirty);
      const live = after.doc.layers[0].nodes.find((n) => n.id === f.curve.id);
      assert.deepEqual(
        live.points.slice(0, 3).map((p) => p.position.value[0]),
        [155, 155, 155],
      );
      assert.deepEqual(live.points[3], f.curve.points[3]);
      $('[data-action="undo"]').click();
      assert.deepEqual(win.nect.document, before.doc);
      assert.equal(win.nect.export(), before.svg);
      assert.deepEqual(ui().points, selected.points);
      assert.deepEqual(ui().focus, selected.focus);
      $('[data-action="redo"]').click();
      assert.deepEqual(win.nect.document, after.doc);
      assert.equal(win.nect.export(), after.svg);
      const noop = snapshot();
      act('center-x', '.graph');
      assert.deepEqual(snapshot(), noop);
    },
  );
  await t.test(
    'transformed source uses artboard range midpoint instead of local or mean coordinates',
    () => {
      const f = load({ transformed: true });
      selectThree(f);
      const route = directSelectionSource(win.nect.document, f.layer.id),
        old = route.path.contours[0].points,
        x = old.slice(0, 3).map((p) => p.position[0]),
        target = (Math.min(...x) + Math.max(...x)) / 2;
      act('center-x');
      const result = directSelectionSource(win.nect.document, f.layer.id).path.contours[0].points;
      result.slice(0, 3).forEach((p, i) => {
        assert.ok(Math.abs(p.position[0] - target) < 1e-9);
        assert.ok(Math.abs(p.position[1] - old[i].position[1]) < 1e-9);
      });
      assert.deepEqual(result[3], old[3]);
    },
  );
  await t.test(
    'no-op leaves dirty/history/Redo intact and both axes have min/center/max routes',
    () => {
      const f = load({
        mutate: (f) =>
          f.curve.points.slice(0, 3).forEach((p, i) => {
            p.position.value[0] = 100;
            p.position.value[1] = 100 + i * 40;
          }),
      });
      selectThree(f);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'rename',
        payload: { layerId: f.layer.id, nodeId: f.curve.id, name: 'Temporary' },
      });
      $('[data-action="undo"]').click();
      $('[data-tool="direct"]').click();
      selectThree(f);
      const before = snapshot();
      assert.ok(before.future > 0);
      act('center-x');
      assert.deepEqual(snapshot(), before);
      for (const action of ['top', 'center-y', 'bottom', 'left', 'right']) {
        const beforeUI = ui();
        act(action);
        assert.deepEqual(ui().points, beforeUI.points);
      }
    },
  );
  await t.test(
    'disabled mixed lock/drive and singular route never partially edit even if an old button is reattached',
    () => {
      for (const mutate of [
        (f) => {
          f.curve.points[1].position.locked = true;
        },
        (f) => {
          f.curve.points[1].position.expression = 'value; value';
        },
        (f) => {
          f.transform.params.scale.value = 0;
        },
      ]) {
        const f = load();
        selectThree(f);
        const old = button('left').cloneNode(true);
        const changed = directMarqueeFixture({ transformed: true });
        mutate(changed);
        win.nect.load(serialize(changed.doc));
        $('[data-tool="direct"]').click();
        if (changed.transform.params.scale.value !== 0) {
          pick('anchor-0');
          pick('anchor-1', true);
          pick('anchor-2', true);
          assert.equal(button('left').disabled, true);
        }
        const before = snapshot();
        cloneClick(old);
        assert.deepEqual(snapshot(), before);
      }
    },
  );
  await t.test(
    'old selection, revision and Session button snapshots reject without mutation',
    () => {
      for (const change of ['selection', 'revision', 'session']) {
        const f = load();
        selectThree(f);
        const old = button('center-y').cloneNode(true);
        if (change === 'selection') pick('anchor-3');
        if (change === 'revision')
          win.nect.dispatch({
            ...win.nect.commandState,
            type: 'rename',
            payload: { layerId: f.layer.id, nodeId: f.curve.id, name: 'New source name' },
          });
        if (change === 'session') {
          win.nect.load(serialize(f.doc));
          $('[data-tool="direct"]').click();
          selectThree(f);
        }
        const before = snapshot(),
          selection = ui();
        cloneClick(old);
        assert.deepEqual(snapshot(), before);
        assert.deepEqual(ui(), selection);
        assert.match($('.status').textContent, /alignment cancelled/i);
      }
    },
  );
  await t.test(
    'Path Remap actual IDs can align without source/handles changes; source fingerprint invalidates held actions',
    () => {
      const f = compoundFixture([4, 4, 4, 2]),
        remap = node('Path Remap');
      f.layer.nodes.splice(1, 0, remap);
      f.fill.params.path.source = null;
      connect(f.layer, f.path.id, remap.id, 'path');
      connect(f.layer, remap.id, f.fill.id, 'path');
      win.nect.load(serialize(f.doc));
      $('[data-tool="direct"]').click();
      const ids = f.path.contours[0].points.slice(0, 3);
      ids.forEach((id, i) => pick(id, i > 0));
      const before = snapshot();
      act('left', '.graph');
      const changed = win.nect.document;
      assert.equal(changed.layers[0].nodes.find((n) => n.id === remap.id).edits.length, 2);
      assert.deepEqual(
        changed.layers[0].nodes.find((n) => n.id === f.path.id),
        f.path,
      );
      $('[data-action="undo"]').click();
      assert.deepEqual(win.nect.document, before.doc);
      const old = button('top', '.graph').cloneNode(true);
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'translate-points',
        payload: {
          layerId: f.layer.id,
          nodeId: f.path.id,
          pointIds: [f.path.points[0].id],
          delta: [1, 0],
        },
      });
      const fresh = snapshot();
      cloneClick(old);
      assert.deepEqual(snapshot(), fresh);
      assert.deepEqual(win.nect.document.layers[0].nodes.find((n) => n.id === remap.id).edits, []);
    },
  );
  await t.test(
    'another active gesture owns input; stale alignment button cannot commit',
    async () => {
      const f = load();
      selectThree(f);
      const old = button('left').cloneNode(true),
        before = snapshot();
      fire($('.artboard'), 'pointerdown', [40, 40]);
      fire(document, 'pointermove', [250, 250]);
      cloneClick(old);
      assert.deepEqual(snapshot(), before);
      fire(document, 'pointercancel');
      await tick();
      assert.deepEqual(snapshot(), before);
    },
  );
});
