import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import {
  document as makeDocument,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
} from '../src/core.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { separateVectorDeclaration } from '../src/separate-vector-declaration.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { openDOMPanel, revealDOMBoundary } from './helpers/dom-workspace-controls.js';

test('registered vector projection binding is captured in immutable snapshot', () => {
  const binding = {
    evaluate: (_operation, read) => read('vector'),
    project: (id, value) => value[id === 'x' ? 0 : 1],
  };
  const map = new Map([['builtin.separate-vector.v1', binding]]);
  const input = structuredClone(separateVectorDeclaration);
  const resolved = createNodeRegistry([input], map, nectNodeHost).get('Separate Vector');
  binding.project = () => 999;
  map.set('builtin.separate-vector.v1', { evaluate: () => [999, 999], project: () => 999 });
  input.outputs[0].id = 'changed';
  assert.equal(
    resolved.project(
      'x',
      resolved.evaluate(undefined, () => [3, 8]),
    ),
    3,
  );
  assert.equal(resolved.project('y', [3, 8]), 8);
  assert.throws(() => resolved.project('unknown', [3, 8]), /output/);
});

test('public evaluation metrics show shared vector cache and invalidation', () => {
  const doc = makeDocument(),
    layer = doc.layers[0],
    vector = node('Vector'),
    separate = node('Separate Vector'),
    combine = node('Combine Vector');
  layer.nodes.push(vector, separate, combine);
  vector.params.value.value = [10, 21];
  connect(layer, vector.id, separate.id, 'vector');
  connect(layer, separate.id, combine.id, 'x', false, 'x');
  connect(layer, separate.id, combine.id, 'y', false, 'y');
  layer.output = combine.id;
  const first = evaluate(layer);
  assert.equal(first.error, null);
  assert.deepEqual(first.path, [10, 21]);
  assert.deepEqual(first.stats, { computed: 3, reused: 0 });
  assert.equal(first.values.has(separate.id), true);
  assert.deepEqual(first.values.get(separate.id), [10, 21]);
  const second = evaluate(layer);
  assert.deepEqual(second.path, [10, 21]);
  assert.deepEqual(second.stats, { computed: 0, reused: 3 });
  vector.params.value.value = [32, 45];
  const changed = evaluate(layer);
  assert.deepEqual(changed.path, [32, 45]);
  assert.deepEqual(changed.stats, { computed: 3, reused: 0 });
  connect(layer, separate.id, combine.id, 'x', false, 'y');
  const switched = evaluate(layer);
  assert.deepEqual(switched.path, [45, 45]);
  assert.deepEqual(switched.stats, { computed: 1, reused: 2 });
});

test('Separate Vector Graph sockets and Inspector use normal Session handlers', async (t) => {
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
    vector = node('Vector'),
    separate = node('Separate Vector'),
    math = node('Math'),
    combine = node('Combine Vector');
  vector.params.value.value = [13, 29];
  doc.layers[0].nodes.push(vector, separate, math, combine);
  win.nect.load(serialize(doc));
  revealDOMBoundary(document, 'graph');
  const graphField = (id, key) =>
    document.querySelector(`.graph .field[data-node="${id}"][data-key="${key}"]`);
  const select = (id) => {
    const head = document.querySelector(`.graph .node[data-id="${id}"] .node-head .type-tag`);
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
  };
  select(separate.id);
  openDOMPanel(document, 'inspector');
  const field = document.querySelector(
    `.inspector-body .field[data-node="${separate.id}"][data-key="vector"]`,
  );
  assert.ok(field);
  assert.deepEqual(
    [...field.querySelectorAll('[data-value]')].map((el) => el.value),
    ['0', '0'],
  );
  assert.equal(document.querySelector(`.inspector-body [data-operation="${separate.id}"]`), null);
  for (const [id, label] of [
    ['x', 'X'],
    ['y', 'Y'],
  ]) {
    const socket = document.querySelector(
      `.graph [data-source="${separate.id}"][data-source-port="${id}"]`,
    );
    assert.ok(socket);
    assert.equal(socket.closest('.output-port').textContent.trim(), label);
    assert.match(socket.title, /Scalar output/);
  }
  const wire = async (from, to, key, port) => {
    const socket = document.querySelector(
      `.graph [data-source="${from}"]${port ? `[data-source-port="${port}"]` : ':not([data-source-port])'}`,
    );
    assert.ok(socket);
    socket.dispatchEvent(
      new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 7 }),
    );
    assert.ok(document.querySelector('.wire-preview'));
    graphField(to, key).dispatchEvent(
      new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 7 }),
    );
    await tick();
  };
  await wire(vector.id, separate.id, 'vector');
  await wire(separate.id, math.id, 'a', 'x');
  await wire(math.id, combine.id, 'x');
  await wire(separate.id, combine.id, 'y', 'y');
  assert.deepEqual(win.nect.evaluate().run(combine.id), [13, 29]);
  const getNode = (id) => win.nect.document.layers[0].nodes.find((item) => item.id === id);
  assert.equal(getNode(math.id).params.a.sourcePort, 'x');
  assert.equal(getNode(combine.id).params.y.sourcePort, 'y');
  select(vector.id);
  const vectorInput = document.querySelector(
    `.inspector-body .field[data-node="${vector.id}"][data-key="value"] [data-value][data-axis="0"]`,
  );
  assert.ok(vectorInput);
  vectorInput.value = '41';
  vectorInput.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.deepEqual(win.nect.evaluate().run(combine.id), [41, 29]);
  document.querySelector('[data-action="undo"]').click();
  assert.deepEqual(win.nect.evaluate().run(combine.id), [13, 29]);
  document.querySelector('[data-action="redo"]').click();
  assert.deepEqual(win.nect.evaluate().run(combine.id), [41, 29]);
  const saved = serialize(win.nect.document);
  assert.equal(serialize(deserialize(saved)), saved);
  win.nect.load(saved);
  assert.deepEqual(win.nect.evaluate().run(combine.id), [41, 29]);
  assert.equal(getNode(math.id).params.a.sourcePort, 'x');
  assert.equal(getNode(combine.id).params.y.sourcePort, 'y');
  await win.happyDOM.close();
});
