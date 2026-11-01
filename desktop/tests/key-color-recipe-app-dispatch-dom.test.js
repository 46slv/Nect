import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { serialize } from '../src/core.js';
import { serializeCompositionProject } from '../src/composition-project-model.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';

test('real main.js routes the experimental editor through closed Apply, not a layer edit', async (t) => {
  const keys = [
    'window',
    'document',
    'HTMLElement',
    'MutationObserver',
    'requestAnimationFrame',
    'DOMPoint',
    'fetch',
  ];
  const original = new Map(
    keys.map((key) => [key, Object.getOwnPropertyDescriptor(globalThis, key)]),
  );
  t.after(() => {
    for (const [key, descriptor] of original)
      if (descriptor) Object.defineProperty(globalThis, key, descriptor);
      else delete globalThis[key];
  });
  const main = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  async function boot(source, label) {
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
      matrixTransform(m) {
        return { x: m.a * this.x + m.c * this.y + m.e, y: m.b * this.x + m.d * this.y + m.f };
      }
    };
    win.SVGElement.prototype.getScreenCTM = () => ({
      inverse: () => ({ a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 }),
    });
    globalThis.fetch = async () => {
      throw Error('This color-only harness does not read font bytes');
    };
    // Disable only unrelated startup font loading. App, panel, Session and
    // commandChange stay production code; no font bytes or user files are read.
    const start = source.indexOf("fetch('/fonts/DejaVuSans.ttf')");
    const end = source.indexOf('// Refresh actual disabled state', start);
    assert.ok(start >= 0 && end > start);
    source = source.slice(0, start) + source.slice(end);
    source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
    source +=
      '\nwindow.keyColorAppTest = () => ({ past: history.past.length, future: history.future.length, message, error });';
    source += `\n//# sourceURL=main-key-color-${label}.js`;
    await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    const query = (selector) => win.document.querySelector(selector);
    const fire = (el, type, options = {}) =>
      el.dispatchEvent(
        new (type === 'pointerdown'
          ? win.PointerEvent
          : type === 'keydown'
            ? win.KeyboardEvent
            : win.Event)(type, { bubbles: true, cancelable: true, ...options }),
      );
    const click = (selector) => {
      assert.ok(query(selector), `Missing ${selector}`);
      query(selector).click();
    };
    const input = (kind, value) => {
      const el = query(`[data-key-color-input="${kind}"]`);
      el.value = String(value);
      fire(el, 'input');
    };
    return {
      win,
      query,
      fire,
      click,
      input,
      ui: () => win.keyColorAppTest(),
      snapshot: () => win.nect.projectSnapshot(),
      load: (doc = retainedRecipeFixture()) => win.nect.load(serialize(doc)),
      open: () => {
        openDOMPanel(win.document, 'colors');
        click('[data-key-color-recipe-open]');
      },
    };
  }

  await t.test(
    'empty state and real Apply/Undo/Redo/native reopen preserve legacy data/IDs/refs',
    async () => {
      const s = await boot(main, 'closed-apply');
      try {
        const blank = s.snapshot();
        s.open();
        assert.ok(s.query('[data-key-color-empty]'));
        assert.deepEqual(s.snapshot(), blank);
        assert.equal(s.ui().past, 0);
        s.click('[data-key-color-cancel]');
        s.load();
        const before = s.snapshot();
        s.open();
        s.input('hex', '#DeAd09');
        s.input('alpha', 0);
        s.input('deltaL', -0.125);
        s.input('deltaRatioCmax', 0.375);
        s.click('[data-key-color-preview]');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, 0);
        s.click('[data-key-color-apply]');
        const after = s.snapshot();
        assert.equal(s.query('dialog'), null);
        assert.equal(s.ui().error, false);
        assert.match(s.ui().message, /Apply accepted/);
        assert.equal(s.ui().past, 1);
        assert.equal(s.ui().future, 0);
        assert.deepEqual(after.colorSlots.find((slot) => slot.id === 'key').value, {
          hex: '#DeAd09',
          alpha: 0,
        });
        assert.deepEqual(after.colorDesign, before.colorDesign);
        assert.deepEqual(after.compositions, before.compositions);
        assert.deepEqual(after.colorSlots.slice(2), before.colorSlots.slice(2));
        assert.deepEqual(after.keyColorRecipe.committedSnapshot.intent.slots[0], {
          id: 'derived',
          role: 'Background',
          deltaL: -0.125,
          deltaRatioCmax: 0.375,
        });
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), before);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), after);
        s.win.nect.load(serializeCompositionProject(after));
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.ui().past, 0);
        s.open();
        assert.equal(s.query('[data-key-color-input="hex"]').value, '#DeAd09');
        s.fire(s.query('[data-key-color-input]'), 'keydown', { key: 'Escape' });
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.ui().past, 0);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'launcher cancels an actual legacy live gesture without blur auto-commit',
    async () => {
      const s = await boot(main, 'legacy-isolation');
      try {
        s.load();
        openDOMPanel(s.win.document, 'colors');
        const mode = s.query('[data-wheel-chroma-mode]');
        mode.value = 'override';
        s.fire(mode, 'change');
        const before = s.snapshot(),
          past = s.ui().past;
        const input = s.query('[data-wheel-chroma]');
        input.focus();
        input.value = '0.03';
        s.fire(input, 'input');
        await new Promise((resolve) => setTimeout(resolve, 40));
        assert.ok(s.win.nect.colorPreviewSnapshot(), 'legacy preview is genuinely pending');
        assert.deepEqual(s.snapshot(), before);
        const launcher = s.query('[data-key-color-recipe-open]');
        assert.equal(
          s.fire(launcher, 'pointerdown', { button: 0, pointerId: 19, isPrimary: true }),
          false,
        );
        assert.equal(s.win.nect.colorPreviewSnapshot(), null);
        // An ensuing native change cannot re-author the canceled draft: launcher
        // pointerdown prevents that native blur, then click opens the separate modal.
        s.click('[data-key-color-recipe-open]');
        assert.ok(s.query('dialog'));
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.snapshot(), before);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'real external edit and Undo invalidate, never refresh, the open draft envelope',
    async () => {
      const s = await boot(main, 'stale');
      try {
        s.load();
        s.open();
        s.click('[data-key-color-preview]');
        s.win.nect.dispatch({
          ...s.win.nect.commandState,
          type: 'set-color-slot',
          payload: { slotId: 'other', value: { hex: '#010203', alpha: 1 } },
        });
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /stale/);
        s.win.nect.dispatch({ ...s.win.nect.commandState, type: 'undo', payload: {} });
        assert.equal(s.query('[data-key-color-preview]').disabled, true);
        const before = s.snapshot(),
          past = s.ui().past,
          future = s.ui().future;
        s.fire(s.query('[data-key-color-apply]'), 'click');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        assert.equal(s.ui().future, future);
        s.click('[data-key-color-cancel]');
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'negative control: old main layerId injection yields null, never a success notice',
    async () => {
      const start = main.indexOf('function commandChange('),
        end = main.indexOf('\nfunction settleNumericControl(', start);
      const commandChange = main.slice(start, end),
        dispatchCall = /result = session\.dispatch\(\{[\s\S]*?\n    \}\);/g;
      assert.equal([...commandChange.matchAll(dispatchCall)].length, 1);
      const broken =
        main.slice(0, start) +
        commandChange.replace(
          dispatchCall,
          'result = session.dispatch({ ...envelope, type, payload: { layerId: current().id, ...payload } });',
        ) +
        main.slice(end);
      const s = await boot(broken, 'old-injection-negative');
      try {
        s.load();
        const before = s.snapshot();
        s.open();
        s.input('hex', '#DeAd09');
        s.click('[data-key-color-preview]');
        s.click('[data-key-color-apply]');
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, 0);
        assert.equal(s.ui().future, 0);
        assert.ok(s.query('dialog'));
        assert.equal(s.query('[data-key-color-apply]').disabled, true);
        assert.match(s.query('[data-key-color-status]').textContent, /not accepted/);
        assert.equal(s.ui().error, true);
        assert.doesNotMatch(s.ui().message, /Apply accepted/);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );
});
