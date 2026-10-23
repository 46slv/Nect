import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('selected owned-point delta controls', async (t) => {
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
    '\nwindow.deltaState = () => ({ids: [...pointSelection.ids], past: history.past.length, draft: pointDeltaDraft});';
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

  await t.test(
    'both axes commit all selected contours, irrespective of browsing, in one Undo',
    () => {
      const f = load(),
        before = saved(),
        ids = win.deltaState().ids;
      const scope = $('.graph [data-point-panel-contour]');
      scope.value = 'scope-hole';
      scope.dispatchEvent(new win.Event('change', { bubbles: true }));
      assert.match(panel().textContent, /2 selected points.*Node-local.*document units/);
      assert.equal(document.querySelectorAll('.graph [data-point-row]').length, 5);
      type(0, '12.5');
      type(1, '-7.25');
      assert.equal(input(0, '.inspector').value, '12.5');
      input().dispatchEvent(new win.Event('change', { bubbles: true }));
      assert.equal(saved(), before, 'blur only stages');
      key('Enter');
      assert.equal(win.deltaState().past, 1);
      const live = win.nect.document.layers[0].nodes.find((n) => n.id === f.path.id);
      assert.deepEqual(live.contours, f.path.contours);
      assert.deepEqual(live.units, f.path.units);
      for (const p of f.path.points) {
        const expected = structuredClone(p);
        if (ids.includes(p.id))
          expected.position.value = p.position.value.map((v, i) => v + [12.5, -7.25][i]);
        assert.deepEqual(
          live.points.find((q) => q.id === p.id),
          expected,
        );
      }
      const after = saved();
      assert.deepEqual(win.deltaState().ids, ids);
      assert.equal(input().value, '0');
      $('[data-action="undo"]').click();
      assert.equal(saved(), before);
      $('[data-action="redo"]').click();
      assert.equal(saved(), after);
      win.nect.load(after);
      assert.equal(saved(), after);
    },
  );

  await t.test('Cancel, Escape, zero and IME confirmation remain document-neutral', () => {
    load();
    const before = saved(),
      state = win.nect.commandState;
    type(0, '9');
    type(1, '-4');
    key('Enter', { isComposing: true, keyCode: 229 });
    assert.equal(saved(), before);
    key('Escape', { isComposing: true, keyCode: 229 });
    assert.equal(input().value, '9');
    key('Escape');
    assert.equal(input().value, '0');
    assert.equal(input(1, '.inspector').value, '0');
    type(1, '7');
    panel().querySelector('[data-cancel-point-delta]').click();
    assert.equal(input(1).value, '0');
    type(0, '0');
    apply();
    assert.equal(saved(), before);
    assert.deepEqual(win.nect.commandState, state);
    assert.equal(win.deltaState().past, 0);
  });

  await t.test('invalid numbers and stale revision reject without a second history entry', () => {
    const f = load(),
      before = saved();
    type(0, '');
    apply();
    assert.equal(saved(), before);
    type(0, '8');
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'translate-points',
      payload: {
        layerId: f.layer.id,
        nodeId: f.path.id,
        pointIds: [f.path.contours[2].points[0]],
        delta: [1, 0],
      },
    });
    const changed = saved(),
      state = win.nect.commandState;
    apply();
    assert.equal(saved(), changed);
    assert.deepEqual(win.nect.commandState, state);
    assert.equal(win.deltaState().past, 1);
    assert.match(document.body.textContent, /stale|revision/i);
  });

  await t.test('selection change discards its draft, with no retargeted move', () => {
    const f = load(),
      before = saved();
    type(0, '40');
    pick(f.path.contours[1].points[0]);
    assert.equal(input().value, '0');
    apply();
    assert.equal(saved(), before);
    assert.equal(win.deltaState().past, 0);
  });

  await t.test('locked/driven selection disables the whole common move', () => {
    for (const property of ['locked', 'expression']) {
      const f = compoundFixture([8, 5, 7, 4]);
      f.path.points.find((p) => p.id === f.path.contours[3].points[2]).position[property] =
        property === 'locked' ? true : 'value; value';
      load(f);
      const before = saved();
      assert.equal(input().disabled, true);
      assert.equal(input(1).disabled, true);
      assert.equal(panel().querySelector('[data-apply-point-delta]').disabled, true);
      assert.equal(saved(), before);
      assert.equal(win.deltaState().past, 0);
    }
  });

  await t.test('ordinary Curve gets the same controls; generated Remap stays separate', () => {
    const f = compoundFixture(),
      curve = node('Curve');
    f.layer.nodes.unshift(curve);
    win.nect.load(serialize(f.doc));
    inspect(curve.id);
    pick(curve.points[0].id);
    assert.ok(panel());
    type(0, '3');
    apply();
    assert.equal(win.deltaState().past, 1);
    const remap = node('Path Remap');
    remap.params.path.source = f.path.id;
    f.layer.nodes.push(remap);
    win.nect.load(serialize(f.doc));
    inspect(remap.id);
    pick(f.path.contours[0].points[0]);
    assert.equal(panel(), null);
    assert.ok($('.graph [data-batch-point]'));
  });
});
