import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, connect, serialize, exportSVG } from '../src/core.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('tiny connected Scalar and Vector2 readouts preserve base, source priority and document output', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  try {
    win.document.body.innerHTML = '<div id="app"></div>';
    for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
    globalThis.MutationObserver = win.MutationObserver;
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
        const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
        return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      },
    });
    let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
    source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
    await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));

    const doc = makeDoc(),
      layer = doc.layers[0],
      rectangle = node('Rectangle'),
      transform = node('Transform'),
      fill = node('Fill'),
      output = node('Output'),
      scalar = node('Value'),
      vector = node('Vector');
    doc.schemaVersion = 27;
    layer.nodes = [rectangle, transform, fill, output, scalar, vector];
    layer.output = output.id;
    scalar.params.value.value = 0.0001;
    vector.params.value.value = [0.0001, -0.0001];
    transform.params.rotation.value = 0.0001;
    transform.params.rotation.expression = 'value + 42';
    rectangle.params.center.value = [0.000123456, -0.000234567];
    rectangle.params.center.expression = 'value + 10; value + 20';
    connect(layer, rectangle.id, transform.id, 'path');
    connect(layer, transform.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    connect(layer, scalar.id, transform.id, 'rotation');
    connect(layer, vector.id, rectangle.id, 'center');
    win.nect.load(serialize(doc));

    const field = (id, key) =>
      document.querySelector(`.graph .field[data-node="${id}"][data-key="${key}"]`);
    const saved = serialize(win.nect.document),
      expectedSVG = exportSVG(win.nect.document);
    // Re-render and reload must not round authored state or exported geometry.
    for (let pass = 0; pass < 2; pass++) {
      const rotation = field(transform.id, 'rotation'),
        center = field(rectangle.id, 'center'),
        evaluation = win.nect.evaluate();
      assert.equal(rotation.querySelector('output').textContent, 'Connected result · 0.0001');
      assert.equal(
        center.querySelector('output').textContent,
        'Connected result · 0.0001, -0.0001',
      );
      assert.equal(rotation.querySelector('[data-base]').value, '0.0001');
      assert.deepEqual(
        [...center.querySelectorAll('[data-base]')].map((input) => input.value),
        ['0.000123456', '-0.000234567'],
      );
      assert.match(rotation.textContent, /Expression paused by connection/);
      assert.equal(evaluation.parameterValue(transform.id, 'rotation'), 0.0001);
      assert.deepEqual(evaluation.parameterValue(rectangle.id, 'center'), [0.0001, -0.0001]);
      const nodes = win.nect.document.layers[0].nodes;
      assert.equal(nodes.find((n) => n.id === transform.id).params.rotation.value, 0.0001);
      assert.deepEqual(
        nodes.find((n) => n.id === rectangle.id).params.center.value,
        [0.000123456, -0.000234567],
      );
      assert.equal(serialize(win.nect.document), saved);
      assert.equal(win.nect.export(), expectedSVG);
      if (pass === 0) win.nect.load(saved);
    }
  } finally {
    await win.happyDOM.abort();
    win.close();
  }
});
