import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument, serialize } from '../src/core.js';

test('shipped Shared Colors sends closed harmony payloads through the app command dispatcher', async (t) => {
  const originalGlobals = new Map(
    [
      'window',
      'document',
      'HTMLElement',
      'MutationObserver',
      'requestAnimationFrame',
      'DOMPoint',
      'fetch',
    ].map((key) => [key, Object.getOwnPropertyDescriptor(globalThis, key)]),
  );
  t.after(() => {
    for (const [key, descriptor] of originalGlobals)
      if (descriptor) Object.defineProperty(globalThis, key, descriptor);
      else delete globalThis[key];
  });
  const main = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  const font = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
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
    globalThis.fetch = async () => ({
      ok: true,
      arrayBuffer: async () =>
        font.buffer.slice(font.byteOffset, font.byteOffset + font.byteLength),
    });
    source = source
      .replace("import './style.css';", '')
      .replace(
        /from '(\.\/[^']+)'/g,
        (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
      );
    source +=
      '\nwindow.harmonyAppTest = () => ({ past: history.past.length, future: history.future.length, message, error });';
    source +=
      '\nconst deadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() > deadline) throw Error("Font unavailable"); await new Promise(r => setTimeout(r, 10)); }';
    source += `\n//# sourceURL=main-harmony-app-${label}.js`;
    try {
      await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    } catch (error) {
      await win.happyDOM.abort();
      throw error;
    }
    const query = (selector) => win.document.querySelector(selector);
    const click = (selector) => {
      const control = query(selector);
      assert.ok(control, `Missing ${selector}`);
      assert.equal(control.disabled, false, `${selector} is enabled`);
      control.click();
    };
    const change = (selector, value) => {
      const control = query(selector);
      assert.ok(control, `Missing ${selector}`);
      control.value = value;
      control.dispatchEvent(new win.Event('change', { bubbles: true }));
    };
    const snapshot = () => structuredClone(win.nect.projectSnapshot());
    const ui = () => win.harmonyAppTest();
    win.nect.load(serialize(blankDocument()));
    click('[data-workspace-panel="colors"]');
    return { win, query, click, change, snapshot, ui };
  }

  let created;
  await t.test(
    'Create and both Add kinds author one undoable update each with stable slot IDs',
    async () => {
      const s = await boot(main, 'production');
      try {
        const initial = s.snapshot();
        assert.equal(initial.schemaVersion, 27);
        assert.equal(s.ui().past, 0);
        assert.equal(s.query('[data-harmony-create]').textContent, 'Create palette');
        s.click('[data-harmony-create]');
        created = s.snapshot();
        assert.equal(created.schemaVersion, 56);
        assert.equal(created.colorDesign.version, 2);
        assert.equal(created.colorSlots.length, 5);
        assert.equal(created.colorDesign.palette.length, 5);
        const firstIds = created.colorSlots.map((slot) => slot.id);
        assert.equal(new Set(firstIds).size, 5);
        assert.deepEqual(
          created.colorDesign.palette.map((entry) => entry.id),
          firstIds,
        );
        assert.equal(s.query('.status.error'), null);
        assert.doesNotMatch(s.ui().message, /Invalid harmony command payload/);
        assert.equal(s.ui().past, 1);
        assert.equal(s.ui().future, 0);
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), initial);
        assert.equal(s.ui().past, 0);
        assert.equal(s.ui().future, 1);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), created);

        for (const [index, kind] of ['neutral', 'chromatic'].entries()) {
          const before = s.snapshot();
          s.change('[data-wheel-add-kind]', kind);
          assert.deepEqual(s.snapshot(), before, 'choosing Add kind is only a local draft');
          s.click('[data-harmony-add]');
          const added = s.snapshot();
          assert.equal(added.schemaVersion, 56);
          assert.equal(added.colorDesign.version, 2);
          assert.equal(added.colorSlots.length, 6 + index);
          assert.equal(added.colorDesign.palette.length, 6 + index);
          assert.deepEqual(added.colorSlots.slice(0, -1), before.colorSlots);
          assert.deepEqual(added.colorDesign.palette.slice(0, -1), before.colorDesign.palette);
          assert.deepEqual(added.colorDesign.wheel, created.colorDesign.wheel);
          assert.equal(added.colorDesign.palette.at(-1).kind, kind);
          assert.equal(added.colorDesign.palette.at(-1).id, added.colorSlots.at(-1).id);
          assert.equal(new Set(added.colorSlots.map((slot) => slot.id)).size, 6 + index);
          assert.deepEqual(
            added.colorSlots.slice(0, 5).map((slot) => slot.id),
            firstIds,
          );
          assert.equal(s.ui().past, 2 + index);
          assert.equal(s.ui().future, 0);
          assert.equal(s.ui().error, false);
          assert.doesNotMatch(s.ui().message, /Invalid harmony command payload/);
          s.click('[data-action="undo"]');
          assert.deepEqual(s.snapshot(), before);
          s.click('[data-action="redo"]');
          assert.deepEqual(s.snapshot(), added);
          assert.equal(s.ui().past, 2 + index);
          assert.equal(s.ui().future, 0);
        }
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'production live Chroma uses detached input feedback and one history entry per edit',
    async () => {
      const s = await boot(main, 'live-chroma');
      try {
        s.click('[data-harmony-create]');
        assert.equal(s.query('[data-chroma-preview]'), null);
        s.change('[data-wheel-chroma-mode]', 'override');
        const before = s.snapshot(),
          past = s.ui().past;
        const id = before.colorDesign.palette[0].id;
        let input = s.query('[data-wheel-chroma]');
        const inspector = s.query('.inspector');
        input.focus();
        input.value = '0.03';
        input.dispatchEvent(new s.win.Event('input', { bubbles: true }));
        await new Promise((resolve) => setTimeout(resolve, 40));
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        assert.equal(s.query('[data-wheel-chroma]'), input);
        assert.equal(s.query('.inspector'), inspector);
        const projected = s.win.nect.colorPreviewSnapshot();
        assert.equal(projected.colorDesign.palette.find((entry) => entry.id === id).overrideC, 0.03);
        input.dispatchEvent(
          new s.win.KeyboardEvent('keydown', {
            key: 'Escape',
            bubbles: true,
            cancelable: true,
          }),
        );
        assert.equal(s.win.nect.colorPreviewSnapshot(), null);
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        assert.ok(s.query('.shared-colors-panel'));
        input = s.query('[data-wheel-chroma]');
        const originalValue = input.defaultValue;
        input.value = '0.03';
        input.dispatchEvent(new s.win.Event('input', { bubbles: true }));
        input.value = originalValue;
        input.dispatchEvent(new s.win.Event('input', { bubbles: true }));
        input.dispatchEvent(new s.win.Event('change', { bubbles: true }));
        assert.equal(s.win.nect.colorPreviewSnapshot(), null);
        assert.deepEqual(s.snapshot(), before);
        assert.equal(s.ui().past, past);
        input = s.query('[data-wheel-chroma]');
        input.value = '0.03';
        input.dispatchEvent(new s.win.Event('input', { bubbles: true }));
        input.dispatchEvent(new s.win.Event('change', { bubbles: true }));
        const after = s.snapshot();
        assert.equal(after.schemaVersion, 57);
        assert.equal(after.colorDesign.palette.find((entry) => entry.id === id).overrideC, 0.03);
        assert.equal(s.ui().past, past + 1);
        assert.equal(s.ui().error, false);
        s.click('[data-action="undo"]');
        assert.deepEqual(s.snapshot(), before);
        s.click('[data-action="redo"]');
        assert.deepEqual(s.snapshot(), after);
        s.win.nect.load(JSON.stringify(after));
        assert.deepEqual(s.snapshot(), after);
        assert.equal(s.query('[data-wheel-chroma-mode]').value, 'override');
        assert.equal(Number(s.query('[data-wheel-chroma]').value), 0.03);
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );

  await t.test(
    'negative control: original layerId injection rejects Create and Add atomically',
    async () => {
      // Change only this isolated import's dispatcher back to the pre-fix app
      // behavior. No production file or panel/Session implementation is changed.
      const start = main.indexOf('function commandChange(');
      const end = main.indexOf('\nfunction settleNumericControl(', start);
      assert.ok(start >= 0 && end > start);
      const commandChange = main.slice(start, end);
      const dispatchCall = /result = session\.dispatch\(\{[\s\S]*?\n    \}\);/g;
      assert.equal([...commandChange.matchAll(dispatchCall)].length, 1);
      const broken =
        main.slice(0, start) +
        commandChange.replace(
          dispatchCall,
          'result = session.dispatch({ ...envelope, type, payload: { layerId: current().id, ...payload } });',
        ) +
        main.slice(end);
      const s = await boot(broken, 'original-layer-id-injection');
      try {
        const initial = s.snapshot();
        s.click('[data-harmony-create]');
        assert.deepEqual(s.snapshot(), initial);
        assert.equal(s.snapshot().schemaVersion, 27);
        assert.equal(s.ui().past, 0);
        assert.equal(s.ui().error, true);
        assert.match(
          s.query('.status.error .message').textContent,
          /Invalid harmony command payload/,
        );
        assert.ok(created, 'production Create succeeded before the Add negative control');
        s.win.nect.load(JSON.stringify(created));
        for (const kind of ['neutral', 'chromatic']) {
          s.change('[data-wheel-add-kind]', kind);
          s.click('[data-harmony-add]');
          assert.deepEqual(s.snapshot(), created);
          assert.equal(s.ui().past, 0);
          assert.equal(s.ui().future, 0);
          assert.equal(s.ui().error, true);
          assert.match(
            s.query('.status.error .message').textContent,
            /Invalid harmony command payload/,
          );
        }
      } finally {
        await s.win.happyDOM.abort();
      }
    },
  );
});
