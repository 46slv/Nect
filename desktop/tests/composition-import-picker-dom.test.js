import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc } from '../src/core.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('browser composition import settles and releases its file picker without changing existing work', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'Event', 'MutationObserver'])
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
  win.SVGElement.prototype.getScreenCTM = () => ({ a: 1, b: 0, inverse: () => ({}) });
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    },
  });
  t.after(() => win.happyDOM.abort());
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
  // Export only in this test's copy so the production async function's settlement is observable.
  source += `
    export { openCompositionImportDialog, selectionSnapshot };
    export const importHistoryForTest = () => structuredClone({ past: history.past, future: history.future });
    while (!bundledFont) await new Promise(resolve => setTimeout(resolve, 10));
  `;
  const { openCompositionImportDialog, selectionSnapshot, importHistoryForTest } = await import(
    'data:text/javascript;base64,' + Buffer.from(source).toString('base64')
  );
  const api = win.nect;
  api.load(JSON.stringify(makeDoc()));
  document.querySelector('[data-tool="Rectangle"]').click();
  document.querySelector('[data-tool="Circle"]').click();
  document.querySelector('[data-action="undo"]').click();
  const snapshot = () => ({
    project: api.projectSnapshot(),
    command: api.commandState,
    file: api.fileState,
    gesture: api.gestureState,
    activeCompositionId: api.activeCompositionId,
    selection: selectionSnapshot(true),
    history: importHistoryForTest(),
    undoDisabled: document.querySelector('[data-action="undo"]').disabled,
    redoDisabled: document.querySelector('[data-action="redo"]').disabled,
  });
  const inputs = () => document.querySelectorAll('body > input[type="file"]');
  const dialogs = () => document.querySelectorAll('body > dialog');
  let lastInput, pickerError;
  win.HTMLInputElement.prototype.click = function () {
    assert.equal(this.type, 'file');
    lastInput = this;
    const remove = this.remove.bind(this);
    this.removals = 0;
    this.remove = () => {
      this.removals++;
      remove();
    };
    if (pickerError) throw pickerError;
  };
  const begin = () => {
    let completed = false;
    const result = openCompositionImportDialog().then(
      (value) => {
        completed = true;
        return { value };
      },
      (error) => {
        completed = true;
        return { error };
      },
    );
    return { input: lastInput, result, completed: () => completed };
  };
  const tick = () => new Promise((resolve) => setTimeout(resolve, 0));
  const settled = async (attempt) => {
    // A bounded wait detects the old pending Promise without leaving the test waiting forever.
    for (let i = 0; i < 20 && !attempt.completed(); i++) await tick();
    assert.equal(attempt.completed(), true, 'import must settle after picker completion');
    return attempt.result;
  };
  const released = (input) => {
    assert.equal(input.isConnected, false);
    assert.equal(input.removals, 1, 'temporary input must be removed exactly once');
    assert.equal(input.onchange, null);
    assert.equal(input.oncancel, null);
    assert.equal(inputs().length, 0);
  };
  const choose = (input, file) => {
    Object.defineProperty(input, 'files', { configurable: true, value: file ? [file] : [] });
    input.dispatchEvent(new win.Event('change'));
  };
  const file = (text = JSON.stringify(makeDoc()), extra = {}) => ({
    name: 'source.json',
    size: Buffer.byteLength(text),
    text: async () => text,
    ...extra,
  });
  const close = (dialog, value) => {
    dialog.returnValue = value;
    dialog.dispatchEvent(new win.Event('close'));
  };

  await t.test(
    'repeated cancel events settle, clean up once and preserve project and history',
    async () => {
      const before = snapshot();
      for (let i = 0; i < 5; i++) {
        const attempt = begin(),
          staleChange = attempt.input.onchange,
          staleCancel = attempt.input.oncancel;
        attempt.input.dispatchEvent(new win.Event('cancel'));
        assert.deepEqual(await settled(attempt), { value: undefined });
        // Queued callbacks cannot revive a canceled import, even if a file becomes available.
        choose(attempt.input, file());
        await staleChange?.();
        staleCancel?.();
        released(attempt.input);
        assert.equal(dialogs().length, 0);
        assert.deepEqual(snapshot(), before);
      }
    },
  );

  await t.test('an empty change also settles as cancellation', async () => {
    const before = snapshot(),
      attempt = begin();
    choose(attempt.input, null);
    assert.deepEqual(await settled(attempt), { value: undefined });
    released(attempt.input);
    assert.deepEqual(snapshot(), before);
  });

  await t.test(
    'selection reads once, ignores late cancel/change and imports in one undo step',
    async () => {
      const before = snapshot(),
        attempt = begin(),
        staleChange = attempt.input.onchange,
        staleCancel = attempt.input.oncancel;
      let reads = 0,
        finishRead;
      const sourceText = JSON.stringify(makeDoc());
      choose(
        attempt.input,
        file(sourceText, {
          text: () => {
            reads++;
            return new Promise((resolve) => (finishRead = resolve));
          },
        }),
      );
      released(attempt.input);
      await staleChange?.();
      staleCancel?.();
      assert.equal(reads, 1);
      assert.equal(attempt.completed(), false, 'selected file must finish reading');
      finishRead(sourceText);
      assert.deepEqual(await settled(attempt), { value: undefined });
      assert.equal(dialogs().length, 1, 'successful selection opens the composition chooser once');
      assert.deepEqual(snapshot(), before, 'reading alone must not change project state');
      const dialog = dialogs()[0];
      dialog.querySelector('[data-composition-choice]').value = '__all__';
      close(dialog, 'import');
      const imported = api.projectSnapshot();
      assert.equal(imported.compositions.length, before.project.compositions.length + 1);
      assert.deepEqual(imported.compositions.slice(0, -1), before.project.compositions);
      assert.equal(importHistoryForTest().past.length, before.history.past.length + 1);
      assert.equal(importHistoryForTest().future.length, 0);
      document.querySelector('[data-action="undo"]').click();
      assert.deepEqual(api.projectSnapshot(), before.project);
      document.querySelector('[data-action="redo"]').click();
      assert.deepEqual(api.projectSnapshot(), imported);
      document.querySelector('[data-action="undo"]').click();
      released(attempt.input);
    },
  );

  await t.test(
    'cancel preserves pending redo and the later composition-dialog Cancel remains safe',
    async () => {
      const before = snapshot();
      assert.equal(before.redoDisabled, false);
      const canceled = begin();
      canceled.input.dispatchEvent(new win.Event('cancel'));
      assert.deepEqual(await settled(canceled), { value: undefined });
      released(canceled.input);
      const selected = begin();
      choose(selected.input, file());
      assert.deepEqual(await settled(selected), { value: undefined });
      close(dialogs()[0], 'cancel');
      released(selected.input);
      assert.equal(dialogs().length, 0);
      assert.deepEqual(snapshot(), before);
    },
  );

  await t.test(
    'read failure and oversized file reject and release without changing work',
    async () => {
      const before = snapshot();
      for (const selected of [
        file('', {
          text: async () => {
            throw Error('read failed');
          },
        }),
        file('', {
          size: 50 * 1024 * 1024 + 1,
          text: () => assert.fail('must not read oversized file'),
        }),
      ]) {
        const attempt = begin();
        choose(attempt.input, selected);
        assert.match((await settled(attempt)).error.message, /read failed|Project exceeds 50 MB/);
        released(attempt.input);
        assert.deepEqual(snapshot(), before);
      }
    },
  );

  await t.test('invalid project contents are rejected after releasing the picker', async () => {
    const before = snapshot(),
      attempt = begin();
    choose(attempt.input, file('{'));
    assert.deepEqual(await settled(attempt), { value: undefined });
    released(attempt.input);
    assert.equal(dialogs().length, 0);
    assert.match(
      document.querySelector('.status .message').textContent,
      /Composition import rejected/,
    );
    assert.deepEqual(snapshot(), before);
  });

  await t.test('picker opening exception rejects and releases its appended input', async () => {
    const before = snapshot();
    pickerError = Error('picker unavailable');
    const attempt = begin();
    pickerError = null;
    assert.equal((await settled(attempt)).error.message, 'picker unavailable');
    released(attempt.input);
    assert.deepEqual(snapshot(), before);
  });
});
