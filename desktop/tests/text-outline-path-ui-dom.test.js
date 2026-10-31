import { openDOMPanel } from './helpers/dom-workspace-controls.js';
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
  globalThis.MutationObserver = win.MutationObserver;
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
    openDOMPanel(document, 'inspector');
  };
  const live = () => win.nect.document.layers[0].nodes.find((n) => n.id === a.id);
  const snapshot = () => serialize(win.nect.document);
  const anchor = (p) => $(`.canvas-stage .point-hit[data-point="${p.id}"]:not([data-handle])`);
  const undo = () => key('z', $('.graph-scroll'), { ctrlKey: true });
  const panel = (id = a.id) => $(`[data-point-panel="${id}"]`);
  const rows = (id = a.id) => [...panel(id).querySelectorAll('[data-point-row]')];
  const pick = (p, shiftKey = false) => {
    fire(anchor(p), 'pointerdown', ...p.position.value, { shiftKey });
    fire(document, 'pointerup');
  };
  await t.test(
    'point panel starts empty, follows Canvas selection and highlights active ID',
    () => {
      reset('Curve');
      const [p, q] = live().points;
      assert.equal(rows().length, 0);
      assert.equal(panel().querySelectorAll('input').length, 0);
      pick(p);
      assert.deepEqual(
        rows().map((r) => r.dataset.pointRow),
        [p.id],
      );
      assert.ok(rows()[0].classList.contains('point-active'));
      pick(q, true);
      assert.equal(rows().length, 2);
      assert.equal(panel().querySelector('.point-active').dataset.pointRow, q.id);
      pick(q, true);
      assert.deepEqual(
        rows().map((r) => r.dataset.pointRow),
        [p.id],
      );
      pick(p, true);
      assert.equal(rows().length, 0);
      assert.equal(panel().querySelectorAll('input').length, 0);
    },
  );
  await t.test('focused point input blurs before another Canvas point refreshes its panel', () => {
    reset('Curve');
    const [p, q] = live().points;
    pick(p);
    openDOMPanel(document, 'inspector');
    const input = panel().querySelector(`[data-key="${p.id}.position"] input[data-axis="0"]`);
    input.focus();
    assert.equal(document.activeElement === input, true, 'point input receives focus');
    const before = snapshot();
    fire(anchor(q), 'pointerdown', ...q.position.value);
    assert.ok(document.activeElement.classList.contains('canvas-stage'));
    assert.deepEqual(
      rows().map((row) => row.dataset.pointRow),
      [q.id],
    );
    assert.equal(panel().querySelector('.point-active').dataset.pointRow, q.id);
    fire(document, 'pointerup', ...q.position.value);
    assert.deepEqual(
      rows().map((row) => row.dataset.pointRow),
      [q.id],
    );
    assert.equal(snapshot(), before);
  });
  await t.test('selected point numeric editing and scrub retain command history', async () => {
    reset('Curve');
    const p = live().points[1];
    pick(p);
    const positionInput = () =>
      panel().querySelector(`[data-key="${p.id}.position"] input[data-axis="0"]`);
    const before = snapshot(),
      past = state().past;
    assert.equal(positionInput().dataset.scrubStep, '1');
    fire(positionInput(), 'pointerdown', 100, 100);
    fire(document, 'pointermove', 124, 100);
    fire(document, 'pointerup', 124, 100);
    assert.equal(state().past, past + 1);
    assert.notEqual(live().points[1].position.value[0], p.position.value[0]);
    undo();
    assert.equal(snapshot(), before);
    pick(live().points[1]);
    positionInput().value = String(p.position.value[0] + 7);
    positionInput().dispatchEvent(new win.Event('change', { bubbles: true }));
    await new Promise((resolve) => setTimeout(resolve, 30));
    assert.equal(live().points[1].position.value[0], p.position.value[0] + 7);
    assert.equal(state().past, past + 1);
    undo();
    assert.equal(snapshot(), before);
    pick(live().points[1]);
    fire(positionInput(), 'pointerdown', 100, 100);
    fire(document, 'pointermove', 124, 100);
    fire(document, 'pointerup', 124, 100);
    assert.equal(state().past, past + 1);
    assert.notEqual(live().points[1].position.value[0], p.position.value[0]);
    undo();
    assert.equal(snapshot(), before);
  });
  await t.test('all-point browsing is explicit, bounded and reaches later stable IDs', () => {
    reset();
    $('[data-bake-text-outline]').click();
    const n = live();
    assert.ok(n.points.length > 24);
    assert.equal(rows().length, 0);
    panel().querySelector('[data-point-panel-mode]').click();
    assert.equal(rows().length, 24);
    panel().querySelector('[data-point-panel-page][data-page="1"]').click();
    assert.equal(rows()[0].dataset.pointRow, n.points[24].id);
    rows()[0].querySelector('[data-select-panel-point]').click();
    assert.deepEqual(state().points, [n.points[24].id]);
    assert.equal(rows().length, 1);
    assert.ok(rows()[0].classList.contains('point-active'));
    $(`[data-path-unit="${n.units[0].id}"]`).click();
    assert.ok(rows().length <= 24);
    assert.ok(panel().textContent.includes(`${state().points.length} Canvas selected`));
  });
  await t.test(
    'generated Remap selection does not silently alter independent batch selection',
    async () => {
      reset('Text');
      const doc = structuredClone(win.nect.document),
        convert = node('Text to Path', 300, 100),
        remap = node('Path Remap', 600, 100);
      convert.params.path.source = a.id;
      remap.params.path.source = convert.id;
      doc.layers[0].nodes.splice(1, 0, convert, remap);
      win.nect.load(serialize(doc));
      clickNode(remap.id);
      assert.equal(rows(remap.id).length, 0);
      const targets = [
        ...document.querySelectorAll('.canvas-stage .point-hit[data-point]:not([data-handle])'),
      ];
      const canvasPoint = targets.find((target) =>
        targets.every(
          (other) =>
            other === target ||
            Math.hypot(
              Number(target.getAttribute('cx')) - Number(other.getAttribute('cx')),
              Number(target.getAttribute('cy')) - Number(other.getAttribute('cy')),
            ) > 0.5,
        ),
      );
      assert.ok(canvasPoint, 'Choose a non-overlapping generated anchor');
      const canvasId = canvasPoint.dataset.point;
      const x = Number(canvasPoint.getAttribute('cx')),
        y = Number(canvasPoint.getAttribute('cy'));
      fire(canvasPoint, 'pointerdown', x, y);
      fire(document, 'pointerup', x, y);
      assert.deepEqual(
        rows(remap.id).map((r) => r.dataset.pointRow),
        [canvasId],
      );
      assert.ok(rows(remap.id)[0].classList.contains('point-active'));
      const beforeDelete = serialize(win.nect.document);
      for (const name of ['Delete', 'Backspace']) {
        key(name, $('.canvas-stage'));
        assert.equal(serialize(win.nect.document), beforeDelete);
        assert.ok(win.nect.document.layers[0].nodes.some((n) => n.id === remap.id));
      }
      fire(
        $(`.canvas-stage .point-hit[data-point="${canvasId}"]:not([data-handle])`),
        'pointerdown',
        x,
        y,
        { shiftKey: true },
      );
      fire(document, 'pointerup', x, y);
      assert.equal(rows(remap.id).length, 0);
      panel(remap.id).querySelector('[data-point-panel-mode]').click();
      assert.equal(rows(remap.id).length, 24);
      const first = rows(remap.id)[0],
        id = first.dataset.pointRow;
      first.querySelector('[data-select-panel-point]').click();
      assert.deepEqual(state().points, [id]);
      assert.equal(rows(remap.id).length, 1);
      assert.equal(panel(remap.id).querySelector('[data-batch-point]').checked, false);
      const checkbox = panel(remap.id).querySelector('[data-batch-point]');
      checkbox.checked = true;
      checkbox.dispatchEvent(new win.Event('change', { bubbles: true }));
      await new Promise((resolve) => setTimeout(resolve, 30));
      assert.deepEqual(state().points, [id]);
      assert.ok(
        card(remap.id)
          .querySelector(`[data-apply-batch="${remap.id}"]`)
          .closest('.batch-edit')
          .textContent.includes('1 batch checked'),
      );
      const button = panel(remap.id).querySelector('[data-select-panel-point]');
      button.dispatchEvent(new win.MouseEvent('click', { bubbles: true, shiftKey: true }));
      assert.equal(rows(remap.id).length, 0);
      assert.ok(
        card(remap.id)
          .querySelector(`[data-apply-batch="${remap.id}"]`)
          .closest('.batch-edit')
          .textContent.includes('1 batch checked'),
      );
    },
  );
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
