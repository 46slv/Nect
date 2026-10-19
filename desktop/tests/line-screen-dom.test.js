import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, connect, serialize } from '../src/core.js';

test('Line Screen picker, direction, luminance fields and guidance are available in the editor', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
  globalThis.MutationObserver = win.MutationObserver;
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
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
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
  const tick = () => new Promise((resolve) => setTimeout(resolve, 15));
  for (let attempt = 0; attempt < 100 && !win.nect; attempt++) await tick();
  assert.ok(win.nect, 'editor booted');

  const doc = makeDoc(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    fill = node('Fill'),
    screen = node('Line Screen'),
    out = node('Output');
  shape.params.center.value = [16, 8];
  shape.params.size.value = [32, 16];
  fill.params.color.value = { hex: '#bcbcbc', alpha: 1 };
  layer.nodes = [shape, fill, screen, out];
  layer.output = out.id;
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, screen.id, 'path');
  connect(layer, screen.id, out.id, 'path');
  win.nect.load(serialize(doc));
  await tick();

  const query = (selector) => document.querySelector(selector),
    field = (key, panel = '.graph') =>
      query(`${panel} .field[data-node="${screen.id}"][data-key="${key}"]`),
    savedNode = () => win.nect.document.layers[0].nodes.find((item) => item.id === screen.id),
    change = async (element, value) => {
      element.value = value;
      element.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
    };

  assert.ok([...query('#add-node').options].some((option) => option.value === 'Line Screen'));
  assert.equal(savedNode().operation, 'horizontal');
  assert.equal(field('path').dataset.type, 'PaintedPath');
  assert.ok(field('pitch').querySelector('[data-value]'));
  assert.ok(field('invert').querySelector('[data-value][type="checkbox"]'));
  assert.equal(field('gridOrigin').dataset.type, 'Vector2');
  assert.equal(
    field('gridOrigin').querySelector('label').textContent,
    'Grid origin · cell (0, 0) top-left',
  );
  assert.deepEqual(
    [...field('gridOrigin').querySelectorAll('input[data-value]')].map((input) => [
      input.min,
      input.max,
    ]),
    [
      ['-100000', '100000'],
      ['-100000', '100000'],
    ],
  );
  const direction = () => query(`.graph [data-operation="${screen.id}"]`);
  assert.deepEqual(
    [...direction().options].map((option) => option.value),
    ['horizontal', 'vertical'],
  );

  query(`.node[data-id="${screen.id}"] .node-head strong`).dispatchEvent(
    new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 1 }),
  );
  document.dispatchEvent(
    new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 1 }),
  );
  await change(direction(), 'vertical');
  assert.equal(savedNode().operation, 'vertical');
  assert.equal(win.nect.document.schemaVersion, 21);
  await change(field('pitch').querySelector('[data-value]'), '8');
  assert.equal(savedNode().params.pitch.value, 8);
  const originInputs = field('gridOrigin').querySelectorAll('input[data-value]');
  await change(originInputs[0], '8');
  await change(field('gridOrigin').querySelectorAll('input[data-value]')[1], '-4');
  assert.deepEqual(savedNode().params.gridOrigin.value, [8, -4]);
  assert.equal(win.nect.document.schemaVersion, 21);
  const invert = field('invert').querySelector('[data-value]');
  invert.checked = true;
  invert.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(savedNode().params.invert.value, true);
  assert.match(query('.inspector-body').textContent, /cell-sampled rectangle/);
  assert.match(
    query('.inspector-body').textContent,
    /top-left.*cell \(0, 0\).*Y increases downward/i,
  );
  assert.match(query('.inspector-body').textContent, /not continuous engraving/);

  query('[data-action="undo"]').click();
  await tick();
  assert.equal(savedNode().params.invert.value, false);
  assert.deepEqual(savedNode().params.gridOrigin.value, [8, -4]);
  query('[data-action="undo"]').click();
  await tick();
  assert.deepEqual(savedNode().params.gridOrigin.value, [8, 0]);
  win.happyDOM.abort();
});
