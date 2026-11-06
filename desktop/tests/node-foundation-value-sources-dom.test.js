import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDocument, node, serialize, deserialize } from '../src/core.js';
import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('Value, Vector and Expression Graph/Inspector use the same registered source and Session route', async (t) => {
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
    nodes = ['Value', 'Vector', 'Expression'].map((type) => node(type));
  doc.layers[0].nodes.push(...nodes);
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  for (const n of nodes) {
    const head = document.querySelector(`.graph .node[data-id="${n.id}"] .node-head .type-tag`);
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
    const field = document.querySelector(
      `.inspector-body .field[data-node="${n.id}"][data-key="value"]`,
    );
    assert.ok(field);
    const input = field.querySelector('[data-value]');
    assert.ok(input);
    assert.equal(input.value, n.type === 'Vector' ? '300' : '100');
    const output = document.querySelector(`.graph [data-source="${n.id}"]:not([data-source-port])`);
    assert.ok(output);
    assert.match(output.title, n.type === 'Vector' ? /Vector2 output/ : /Scalar output/);
    assert.equal(document.querySelector(`.inspector-body [data-operation="${n.id}"]`), null);
    const before = serialize(win.nect.document);
    input.value = '42';
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
    assert.deepEqual(win.nect.evaluate().run(n.id), n.type === 'Vector' ? [42, 250] : 42);
    const after = serialize(win.nect.document);
    document.querySelector('[data-action="undo"]').click();
    assert.equal(serialize(win.nect.document), before);
    document.querySelector('[data-action="redo"]').click();
    assert.equal(serialize(win.nect.document), after);
  }
  const after = serialize(win.nect.document);
  assert.equal(serialize(deserialize(after)), after);
  win.nect.load(after);
  for (const n of nodes)
    assert.deepEqual(win.nect.evaluate().run(n.id), n.type === 'Vector' ? [42, 250] : 42);
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.type === 'Vector').canvasPositionHandle,
    false,
  );
  await win.happyDOM.close();
});
