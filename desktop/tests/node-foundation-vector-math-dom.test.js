import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize, evaluate } from '../src/core.js';
import { VECTOR_MATH } from '../src/builtin-node-registry.js';

test('synthetic Vector Math inspector projects dynamic registered operation interface', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
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
  globalThis.fetch = async () => ({ ok: false });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let i = 0; i < 200 && !win.nect; i++) await tick();
  assert.ok(win.nect);
  const doc = makeDocument(),
    math = node('Vector Math');
  math.params.a.value = [3, 4];
  doc.layers[0].nodes.push(math);
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  const head = document.querySelector(`.graph .node[data-id="${math.id}"] .node-head .type-tag`);
  assert.ok(head);
  const fire = (target, type) =>
    target.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 7,
        clientX: 100,
        clientY: 20,
      }),
    );
  fire(head, 'pointerdown');
  fire(document, 'pointerup');
  openDOMPanel(document, 'inspector');
  const operation = () => document.querySelector(`.inspector-body [data-operation="${math.id}"]`);
  assert.ok(operation());
  assert.deepEqual(
    [...operation().options].map((option) => option.value),
    VECTOR_MATH.declaration.operations.map((item) => item.id),
  );
  const active = () =>
    [...document.querySelectorAll(`.inspector-body .field[data-node="${math.id}"]`)].map(
      (el) => el.dataset.key,
    );
  assert.ok(active().includes('a'));
  assert.ok(active().includes('b'));
  assert.ok(!active().includes('min'));
  operation().value = 'length';
  operation().dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(win.nect.document.layers[0].nodes.find((n) => n.id === math.id).operation, 'length');
  assert.ok(active().includes('a'));
  assert.ok(!active().includes('b'));
  assert.equal(evaluate(win.nect.document.layers[0]).run(math.id), 5);
  operation().blur();
  document.querySelector('[data-action="undo"]').click();
  assert.equal(win.nect.document.layers[0].nodes.find((n) => n.id === math.id).operation, 'add');
  document.querySelector('[data-action="redo"]').click();
  assert.equal(win.nect.document.layers[0].nodes.find((n) => n.id === math.id).operation, 'length');
  await win.happyDOM.close();
});
