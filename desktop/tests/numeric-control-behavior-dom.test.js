import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

test('P1 numeric controls retain live and staged owners', async (t) => {
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
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
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
    '\nwindow.p1NumericState = () => ({past: history.past.length, future: history.future.length, draft: pointDeltaDraft, drag: drag?.type, selected, hasGesture: session.hasGesture});';
  source += '\nwindow.p1SelectNode = (id) => { selected = id; selection = new Set([id]); };';
  source +=
    '\nwindow.p1SelectPoints = (id, ids) => { pointSelection = {nodeId:id, ids:new Set(ids)}; cancelPointDelta(); refreshPointPanels(); };';
  source += '\nwhile (!bundledFont) await new Promise(r => setTimeout(r, 10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (selector) => document.querySelector(selector);
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  const fire = (el, type, x = 100, extra = {}) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 7,
        clientX: x,
        clientY: 20,
        ...extra,
      }),
    );
  const inspect = (id) => {
    fire($(`.graph .node[data-id="${id}"] .node-head .type-tag`), 'pointerdown');
    fire(document, 'pointerup');
  };
  const pick = (id, shiftKey = false) => {
    const el = $(`.canvas-stage .point-hit[data-point="${id}"]:not([data-handle])`);
    assert.ok(el, 'fixture point is selectable');
    fire(el, 'pointerdown', +el.getAttribute('cx'), { clientY: +el.getAttribute('cy'), shiftKey });
    fire(document, 'pointerup', +el.getAttribute('cx'), { clientY: +el.getAttribute('cy') });
  };
  const load = async () => {
    const f = compoundFixture([8, 5, 7, 4]);
    f.circle = node('Circle', 1100, 40);
    f.layer.nodes.push(f.circle);
    win.nect.load(serialize(f.doc));
    inspect(f.path.id);
    pick(f.path.contours[0].points[1]);
    pick(f.path.contours[3].points[2], true);
    await tick();
    return f;
  };
  const saved = () => serialize(win.nect.document);
  const sameSaved = (before) => {
    const after = saved();
    let index = 0;
    while (index < before.length && before[index] === after[index]) index++;
    assert.equal(
      after === before,
      true,
      JSON.stringify({
        index,
        before: before.slice(index - 50, index + 90),
        after: after.slice(index - 50, index + 90),
      }),
    );
  };
  const state = () => structuredClone(win.p1NumericState());
  const delta = (axis = 0, region = '.graph') => $(`${region} [data-point-delta-axis="${axis}"]`);
  const radius = (f) =>
    $(`.graph .field[data-node="${f.circle.id}"][data-key="radius"] input[data-value]`);
  const radiusValue = (f) =>
    win.nect.document.layers[0].nodes.find((n) => n.id === f.circle.id).params.radius.value;
  const type = (el, value, event = 'input') => {
    el.value = value;
    el.dispatchEvent(new win.Event(event, { bubbles: true }));
  };
  const begin = (el, dx = 24) => {
    el.focus();
    fire(el, 'pointerdown');
    fire(document, 'pointermove', 100 + dx);
  };
  const end = (dx = 24) => fire(document, 'pointerup', 100 + dx);
  const key = (el, name) =>
    el.dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name }),
    );
  const apply = () => $('.graph [data-apply-point-delta]').click();
  const undo = () => {
    document.activeElement?.blur();
    $('[data-action="undo"]').click();
  };
  const redo = () => {
    document.activeElement?.blur();
    $('[data-action="redo"]').click();
  };

  await t.test(
    'ordinary Circle scalar keeps typing, one-Undo scrub and cancelled Redo',
    async () => {
      const f = await load();
      inspect(f.circle.id);
      type(radius(f), '130.25', 'change');
      assert.equal(radiusValue(f), 130.25);
      const before = saved();
      const target = radius(f);
      begin(target);
      assert.equal(target.isConnected, true);
      assert.equal(radiusValue(f), 130.25, 'preview is detached from authored state');
      end();
      assert.equal(radiusValue(f), 150.25);
      assert.equal(state().past, 2);
      undo();
      assert.equal(saved(), before);
      assert.equal(state().future, 1);
      const pending = state();
      begin(radius(f));
      key(radius(f), 'Escape');
      end();
      sameSaved(before);
      assert.equal(state().past, pending.past);
      assert.equal(state().future, pending.future);
      redo();
      assert.equal(radiusValue(f), 150.25);
    },
  );
  await t.test('Point Delta scrub changes only draft and Apply is one Undo', async () => {
    const f = await load();
    type(delta(), '12.5');
    type(delta(1), '-7.25');
    const before = saved(),
      original = state(),
      target = delta();
    begin(target);
    assert.equal(target.isConnected, true);
    assert.equal(document.activeElement === target, true);
    assert.equal(target.value, '32.5');
    assert.equal(delta(0, '.inspector-body').value, '32.5');
    key(target, 'Enter');
    assert.equal(saved(), before, 'Enter cannot Apply during an active scrub');
    assert.equal(saved(), before);
    assert.equal(state().past, original.past);
    assert.equal(state().future, original.future);
    end();
    assert.equal(saved(), before);
    assert.deepEqual(state().draft.values, ['32.5', '-7.25']);
    apply();
    assert.equal(state().past, original.past + 1);
    const after = saved();
    assert.notEqual(after, before);
    undo();
    assert.equal(saved(), before);
    redo();
    assert.equal(saved(), after);
    assert.equal(win.nect.document.schemaVersion, f.doc.schemaVersion);
  });
  await t.test(
    'Point Delta gesture cancellations restore the prior staged strings and Redo',
    async () => {
      for (const reason of ['Escape', 'pointercancel', 'lostpointercapture', 'blur']) {
        const f = await load();
        inspect(f.circle.id);
        type(radius(f), '140', 'change');
        undo();
        inspect(f.path.id);
        type(delta(), '12.5000');
        type(delta(1), '-7.2500');
        const before = saved(),
          original = state(),
          target = delta();
        begin(target);
        assert.equal(target.value, '32.5', reason);
        if (reason === 'Escape') key(target, reason);
        else if (reason === 'blur') win.dispatchEvent(new win.Event('blur'));
        else fire(reason === 'lostpointercapture' ? target : document, reason, 124);
        end();
        assert.equal(saved(), before, reason);
        assert.deepEqual(state().draft, original.draft, reason);
        assert.equal(delta().value, '12.5000', reason);
        assert.equal(delta(1).value, '-7.2500', reason);
        assert.equal(state().future, original.future, reason);
        assert.equal(state().past, original.past, reason);
      }
    },
  );
  await t.test('Point Delta no-op and return-to-origin retain exact draft values', async () => {
    for (const dx of [3, 4, 24]) {
      await load();
      type(delta(), '12.5000');
      const before = saved(),
        original = state();
      begin(delta(), dx);
      if (dx === 24) fire(document, 'pointermove', 100);
      end(dx === 24 ? 0 : dx);
      assert.equal(saved(), before);
      assert.deepEqual(state().draft, original.draft);
      assert.equal(delta().value, '12.5000');
    }
  });
  await t.test('Point Delta stale point selection never revives or retargets a draft', async () => {
    const f = await load(),
      before = saved();
    type(delta(), '8');
    begin(delta());
    assert.equal(delta().value, '28');
    win.p1SelectPoints(f.path.id, [f.path.contours[1].points[0]]);
    fire(document, 'pointermove', 140);
    end(40);
    apply();
    assert.equal(saved(), before);
    assert.equal(state().draft, null);
    assert.equal(state().past, 0);
  });
  await t.test(
    'Point Delta blocks concurrent commands and refuses stale staged Apply',
    async () => {
      const f = await load();
      assert.equal(state().hasGesture, false, 'fixture has no active Session gesture');
      type(delta(), '8');
      begin(delta());
      assert.equal(delta().value, '28');
      assert.equal(state().hasGesture, false, 'staged scrub does not reserve a Session gesture');
      const request = () => ({
        ...win.nect.commandState,
        type: 'translate-points',
        payload: {
          layerId: f.layer.id,
          nodeId: f.path.id,
          pointIds: [f.path.contours[2].points[0]],
          delta: [1, 0],
        },
      });
      const before = saved();
      assert.throws(() => win.nect.dispatch(request()), /GESTURE_ACTIVE/);
      assert.equal(saved(), before);
      end();
      win.nect.dispatch(request());
      const changed = saved(),
        revision = win.nect.commandState;
      apply();
      assert.equal(saved(), changed);
      assert.deepEqual(win.nect.commandState, revision);
      assert.equal(state().past, 1);
    },
  );
  await t.test('ordinary scalar stale selection cancels the Session gesture', async () => {
    const f = await load();
    inspect(f.circle.id);
    const before = saved();
    begin(radius(f));
    win.p1SelectNode(f.path.id);
    end();
    assert.equal(saved(), before);
    assert.equal(state().past, 0);
  });
});
