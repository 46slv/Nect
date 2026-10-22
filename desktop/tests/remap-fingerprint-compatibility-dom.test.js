import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('synthetic Inspector and canvas drag agree on both saved fingerprint eras', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
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
      const data = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
  source += '\nwhile (!bundledFont) await new Promise(r => setTimeout(r, 10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const fire = (element, type, x = 0, y = 0) =>
    element.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: 7,
        button: 0,
        buttons: 1,
        clientX: x,
        clientY: y,
      }),
    );
  for (const era of ['historical', 'current']) {
    win.nect.load(
      await fs.readFile(new URL(`./fixtures/remap-saved-${era}.json`, import.meta.url), 'utf8'),
    );
    const remap = win.nect.document.layers[0].nodes.find((n) => n.id === 'saved-remap'),
      fingerprint = remap.edits[0].fingerprint,
      id = remap.edits[0].pointId;
    fire(
      document.querySelector('.graph .node[data-id="saved-remap"] .node-head .type-tag'),
      'pointerdown',
    );
    fire(document, 'pointerup');
    assert.equal(document.querySelector('[data-rebind-remap]'), null, era);
    const hit = document.querySelector(
      `.canvas-stage .point-hit[data-point="${id}"]:not([data-handle])`,
    );
    assert.ok(hit, era);
    const x = Number(hit.getAttribute('cx')),
      y = Number(hit.getAttribute('cy'));
    const before = serialize(win.nect.document);
    fire(hit, 'pointerdown', x, y);
    fire(document, 'pointermove', x + 6, y - 2);
    fire(document, 'pointerup', x + 6, y - 2);
    const edited = win.nect.document.layers[0].nodes.find((n) => n.id === 'saved-remap');
    assert.notEqual(serialize(win.nect.document), before, era);
    assert.equal(edited.edits[0].fingerprint, fingerprint, era);
    assert.notEqual(win.nect.evaluate().path.remapSuspended, true, era);
    assert.equal(document.querySelector('[data-rebind-remap]'), null, era);
  }
});
