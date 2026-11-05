import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize } from '../src/core.js';

test('prepared Reverse Path Graph/Inspector uses registered path and normal socket handler', async (t) => {
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
    curve = node('Curve'),
    reverse = node('Reverse Path');
  doc.layers[0].nodes.push(curve, reverse);
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  const head = document.querySelector(`.graph .node[data-id="${reverse.id}"] .node-head .type-tag`);
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
  const inspector = document.querySelector('.inspector-body');
  assert.match(inspector.textContent, /Reverse contour direction/);
  assert.match(inspector.textContent, /Trim Path and Dash Path/);
  const output = document.querySelector(
      `.graph [data-source="${curve.id}"]:not([data-source-port])`,
    ),
    field = document.querySelector(`.graph .field[data-node="${reverse.id}"][data-key="path"]`);
  assert.ok(output);
  assert.ok(field);
  output.dispatchEvent(
    new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 7 }),
  );
  assert.ok(document.querySelector('.wire-preview'));
  field.dispatchEvent(
    new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 7 }),
  );
  await tick();
  const live = win.nect.document.layers[0].nodes.find((n) => n.id === reverse.id);
  assert.equal(live.params.path.source, curve.id);
  assert.equal(win.nect.evaluate().run(reverse.id).unstable, true);
  document.querySelector('[data-action="undo"]').click();
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.id === reverse.id).params.path.source,
    undefined,
  );
  document.querySelector('[data-action="redo"]').click();
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.id === reverse.id).params.path.source,
    curve.id,
  );
  await win.happyDOM.close();
});
