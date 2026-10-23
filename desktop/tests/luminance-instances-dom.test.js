import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize } from '../src/core.js';

test('Luminance Instances is discoverable with typed sockets, bounded controls, and motif bypass help', async () => {
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
    effect = node('Luminance Instances');
  layer.nodes.push(effect);
  win.nect.load(serialize(doc));
  await tick();
  const query = (s) => win.document.querySelector(s),
    root = query(`.node[data-id="${effect.id}"]`);
  assert.ok([...query('#add-node').options].some((o) => o.textContent === 'Luminance Instances'));
  assert.equal(root.querySelector('.field[data-key="path"]').dataset.type, 'PaintedPath');
  assert.equal(root.querySelector('.field[data-key="motif"]').dataset.type, 'Path');
  for (const [key, min, max] of [
    ['pitch', '1', '1024'],
    ['minScale', '0', '100'],
    ['maxScale', '0', '100'],
    ['minRotation', '-360', '360'],
    ['maxRotation', '-360', '360'],
  ]) {
    const input = root.querySelector(`.field[data-key="${key}"] input`);
    assert.equal(input.min, min);
    assert.equal(input.max, max);
  }
  assert.match(root.querySelector('.font-note').textContent, /unpainted Path/);
  assert.match(root.querySelector('.font-note').textContent, /Bypass passes the motif/);
  win.happyDOM.abort();
});
