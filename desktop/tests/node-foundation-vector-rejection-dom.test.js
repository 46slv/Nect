import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import {
  document as makeDocument,
  node,
  serialize,
  deserialize,
  setReference,
} from '../src/core.js';

test('Inspector rejects incompatible Vector Math operation while preserving Redo', async (t) => {
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
    layer = doc.layers[0],
    math = node('Vector Math'),
    ref = node('Reference');
  layer.nodes.push(math, ref);
  setReference(layer, ref.id, { nodeId: math.id, portId: 'output' });
  assert.equal(serialize(deserialize(serialize(doc))), serialize(doc));
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
  const active = () =>
    [...document.querySelectorAll(`.inspector-body .field[data-node="${math.id}"]`)].map(
      (e) => e.dataset.key,
    );
  const socket = () => document.querySelector(`.graph .node[data-id="${math.id}"] .socket.output`);
  const current = () => win.nect.document.layers[0].nodes.find((n) => n.id === math.id);
  assert.equal(operation().value, 'add');
  operation().value = 'scale';
  operation().dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(current().operation, 'scale');
  document.querySelector('[data-action="undo"]').click();
  assert.equal(current().operation, 'add');
  const before = serialize(win.nect.document),
    fields = active(),
    socketTitle = socket().title;
  assert.match(socketTitle, /Vector2/);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  operation().value = 'length';
  operation().dispatchEvent(new win.Event('change', { bubbles: true }));
  assert.equal(serialize(win.nect.document), before);
  assert.equal(current().operation, 'add');
  assert.equal(operation().value, 'add');
  assert.deepEqual(active(), fields);
  assert.equal(socket().title, socketTitle);
  assert.equal(document.querySelector('.inspector-body').dataset.inspectorNode, math.id);
  assert.match(document.querySelector('.status .message').textContent, /break|Restore/i);
  assert.equal(document.querySelector('[data-action="redo"]').disabled, false);
  document.querySelector('[data-action="redo"]').click();
  assert.equal(current().operation, 'scale');
  // HappyDOM can report a different value despite selected markup; see select-probe evidence.
  assert.equal(operation().querySelector('option[selected]').value, 'scale');
  assert.equal(serialize(deserialize(serialize(win.nect.document))), serialize(win.nect.document));
  await win.happyDOM.close();
});
