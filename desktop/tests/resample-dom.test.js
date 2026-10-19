import test from 'node:test';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
// Load geometry before a synthetic browser global exists: geometry remains headless.
import '../src/core.js';
test('Resample Path mode exposes bounded draggable controls with undo', async () => {
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
      let b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
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
  await new Promise((r) => setTimeout(r, 100));
  const tick = () => new Promise((r) => setTimeout(r, 10));
  const change = async (el, value) => {
    el.value = value;
    el.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
  };
  const add = async (type) => {
    await change(document.querySelector('#add-node'), type);
    return win.nect.document.layers[0].nodes.at(-1).id;
  };
  const resample = await add('Resample Path');
  const count = () =>
    document.querySelector(`.graph .field[data-node="${resample}"][data-key="count"] [data-value]`);
  const mode = () => document.querySelector(`.graph [data-operation="${resample}"]`);
  const spacing = () =>
    document.querySelector(
      `.graph .field[data-node="${resample}"][data-key="spacing"] [data-value]`,
    );
  const read = () => win.nect.document.layers[0].nodes.find((n) => n.id === resample);
  assert.ok(count().hasAttribute('data-scrub-step'));
  assert.equal(count().min, '2');
  assert.equal(count().max, '1024');
  await change(count(), '48');
  assert.equal(read().params.count.value, 48);
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(read().params.count.value, 32);
  assert.equal(mode().value, 'count');
  await change(mode(), 'spacing');
  assert.equal(read().operation, 'spacing');
  assert.equal(win.nect.document.schemaVersion, 12);
  assert.equal(count(), null);
  assert.ok(spacing().hasAttribute('data-scrub-step'));
  assert.equal(spacing().min, '0.0001');
  assert.equal(spacing().max, '100000');
  await change(spacing(), '18.5');
  assert.equal(read().params.spacing.value, 18.5);
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(read().params.spacing.value, 24);
  document.querySelector('[data-action="undo"]').click();
  await tick();
  assert.equal(read().operation, 'count');
  assert.ok(count());
  assert.equal(spacing(), null);
  assert.match(document.querySelector('.inspector-body').textContent, /maximum arc-length gap/);
  win.happyDOM.abort();
});
