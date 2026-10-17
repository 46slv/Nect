import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import '../src/core.js';

test('expandable Join Geometry renders stable input sockets, spare labels, wires and undo', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  const frames = [];
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
    globalThis[key] =
      key === 'requestAnimationFrame' ? (callback) => frames.push(callback) : win[key];
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
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await new Promise((resolve) => setTimeout(resolve, 100));
  const tick = () => new Promise((resolve) => setTimeout(resolve, 10));
  const add = async (type) => {
    const select = document.querySelector('#add-node');
    select.value = type;
    select.dispatchEvent(new win.Event('change', { bubbles: true }));
    await tick();
    return win.nect.document.layers[0].nodes.at(-1).id;
  };

  const sourceA = await add('Rectangle'),
    sourceB = await add('Ellipse'),
    sourceC = await add('Rectangle'),
    id = await add('Join Geometry');
  const node = () => win.nect.document.layers[0].nodes.find((item) => item.id === id);
  const fields = () => [...document.querySelectorAll(`.graph .field[data-node="${id}"][data-key]`)];
  const field = (key) => fields().find((element) => element.dataset.key === key);
  const keys = () => fields().map((element) => element.dataset.key);
  const labels = () => fields().map((element) => element.querySelector('label').textContent);
  const endpointRows = new Map([
    ['a', 30],
    ['b', 60],
  ]);
  const originalRect = win.HTMLElement.prototype.getBoundingClientRect;
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    const row = this.matches('.input') ? this.closest('.field') : null;
    if (row?.dataset.node === id && endpointRows.has(row.dataset.key)) {
      const y = endpointRows.get(row.dataset.key);
      return { x: 500, y, left: 500, top: y, right: 500, bottom: y, width: 0, height: 0 };
    }
    return originalRect.call(this);
  };
  const flushFrames = () => {
    const pending = frames.splice(0);
    for (const callback of pending) callback(0);
  };
  const wire = async (source, key) => {
    document
      .querySelector(`.graph [data-source="${source}"]`)
      .dispatchEvent(new win.PointerEvent('pointerdown', { bubbles: true, button: 0 }));
    field(key).dispatchEvent(new win.PointerEvent('pointerup', { bubbles: true, button: 0 }));
    await tick();
    flushFrames();
    assert.equal(node().params[key].source, source);
  };
  const history = async (action) => {
    document.querySelector(`[data-action="${action}"]`).click();
    await tick();
    flushFrames();
  };
  const hasEndpoint = (key) =>
    [...document.querySelectorAll('.wires .wire')].some((element) =>
      element.getAttribute('d').endsWith(` 500,${endpointRows.get(key)}`),
    );

  assert.equal(node().type, 'Join Geometry');
  assert.equal(node().name, 'Join Paths');
  assert.equal(document.querySelector(`[data-rename-node="${id}"]`).textContent, 'Join Paths');
  assert.equal(document.querySelector('.inspector h3').textContent, 'Join Paths');
  const joinOption = [...document.querySelector('#add-node').options].find(
    (option) => option.value === 'Join Geometry',
  );
  assert.equal(joinOption.textContent, 'Join Paths');
  for (const query of ['Join Paths', 'Join Geometry']) {
    document
      .querySelector('#add-node')
      .dispatchEvent(new win.PointerEvent('pointerdown', { bubbles: true, button: 0 }));
    const search = document.querySelector('.search-panel input[type="search"]');
    assert.ok(search);
    search.value = query;
    search.dispatchEvent(new win.Event('input', { bubbles: true }));
    const matches = [...document.querySelectorAll('.search-panel .picker-option')];
    assert.equal(matches.length, 1, `${query} resolves to exactly one node type`);
    assert.equal(matches[0].dataset.id, 'Join Geometry');
    assert.equal(matches[0].querySelector('span').textContent, 'Join Paths');
    document.querySelector('.search-panel .picker-cancel').click();
  }

  assert.deepEqual(keys(), ['a', 'b']);
  assert.deepEqual(labels(), ['Path 1', 'Add path']);
  await wire(sourceA, 'a');
  assert.deepEqual(keys(), ['a', 'b'], 'a non-final connection does not allocate a spare');
  await wire(sourceB, 'b');
  const third = node().joinInputOrder[2];
  assert.match(third, /^input_slot-/);
  endpointRows.set(third, 90);
  assert.deepEqual(keys(), ['a', 'b', third]);
  assert.deepEqual(labels(), ['Path 1', 'Path 2', 'Add path']);
  assert.ok(hasEndpoint('b'), 'the original b wire still targets its original socket');

  await wire(sourceC, third);
  const fourth = node().joinInputOrder[3];
  assert.notEqual(fourth, third);
  assert.deepEqual(keys(), ['a', 'b', third, fourth]);
  assert.deepEqual(labels(), ['Path 1', 'Path 2', 'Path 3', 'Add path']);
  assert.ok(hasEndpoint(third), 'a generated-key wire reaches its rendered socket');
  assert.ok(hasEndpoint('b'));
  assert.equal(field(fourth).classList.contains('connected'), false);

  await history('undo');
  assert.deepEqual(keys(), ['a', 'b', third]);
  assert.deepEqual(labels(), ['Path 1', 'Path 2', 'Add path']);
  assert.equal(node().params[third].source, undefined);
  assert.equal(
    node().params[fourth],
    undefined,
    'undo removes the newly allocated spare atomically',
  );
  assert.ok(hasEndpoint('b'));
  assert.equal(hasEndpoint(third), false);
  await history('redo');
  assert.deepEqual(keys(), ['a', 'b', third, fourth], 'redo restores the same stable keys');
  assert.ok(hasEndpoint(third));

  field('a')
    .querySelector('.input')
    .dispatchEvent(new win.MouseEvent('contextmenu', { bubbles: true }));
  await tick();
  flushFrames();
  assert.deepEqual(keys(), ['a', 'b', third, fourth]);
  assert.equal(
    node().params[third].source,
    sourceC,
    'disconnecting earlier inputs never retargets later wires',
  );
  assert.ok(hasEndpoint(third));
  await history('undo');
  assert.equal(node().params.a.source, sourceA);

  for (const savedName of ['Join Geometry', 'Foreground paths']) {
    const saved = win.nect.document;
    saved.layers[0].nodes.find((item) => item.id === id).name = savedName;
    win.nect.load(JSON.stringify(saved));
    await tick();
    flushFrames();
    assert.equal(node().type, 'Join Geometry', 'display naming never rewrites the serialized type');
    assert.equal(node().name, savedName, 'loading and rendering preserve the authored name');
    assert.equal(
      document.querySelector(`[data-rename-node="${id}"]`).textContent,
      savedName === 'Join Geometry' ? 'Join Paths' : savedName,
    );
    assert.deepEqual(keys(), ['a', 'b', third, fourth]);
    assert.ok(hasEndpoint(third));
  }
});
