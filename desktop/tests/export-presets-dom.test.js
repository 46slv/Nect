import assert from 'node:assert/strict';
import test from 'node:test';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import {
  EXPORT_PRESETS_STORAGE_KEY,
  PNG_EXPORT_SCALES,
  loadExportPresets,
} from '../src/export-presets.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('toolbar presets save, reload, and apply only to their own export controls', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
  globalThis.MutationObserver = win.MutationObserver;
  Object.defineProperty(globalThis, 'localStorage', {
    configurable: true,
    value: win.localStorage,
  });
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
  await new Promise((resolve) => setTimeout(resolve, 50));

  try {
    const doc = win.document;
    const setSelect = (selector, value) => {
      const el = doc.querySelector(selector);
      el.value = value;
      el.dispatchEvent(new win.Event('change', { bubbles: true }));
    };
    const setName = (selector, value) => {
      const el = doc.querySelector(selector);
      el.value = value;
      el.dispatchEvent(new win.Event('input', { bubbles: true }));
    };
    const original = JSON.stringify(win.nect.document);

    assert.equal(doc.querySelector('#svg-export-sizing').value, 'fixed');
    assert.equal(doc.querySelector('#svg-export-unit').value, 'px');
    assert.equal(doc.querySelector('#svg-export-precision').value, 'default');
    assert.equal(doc.querySelector('#png-export-mode').value, 'scale');
    assert.equal(doc.querySelector('#png-scale').value, '1');
    assert.deepEqual(
      [...doc.querySelector('#png-scale').options].map((option) => Number(option.value)),
      PNG_EXPORT_SCALES,
    );
    assert.equal(doc.querySelector('#svg-export-preset').value, '');
    assert.equal(doc.querySelector('[data-action="undo"]').disabled, true);
    doc.querySelector('#export-presets > summary').click();
    doc.querySelector('[data-action="save-svg-export-preset"]').click();
    await Promise.resolve();
    assert.match(doc.querySelector('.status .message').textContent, /Preset name/);
    assert.deepEqual(loadExportPresets(win.localStorage), []);

    setSelect('#svg-export-sizing', 'responsive');
    setSelect('#svg-export-unit', 'mm');
    setSelect('#svg-export-precision', '3');
    setName('#svg-export-preset-name', 'Responsive mm');
    doc.querySelector('[data-action="save-svg-export-preset"]').click();
    await Promise.resolve();

    assert.equal(doc.querySelector('#svg-export-preset').value, 'Responsive mm');
    assert.equal(
      doc.querySelector('[data-action="save-svg-export-preset"]').textContent,
      'Update SVG preset',
    );
    assert.deepEqual(loadExportPresets(win.localStorage)[0], {
      format: 'svg',
      name: 'Responsive mm',
      settings: { sizing: 'responsive', unit: 'mm', precision: '3' },
    });

    setSelect('#svg-export-sizing', 'fixed');
    setSelect('#svg-export-unit', 'px');
    setSelect('#svg-export-precision', 'default');
    setSelect('#svg-export-preset', 'Responsive mm');
    assert.equal(doc.querySelector('#svg-export-sizing').value, 'responsive');
    assert.equal(doc.querySelector('#svg-export-unit').value, 'mm');
    assert.equal(doc.querySelector('#svg-export-precision').value, '3');
    assert.equal(doc.querySelector('#png-export-mode').value, 'scale');
    assert.equal(doc.querySelector('#png-scale').value, '1');

    setSelect('#png-export-mode', 'ppi');
    const ppi = doc.querySelector('#png-ppi');
    ppi.value = '300';
    ppi.dispatchEvent(new win.Event('input', { bubbles: true }));
    setName('#png-export-preset-name', 'Print 300');
    doc.querySelector('[data-action="save-png-export-preset"]').click();
    await Promise.resolve();
    assert.equal(doc.querySelector('#png-export-preset').value, 'Print 300');

    setSelect('#png-export-mode', 'scale');
    setSelect('#png-scale', '2');
    setSelect('#png-export-preset', 'Print 300');
    assert.equal(doc.querySelector('#png-export-mode').value, 'ppi');
    assert.equal(doc.querySelector('#png-ppi').value, '300');
    assert.equal(doc.querySelector('#svg-export-sizing').value, 'responsive');
    assert.equal(JSON.stringify(win.nect.document), original);
    assert.equal(doc.querySelector('[data-action="undo"]').disabled, true);
    assert.equal(loadExportPresets(win.localStorage).length, 2);
    assert.ok(win.localStorage.getItem(EXPORT_PRESETS_STORAGE_KEY));
  } finally {
    await win.happyDOM.close();
  }
});
