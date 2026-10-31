import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node } from '../src/core.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test(
  'Repeater Inspector dial shares exact Base, Session gestures and interrupted editing safety',
  { timeout: 900000 },
  async (t) => {
    const win = new Window({ url: 'http://localhost:5173' });
    t.after(async () => {
      await win.happyDOM.abort();
    });
    win.document.body.innerHTML = '<div id="app"></div>';
    for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
    globalThis.MutationObserver = win.MutationObserver;
    let frames = [];
    globalThis.requestAnimationFrame = (fn) => {
      frames.push(fn);
      return frames.length;
    };
    const flush = () => {
      const jobs = frames;
      frames = [];
      jobs.forEach((fn) => fn(0));
    };
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
    const source = resolveMainRelativeImports(
      (await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8')).replace(
        "import './style.css';",
        '',
      ),
    );
    await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    await new Promise((r) => setTimeout(r, 30));
    flush();
    t.diagnostic(
      'Synthetic editor mounted. This is DOM/command evidence, not browser rendering acceptance.',
    );
    let id, layerId;
    const fixture = (value = 725, { driven, locked, layerLocked } = {}) => {
      const doc = makeDoc(),
        layer = doc.layers[0],
        repeat = node('Geometry Repeater'),
        source = node('Value'),
        out = node('Output');
      doc.schemaVersion = 37;
      repeat.operation = 'ae-2d';
      repeat.params.rotation.value = value;
      repeat.params.rotation.locked = !!locked;
      layer.locked = !!layerLocked;
      source.params.value.value = 40;
      if (driven) {
        repeat.params.rotation.expression = 'value * 2';
        if (driven === 'connected') repeat.params.rotation.source = source.id;
      }
      layer.nodes = [repeat, source, out];
      layer.output = out.id;
      id = repeat.id;
      layerId = layer.id;
      win.nect.load(JSON.stringify(doc));
    };
    const q = () => win.nect.document.layers[0].nodes.find((n) => n.id === id)?.params.rotation;
    const saved = () => JSON.stringify(win.nect.document);
    const field = () =>
      document.querySelector(`.inspector .field[data-node="${id}"][data-key="rotation"]`);
    const dial = () => field()?.querySelector('[data-repeater-angle]');
    const input = () => field().querySelector('input[data-base],input[data-value]');
    const indicator = () => dial().querySelector('[data-angle-indicator]').style.transform;
    const undo = () => document.querySelector('[data-action="undo"]').click();
    const redo = () => document.querySelector('[data-action="redo"]').click();
    const fire = (el, type, degrees = 0, extra = {}) => {
      const radians = (degrees * Math.PI) / 180;
      el.dispatchEvent(
        new win.PointerEvent(type, {
          bubbles: true,
          cancelable: true,
          button: 0,
          pointerId: 3,
          clientX: 118 + Math.cos(radians) * 14,
          clientY: 218 + Math.sin(radians) * 14,
          ...extra,
        }),
      );
      flush();
    };
    const key = (el, key, extra = {}) =>
      el.dispatchEvent(
        new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key, ...extra }),
      );
    const start = (degrees = 0) => {
      const el = dial();
      assert.ok(el, 'Inspector dial exists');
      el.getBoundingClientRect = () => ({ left: 100, top: 200, width: 36, height: 36 });
      let captured = false;
      el.setPointerCapture = () => {
        captured = true;
      };
      el.hasPointerCapture = () => captured;
      el.releasePointerCapture = () => {
        captured = false;
        fire(el, 'lostpointercapture');
      };
      fire(el, 'pointerdown', degrees);
      return el;
    };
    const drag = (angles) => {
      const el = start(angles[0]);
      for (const angle of angles.slice(1)) fire(document, 'pointermove', angle);
      fire(document, 'pointerup', angles.at(-1));
      return el;
    };
    const near = (a, b) => assert.ok(Math.abs(a - b) < 1e-10, `${a} != ${b}`);
    fixture(0);
    openDOMPanel(document, 'inspector');
    input().value = '725';
    input().dispatchEvent(new win.Event('change', { bubbles: true }));
    // End any deferred field render before acquiring the stable pointer target.
    await new Promise((r) => setTimeout(r, 20));
    assert.equal(q().value, 725);
    assert.equal(input().value, '725');
    assert.equal(indicator(), 'rotate(5deg)');
    assert.equal(
      document.querySelector('.graph [data-repeater-angle]'),
      null,
      'first scope is Inspector only',
    );
    assert.equal(dial().getAttribute('role'), 'slider');
    assert.match(dial().getAttribute('aria-label'), /Rotation dial/);
    assert.match(dial().title, /0° right; clockwise positive/);
    const before = saved(),
      state = win.nect.commandState;
    const el = start(170);
    fire(document, 'pointermove', 180);
    assert.equal(dial(), el, 'preview never replaces the captured element');
    assert.equal(q().value, 725, 'preview is not authored');
    assert.equal(Number(input().value), 735);
    assert.equal(
      win.nect.previewSnapshot(win.nect.gestureState.gestureId).document.layers[0].nodes[0].params
        .rotation.value,
      735,
    );
    fire(document, 'pointerup', 180);
    assert.equal(win.nect.gestureState, null, 'own release does not cancel the finished drag');
    assert.equal(q().value, 735);
    assert.equal(indicator(), 'rotate(15deg)');
    assert.equal(win.nect.commandState.expectedRevision, state.expectedRevision + 1);
    undo();
    assert.equal(saved(), before);
    redo();
    assert.equal(q().value, 735);
    drag([0, 90, 180, -90, 0]);
    near(q().value, 1095);
    drag([0, -90, -180, 90, 0]);
    near(q().value, 735);
    drag([-170, 180, 170]);
    near(q().value, 715);
    t.diagnostic(
      'Exact numeric 725→dial 735, one Undo/Redo, stable capture, signed seams and full turns passed.',
    );
    // Both Undo and Redo must survive every cancellation path.
    undo();
    const cancelBefore = saved(),
      cancelState = win.nect.commandState;
    for (const ending of [
      'Escape',
      'pointercancel',
      'lostpointercapture',
      'window-blur',
      'control-blur',
      'detach',
    ]) {
      const el = start();
      fire(document, 'pointermove', 30);
      assert.ok(win.nect.gestureState, `${ending} begins a preview`);
      if (ending === 'Escape') key(el, 'Escape');
      else if (ending === 'window-blur') win.dispatchEvent(new win.Event('blur'));
      else if (ending === 'control-blur') el.blur();
      else if (ending === 'detach') {
        el.remove();
        fire(document, 'pointerup', 30);
      } else fire(ending === 'lostpointercapture' ? el : document, ending, 30);
      assert.equal(win.nect.gestureState, null, `${ending} cancels`);
      assert.equal(saved(), cancelBefore, `${ending} keeps authored state`);
      assert.deepEqual(win.nect.commandState, cancelState, `${ending} keeps revision`);
      fire(document, 'pointerup', 30);
      assert.equal(saved(), cancelBefore, 'late release never commits');
    }
    redo();
    near(q().value, 715);
    undo();
    assert.equal(saved(), cancelBefore);
    fixture(700);
    input().focus();
    input().value = '725';
    drag([170, 180]);
    assert.equal(q().value, 735, 'pending numeric draft becomes the exact gesture baseline');
    undo();
    assert.equal(q().value, 725, 'typed value and drag are separate edits');
    undo();
    assert.equal(q().value, 700);
    fixture(725);
    const count = document.querySelector(
      `.inspector .field[data-node="${id}"][data-key="count"] input[data-value]`,
    );
    const originalCount = Number(count.value);
    count.focus();
    count.value = '4';
    // happy-dom does not synthesize the browser's native change on blur.
    count.addEventListener(
      'blur',
      () => count.dispatchEvent(new win.Event('change', { bubbles: true })),
      { once: true },
    );
    drag([170, 180]);
    assert.equal(q().value, 735, 'another numeric blur commit does not swallow the first drag');
    undo();
    assert.equal(q().value, 725);
    undo();
    assert.equal(win.nect.document.layers[0].nodes[0].params.count.value, originalCount);
    fixture(0.000000000123456789);
    assert.equal(input().value, '1.23456789e-10', 'no three-decimal quantization');
    const tiny = q().value;
    drag([0, 0]);
    assert.equal(q().value, tiny);
    start();
    // Even a corrupted/rounded widget cannot become the authored start value.
    input().value = '0';
    fire(document, 'pointermove', 90);
    fire(document, 'pointerup', 90);
    assert.equal(q().value, tiny + 90);
    undo();
    assert.equal(q().value, tiny);
    start();
    fire(document, 'pointermove', 0, { clientX: 118, clientY: 218 });
    fire(document, 'pointermove', 180);
    assert.equal(Number(input().value), tiny, 'center transit causes no 180° jump');
    fire(document, 'pointerup', 180);
    assert.equal(q().value, tiny);
    // Numeric editing uses full JS precision, including ordinary horizontal scrubbing.
    input().value = '0.12345678912345678';
    input().dispatchEvent(new win.Event('change', { bubbles: true }));
    await new Promise((r) => setTimeout(r, 20));
    const fraction = q().value;
    assert.equal(input().value, String(fraction));
    fire(input(), 'pointerdown', 0, { clientX: 100 });
    fire(document, 'pointermove', 0, { clientX: 105 });
    fire(document, 'pointerup');
    assert.equal(q().value, fraction + 1);
    undo();
    assert.equal(q().value, fraction);
    for (const bad of ['', 'NaN', 'Infinity']) {
      const prior = saved();
      input().value = bad;
      input().dispatchEvent(new win.Event('change', { bubbles: true }));
      await new Promise((r) => setTimeout(r, 20));
      assert.equal(saved(), prior, `invalid numeric ${bad} rejects`);
    }
    fixture(-1e12);
    drag([170, -180]);
    assert.equal(q().value, -1e12 + 10, 'no legacy ±1e9 bound');
    // Dial keys have their own target and cannot nudge the selected graph node.
    const nodePosition = win.nect.document.layers[0].nodes[0].position;
    dial().focus();
    key(dial(), 'ArrowRight');
    assert.equal(q().value, -1e12 + 11);
    assert.equal(document.activeElement, dial(), 'keyboard focus survives a commit render');
    key(dial(), 'ArrowDown', { shiftKey: true });
    assert.equal(q().value, -1e12 + 1);
    assert.deepEqual(win.nect.document.layers[0].nodes[0].position, nodePosition);
    fixture(10);
    dial().focus();
    key(dial(), 'ArrowUp', { altKey: true });
    assert.equal(q().value, 10.1);
    const numericValue = q().value;
    key(input(), 'ArrowUp');
    assert.equal(q().value, numericValue, 'numeric entry is not a dial key');
    t.diagnostic(
      'Cancellation/history, center dead zone, tiny fractions, invalid values and isolated keyboard editing passed.',
    );
    for (const driven of ['expression', 'connected']) {
      fixture(725, { driven });
      const original = q();
      assert.equal(dial().disabled, false);
      assert.match(dial().getAttribute('aria-label'), /base dial/);
      assert.ok(input().hasAttribute('data-base'));
      assert.match(field().textContent, /Base/);
      const drivenDial = start(0);
      fire(document, 'pointermove', 10);
      assert.equal(dial(), drivenDial, 'effective result refresh keeps the captured dial');
      assert.match(
        field().querySelector('output').textContent,
        driven === 'connected' ? /Connected result · 40/ : /Result · 1470/,
        'live Result uses the same detached preview as Canvas',
      );
      fire(document, 'pointerup', 10);
      assert.deepEqual(q(), { ...original, value: 735 });
      assert.match(
        field().querySelector('output').textContent,
        driven === 'connected' ? /Connected result · 40/ : /Result · 1470/,
      );
      if (driven === 'connected')
        assert.match(field().textContent, /Expression paused by connection/);
      undo();
      assert.deepEqual(q(), original);
    }
    for (const state of [{ locked: true }, { layerLocked: true }]) {
      fixture(725, state);
      const lockedBefore = saved();
      assert.equal(dial().disabled, true);
      assert.equal(dial().getAttribute('aria-disabled'), 'true');
      assert.equal(input().disabled, true);
      drag([0, 10]);
      key(dial(), 'ArrowRight');
      assert.equal(saved(), lockedBefore);
    }
    fixture();
    const stale = dial();
    const oldField = field();
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'set-parameter-base',
      payload: { layerId, nodeId: id, key: 'rotation', value: 800 },
    });
    document.querySelector('.inspector-body').append(oldField);
    const staleBefore = saved();
    fire(stale, 'pointerdown');
    fire(document, 'pointermove', 30);
    fire(document, 'pointerup', 30);
    assert.equal(saved(), staleBefore);
    assert.equal(win.nect.gestureState, null, 'revision-stale control rejects');
    oldField.remove();
    const deleted = dial(),
      deletedField = field();
    win.nect.dispatch({
      ...win.nect.commandState,
      type: 'delete-node',
      payload: { layerId, nodeId: id },
    });
    document.querySelector('.inspector-body').append(deletedField);
    const deletedBefore = saved();
    fire(deleted, 'pointerdown');
    fire(document, 'pointermove', 30);
    fire(document, 'pointerup', 30);
    assert.equal(saved(), deletedBefore);
    assert.equal(win.nect.gestureState, null, 'deleted target rejects');
    t.diagnostic(
      'Driven Base/source/dormant expression priority and locked/stale/deleted targets passed.',
    );
  },
);
