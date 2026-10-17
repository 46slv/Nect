import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
// Load geometry before a synthetic browser global exists: geometry remains headless.
import '../src/core.js';
test('Expand Stroke is searchable and explains geometry conversion', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
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
  const expand = await add('Expand Stroke');
  assert.equal(win.nect.document.layers[0].nodes.at(-1).type, 'Expand Stroke');
  const help = document.querySelector('.inspector-body').textContent;
  assert.match(help, /upstream solid Stroke/);
  assert.match(help, /cubic centerlines.*0\.1 px tolerance/i);
  assert.match(help, /256 flattened vertices/);
  assert.match(help, /normalized centerline arc length on open contours/i);
  assert.doesNotMatch(help, /straight-edge paths only/i);
  assert.ok(document.querySelector(`.graph .field[data-node="${expand}"][data-key="path"]`));
  for (const [key, label] of [
    ['startWidthMultiplier', 'Start width multiplier'],
    ['endWidthMultiplier', 'End width multiplier'],
  ]) {
    const field = document.querySelector(
        `.inspector-body .field[data-node="${expand}"][data-key="${key}"]`,
      ),
      input = field?.querySelector('input[type="number"][data-value]');
    assert.ok(field, `${key} inspector field`);
    assert.equal(field.querySelector('label').textContent, label);
    assert.equal(input.min, '0');
    assert.equal(input.max, '4');
    assert.ok(input.hasAttribute('data-scrub-step'));
    assert.equal(input.disabled, false);
  }
  const start = document.querySelector(
    `.graph .field[data-node="${expand}"][data-key="startWidthMultiplier"] [data-value]`,
  );
  await change(start, '2.5');
  assert.equal(
    win.nect.document.layers[0].nodes.find((n) => n.id === expand).params.startWidthMultiplier
      .value,
    2.5,
  );
  assert.equal(win.nect.document.schemaVersion, 16);
  win.happyDOM.abort();
});
