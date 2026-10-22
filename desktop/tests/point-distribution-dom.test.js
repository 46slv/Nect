import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { node, connect, serialize } from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('selected anchor spacing controls use stable snapshots and atomic Session commands', async (t) => {
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
    '\nwindow.pointSpacingUI=()=>({selected,selection:[...selection],points:[...pointSelection.ids],focus:pointFocus,past:history.past.length,future:history.future.length});';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  source += '\n//# sourceURL=main-point-distribution-test.js';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    ui = () => structuredClone(win.pointSpacingUI()),
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
      $(`${region} [data-point-distribute="${action}"]`),
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
    'two Artboard actions require three anchors and preserve selection/focus with one Undo/Redo',
    () => {
      const f = load();
      pick('anchor-0');
      pick('anchor-1', true);
      assert.equal(button('x').disabled, true);
      assert.equal($('.inspector-body [data-point-align="left"]').disabled, false);
      pick('anchor-2', true);
      assert.equal(document.querySelectorAll('.inspector-body [data-point-distribute]').length, 2);
      assert.match($('.inspector-body [data-point-distribute-panel]').textContent, /Artboard/);
      assert.equal(
        button('x').dataset.pointAlignToken,
        button('x', '.graph').dataset.pointAlignToken,
      );
      const before = snapshot(),
        selection = ui();
      act('x');
      const after = snapshot(),
        live = after.doc.layers[0].nodes.find((n) => n.id === f.curve.id);
      assert.deepEqual(
        live.points.slice(0, 3).map((p) => p.position.value[0]),
        [100, 155, 210],
      );
      assert.deepEqual(live.points[3], f.curve.points[3]);
      assert.equal(after.past, before.past + 1);
      assert.equal(after.file.dirty, true);
      assert.deepEqual(ui().points, selection.points);
      assert.deepEqual(ui().focus, selection.focus);
      act('x', '.graph');
      assert.deepEqual(snapshot(), after);
      $('[data-action="undo"]').click();
      assert.deepEqual(win.nect.document, before.doc);
      assert.equal(win.nect.export(), before.svg);
      assert.deepEqual(ui().points, selection.points);
      assert.deepEqual(ui().focus, selection.focus);
      $('[data-action="redo"]').click();
      assert.deepEqual(win.nect.document, after.doc);
      assert.equal(win.nect.export(), after.svg);
      assert.deepEqual(ui().points, selection.points);
      assert.deepEqual(ui().focus, selection.focus);
    },
  );
  await t.test(
    'ties follow source order rather than click order; vertical action uses inverse artboard route',
    () => {
      const f = load({
        mutate: (f) => {
          f.curve.points[1].position.value = [100, 140];
        },
      });
      pick('anchor-1');
      pick('anchor-0', true);
      pick('anchor-2', true);
      act('x');
      const live = win.nect.document.layers[0].nodes.find((n) => n.id === f.curve.id);
      assert.deepEqual(
        live.points.slice(0, 3).map((p) => p.position.value[0]),
        [100, 155, 210],
      );
      const g = load({ transformed: true });
      selectThree(g);
      const before = directSelectionSource(win.nect.document, g.layer.id).path.contours[0].points;
      const sorted = before.slice(0, 3).toSorted((a, b) => a.position[1] - b.position[1]);
      act('y');
      const after = directSelectionSource(win.nect.document, g.layer.id).path.contours[0].points;
      sorted.forEach((p, i) => {
        const q = after.find((q) => q.id === p.id);
        assert.ok(
          Math.abs(
            q.position[1] -
              (sorted[0].position[1] + ((sorted[2].position[1] - sorted[0].position[1]) * i) / 2),
          ) < 1e-9,
        );
        assert.ok(Math.abs(q.position[0] - p.position[0]) < 1e-9);
      });
      assert.deepEqual(after[3], before[3]);
    },
  );
  await t.test('coincident axis no-op preserves dirty/history and an existing Redo branch', () => {
    const f = load({
      mutate: (f) =>
        f.curve.points.slice(0, 3).forEach((p, i) => {
          p.position.value = [100, 100 + i * 40];
        }),
    });
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
    act('x');
    assert.deepEqual(snapshot(), before);
    act('y');
    assert.deepEqual(snapshot(), before);
  });
  await t.test('stale selection, revision and Session actions reject atomically', () => {
    for (const change of ['selection', 'revision', 'session']) {
      const f = load();
      selectThree(f);
      const old = button('x').cloneNode(true);
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
      cloneClick(old);
      assert.deepEqual(snapshot(), before);
      assert.deepEqual(ui(), selection);
      assert.match($('.status').textContent, /spacing cancelled/i);
    }
  });
  await t.test(
    'lock, drive, singular route and another gesture cannot partially distribute',
    async () => {
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
        const old = button('x').cloneNode(true);
        const changed = directMarqueeFixture({ transformed: true });
        mutate(changed);
        win.nect.load(serialize(changed.doc));
        $('[data-tool="direct"]').click();
        if (changed.transform.params.scale.value !== 0) {
          selectThree(changed);
          assert.equal(button('x').disabled, true);
        }
        const before = snapshot();
        cloneClick(old);
        assert.deepEqual(snapshot(), before);
      }
      const f = load();
      selectThree(f);
      const old = button('x').cloneNode(true),
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
  await t.test('Remap spacing preserves source; changed fingerprint rejects held control', () => {
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
    act('y', '.graph');
    assert.deepEqual(
      win.nect.document.layers[0].nodes.find((n) => n.id === f.path.id),
      f.path,
    );
    assert.equal(win.nect.document.layers[0].nodes.find((n) => n.id === remap.id).edits.length, 1);
    $('[data-action="undo"]').click();
    assert.deepEqual(win.nect.document, before.doc);
    const old = button('y', '.graph').cloneNode(true);
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
  });
});
