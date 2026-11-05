import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';

test('prepared Join Graph/Inspector connects stable slots through production handlers', async (t) => {
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
    a = node('Rectangle'),
    b = node('Rectangle'),
    join = node('Join Geometry');
  doc.layers[0].nodes.push(a, b, join);
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  const head = document.querySelector(`.graph .node[data-id="${join.id}"] .node-head .type-tag`);
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
  assert.match(document.querySelector('.inspector-body').textContent, /256 geometry groups/);
  const wire = async (from, key) => {
    const socket = document.querySelector(`.graph [data-source="${from}"]:not([data-source-port])`),
      field = document.querySelector(`.graph .field[data-node="${join.id}"][data-key="${key}"]`);
    assert.ok(socket);
    assert.ok(field);
    socket.dispatchEvent(
      new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 7 }),
    );
    assert.ok(document.querySelector('.wire-preview'));
    field.dispatchEvent(
      new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 7 }),
    );
    await tick();
  };
  await wire(a.id, 'a');
  await wire(b.id, 'b');
  let live = win.nect.document.layers[0].nodes.find((n) => n.id === join.id);
  assert.equal(live.params.a.source, a.id);
  assert.equal(live.params.b.source, b.id);
  assert.equal(live.joinInputOrder.length, 3);
  assert.equal(win.nect.evaluate().run(join.id).contours.length, 2);
  document.querySelector('[data-action="undo"]').click();
  live = win.nect.document.layers[0].nodes.find((n) => n.id === join.id);
  assert.equal(live.params.b.source, undefined);
  assert.equal(live.params.a.source, a.id);
  document.querySelector('[data-action="redo"]').click();
  live = win.nect.document.layers[0].nodes.find((n) => n.id === join.id);
  assert.equal(live.params.b.source, b.id);
  assert.equal(live.params.a.source, a.id);
  await win.happyDOM.close();
});
