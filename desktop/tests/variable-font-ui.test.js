import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Worker as NodeWorker } from 'node:worker_threads';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize } from '../src/core.js';
const buffer = async (path) => {
  const b = await fs.readFile(new URL(path, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
test('opt-in variable UI: lazy worker, real axes preview/cancel/commit, history, byte reuse, open and failure atomicity', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint']) globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: () => buffer('../public/fonts/DejaVuSans.ttf'),
  });
  let workers = 0,
    delay = 0;
  const live = new Set();
  globalThis.Worker = class {
    constructor() {
      workers++;
      this.worker = new NodeWorker(new URL('./variable-font-worker-node.mjs', import.meta.url));
      live.add(this);
      this.worker.on('message', (data) => setTimeout(() => this.onmessage?.({ data }), delay));
      this.worker.on('error', (e) => this.onerror?.(e));
    }
    postMessage(data) {
      this.worker.postMessage(data);
    }
    terminate() {
      live.delete(this);
      return this.worker.terminate();
    }
  };
  t.after(async () => {
    for (const w of live) await w.terminate();
    await win.happyDOM.close();
  });
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /from '(.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const wait = async (condition, label = 'condition') => {
    const end = Date.now() + 15000;
    while (!condition()) {
      if (Date.now() > end)
        throw Error(`Timed out: ${label}; ${document.querySelector('.message')?.textContent}`);
      await new Promise((r) => setTimeout(r, 20));
    }
  };
  await new Promise((r) => setTimeout(r, 50));
  const doc = newDocument(),
    text = node('Text'),
    out = node('Output');
  text.text = 'AV';
  out.params.path.source = text.id;
  doc.layers[0].nodes = [text, out];
  doc.layers[0].output = out.id;
  win.nect.load(serialize(doc));
  const snapshot = () => JSON.stringify(win.nect.document);
  const current = () => win.nect.document.layers[0].nodes[0];
  const status = () => document.querySelector('.message').textContent;
  const baseline = snapshot();
  assert.equal(workers, 0, 'ordinary startup/static document never starts a Worker');
  const bytes = await buffer('./fixtures/variable-font/Roboto-variable.ttf');
  const file = {
    name: 'Roboto-variable.ttf',
    size: bytes.byteLength,
    arrayBuffer: async () => bytes,
  };
  const start = (files) => {
    document.querySelector('.inspector-body [data-load-variable-font]').click();
    const input = document.querySelector('#font-file');
    Object.defineProperty(input, 'files', { configurable: true, value: files });
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  start([]);
  await new Promise((r) => setTimeout(r, 20));
  assert.equal(snapshot(), baseline);
  start([file]);
  await wait(() => !!current().variation, 'import');
  assert.equal(win.nect.evaluate().error, null);
  const imported = snapshot(),
    originalSVG = win.nect.export();
  assert.equal(Object.keys(win.nect.document.fontAssets).length, 1);
  let axis = document.querySelector('.inspector-body [data-variable-axis="wght"]');
  assert.equal(axis.min, '100');
  assert.equal(axis.max, '900');
  assert.equal(axis.value, '400');
  axis.value = '700';
  axis.dispatchEvent(new win.Event('input', { bubbles: true }));
  await wait(() => status().includes('axis preview'), 'preview');
  assert.equal(snapshot(), imported, 'preview is not authored history');
  document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(snapshot(), imported);
  assert.equal(win.nect.export(), originalSVG);
  axis = document.querySelector('.inspector-body [data-variable-axis="wght"]');
  axis.value = '700';
  axis.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => current().variation.axes.wght === 700, 'axis commit');
  assert.notEqual(win.nect.export(), originalSVG);
  document.querySelector('[data-action="undo"]').click();
  await wait(() => snapshot() === imported, 'one undo');
  document.querySelector('[data-action="redo"]').click();
  await wait(() => current().variation.axes.wght === 700, 'redo');
  const changed = snapshot();
  // Unprepared ordinary commands cannot mutate variable text.
  assert.throws(
    () =>
      win.nect.dispatch({
        ...win.nect.commandState,
        type: 'set-text-content',
        payload: { layerId: doc.layers[0].id, nodeId: text.id, text: 'Unsafe' },
      }),
    /PREPARATION_REQUIRED/,
  );
  assert.equal(snapshot(), changed);
  const writing = document.querySelector('.inspector-body [data-writing-mode]');
  writing.value = 'vertical-rl';
  writing.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => status().includes('horizontal text only'), 'unsupported direction');
  assert.equal(snapshot(), changed);
  start([file]);
  await wait(() => status().includes('Variable font loaded'), 'reuse import');
  assert.equal(Object.keys(win.nect.document.fontAssets).length, 1);
  const saved = snapshot();
  win.nect.load(serialize(doc));
  assert.equal(snapshot(), baseline);
  await win.nect.load(saved);
  assert.equal(snapshot(), saved);
  assert.equal(win.nect.evaluate().error, null);
  // Real Worker transport delayed to prove cancellation and latest-only publication.
  delay = 150;
  axis = document.querySelector('.inspector-body [data-variable-axis="wght"]');
  axis.value = '550';
  axis.dispatchEvent(new win.Event('input', { bubbles: true }));
  axis.value = '650';
  axis.dispatchEvent(new win.Event('input', { bubbles: true }));
  document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  await new Promise((r) => setTimeout(r, 400));
  assert.equal(snapshot(), saved);
  delay = 0;
  const field = document.querySelector('.inspector-body textarea[data-text]');
  field.value = 'AV AV';
  field.dispatchEvent(new win.Event('input', { bubbles: true }));
  field.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => current().text === 'AV AV', 'text edit');
  assert.equal(win.nect.evaluate().error, null);
  const latin = snapshot();
  const unsupported = document.querySelector('.inspector-body textarea[data-text]');
  unsupported.value = '日本語';
  unsupported.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => status().includes('single-line Latin'), 'complex mapping rejection');
  assert.equal(snapshot(), latin);
  const sizeWorkers = workers;
  const size = document.querySelector('.inspector-body [data-key="size"] input[data-value]');
  assert(size, 'size control exists');
  size.value = '80';
  size.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => current().params.size.value === 80, 'size reuse');
  assert.equal(workers, sizeWorkers, 'size reuses prepared units');
  assert.equal(win.nect.evaluate().error, null);
  // A static edit during pending axes wins; the delayed candidate cannot publish.
  delay = 150;
  axis = document.querySelector('.inspector-body [data-variable-axis="wght"]');
  axis.value = '575';
  axis.dispatchEvent(new win.Event('change', { bubbles: true }));
  const nextSize = document.querySelector('.inspector-body [data-key="size"] input[data-value]');
  nextSize.value = '90';
  nextSize.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => current().params.size.value === 90, 'concurrent static edit');
  const staticWon = snapshot();
  await new Promise((r) => setTimeout(r, 400));
  assert.equal(snapshot(), staticWon);
  // Cancelling asynchronous open retains the current document and font bank.
  const openPromise = win.nect.load(saved);
  document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  await assert.rejects(openPromise);
  assert.equal(snapshot(), staticWon);
  assert.equal(win.nect.evaluate().error, null);
  delay = 0;
  const final = snapshot();
  start([{ name: 'bad.ttf', size: 20, arrayBuffer: async () => new ArrayBuffer(20) }]);
  await wait(() => status().includes('Font rejected'), 'invalid font');
  assert.equal(snapshot(), final);
  win.nect.load(serialize(doc));
});
