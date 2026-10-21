import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('selected-point bounds summary', async (t) => {
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
    '\nwindow.boundsOwner = () => pointSelection.nodeId; window.forceBounds = (nodeId, ids) => { pointSelection = {nodeId, ids: new Set(ids)}; refreshPointPanels(); }; window.deltaState = () => ({ids: [...pointSelection.ids], past: history.past.length, draft: pointDeltaDraft});';
  source += '\nwhile (!bundledFont) await new Promise(r => setTimeout(r, 10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s);
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
  const inspect = (id) => {
    fire($(`.graph .node[data-id="${id}"] .node-head .type-tag`), 'pointerdown');
    fire(document, 'pointerup');
  };
  const pick = (id, shiftKey = false) => {
    const el = $(`.canvas-stage .point-hit[data-point="${id}"]:not([data-handle])`);
    assert.ok(el, id);
    const x = +el.getAttribute('cx'),
      y = +el.getAttribute('cy');
    fire(el, 'pointerdown', x, y, { shiftKey });
    fire(document, 'pointerup', x, y);
  };
  const panel = () => $('.graph [data-point-delta-panel]');
  const input = (axis = 0, region = '.graph') => $(`${region} [data-point-delta-axis="${axis}"]`);
  const type = (axis, value) => {
    const el = input(axis);
    el.value = value;
    el.dispatchEvent(new win.Event('input', { bubbles: true }));
    return el;
  };
  const key = (name, extra = {}) =>
    input().dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name, ...extra }),
    );
  const saved = () => serialize(win.nect.document);
  const apply = () => panel().querySelector('[data-apply-point-delta]').click();
  const load = (f = compoundFixture([8, 5, 7, 4])) => {
    f.doc.schemaVersion = 42;
    for (const p of f.path.points) {
      p.handleA.value = [-3, 2];
      p.handleB.value = [5, -4];
    }
    win.nect.load(serialize(f.doc));
    inspect(f.path.id);
    pick(f.path.contours[0].points[1]);
    pick(f.path.contours[3].points[2], true);
    return f;
  };

  const summary = (region = '.graph') =>
    $(
      win.boundsOwner()
        ? `${region} [data-point-bounds="${win.boundsOwner()}"]`
        : `${region} [data-point-bounds]`,
    );
  const values = (region = '.graph') =>
    Object.fromEntries(
      [...summary(region).querySelectorAll('[data-point-bound]')].map((el) => [
        el.dataset.pointBound,
        Number(el.title),
      ]),
    );
  const expected = (f, ids) => {
    const pts = f.path.points.filter((p) => ids.includes(p.id)).map((p) => p.position.value);
    const x = pts.map((p) => p[0]),
      y = pts.map((p) => p[1]);
    return {
      minX: Math.min(...x),
      maxX: Math.max(...x),
      width: Math.max(...x) - Math.min(...x),
      minY: Math.min(...y),
      maxY: Math.max(...y),
      height: Math.max(...y) - Math.min(...y),
    };
  };
  await t.test(
    'single/cross-contour selection and filtering display only selected anchor bounds without mutations',
    () => {
      const f = load(),
        before = saved(),
        state = win.nect.commandState;
      assert.equal(summary().dataset.state, 'ready');
      assert.match(summary().textContent, /2 anchors.*Node-local.*document units/);
      assert.deepEqual(values(), expected(f, win.deltaState().ids));
      assert.deepEqual(values('.inspector'), values());
      assert.equal(summary().querySelectorAll('input,button,select').length, 0);
      const scope = $('.graph [data-point-panel-contour]');
      scope.value = 'scope-hole';
      scope.dispatchEvent(new win.Event('change', { bubbles: true }));
      assert.deepEqual(values(), expected(f, win.deltaState().ids));
      assert.equal(saved(), before);
      assert.deepEqual(win.nect.commandState, state);
      pick(f.path.contours[1].points[0]);
      assert.equal(values().width, 0);
      assert.equal(values().height, 0);
      assert.match(summary().textContent, /1 anchor\b/);
      pick(f.path.contours[1].points[0], true);
      assert.equal(summary().dataset.state, 'empty');
      assert.match(summary().textContent, /No points selected/);
      assert.equal(summary().querySelector('[data-point-bound]'), null);
      assert.equal(saved(), before);
    },
  );
  await t.test('position edits and common delta update bounds through Undo/Redo', async () => {
    const f = load(),
      original = values(),
      ids = win.deltaState().ids;
    type(0, '12');
    type(1, '-8');
    key('Enter');
    const moved = values();
    assert.deepEqual(moved, {
      ...original,
      minX: original.minX + 12,
      maxX: original.maxX + 12,
      minY: original.minY - 8,
      maxY: original.maxY - 8,
    });
    $('[data-action="undo"]').click();
    assert.deepEqual(values(), original);
    $('[data-action="redo"]').click();
    assert.deepEqual(values(), moved);
    const pointId = ids[0];
    const field = $(`.graph [data-key="${pointId}.position"] [data-value][data-axis="0"]`);
    field.value = '-30';
    field.dispatchEvent(new win.Event('change', { bubbles: true }));
    await new Promise((r) => setTimeout(r, 25)); // Existing numeric changes defer their render.
    const live = win.nect.document.layers[0].nodes.find((n) => n.id === f.path.id);
    assert.deepEqual(values(), expected({ ...f, path: live }, ids));
    $('[data-action="undo"]').click();
    // Individual field Undo follows existing selection reset semantics.
    assert.equal(summary().dataset.state, win.deltaState().ids.length ? 'ready' : 'empty');
    if (win.deltaState().ids.length) assert.deepEqual(values(), moved);
  });
  await t.test('stale, locked and driven selection never displays a partial numeric range', () => {
    const f = load(),
      before = saved(),
      state = win.nect.commandState;
    win.forceBounds(f.path.id, [f.path.contours[0].points[0], 'missing-point']);
    assert.equal(summary().dataset.state, 'unavailable');
    assert.match(summary().textContent, /Selection changed/);
    assert.equal(summary().querySelector('[data-point-bound]'), null);
    assert.equal(saved(), before);
    assert.deepEqual(win.nect.commandState, state);
    for (const property of ['locked', 'expression']) {
      const f = compoundFixture([8, 5, 7, 4]);
      f.path.points.find((p) => p.id === f.path.contours[3].points[2]).position[property] =
        property === 'locked' ? true : 'value; value';
      load(f);
      assert.equal(summary().dataset.state, 'unavailable');
      assert.match(
        summary().textContent,
        property === 'locked' ? /Locked position/ : /Driven position/,
      );
      assert.equal(summary().querySelector('[data-point-bound]'), null);
    }
  });
  await t.test('read-only bounds refresh while an existing numeric editor retains focus', () => {
    const f = load(),
      before = saved(),
      state = win.nect.commandState;
    const editor = input();
    editor.focus();
    win.forceBounds(f.path.id, [f.path.contours[1].points[0]]);
    assert.equal(document.activeElement, editor);
    assert.equal(values().width, 0);
    assert.equal(values().height, 0);
    assert.match(summary().textContent, /1 anchor\b/);
    assert.equal(saved(), before);
    assert.deepEqual(win.nect.commandState, state);
    editor.blur();
  });
  await t.test(
    'generated Remap has a clear unsupported state and document reload resets bounds',
    () => {
      const f = load(),
        remap = node('Path Remap');
      remap.params.path.source = f.path.id;
      f.layer.nodes.push(remap);
      win.nect.load(serialize(f.doc));
      inspect(remap.id);
      pick(f.path.contours[0].points[0]);
      assert.equal(summary().dataset.state, 'unavailable');
      assert.match(summary().textContent, /Owned points only/);
      assert.equal(summary().querySelector('[data-point-bound]'), null);
      win.nect.load(serialize(f.doc));
      inspect(f.path.id);
      assert.equal(summary().dataset.state, 'empty');
      assert.match(summary().textContent, /0 anchors/);
    },
  );
});
