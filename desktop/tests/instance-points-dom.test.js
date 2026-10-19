import test from 'node:test';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import '../src/core.js';

test('Instance on Points exposes two required typed inputs and an optional tangent inspector control', async () => {
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
  for (const file of [
    'artwork-gap-distribution',
    'artwork-distribution',
    'artwork-alignment',
    'artwork-multi-move',
    'images',
    'raster',
    'focus',
    'layout',
    'core',
    'artwork-creation',
    'artwork-move-target',
    'artwork-deletion',
    'artwork-ordering',
    'svg-import',
    'storage',
    'fonts',
  ]) {
    const moduleUrl = new URL(`../src/${file}.js`, import.meta.url);
    source = source.replaceAll(`from './${file}.js'`, `from '${moduleUrl}'`);
  }
  source = source.replace("import './style.css';", '');
  source = resolveMainRelativeImports(source);
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  await new Promise((resolve) => setTimeout(resolve, 100));

  const select = document.querySelector('#add-node');
  select.value = 'Instance on Points';
  select.dispatchEvent(new win.Event('change', { bubbles: true }));
  await new Promise((resolve) => setTimeout(resolve, 20));
  const effect = win.nect.document.layers[0].nodes.at(-1);
  assert.equal(effect.type, 'Instance on Points');
  assert.equal(
    document.querySelector(`.graph .field[data-node="${effect.id}"][data-key="path"] label`)
      .textContent,
    'Source',
  );
  assert.equal(
    document.querySelector(`.graph .field[data-node="${effect.id}"][data-key="points"] label`)
      .textContent,
    'Points',
  );
  assert.equal(
    document.querySelectorAll(`.graph .field[data-node="${effect.id}"] select[data-path-source]`)
      .length,
    2,
  );
  const alignField = document.querySelector(
      `.graph .field[data-node="${effect.id}"][data-key="alignTangent"]`,
    ),
    alignControl = alignField?.querySelector('input[type="checkbox"][data-value]');
  assert.equal(alignField?.querySelector('label')?.textContent, 'Align Tangent');
  assert.ok(alignControl, 'Align Tangent is a Boolean inspector option');
  assert.equal(alignControl.checked, false, 'the option defaults off');
  assert.match(
    document.querySelector('.inspector-body').textContent,
    /bounds center.*every authored anchor/i,
  );
  assert.match(document.querySelector('.inspector-body').textContent, /Resample Path first/i);
  assert.match(document.querySelector('.inspector-body').textContent, /outgoing handle B/i);
  assert.match(document.querySelector('.inspector-body').textContent, /not arc-length-sampled/i);
  assert.match(document.querySelector('.inspector-body').textContent, /bounded output/i);

  alignControl.checked = true;
  alignControl.dispatchEvent(new win.Event('change', { bubbles: true }));
  await new Promise((resolve) => setTimeout(resolve, 20));
  assert.equal(
    win.nect.document.layers[0].nodes.find((node) => node.id === effect.id).params.alignTangent
      .value,
    true,
    'the inspector checkbox records the setting through the common Session command',
  );
  win.happyDOM.abort();
});
