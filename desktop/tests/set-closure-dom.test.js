import test from 'node:test';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import '../src/core.js';

test('Set Closure exposes its Closed checkbox and geometry-vs-Fill note in graph and inspector', async () => {
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
      "from './artwork-gap-distribution.js'",
      `from '${new URL('../src/artwork-gap-distribution.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-distribution.js'",
      `from '${new URL('../src/artwork-distribution.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-alignment.js'",
      `from '${new URL('../src/artwork-alignment.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-multi-move.js'",
      `from '${new URL('../src/artwork-multi-move.js', import.meta.url)}'`,
    )
    .replace("from './images.js'", `from '${new URL('../src/images.js', import.meta.url)}'`)
    .replace("from './raster.js'", `from '${new URL('../src/raster.js', import.meta.url)}'`)
    .replace("from './focus.js'", `from '${new URL('../src/focus.js', import.meta.url)}'`)
    .replace("from './layout.js'", `from '${new URL('../src/layout.js', import.meta.url)}'`)
    .replace("from './core.js'", `from '${new URL('../src/core.js', import.meta.url)}'`)
    .replace(
      "from './artwork-creation.js'",
      `from '${new URL('../src/artwork-creation.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-move-target.js'",
      `from '${new URL('../src/artwork-move-target.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-deletion.js'",
      `from '${new URL('../src/artwork-deletion.js', import.meta.url)}'`,
    )
    .replace(
      "from './artwork-ordering.js'",
      `from '${new URL('../src/artwork-ordering.js', import.meta.url)}'`,
    )
    .replace("from './svg-import.js'", `from '${new URL('../src/svg-import.js', import.meta.url)}'`)
    .replace("from './storage.js'", `from '${new URL('../src/storage.js', import.meta.url)}'`)
    .replace("from './fonts.js'", `from '${new URL('../src/fonts.js', import.meta.url)}'`);
  source = resolveMainRelativeImports(source);
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await new Promise((resolve) => setTimeout(resolve, 100));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 10));
  const change = async (element, checked) => {
    element.checked = checked;
    element.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
  };
  const add = async (type) => {
    const select = document.querySelector('#add-node');
    select.value = type;
    select.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
    return win.nect.document.layers[0].nodes.at(-1).id;
  };

  const id = await add('Set Closure'),
    node = () => win.nect.document.layers[0].nodes.find((item) => item.id === id),
    graphField = () =>
      document.querySelector(`.graph .field[data-node="${id}"][data-key="closed"]`),
    checkbox = (field) => field.querySelector('input[type="checkbox"]');
  assert.equal(node().params.closed.value, true);
  assert.equal(graphField().querySelector('label').textContent, 'Closed');
  assert.equal(checkbox(graphField()).checked, true);
  assert.match(
    document.querySelector(`.graph .node[data-id="${id}"] .node-body`).textContent,
    /actual closure/,
  );
  assert.match(
    document.querySelector('.inspector-body').textContent,
    /Fill can close an open path visually/,
  );
  assert.match(document.querySelector('.inspector-body').textContent, /No endpoints are welded/);

  await change(checkbox(graphField()), false);
  assert.equal(node().params.closed.value, false);
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(node().params.closed.value, true);
  win.happyDOM.abort();
});
