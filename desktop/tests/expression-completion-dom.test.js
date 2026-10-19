import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node } from '../src/core.js';
test('expression dialog completion preserves explicit Apply, Cancel, Undo and searchable insertion', async () => {
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
    sourceNode = node('Vector'),
    sink = node('Value'),
    out = node('Output');
  sourceNode.name = 'Controller';
  sourceNode.params.value.value = [40, 50];
  layer.nodes = [sourceNode, sink, out];
  layer.output = out.id;
  win.nect.load(JSON.stringify(d));
  await tick();
  const saved = () => JSON.stringify(win.nect.document);
  const before = saved();
  const open = () => {
    document.querySelector(`[data-node="${sink.id}"][data-key="value"] [data-fx]`).click();
    return document.querySelector('input[aria-label="Parameter expression"]');
  };
  const type = (input, text) => {
    input.value = text;
    input.setSelectionRange(text.length, text.length);
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
  };
  const key = (input, key) =>
    input.dispatchEvent(new win.KeyboardEvent('keydown', { key, bubbles: true, cancelable: true }));
  let input = open();
  type(input, 'cont');
  assert.equal(document.querySelectorAll('.expression-completions [role=option]').length, 2);
  key(input, 'Enter');
  assert.equal(input.value, `ref("${sourceNode.id}", "value", "x")`);
  assert.equal(saved(), before);
  assert.ok(document.contains(input));
  document.querySelector('[data-cancel]').click();
  assert.equal(saved(), before);
  input = open();
  type(input, 'cont');
  key(input, 'ArrowDown');
  key(input, 'Tab');
  assert.match(input.value, /"y"/);
  key(input, 'Enter');
  await tick();
  assert.ok(!document.contains(input));
  assert.match(
    win.nect.document.layers[0].nodes.find((n) => n.id === sink.id).params.value.expression,
    /"y"/,
  );
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(saved(), before);
  input = open();
  type(input, '10 + ');
  document.querySelector('[data-insert-reference]').click();
  const search = document.querySelector('.search-panel input');
  assert.ok(search);
  search.value = 'Controller';
  search.dispatchEvent(new win.Event('input', { bubbles: true }));
  document.querySelector('.search-panel .picker-apply').click();
  assert.match(input.value, /^10 \+ ref\(/);
  assert.equal(saved(), before);
  document.querySelector('[data-cancel]').click();
  await win.happyDOM.abort();
});
