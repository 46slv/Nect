import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize } from '../src/core.js';

const onePixelPng =
  'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==';

test('Inspector exposes Image only for luminance inputs and wires/exports the same PaintedPath result', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'Event', 'requestAnimationFrame'])
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
  win.SVGElement.prototype.getScreenCTM = () => ({ a: 1, b: 0, inverse: () => ({}) });
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
  const tick = () => new Promise((resolve) => setTimeout(resolve, 25));
  for (let i = 0; i < 100 && !win.nect; i++) await tick();
  assert.ok(win.nect);

  const doc = newDocument(),
    layer = doc.layers[0],
    image = node('Image'),
    effect = node('Halftone'),
    fill = node('Fill'),
    output = node('Output');
  doc.imageAssets = {
    'image-dom': { name: 'sample.png', mime: 'image/png', width: 1, height: 1, data: onePixelPng },
  };
  image.asset = 'image-dom';
  image.params.position.value = [3, 5];
  image.params.size.value = [13, 19];
  effect.params.pitch.value = 16;
  fill.params.path.value = null;
  output.params.path.source = effect.id;
  layer.nodes = [image, effect, fill, output];
  layer.output = output.id;
  win.nect.load(serialize(doc));
  await tick();

  const query = (selector) => win.document.querySelector(selector),
    effectField = query(`.node[data-id="${effect.id}"] .field[data-key="path"]`),
    fillField = query(`.node[data-id="${fill.id}"] .field[data-key="path"]`),
    imageChoice = effectField.querySelector(`option[value="${image.id}"]`);
  assert.ok(imageChoice, 'the luminance selector offers the Image source');
  assert.equal(fillField.querySelector(`option[value="${image.id}"]`), null);
  assert.equal(effectField.dataset.type, 'PaintedPath', 'saved field type remains unchanged');
  assert.equal(effectField.querySelector('label').textContent, 'Source fill');
  assert.match(
    query(`.node[data-id="${effect.id}"] .font-note`).textContent,
    /nearest texel.*alpha.*white.*placed image rectangle/i,
  );

  const selector = effectField.querySelector('[data-path-source]');
  selector.value = image.id;
  selector.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(win.nect.document.schemaVersion, 21);
  assert.equal(
    win.nect.document.layers[0].nodes.find((item) => item.id === effect.id).params.path.source,
    image.id,
  );
  assert.equal(win.nect.evaluate().error, null);
  assert.equal(query(`[data-bypass="${effect.id}"]`).disabled, true);
  assert.match(win.nect.export(), /<path/);
  assert.doesNotMatch(win.nect.export(), /<image/);

  const editable = structuredClone(win.nect.document),
    editableLayer = editable.layers[0],
    converter = node('Convert to Path');
  editableLayer.nodes.splice(
    editableLayer.nodes.findIndex((item) => item.id === output.id),
    0,
    converter,
  );
  win.nect.load(serialize(editable));
  await tick();
  const converterField = query(`.node[data-id="${converter.id}"] .field[data-key="path"]`),
    converterSelector = converterField.querySelector('[data-path-source]');
  assert.equal(converterField.dataset.type, 'Path');
  assert.equal(converterSelector.options[0].textContent, 'Connect Path or PaintedPath…');
  assert.ok(converterSelector.querySelector(`option[value="${effect.id}"]`));
  assert.equal(converterSelector.querySelector(`option[value="${image.id}"]`), null);
  converterSelector.value = effect.id;
  converterSelector.dispatchEvent(new win.Event('change', { bubbles: true }));
  await tick();
  assert.equal(win.nect.document.schemaVersion, 21);
  assert.equal(
    win.nect.document.layers[0].nodes.find((item) => item.id === converter.id).params.path.source,
    effect.id,
  );
  assert.equal(query(`[data-bypass="${converter.id}"]`).disabled, true);
  await win.happyDOM.close();
});
