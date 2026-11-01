import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, parameterReferenceExpression as ref } from '../src/core.js';

test('actual Color editor exposes typed completion, base/expression/result, cancel, errors and one Undo', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
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
  const saved = () => JSON.stringify(win.nect.document);
  const change = (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const key = (el, k) =>
    el.dispatchEvent(new win.KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true }));
  const undo = () => document.querySelector('[data-action="undo"]').click();
  await tick();
  for (const [type, colorKey] of [
    ['Color', 'value'],
    ['Linear Gradient Fill', 'startColor'],
    ['Radial Gradient Fill', 'endColor'],
  ]) {
    const d = makeDoc(),
      l = d.layers[0],
      a = node('Color'),
      b = node(type),
      scalar = node('Value');
    a.name = 'Shared Color';
    a.params.value.value = { hex: '#123456', alpha: 0.25 };
    b.params[colorKey].value = { hex: '#abcdef', alpha: 0.6 };
    l.nodes.push(a, b, scalar);
    win.nect.load(JSON.stringify(d));
    await tick();
    const field = () =>
      document.querySelector(`.field[data-node="${b.id}"][data-key="${colorKey}"]`);
    const q = () => win.nect.document.layers[0].nodes.find((n) => n.id === b.id).params[colorKey];
    const before = saved();
    field().querySelector('[data-fx]').click();
    let input = document.querySelector('[aria-label="Parameter expression"]');
    input.value = 'va';
    input.setSelectionRange(2, 2);
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
    assert.match(document.querySelector('.expression-completions').textContent, /Color/);
    assert.doesNotMatch(document.querySelector('.expression-completions').textContent, /Scalar/);
    key(input, 'Tab');
    assert.equal(input.value, 'value');
    assert.equal(saved(), before);
    document.querySelector('[data-cancel]').click();
    assert.equal(saved(), before);
    field().querySelector('[data-fx]').click();
    input = document.querySelector('[aria-label="Parameter expression"]');
    input.value = 'value';
    document.querySelector('[data-confirm]').click();
    await tick();
    assert.equal(q().expression, 'value');
    assert.match(field().querySelector('output').textContent, /#abcdef.*0.6/);
    assert.ok(field().querySelector('[data-expression]'), 'saved expression stays visible');
    const withExpression = saved();
    field().querySelector('[data-hsl-picker-open]').click();
    change(document.querySelector('[data-hsl-hex]'), '#987654');
    document.querySelector('[data-hsl-close]').click();
    await tick();
    assert.equal(q().value.hex, '#987654');
    assert.equal(q().expression, 'value');
    undo();
    await tick();
    assert.equal(saved(), withExpression);
    let alpha = field().querySelector('[data-color="alpha"]');
    alpha.value = '0.1';
    key(alpha, 'Escape');
    assert.equal(saved(), withExpression);
    assert.equal(alpha.value, '0.6');
    alpha = field().querySelector('[data-color="alpha"]');
    alpha.value = '0.35';
    key(alpha, 'Enter');
    await tick();
    assert.equal(q().value.alpha, 0.35);
    undo();
    await tick();
    assert.equal(saved(), withExpression);
    field().querySelector('[data-fx]').click();
    input = document.querySelector('[aria-label="Parameter expression"]');
    input.value = ref(a.id, 'value');
    document.querySelector('[data-confirm]').click();
    await tick();
    assert.match(field().querySelector('output').textContent, /#123456.*0.25/);
    assert.equal(
      field().querySelector('[data-hsl-picker-open]').getAttribute('style'),
      'background:#abcdef',
    );
    const linked = saved();
    change(field().querySelector('[data-color="alpha"]'), '2');
    await tick();
    assert.equal(saved(), linked);
    field().querySelector('[data-fx]').click();
    input = document.querySelector('[aria-label="Parameter expression"]');
    input.value = 'value + 1';
    document.querySelector('[data-confirm]').click();
    await tick();
    assert.equal(saved(), linked);
    assert.ok(document.querySelector('dialog'), 'invalid draft remains available for repair');
    document.querySelector('[data-cancel]').click();
    const missing = structuredClone(win.nect.document);
    missing.layers[0].nodes = missing.layers[0].nodes.filter((n) => n.id !== a.id);
    win.nect.load(JSON.stringify(missing));
    await tick();
    assert.match(field().querySelector('output').textContent, /Cannot evaluate/);
    assert.equal(
      field().querySelector('[data-hsl-picker-open]').getAttribute('style'),
      'background:#abcdef',
    );
  }
  await win.happyDOM.abort();
});
