import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node } from '../src/core.js';
test('authored base controls edit, scrub, cancel, show effective result and distinguish dormant source', async () => {
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
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((r) => setTimeout(r, 20));
  await tick();

  const d = makeDoc(),
    layer = d.layers[0],
    a = node('Vector'),
    b = node('Vector'),
    out = node('Output');
  a.params.value.value = [100, 200];
  b.params.value.value = [20, 10];
  d.schemaVersion = 26;
  b.params.value.expression = `ref("${a.id}", "value", "x") + value; ref("${a.id}", "value", "y") + value`;
  layer.nodes = [a, b, out];
  layer.output = out.id;
  win.nect.load(JSON.stringify(d));
  await tick();
  const q = () => win.nect.document.layers[0].nodes.find((n) => n.id === b.id).params.value;
  const field = () => document.querySelector(`.field[data-node="${b.id}"][data-key="value"]`);
  const base = (axis = 0) => field().querySelector(`[data-base][data-axis="${axis}"]`);
  const result = () => field().querySelector('output').textContent;
  const saved = () => JSON.stringify(win.nect.document);
  const undo = () => document.querySelector('[data-action="undo"]').click();
  const pointer = (el, type, x) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 1,
        clientX: x,
        clientY: 20,
      }),
    );
  const initial = saved();
  assert.match(result(), /120, 210/);
  assert.match(field().textContent, /value = each component/);
  base().value = '30';
  base().dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.deepEqual(q().value, [30, 10]);
  assert.match(result(), /130, 210/);
  undo();
  await tick();
  assert.equal(saved(), initial);
  pointer(base(), 'pointerdown', 100);
  pointer(document, 'pointermove', 114);
  pointer(document, 'pointerup', 114);
  await tick();
  assert.deepEqual(q().value, [30, 10]);
  assert.match(result(), /130, 210/);
  const scrubbed = saved();
  undo();
  await tick();
  assert.equal(saved(), initial);
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-parameter-base',
    payload: { layerId: layer.id, nodeId: b.id, key: 'value', axis: 0, value: 30 },
  });
  await tick();
  assert.equal(saved(), scrubbed, 'machine command matches the numeric scrub document');
  undo();
  await tick();
  assert.equal(saved(), initial, 'machine command Undo restores the original document');
  pointer(base(), 'pointerdown', 100);
  pointer(document, 'pointermove', 114);
  document.dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
  );
  await tick();
  assert.equal(saved(), initial);
  assert.equal(document.querySelector('[data-action="undo"]').disabled, true);
  base().value = '31';
  base().dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }),
  );
  assert.equal(base().value, '20');
  assert.equal(saved(), initial);
  base().value = '31';
  base().dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true, cancelable: true }),
  );
  await tick();
  assert.deepEqual(q().value, [31, 10]);
  undo();
  await tick();
  assert.equal(saved(), initial);
  // Completion inserts the component base only into the draft; cancel is nonmutating.
  field().querySelector('[data-fx]').click();
  let input = document.querySelector('input[aria-label="Parameter expression"]');
  input.value = 'value; va';
  input.setSelectionRange(9, 9);
  input.dispatchEvent(new win.Event('input', { bubbles: true }));
  assert.match(document.querySelector('.expression-completions').textContent, /base Y component/);
  input.dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true }),
  );
  assert.equal(input.value, 'value; value');
  assert.equal(saved(), initial);
  document.querySelector('[data-cancel]').click();
  assert.equal(saved(), initial);
  // Missing references are never displayed as base or a last-good result.
  const missing = structuredClone(win.nect.document);
  missing.layers[0].nodes = missing.layers[0].nodes.filter((n) => n.id !== a.id);
  win.nect.load(JSON.stringify(missing));
  await tick();
  assert.match(result(), /Cannot evaluate/);
  assert.equal(base().value, '20');
  const beforeBad = saved();
  base().value = '';
  base().dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(saved(), beforeBad);
  // Connected state labels base editing as dormant, even with a retained expression.
  const connected = structuredClone(d);
  connected.layers[0].nodes.find((n) => n.id === b.id).params.value.source = a.id;
  win.nect.load(JSON.stringify(connected));
  await tick();
  assert.match(result(), /Connected result · 100, 200/);
  assert.match(field().textContent, /Expression paused by connection/);
  base().value = '33';
  base().dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.deepEqual(q().value, [33, 10]);
  assert.match(result(), /100, 200/);
  const locked = structuredClone(d);
  locked.layers[0].nodes.find((n) => n.id === b.id).params.value.locked = true;
  win.nect.load(JSON.stringify(locked));
  await tick();
  assert.equal(base().disabled, true);
  await win.happyDOM.abort();
});
