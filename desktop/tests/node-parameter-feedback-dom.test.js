import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node } from '../src/core.js';

test('node parameter feedback: split XY, searchable connections and expressions, anchored Alt zoom and Find node', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint']) globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.requestAnimationFrame = () => 0;
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
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const tick = () => new Promise((r) => setTimeout(r, 20));
  const change = async (el, value) => {
    if (el.type === 'checkbox') el.checked = value;
    else el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
  };
  await tick();
  for (const scope of ['.inspector-body', '.graph']) {
    for (const initial of [1, 1.75]) {
      const d = makeDoc(),
        t = node('Transform'),
        out = node('Output');
      t.params.scale.value = initial;
      d.layers[0].nodes = [t, out];
      d.layers[0].output = out.id;
      win.nect.load(JSON.stringify(d));
      await tick();
      const field = (key) =>
        document.querySelector(`${scope} [data-node="${t.id}"][data-key="${key}"] [data-value]`);
      const values = () => win.nect.evaluate().transformValues(t.id);
      await change(field('uniformScale'), false);
      await change(field('scale'), 230);
      assert.equal(values().scale, 2.3);
      assert.equal(values().scaleWidth, initial, `${scope}: first Y edit preserves X`);
      document.querySelector('[data-action="undo"]').click();
      await tick();
      assert.equal(values().scale, initial);
      assert.equal(values().scaleWidth, initial);
      document.querySelector('[data-action="redo"]').click();
      await tick();
      assert.equal(values().scale, 2.3);
      assert.equal(values().scaleWidth, initial);
      await change(field('scaleWidth'), 140);
      assert.equal(values().scale, 2.3);
      assert.equal(values().scaleWidth, 1.4);
      await change(field('uniformScale'), true);
      await change(field('scale'), 180);
      assert.equal(values().scaleWidth, 1.8);
    }
  }

  const d = makeDoc(),
    vector = node('Vector'),
    other = node('Vector'),
    value = node('Value'),
    text = node('Text'),
    ref = node('Reference'),
    out = node('Output');
  vector.name = 'Driver same';
  other.name = 'Driver same';
  value.name = 'Scalar incompatible';
  vector.params.value.value = [20, 30];
  d.layers[0].nodes = [text, vector, other, value, ref, out];
  d.layers[0].output = out.id;
  win.nect.load(JSON.stringify(d));
  await tick();
  const field = (key) =>
    document.querySelector(`.inspector-body [data-node="${text.id}"][data-key="${key}"]`);
  openDOMPanel(document, 'inspector');
  field('position').querySelector('[data-connect-menu]').click();
  await tick();
  let dialog = document.querySelector('dialog'),
    search = dialog.querySelector('input[type="search"]'),
    select = dialog.querySelector('select');
  assert.ok(![...select.options].some((o) => o.textContent.includes('Scalar incompatible')));
  search.value = other.id;
  search.dispatchEvent(new win.Event('input', { bubbles: true }));
  assert.equal(select.options.length, 1);
  assert.match(select.options[0].textContent, new RegExp(other.id));
  search.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  await tick();
  assert.equal(win.nect.document.layers[0].nodes[0].params.position.source, other.id);
  field('position').querySelector('[data-connect-menu]').click();
  await tick();
  dialog = document.querySelector('dialog');
  select = dialog.querySelector('select');
  assert.ok(select.value.includes(other.id), select.outerHTML);
  const before = JSON.stringify(win.nect.document);
  dialog
    .querySelector('input')
    .dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(document.querySelector('dialog'), null);
  assert.equal(JSON.stringify(win.nect.document), before);
  // A searchable expression picker inserts a stable parameter token into arithmetic.
  field('size').querySelector('[data-fx]').click();
  await tick();
  dialog = document.querySelector('dialog');
  dialog.querySelector('[data-insert-reference]').click();
  await tick();
  const picker = document.querySelector('.search-panel');
  search = picker.querySelector('input');
  search.value = `${vector.id} value.x`;
  search.dispatchEvent(new win.Event('input', { bubbles: true }));
  search.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  const expression = dialog.querySelector('input');
  assert.match(expression.value, /ref\(/);
  expression.value += ' * 2 + 5';
  dialog.querySelector('[data-confirm]').click();
  await tick();
  assert.equal(win.nect.evaluate().parameterValue(text.id, 'size'), 45);
  // Graph Alt wheel shares pointer anchoring with existing Ctrl/Cmd wheel.
  const graph = document.querySelector('.graph-scroll');
  Object.defineProperty(graph, 'clientWidth', { value: 800 });
  Object.defineProperty(graph, 'clientHeight', { value: 400 });
  graph.getBoundingClientRect = () => ({ left: 0, top: 0, width: 800, height: 400 });
  graph.scrollLeft = 100;
  graph.scrollTop = 80;
  const snapshot = JSON.stringify(win.nect.document);
  const wheel = new win.WheelEvent('wheel', {
    bubbles: true,
    cancelable: true,
    altKey: true,
    deltaY: -100,
    clientX: 300,
    clientY: 200,
  });
  for (const [key, value] of Object.entries({ altKey: true, clientX: 300, clientY: 200 }))
    Object.defineProperty(wheel, key, { value });
  graph.dispatchEvent(wheel);
  assert.equal(wheel.defaultPrevented, true);
  assert.ok(parseFloat(document.querySelector('[data-zoom="graph"]').textContent) > 100);
  assert.ok(Math.abs(graph.scrollLeft - ((100 + 300) * Math.exp(0.2) - 300)) < 0.001);
  assert.equal(JSON.stringify(win.nect.document), snapshot);
  const ordinary = new win.WheelEvent('wheel', { bubbles: true, cancelable: true, deltaY: 100 });
  graph.dispatchEvent(ordinary);
  assert.equal(ordinary.defaultPrevented, false);
  const zoom = document.querySelector('[data-zoom="graph"]').textContent;
  document.querySelector('[data-action="find-node"]').click();
  await tick();
  const finder = document.querySelector('.search-panel'),
    find = finder.querySelector('input');
  find.value = other.id;
  find.dispatchEvent(new win.Event('input', { bubbles: true }));
  find.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  assert.equal(document.querySelector('.node.selected').dataset.id, other.id);
  assert.equal(document.querySelector('[data-zoom="graph"]').textContent, zoom);
  assert.equal(JSON.stringify(win.nect.document), snapshot);
  assert.equal(document.activeElement, document.querySelector('.graph-scroll'));
  win.happyDOM.abort();
});
