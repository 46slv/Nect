import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { featherFixture } from './fixtures/mask-feather.js';

test('Mask feather parameter UI', async (t) => {
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
    '\nwindow.maskUI=()=>({past:history.past.length,future:history.future.length});window.maskRender=()=>render();';
  source += '\nwhile(!bundledFont) await new Promise(r=>setTimeout(r,10));';
  source += '\n//# sourceURL=main-mask-feather-test.js';

  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s),
    ui = () => win.maskUI(),
    state = () => ({
      doc: win.nect.document,
      svg: win.nect.export(),
      file: win.nect.fileState,
      command: win.nect.commandState,
      past: ui().past,
      future: ui().future,
    }),
    load = (feather = 0, mutate) => {
      const f = featherFixture(feather);
      mutate?.(f);
      win.nect.load(serialize(f.doc));
      const header = $('.graph .node[data-id="example-mask"] .node-head strong');
      header.dispatchEvent(
        new win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 91 }),
      );
      document.dispatchEvent(
        new win.PointerEvent('pointerup', { bubbles: true, button: 0, pointerId: 91 }),
      );
      return f;
    },
    input = () => $('.inspector-body [data-node="example-mask"][data-key="feather"] [data-value]'),
    change = (value, selector) => {
      const el = selector || input();
      el.value = value;
      el.dispatchEvent(new win.Event('change', { bubbles: true }));
      win.maskRender();
    };
  await t.test(
    'legacy zero appears in both panels with explicit units and reads leave old JSON/dirty/history untouched',
    () => {
      const f = load(),
        before = state();
      assert.equal(input().value, '0');
      assert.equal(input().min, '0');
      assert.equal(input().max, '256');
      assert.match(
        $('.inspector-body [data-node="example-mask"][data-key="feather"] label').textContent,
        /Feather.*document units/,
      );
      assert.ok($('.graph [data-node="example-mask"][data-key="feather"] [data-value]'));
      assert.deepEqual(before.doc, f.doc);
      win.maskRender();
      assert.deepEqual(state(), before);
      change('0');
      assert.deepEqual(state(), before);
      assert.equal(
        Object.hasOwn(
          win.nect.document.layers[0].nodes.find((n) => n.id === 'example-mask').params,
          'feather',
        ),
        false,
      );
    },
  );
  await t.test(
    'finite positive edit is one Undo, keeps image/geometry and roundtrips zero/Redo without filters at zero',
    () => {
      load();
      const before = state();
      change('4');
      const after = state();
      assert.equal(after.past, before.past + 1);
      assert.equal(after.doc.schemaVersion, 50);
      assert.match(after.svg, /stdDeviation="4"/);
      assert.deepEqual(after.doc.imageAssets, before.doc.imageAssets);
      $('[data-action="undo"]').click();
      assert.deepEqual(win.nect.document, before.doc);
      assert.equal(win.nect.export(), before.svg);
      assert.doesNotMatch(win.nect.export(), /<filter/);
      $('[data-action="redo"]').click();
      assert.deepEqual(win.nect.document, after.doc);
      assert.equal(win.nect.export(), after.svg);
      const noop = state();
      change('4');
      assert.deepEqual(state(), noop);
    },
  );
  await t.test(
    'negative/NaN/huge inputs reject atomically; parameter locks disable the same field',
    () => {
      for (const value of ['-1', 'NaN', '257', '1e100']) {
        load();
        const before = state();
        change(value);
        assert.deepEqual(state(), before);
      }
      load(4, (f) => {
        f.mask.params.feather.locked = true;
      });
      assert.equal(input().disabled, true);
      load(0, (f) => {
        f.layer.locked = true;
      });
      const locked = state();
      change('4');
      assert.deepEqual(state(), locked);
    },
  );
  await t.test(
    'common Scalar base/expression path retains expression and validates resolved sigma',
    () => {
      load(4, (f) => {
        f.mask.params.feather.expression = 'value * 2';
      });
      const base = $('.inspector-body [data-node="example-mask"][data-key="feather"] [data-base]');
      assert.ok(base);
      change('6', base);
      const q = win.nect.document.layers[0].nodes.find((n) => n.id === 'example-mask').params
        .feather;
      assert.equal(q.value, 6);
      assert.equal(q.expression, 'value * 2');
      assert.match(win.nect.export(), /stdDeviation="12"/);
      const before = state();
      change(
        '200',
        $('.inspector-body [data-node="example-mask"][data-key="feather"] [data-base]'),
      );
      assert.deepEqual(state(), before);
    },
  );
});
