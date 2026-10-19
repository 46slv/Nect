import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import '../src/core.js';

test('Boolean UI exposes a legacy virtual C socket and connects it through the shared command', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  const frames = [];
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
    globalThis[key] =
      key === 'requestAnimationFrame' ? (callback) => frames.push(callback) : win[key];
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
  source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
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
    sourceB = await add('Rectangle'),
    sourceC = await add('Rectangle'),
    id = await add('Boolean');
  const node = () => win.nect.document.layers[0].nodes.find((item) => item.id === id);
  const field = (key) =>
    document.querySelector(`.graph .field[data-node="${id}"][data-key="${key}"]`);
  const keys = () =>
    [...document.querySelectorAll(`.graph .field[data-node="${id}"][data-key]`)].map(
      (f) => f.dataset.key,
    );
  const flushFrames = () => {
    const pending = frames.splice(0);
    for (const callback of pending) callback(0);
  };
  const wire = async (sourceId, key) => {
    document
      .querySelector(`.graph [data-source="${sourceId}"]`)
      .dispatchEvent(new win.PointerEvent('pointerdown', { bubbles: true, button: 0 }));
    field(key).dispatchEvent(new win.PointerEvent('pointerup', { bubbles: true, button: 0 }));
    await tick();
    flushFrames();
    assert.equal(node().params[key].source, sourceId);
  };
  const history = async (action) => {
    document.querySelector(`[data-action="${action}"]`).click();
    await tick();
    flushFrames();
  };

  assert.deepEqual(keys(), ['a', 'b']);
  const legacyVersion = win.nect.document.schemaVersion;
  await wire(sourceA, 'a');
  await wire(sourceB, 'b');
  assert.equal(win.nect.document.schemaVersion, legacyVersion);
  assert.equal(node().booleanInputOrder, undefined);
  assert.ok(field('input_pending'));
  assert.equal(field('input_pending').querySelector('label').textContent, 'Add input');

  await wire(sourceC, 'input_pending');
  const order = [...node().booleanInputOrder],
    bytes = JSON.stringify(win.nect.document);
  assert.deepEqual(order.slice(0, 3), ['a', 'b', 'input_pending']);
  assert.equal(order.length, 4);
  assert.equal(win.nect.document.schemaVersion, 40);
  assert.equal(field(order[3]).querySelector('label').textContent, 'Add input');

  await history('undo');
  assert.equal(node().booleanInputOrder, undefined);
  assert.equal(win.nect.document.schemaVersion, legacyVersion);
  await history('redo');
  assert.deepEqual(node().booleanInputOrder, order);
  assert.equal(JSON.stringify(win.nect.document), bytes);
});
