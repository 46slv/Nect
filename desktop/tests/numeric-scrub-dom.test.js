import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
// Load geometry before a synthetic browser global exists: geometry remains headless.
import '../src/core.js';
test('Numeric scrubbing preserves transactions, bounds and protected inputs', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
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
      let b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
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
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await new Promise((r) => setTimeout(r, 100));
  const tick = () => new Promise((r) => setTimeout(r, 10));
  const change = async (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
  };
  const add = async (type) => {
    await change(document.querySelector('#add-node'), type);
    return win.nect.document.layers[0].nodes.at(-1).id;
  };
  const { document: makeDoc, node, connect } = await import('../src/core.js');
  const load = (type = 'Rectangle', setup = () => {}) => {
    const doc = makeDoc(),
      n = node(type);
    doc.layers[0].nodes.unshift(n);
    setup(n, doc.layers[0]);
    win.nect.load(JSON.stringify(doc));
    return n.id;
  };
  const input = (id, key, axis = 0, panel = '.inspector-body') =>
    document.querySelector(
      `${panel} .field[data-node="${id}"][data-key="${key}"] input[type="number"][data-axis="${axis}"]`,
    );
  const val = (id, key) =>
    win.nect.document.layers[0].nodes.find((n) => n.id === id).params[key].value;
  const pointer = (el, type, x, pointerId = 1, y = 20) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId,
        clientX: x,
        clientY: y,
      }),
    );
  const scrub = (el, dx, end = 'pointerup') => {
    pointer(el, 'pointerdown', 100);
    pointer(document, 'pointermove', 100 + dx);
    pointer(document, end, 100 + dx);
  };
  const undo = () => document.querySelector('[data-action="undo"]').click();
  const redo = () => document.querySelector('[data-action="redo"]').click();
  const undoEmpty = () =>
    assert.equal(document.querySelector('[data-action="undo"]').disabled, true);
  let id = load();
  const original = structuredClone(val(id, 'center'));
  scrub(input(id, 'center'), 24);
  assert.deepEqual(val(id, 'center'), [original[0] + 20, original[1]]);
  undo();
  assert.deepEqual(val(id, 'center'), original);
  undoEmpty();
  redo();
  assert.equal(val(id, 'center')[0], original[0] + 20);
  undo();
  scrub(input(id, 'center', 1, '.graph'), -14);
  assert.deepEqual(val(id, 'center'), [original[0], original[1] - 10]);
  undo();
  undoEmpty();
  assert.deepEqual(win.nect.document.layers[0].nodes.find((n) => n.id === id).position, [40, 40]);
  // Jitter, vertical movement, returning to origin and mismatched pointer events are no-ops.
  scrub(input(id, 'center'), 3);
  undoEmpty();
  let el = input(id, 'center');
  pointer(el, 'pointerdown', 100);
  pointer(document, 'pointermove', 100, 1, 90);
  pointer(document, 'pointerup', 100);
  undoEmpty();
  pointer(el, 'pointerdown', 100);
  pointer(document, 'pointermove', 140, 2);
  pointer(document, 'pointerup', 140, 2);
  assert.deepEqual(val(id, 'center'), original);
  pointer(document, 'pointermove', 124);
  pointer(document, 'pointermove', 100);
  pointer(document, 'pointerup', 100);
  undoEmpty();
  for (const ending of ['pointercancel', 'escape', 'blur', 'lostpointercapture']) {
    el = input(id, 'center');
    pointer(el, 'pointerdown', 100);
    pointer(document, 'pointermove', 124);
    assert.equal(Number(el.value), original[0] + 20);
    assert.deepEqual(val(id, 'center'), original, 'scrub preview must not change committed state');
    if (ending === 'escape')
      el.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
    else if (ending === 'blur') win.dispatchEvent(new win.Event('blur'));
    else pointer(ending === 'lostpointercapture' ? el : document, ending, 124);
    assert.deepEqual(val(id, 'center'), original);
    undoEmpty();
    pointer(document, 'pointerup', 124);
  }
  await change(input(id, 'center'), '123.5');
  assert.equal(val(id, 'center')[0], 123.5);
  undo();
  undoEmpty();
  for (const protectedMode of ['locked', 'expression', 'source', 'layer']) {
    id = load('Rectangle', (n, layer) => {
      if (protectedMode === 'layer') layer.locked = true;
      else if (protectedMode === 'source') {
        const v = node('Vector');
        layer.nodes.push(v);
        connect(layer, v.id, n.id, 'center');
      } else n.params.center[protectedMode] = protectedMode === 'expression' ? '10; 20' : true;
    });
    const before = JSON.stringify(win.nect.document);
    // Authored Base now has its own editable control; effective inputs stay protected.
    el = document.querySelector(
      `.inspector-body .field[data-node="${id}"][data-key="center"] input[type="number"]:not([data-base])`,
    );
    if (el) scrub(el, 24);
    assert.equal(JSON.stringify(win.nect.document), before);
    undoEmpty();
  }
  for (const [type, key, dx, expected] of [
    ['Stroke', 'width', -1000, 0],
    ['Stroke', 'miterLimit', -100, 1],
    ['Stroke', 'dashOffset', 200004, 100000],
    ['Geometry Repeater', 'count', 1000, 256],
    ['Polygon', 'sides', -100, 3],
    ['Star', 'innerRadius', 1000, 120],
    ['Star', 'outerRadius', -1000, 60],
    ['Transform', 'scale', 14, 1.1],
  ]) {
    id = load(type);
    const before = val(id, key);
    scrub(input(id, key), dx);
    assert.equal(val(id, key), expected, `${type} ${key}`);
    undo();
    assert.equal(val(id, key), before);
    undoEmpty();
  }
  id = load('Color');
  const alpha = () =>
    document.querySelector(`.inspector-body .field[data-node="${id}"] [data-color="alpha"]`);
  scrub(alpha(), -24);
  assert.equal(val(id, 'value').alpha, 0.8);
  undo();
  undoEmpty();
  scrub(alpha(), 24);
  undoEmpty();
  // Exact original precision is restored when the pointer returns to its origin.
  id = load('Value', (n) => (n.params.value.value = 0.12345678901234567));
  el = input(id, 'value');
  pointer(el, 'pointerdown', 100);
  pointer(document, 'pointermove', 124);
  pointer(document, 'pointermove', 100);
  pointer(document, 'pointerup', 100);
  undoEmpty();
  id = load('Polygon', (n) => (n.params.radius.value = 0.0001));
  el = input(id, 'radius');
  pointer(el, 'pointerdown', 100);
  pointer(document, 'pointermove', 104.5);
  pointer(document, 'pointerup', 104);
  undoEmpty();
  assert.equal(val(id, 'radius'), 0.0001);
  id = load('Value');
  el = input(id, 'value');
  el.value = '250';
  scrub(el, 14);
  assert.equal(val(id, 'value'), 260);
  undo();
  assert.equal(val(id, 'value'), 250);
  undo();
  assert.equal(val(id, 'value'), 100);
  undoEmpty();
  // Display percent is converted once, including a typed value before drag.
  id = load('Transform');
  el = input(id, 'scale');
  assert.equal(Number(el.value), 100);
  el.value = '150';
  scrub(el, 14);
  assert.equal(val(id, 'scale'), 1.6);
  undo();
  assert.equal(val(id, 'scale'), 1.5);
  undo();
  assert.equal(val(id, 'scale'), 1);
  undoEmpty();
  // Editor surfaces suppress WebView browser menus; text fields retain editing.
  for (const selector of ['.graph-scroll', '.canvas-stage']) {
    const event = new win.MouseEvent('contextmenu', { bubbles: true, cancelable: true });
    document.querySelector(selector).dispatchEvent(event);
    assert.equal(event.defaultPrevented, true);
  }
  const context = new win.MouseEvent('contextmenu', { bubbles: true, cancelable: true });
  input(id, 'scale').dispatchEvent(context);
  assert.equal(context.defaultPrevented, false);
  // Document settings use the same transactional drag contract.
  const widthBefore = win.nect.document.width;
  el = document.querySelector('[data-dimension="width"]');
  assert.ok(el.dataset.scrubStep);
  scrub(el, 24);
  assert.equal(
    win.nect.document.width,
    widthBefore + 20,
    document.querySelector('.toast')?.textContent || document.body.textContent.slice(-700),
  );
  undo();
  assert.equal(win.nect.document.width, widthBefore);
  undoEmpty();
  el = document.querySelector('[data-dimension="width"]');
  scrub(el, 24, 'pointercancel');
  assert.equal(win.nect.document.width, widthBefore);
  undoEmpty();
  el = document.querySelector('[data-dimension="width"]');
  el.value = String(widthBefore + 100);
  scrub(el, 24);
  assert.equal(win.nect.document.width, widthBefore + 120);
  undo();
  assert.equal(win.nect.document.width, widthBefore + 100);
  undo();
  assert.equal(win.nect.document.width, widthBefore);
  // Locking artwork does not lock local alignment preferences.
  id = load('Rectangle', (_n, layer) => {
    layer.locked = true;
  });
  const lockedDocument = JSON.stringify(win.nect.document);
  el = document.querySelector('[data-align-spacing]');
  await change(el, '12');
  el = document.querySelector('[data-align-spacing]');
  scrub(el, 24);
  assert.equal(Number(document.querySelector('[data-align-spacing]').value), 32);
  assert.equal(JSON.stringify(win.nect.document), lockedDocument);
  undoEmpty();
  el = document.querySelector('[data-align-spacing]');
  scrub(el, 24, 'pointercancel');
  assert.equal(Number(document.querySelector('[data-align-spacing]').value), 32);
  assert.equal(JSON.stringify(win.nect.document), lockedDocument);
  undoEmpty();
  win.happyDOM.abort();
});
