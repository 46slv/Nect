import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize, deserialize, evaluate } from '../src/core.js';

test('Join Graph disconnect preserves stable keys and toolbar Undo/Redo', async (t) => {
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
  const live = () => win.nect.document.layers[0].nodes.find((n) => n.id === join.id);
  const geometry = () => win.nect.evaluate().run(join.id);
  assert.equal(live().params.a.source, a.id);
  assert.equal(live().params.b.source, b.id);
  const before = serialize(win.nect.document),
    order = [...live().joinInputOrder];
  const savedA = JSON.stringify(live().params.a),
    savedB = JSON.stringify(live().params.b);
  assert.equal(order.length, 3);
  const full = structuredClone(geometry());
  assert.equal(full.contours.length, 2);
  assert.equal(serialize(win.nect.document), before);
  const input = document.querySelector(
    `.graph .field[data-node="${join.id}"][data-key="b"] .socket.input`,
  );
  assert.ok(input);
  input.dispatchEvent(
    new win.MouseEvent('contextmenu', { bubbles: true, cancelable: true, button: 2 }),
  );
  const disconnected = serialize(win.nect.document),
    one = structuredClone(geometry());
  assert.equal(live().params.a.source, a.id);
  assert.equal(JSON.stringify(live().params.a), savedA);
  assert.equal(live().params.b.source, undefined);
  assert.equal(live().params.b.value, JSON.parse(savedB).value);
  assert.deepEqual(live().joinInputOrder, order);
  assert.equal(live().params[order[2]].source, undefined);
  assert.equal(one.contours.length, 1);
  assert.equal(serialize(win.nect.document), disconnected);
  document.querySelector('[data-action="undo"]').click();
  assert.equal(serialize(win.nect.document), before);
  assert.deepEqual(live().joinInputOrder, order);
  assert.deepEqual(geometry(), full);
  document.querySelector('[data-action="redo"]').click();
  assert.equal(serialize(win.nect.document), disconnected);
  assert.deepEqual(live().joinInputOrder, order);
  assert.equal(live().params[order[2]].source, undefined);
  assert.equal(live().params.a.source, a.id);
  assert.equal(live().params.b.source, undefined);
  assert.deepEqual(geometry(), one);
  const reopened = deserialize(disconnected);
  assert.equal(serialize(reopened), disconnected);
  assert.deepEqual(evaluate(reopened.layers[0]).run(join.id), one);
  await win.happyDOM.close();
});
