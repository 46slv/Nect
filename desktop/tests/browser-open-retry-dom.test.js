import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { blankDocument } from '../src/core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';

test('browser Open re-arms only after confirmation and preserves an in-flight File', async (t) => {
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
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    },
  });
  t.after(() => win.happyDOM.abort());
  const source = resolveMainRelativeImports(
    (await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8')).replace(
      "import './style.css';",
      '',
    ),
  );
  await import(
    'data:text/javascript;base64,' +
      Buffer.from(
        source +
          `
    window.retryOpenForTest = async project => {
      await prepareOpenDocument(project, { saved:true }); render(true);
    };
    window.retryStateForTest = () => structuredClone({
      project:session.projectSnapshot(), command:session.state, file:window.nect.fileState,
      past:history.past, future:history.future, activeLayer, selected,
      selection:[...selection], artworkTarget, artworkSelection:[...artworkSelection],
      points:{nodeId:pointSelection.nodeId, ids:[...pointSelection.ids]}, svg:window.nect.export()
    });
    while (!bundledFont) await new Promise(resolve => setTimeout(resolve,10));
  `,
      ).toString('base64')
  );
  const waitFor = async (predicate) => {
    for (let i = 0; i < 200; i++) {
      if (predicate()) return;
      await new Promise((resolve) => setTimeout(resolve, 5));
    }
    assert.fail('Timed out waiting for the current Open operation');
  };
  const open = () => win.document.querySelector('[data-action="open"]').click();
  const answer = async (choice) => {
    await waitFor(() => win.document.querySelector('dialog[open]'));
    win.document.querySelector('dialog[open]').close(choice);
  };
  // Observe the value at picker activation. Actual same-path browser event
  // behavior is covered separately by browser-open-retry.e2e.js.
  const picker = () => {
    const input = win.document.querySelector('#open-file'),
      values = [];
    Object.defineProperty(input, 'value', {
      configurable: true,
      writable: true,
      value: 'C:\\fakepath\\project.json',
    });
    input.click = () => values.push(input.value);
    return { input, values };
  };
  const initial = migrateLegacyDocumentToCompositionProject(blankDocument());
  await win.retryOpenForTest(initial);
  win.document.querySelector('[data-tool="Rectangle"]').click();
  win.document.querySelector('[data-tool="Circle"]').click();
  win.document.querySelector('[data-action="undo"]').click();
  const baseline = win.retryStateForTest();
  assert.equal(baseline.file.dirty, true);
  assert.equal(baseline.past.length, 1);
  assert.equal(baseline.future.length, 1);

  await t.test('dirty Cancel retains the selection, history and input; retry re-arms', async () => {
    const { input, values } = picker();
    open();
    await answer('cancel');
    await new Promise((resolve) => setTimeout(resolve, 0));
    assert.deepEqual(values, []);
    assert.equal(input.value, 'C:\\fakepath\\project.json');
    assert.deepEqual(win.retryStateForTest(), baseline);
    open();
    await answer('discard');
    await waitFor(() => values.length === 1);
    assert.deepEqual(values, ['']);
    input.dispatchEvent(new win.Event('cancel', { bubbles: true }));
    assert.deepEqual(win.retryStateForTest(), baseline);
  });

  await t.test('rejected Open keeps its input reusable without losing state', async () => {
    const { input, values } = picker();
    Object.defineProperty(input, 'files', {
      configurable: true,
      value: [{ size: 1, name: 'project.json', text: async () => '{' }],
    });
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
    await waitFor(() =>
      win.document.querySelector('.status .message').textContent.startsWith('Open rejected;'),
    );
    assert.match(win.document.querySelector('.status .message').textContent, /JSON|property name/i);
    assert.equal(win.document.querySelector('#open-file'), input);
    assert.deepEqual(win.retryStateForTest(), baseline);
    open();
    await answer('discard');
    await waitFor(() => values.length === 1);
    assert.deepEqual(values, ['']);
    assert.deepEqual(win.retryStateForTest(), baseline);
  });

  await t.test('reopening and cancelling cannot clear the File already being read', async () => {
    await win.retryOpenForTest(initial);
    const { input, values } = picker();
    let finishRead;
    const contents = new Promise((resolve) => (finishRead = resolve));
    const candidate = structuredClone(initial);
    candidate.compositions[0].width = 317;
    let reads = 0;
    Object.defineProperty(input, 'files', {
      configurable: true,
      value: [
        {
          size: 100,
          name: 'project.json',
          text: () => {
            reads++;
            return contents;
          },
        },
      ],
    });
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
    assert.equal(reads, 1);
    const before = win.retryStateForTest();
    open();
    await waitFor(() => values.length === 1);
    assert.deepEqual(values, ['']);
    input.dispatchEvent(new win.Event('cancel', { bubbles: true }));
    assert.deepEqual(win.retryStateForTest(), before);
    finishRead(JSON.stringify(candidate));
    await waitFor(() => win.nect.commandState.sessionId !== before.command.sessionId);
    assert.deepEqual(win.nect.projectSnapshot(), candidate);
    assert.equal(win.nect.fileState.dirty, false);
    assert.equal(win.retryStateForTest().past.length, 0);
    assert.equal(win.retryStateForTest().future.length, 0);
    assert.equal(reads, 1);
  });
});
