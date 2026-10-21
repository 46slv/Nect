import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, parameterReferenceExpression as ref } from '../src/core.js';

test('Color operation drafts validate before commit, remain neutral on Cancel/error/stale, and preserve one Undo', async () => {
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
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((r) => setTimeout(r, 20));
  const saved = () => JSON.stringify(win.nect.document);
  const change = (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const key = (el, k) =>
    el.dispatchEvent(new win.KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true }));
  const undo = () => document.querySelector('[data-action="undo"]').click();
  await tick();
  const d = makeDoc(),
    l = d.layers[0],
    c = node('Color'),
    weight = node('Value');
  c.params.value.value = { hex: '#ff0000', alpha: 0 };
  weight.params.value.value = 0.5;
  l.nodes.push(c, weight);
  win.nect.load(JSON.stringify(d));
  await tick();
  const field = () => document.querySelector(`.field[data-node="${c.id}"][data-key="value"]`);
  const popup = () => {
    field().querySelector('[data-fx]').click();
    return document.querySelector('[aria-label="Parameter expression"]');
  };
  const type = (input, value) => {
    input.value = value;
    input.setSelectionRange(value.length, value.length);
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
  };
  const initial = saved(),
    originalState = win.nect.commandState;
  const originalFileState = win.nect.fileState;
  let input = popup();
  type(input, 'mi');
  assert.match(
    document.querySelector('.expression-completions').textContent,
    /mix\(Color, Color, Scalar\)/,
  );
  key(input, 'Tab');
  assert.equal(input.value, 'mix()');
  type(input, 'mix(value,#000000,2)');
  assert.equal(input.getAttribute('aria-invalid'), 'true');
  assert.match(input.validationMessage, /0 to 1/);
  assert.equal(saved(), initial);
  assert.deepEqual(win.nect.commandState, originalState);
  assert.deepEqual(
    win.nect.fileState,
    originalFileState,
    'invalid draft does not dirty or advance file state',
  );
  document.querySelector('dialog [data-confirm]').click();
  assert.equal(saved(), initial);
  document.querySelector('dialog [data-cancel]').click();
  assert.equal(saved(), initial);
  input = popup();
  type(input, 'alpha(value,0.5)');
  assert.equal(input.getAttribute('aria-invalid'), 'false');
  document.querySelector('dialog [data-confirm]').click();
  await tick();
  assert.equal(win.nect.document.schemaVersion, 43);
  assert.match(field().querySelector('output').textContent, /#ff0000.*0.5/);
  const authored = saved();
  const alpha = field().querySelector('[data-color="alpha"]');
  change(alpha, '0.25');
  await tick();
  assert.match(
    field().querySelector('output').textContent,
    /#ff0000.*0.5/,
    'alpha replaces base alpha',
  );
  undo();
  await tick();
  assert.equal(saved(), authored);
  input = field().querySelector('[data-expression]');
  type(input, 'alpha(value,-1)');
  assert.equal(input.getAttribute('aria-invalid'), 'true');
  input.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(saved(), authored);
  input = popup();
  type(input, 'mix(mix(#000000,#333333,0.5),#000000,0.5)');
  document.querySelector('dialog [data-confirm]').click();
  await tick();
  assert.match(field().querySelector('output').textContent, /#161616/);
  const nested = saved(),
    nestedState = win.nect.commandState;
  const nestedFileState = win.nect.fileState;
  input = popup();
  document.querySelector('dialog [data-confirm]').click();
  await tick();
  assert.equal(saved(), nested);
  assert.deepEqual(win.nect.commandState, nestedState);
  assert.deepEqual(win.nect.fileState, nestedFileState, 'unchanged Apply preserves file state');
  input = popup();
  type(input, `mix(value,#000000,${ref(weight.id, 'value')})`);
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-parameter-base',
    payload: { layerId: l.id, nodeId: weight.id, key: 'value', value: 0.25 },
  });
  const changed = saved(),
    changedState = win.nect.commandState;
  const changedFileState = win.nect.fileState;
  assert.ok(document.querySelector('dialog'), 'the stable dialog survives external updates');
  document.querySelector('dialog [data-confirm]').click();
  await tick();
  assert.equal(saved(), changed);
  assert.deepEqual(win.nect.commandState, changedState);
  assert.deepEqual(win.nect.fileState, changedFileState, 'stale Apply preserves file state');
  assert.match(document.body.textContent, /STALE|revision/i);
  document.querySelector('dialog [data-cancel]').click();
  await win.happyDOM.abort();
});
