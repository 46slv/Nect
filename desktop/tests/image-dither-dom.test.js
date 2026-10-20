import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize } from '../src/core.js';
import { imageDitherTypes } from '../src/dither-controls.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';

test('web editor offers fixed Image nodes, edits mark settings, wires Image Output and restores history', async () => {
  // DOM-only web integration: no browser/native process or screen is launched.
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
  try {
    for (let i = 0; i < 100 && !win.nect; i++) await tick();
    assert.ok(win.nect);
    const query = (selector) => win.document.querySelector(selector);
    for (const type of imageDitherTypes) {
      const doc = newDocument(),
        layer = doc.layers[0],
        image = node('Image'),
        vector = node('Rectangle'),
        output = node('Image Output');
      doc.schemaVersion = 35;
      const rgba = new Uint8Array(8 * 8 * 4);
      for (let i = 3; i < rgba.length; i += 4) rgba[i] = 255;
      doc.imageAssets = {
        'image-web': {
          name: 'black.png',
          mime: 'image/png',
          width: 8,
          height: 8,
          data: encodeImage(encodeRgbaPng(8, 8, rgba)),
        },
      };
      image.asset = 'image-web';
      image.params.position.value = [0, 0];
      image.params.size.value = [8, 8];
      output.params.path.source = image.id;
      layer.nodes = [image, vector, output];
      layer.output = output.id;
      const assetsBefore = JSON.stringify(doc.imageAssets);
      win.nect.load(serialize(doc));
      await tick();
      const picker = query('#add-node');
      assert.ok(picker.querySelector(`option[value="${type}"]`));
      picker.value = type;
      picker.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
      const effect = win.nect.document.layers[0].nodes.find((n) => n.type === type);
      assert.ok(effect);
      assert.equal(win.nect.document.schemaVersion, 41);
      const field = query(`.node[data-id="${effect.id}"] .field[data-key="path"]`);
      assert.equal(field.dataset.type, 'Image');
      assert.equal(field.querySelector('label').textContent, 'Image');
      assert.ok(field.querySelector(`option[value="${image.id}"]`));
      assert.equal(field.querySelector(`option[value="${vector.id}"]`), null);
      const select = field.querySelector('[data-path-source]');
      select.value = image.id;
      select.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
      query(`[data-preview="${effect.id}"]`).click();
      await tick();
      assert.equal(win.nect.evaluate().path.kind, 'image');
      assert.match(win.nect.export(), /<image /);
      assert.notEqual(win.nect.evaluate().path.imageEffects, undefined);
      const shape = query(`[data-operation="${effect.id}"]`);
      shape.value = 'star';
      shape.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
      const changed = win.nect.document.layers[0].nodes.find((n) => n.id === effect.id);
      assert.equal(changed.operation, 'star');
      const size = query(
        `.node[data-id="${effect.id}"] .field[data-key="dotSize"] input[type="number"]`,
      );
      assert.equal(size.max, '1024');
      size.value = '4';
      size.dispatchEvent(new win.Event('change', { bubbles: true }));
      await tick();
      assert.equal(
        win.nect.document.layers[0].nodes.find((n) => n.id === effect.id).params.dotSize.value,
        4,
      );
      query('[data-action="undo"]').click();
      await tick();
      assert.equal(
        win.nect.document.layers[0].nodes.find((n) => n.id === effect.id).params.dotSize.value,
        16,
      );
      query('[data-action="redo"]').click();
      await tick();
      assert.equal(
        win.nect.document.layers[0].nodes.find((n) => n.id === effect.id).params.dotSize.value,
        4,
      );
      assert.equal(JSON.stringify(win.nect.document.imageAssets), assetsBefore);
      assert.equal(query(`.node[data-id="${effect.id}"] .type-tag`).textContent, 'Image');
    }
  } finally {
    await win.happyDOM.close();
  }
});
