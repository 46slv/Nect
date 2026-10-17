import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize } from '../src/core.js';

test('Halftone and Ordered Dither are discoverable, wired as PaintedPath controls, and bypass in the editor', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'Event', 'requestAnimationFrame'])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
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
    shape = node('Rectangle'),
    fill = node('Fill'),
    effect = node('Halftone'),
    out = node('Output');
  doc.schemaVersion = 10;
  shape.params.center.value = [8, 8];
  shape.params.size.value = [16, 16];
  fill.params.path.source = shape.id;
  fill.params.color.value = { hex: '#808080', alpha: 1 };
  effect.params.path.source = fill.id;
  out.params.path.source = effect.id;
  layer.nodes = [shape, fill, effect, out];
  layer.output = out.id;
  win.nect.load(serialize(doc));
  await tick();
  const query = (selector) => win.document.querySelector(selector),
    types = [...query('#add-node').options].map((option) => option.textContent);
  assert.ok(types.includes('Halftone'));
  assert.ok(types.includes('Ordered Dither'));
  assert.equal(
    query(`.node[data-id="${effect.id}"] .field[data-key="path"]`).dataset.type,
    'PaintedPath',
  );
  assert.equal(
    query(`.node[data-id="${effect.id}"] .field[data-key="path"] label`).textContent,
    'Source fill',
  );
  assert.equal(query(`.node[data-id="${effect.id}"] .field[data-key="pitch"] input`).min, '1');
  assert.equal(query(`.node[data-id="${effect.id}"] .field[data-key="pitch"] input`).max, '1024');
  const origin = query(`.node[data-id="${effect.id}"] .field[data-key="gridOrigin"]`);
  assert.equal(origin.dataset.type, 'Vector2');
  assert.equal(origin.querySelector('label').textContent, 'Grid origin · cell (0, 0) top-left');
  assert.deepEqual(
    [...origin.querySelectorAll('input[data-value]')].map((input) => [input.min, input.max]),
    [
      ['-100000', '100000'],
      ['-100000', '100000'],
    ],
  );
  assert.equal(
    query(`.node[data-id="${effect.id}"] .field[data-key="invert"] input`).type,
    'checkbox',
  );
  assert.match(
    query(`.node[data-id="${effect.id}"] .font-note`).textContent,
    /sRGB.*linear light.*white/i,
  );
  assert.match(
    query(`.node[data-id="${effect.id}"] .font-note`).textContent,
    /top-left.*cell \(0, 0\).*Y increases downward/i,
  );
  assert.equal(win.nect.document.schemaVersion, 15);
  const bypass = query(`[data-bypass="${effect.id}"]`);
  assert.equal(bypass.disabled, false);
  bypass.click();
  assert.equal(
    win.nect.document.layers[0].nodes.find((item) => item.id === effect.id).bypassed,
    true,
  );
  assert.deepEqual(win.nect.evaluate().run(effect.id), win.nect.evaluate().run(fill.id));
  query(`[data-bypass="${effect.id}"]`).click();
  assert.equal(
    win.nect.document.layers[0].nodes.find((item) => item.id === effect.id).bypassed,
    undefined,
  );
  await win.happyDOM.close();
});
