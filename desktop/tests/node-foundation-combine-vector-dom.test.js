import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize, deserialize } from '../src/core.js';
import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('Combine Graph and Inspector retain Scalar fields, Vector2 output and shared Session history', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    matrixTransform() {
      return this;
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({ inverse: () => ({}) });
  globalThis.fetch = async () => ({ ok: false });
  const source = resolveMainRelativeImports(
    (await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8')).replace(
      "import './style.css';",
      '',
    ),
  );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let i = 0; i < 200 && !win.nect; i++) await tick();
  assert.ok(win.nect);
  const doc = makeDocument(),
    combine = node('Combine Vector');
  doc.layers[0].nodes.push(combine);
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  const head = document.querySelector(`.graph .node[data-id="${combine.id}"] .node-head .type-tag`);
  assert.ok(head);
  head.dispatchEvent(
    new win.PointerEvent('pointerdown', {
      bubbles: true,
      cancelable: true,
      button: 0,
      pointerId: 7,
    }),
  );
  document.dispatchEvent(
    new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 7 }),
  );
  openDOMPanel(document, 'inspector');
  const input = (key) =>
    document.querySelector(
      `.inspector-body .field[data-node="${combine.id}"][data-key="${key}"] [data-value]`,
    );
  for (const key of ['x', 'y']) {
    assert.ok(
      document.querySelector(`.graph .field[data-node="${combine.id}"][data-key="${key}"]`),
    );
    assert.equal(input(key).value, '0');
  }
  assert.equal(document.querySelector(`.inspector-body [data-operation="${combine.id}"]`), null);
  const output = document.querySelector(
    `.graph [data-source="${combine.id}"]:not([data-source-port])`,
  );
  assert.ok(output);
  assert.match(output.title, /Vector2 output/);
  const before = serialize(win.nect.document);
  input('x').value = '42';
  input('x').dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.deepEqual(win.nect.evaluate().run(combine.id), [42, 0]);
  const after = serialize(win.nect.document);
  document.querySelector('[data-action="undo"]').click();
  assert.equal(serialize(win.nect.document), before);
  assert.deepEqual(win.nect.evaluate().run(combine.id), [0, 0]);
  document.querySelector('[data-action="redo"]').click();
  assert.equal(serialize(win.nect.document), after);
  assert.deepEqual(win.nect.evaluate().run(combine.id), [42, 0]);
  assert.equal(serialize(deserialize(after)), after);
  win.nect.load(after);
  assert.deepEqual(win.nect.evaluate().run(combine.id), [42, 0]);
  await win.happyDOM.close();
});
