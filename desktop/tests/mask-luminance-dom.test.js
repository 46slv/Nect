import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize, parameter } from '../src/core.js';
import { luminanceFixture } from './fixtures/mask-luminance.js';
import { featherFixture } from './fixtures/mask-feather.js';
test('Mask Image mode UI', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const k of ['window', 'document', 'HTMLElement', 'MutationObserver']) globalThis[k] = win[k];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform(m) {
      return { x: m.a * this.x + m.c * this.y + m.e, y: m.b * this.x + m.d * this.y + m.f };
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => ({
    inverse: () => ({ a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }),
  });
  win.SVGSVGElement.prototype.getScreenCTM = win.SVGElement.prototype.getScreenCTM;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.lumHistory=()=>({past:history.past.length,future:history.future.length});window.lumRender=()=>render();while(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    mode = () => $('.inspector-body [data-node="example-mask"][data-key="luminance"] select'),
    state = () => ({
      doc: win.nect.document,
      svg: win.nect.export(),
      file: win.nect.fileState,
      command: win.nect.commandState,
      ...win.lumHistory(),
    }),
    change = (value) => {
      mode().value = value;
      mode().dispatchEvent(new win.Event('change', { bubbles: true }));
      win.lumRender();
    },
    load = (f) => {
      win.nect.load(serialize(f.doc));
      const head = $('.graph .node[data-id="example-mask"] .node-head strong');
      head.dispatchEvent(
        new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 91 }),
      );
      document.dispatchEvent(
        new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 91 }),
      );
      win.lumRender();
    };
  await t.test(
    'implicit Alpha is readable in both panels with no authored/dirty/history change',
    () => {
      load(luminanceFixture());
      const before = state();
      assert.equal(mode().value, 'alpha');
      assert.equal(mode().disabled, false);
      assert.ok($('.graph [data-node="example-mask"][data-key="luminance"] select'));
      win.lumRender();
      change('alpha');
      assert.deepEqual(state(), before);
      assert.equal(
        Object.hasOwn(
          win.nect.document.layers[0].nodes.find((n) => n.id === 'example-mask').params,
          'luminance',
        ),
        false,
      );
    },
  );
  await t.test(
    'Luminance commits once; Undo/Redo and repeated mode values retain the document contract',
    () => {
      load(luminanceFixture());
      const before = state();
      change('luminance');
      const after = state();
      assert.equal(after.doc.schemaVersion, 52);
      assert.equal(after.past, before.past + 1);
      assert.match(after.svg, /feColorMatrix/);
      change('luminance');
      assert.deepEqual(state(), after);
      $('[data-action="undo"]').click();
      win.lumRender();
      assert.deepEqual(win.nect.document, before.doc);
      assert.equal(win.nect.export(), before.svg);
      $('[data-action="redo"]').click();
      win.lumRender();
      assert.deepEqual(win.nect.document, after.doc);
      assert.equal(mode().value, 'luminance');
      change('alpha');
      assert.equal(win.nect.export(), before.svg);
    },
  );
  await t.test('invalid mode and locked parameter/composition edits are document-neutral', () => {
    load(luminanceFixture());
    const before = state();
    change('invalid');
    assert.deepEqual(state(), before);
    for (const lock of ['parameter', 'composition']) {
      const f = luminanceFixture();
      if (lock === 'parameter') {
        f.mask.params.luminance = { ...parameter(f.mask, 'luminance'), locked: true };
        f.doc.schemaVersion = 52;
      } else f.layer.locked = true;
      load(f);
      assert.equal(mode().disabled, true);
      const saved = state();
      change('luminance');
      assert.deepEqual(state(), saved);
    }
  });
  await t.test(
    'the Image-only selector is disabled for a closed Path and reading does not change its SVG',
    () => {
      load(featherFixture(4));
      const before = state();
      assert.equal(mode().disabled, true);
      win.lumRender();
      assert.deepEqual(state(), before);
    },
  );
});
