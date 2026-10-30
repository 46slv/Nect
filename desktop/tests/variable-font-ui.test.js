import test from 'node:test';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Worker as NodeWorker } from 'node:worker_threads';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize, createCharacterPositions } from '../src/core.js';
import { parseFont, makeFontAsset } from '../src/fonts.js';
const buffer = async (path) => {
  const b = await fs.readFile(new URL(path, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
test('opt-in variable UI: lazy worker, real axes preview/cancel/commit, history, byte reuse, open and failure atomicity', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint']) globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
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
  const staticBytes = await buffer('../public/fonts/DejaVuSans.ttf');
  globalThis.mixedFontCatalog = [
    ['system-static', 'Installed static face', null, parseFont(staticBytes).nectMetadata],
    ['system-variable', 'Installed variable face', 'Variable fonts unsupported'],
  ];
  let readInstalled = async () => staticBytes;
  globalThis.readMixedFont = (id) => readInstalled(id);
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /import \{[^}]*\} from '.\/storage.js';/,
      `import { DocumentLifecycle } from '${new URL('../src/document-lifecycle.js', import.meta.url)}'; const releaseProject = async () => {}, writeProject = async () => null, chooseUnsaved = async () => 'cancel', registerClose = async () => {}; const desktop=true, saveFile=async()=>true, openFile=async()=>null, listSystemFonts=async()=>globalThis.mixedFontCatalog, readSystemFont=(id)=>globalThis.readMixedFont(id); const beginRecovery=async()=> 'test-recovery', writeRecovery=async()=>Date.now(), listRecovery=async()=>[], readRecovery=async()=>'', acknowledgeRecovery=async()=>{};`,
    )
    .replace(
      /from '(.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source = resolveMainRelativeImports(source);
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
  // Named instances carry the font's complete coordinates, never inferred weights.
  // happy-dom does not synchronize parsed option[selected] to select.value.
  // Assert the rendered selection attribute; user events below set value explicitly.
  const instanceSelect = () => document.querySelector('.inspector-body [data-variable-instance]');
  const chooseInstance = (label, event = 'change') => {
    const select = instanceSelect();
    const option = [...select.options].find((option) => option.textContent === label);
    assert(option, `real named instance ${label}`);
    select.value = option.value;
    select.dispatchEvent(new win.Event(event, { bubbles: true }));
  };
  assert.equal(instanceSelect().querySelector('option[selected]').textContent, 'Bold');
  const beforeInstance = snapshot();
  chooseInstance('Condensed Black', 'input');
  await wait(() => status().includes('axis preview'), 'instance preview');
  assert.equal(snapshot(), beforeInstance);
  document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(snapshot(), beforeInstance);
  assert.equal(instanceSelect().querySelector('option[selected]').textContent, 'Bold');
  chooseInstance('Condensed Black');
  await wait(
    () => current().variation.axes.wght === 900 && current().variation.axes.wdth === 75,
    'named instance commit',
  );
  assert.equal(instanceSelect().querySelector('option[selected]').textContent, 'Condensed Black');
  assert.notEqual(win.nect.export(), originalSVG);
  assert.equal(Object.keys(win.nect.document.fontAssets).length, 1);
  assert.equal(
    Object.values(win.nect.document.fontAssets)[0].data,
    Object.values(JSON.parse(imported).fontAssets)[0].data,
  );
  const instanceSaved = snapshot();
  await win.nect.load(instanceSaved);
  assert.equal(snapshot(), instanceSaved);
  assert.equal(instanceSelect().querySelector('option[selected]').textContent, 'Condensed Black');
  // Reopening resets history; make a fresh preset edit and check its single Undo.
  chooseInstance('Bold');
  await wait(
    () => current().variation.axes.wght === 700 && current().variation.axes.wdth === undefined,
    'preset resets omitted defaults',
  );
  document.querySelector('[data-action="undo"]').click();
  await wait(() => snapshot() === instanceSaved, 'preset one undo');
  document.querySelector('[data-action="redo"]').click();
  await wait(() => current().variation.axes.wght === 700, 'preset redo');
  axis = document.querySelector('.inspector-body [data-variable-axis="wght"]');
  axis.value = '701';
  axis.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => current().variation.axes.wght === 701, 'custom axes');
  assert.equal(
    instanceSelect().querySelector('option[selected]').value,
    '',
    'nearby custom coordinates must not claim a named preset',
  );
  assert.equal(instanceSelect().querySelector('option[selected]').textContent, 'Custom axes');
  delay = 150;
  chooseInstance('Thin', 'input');
  chooseInstance('Black', 'input');
  document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  await new Promise((r) => setTimeout(r, 400));
  assert.equal(current().variation.axes.wght, 701, 'cancelled stale preset cannot commit');
  assert.equal(instanceSelect().querySelector('option[selected]').value, '');
  delay = 0;
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
  // Explicit static import replaces an existing variable Text with the parsed asset ID in one edit.
  const variableBeforeStatic = structuredClone(current());
  const beforeStaticAssets = new Set(Object.keys(win.nect.document.fontAssets));
  const beforeStaticRevision = win.nect.commandState.expectedRevision;
  document.querySelector('.inspector-body [data-load-font]').click();
  const wholeStaticInput = document.querySelector('#font-file');
  Object.defineProperty(wholeStaticInput, 'files', { configurable: true, value: [{
    name: 'static-from-variable.ttf', size: staticBytes.byteLength, arrayBuffer: async () => staticBytes,
  }] });
  wholeStaticInput.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => !current().variation && current().font !== variableBeforeStatic.font, 'variable to static explicit import');
  const actualStaticId = current().font;
  assert.equal(beforeStaticAssets.has(actualStaticId), false);
  assert.equal(Object.keys(win.nect.document.fontAssets).length, beforeStaticAssets.size + 1);
  assert.ok(win.nect.document.fontAssets[actualStaticId]);
  assert.equal(win.nect.commandState.expectedRevision, beforeStaticRevision + 1);
  assert.equal(win.nect.evaluate().error, null);
  assert.deepEqual(current().params, variableBeforeStatic.params);
  assert.equal(current().text, variableBeforeStatic.text);
  assert.deepEqual(Buffer.from(win.nect.document.fontAssets[actualStaticId].data, 'base64'), Buffer.from(staticBytes));
  const importedWholeStatic = snapshot();
  document.querySelector('[data-action="undo"]').click();
  await wait(() => snapshot() === final, 'variable to static one Undo restores exact source');
  document.querySelector('[data-action="redo"]').click();
  await wait(() => snapshot() === importedWholeStatic, 'variable to static exact Redo');
  document.querySelector('[data-action="undo"]').click();
  await wait(() => snapshot() === final, 'restore variable source for mixed tests');
  // Static Text keeps its rich picker even while another Text uses a live variable bank.
  const mixed = JSON.parse(final),
    staticText = node('Text');
  const staticAsset = makeFontAsset(staticBytes, 'Embedded static fixture', mixed.fontAssets);
  mixed.fontAssets = { ...mixed.fontAssets, [staticAsset.id]: staticAsset.asset };
  staticText.text = 'AV';
  staticText.characterPositions = createCharacterPositions(staticText.text);
  mixed.layers[0].nodes.unshift(staticText);
  await win.nect.load(JSON.stringify(mixed));
  const q = (selector) => document.querySelector(selector);
  const openPicker = async (range = false) => {
    q(range ? '[data-character-style="font"]' : '.inspector-body [data-change-font]').dispatchEvent(
      new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true }),
    );
    await wait(() => !!q('dialog[open]'), 'mixed picker open');
    for (const group of [...document.querySelectorAll('.picker-family')]) {
      const current = [...document.querySelectorAll('.picker-family')].find((g) => g.dataset.fontFamily === group.dataset.fontFamily);
      if (current?.getAttribute('aria-expanded') === 'false') current.querySelector('.picker-family-heading').click();
    }
  };
  const row = (id) => [...document.querySelectorAll('.picker-option')].find((el) =>
    el.dataset.id === JSON.stringify([id.startsWith('system-') ? 'installed' : 'embedded', id]));
  const apply = async () => {
    await wait(() => q('.picker-apply') && !q('.picker-apply').disabled, 'mixed picker ready');
    q('.picker-apply').click();
    await wait(() => !q('dialog[open]'), 'mixed picker applied');
  };
  const mixedBefore = snapshot(),
    variableId = mixed.layers[0].nodes[1].font;
  const variableBefore = JSON.stringify(mixed.layers[0].nodes[1]);
  await openPicker();
  assert.ok(q('[aria-label="Font family"]'), 'static family filters survive mixed documents');
  assert.ok(row('system-static').querySelector('.picker-star'));
  assert.equal(row('system-variable').getAttribute('aria-disabled'), 'true');
  assert.equal(row('system-static').getAttribute('aria-disabled'), 'true');
  row(staticAsset.id).click();
  await apply();
  assert.notEqual(current().font, 'default');
  assert.equal(JSON.stringify(win.nect.document.layers[0].nodes[1]), variableBefore);
  assert.equal(win.nect.evaluate().error, null, 'static commit preserves variable bank readiness');
  const mixedWhole = snapshot();
  q('[data-action="undo"]').click();
  await wait(() => snapshot() === mixedBefore, 'mixed static undo');
  q('[data-action="redo"]').click();
  await wait(() => snapshot() === mixedWhole, 'mixed static redo');
  const rangeText = q('.inspector-body [data-text]');
  rangeText.setSelectionRange(0, 1);
  rangeText.dispatchEvent(new win.Event('select', { bubbles: true }));
  q('.inspector-body [data-style-text-range]').click();
  await openPicker(true);
  assert.equal(
    row(variableId).getAttribute('aria-disabled'),
    'true',
    'variable range stays unavailable',
  );
  row('default').click();
  await apply();
  assert.equal(current().characterPositions.styles.c1.font, 'default');
  assert.equal(win.nect.evaluate().error, null);
  const mixedRange = snapshot();
  // A byte-only discovery never starts a normal picker read, even in a mixed document.
  let installedReads = 0;
  readInstalled = async () => { installedReads++; return staticBytes; };
  await openPicker();
  assert.equal(row('system-static').getAttribute('aria-disabled'), 'true');
  row('system-static').click();
  q('.picker-cancel').click();
  await new Promise((r) => setTimeout(r, 40));
  assert.equal(installedReads, 0);
  assert.equal(snapshot(), mixedRange);
  assert.equal(win.nect.evaluate().error, null);
  const importedStaticBytes = await buffer('./fixtures/fonts/NectJPFixture-Regular.otf');
  q('.inspector-body [data-load-font]').click();
  const staticInput = q('#font-file');
  Object.defineProperty(staticInput, 'files', {
    configurable: true,
    value: [
      {
        name: 'mixed-static.otf',
        size: importedStaticBytes.byteLength,
        arrayBuffer: async () => importedStaticBytes,
      },
    ],
  });
  staticInput.dispatchEvent(new win.Event('change', { bubbles: true }));
  await wait(() => snapshot() !== mixedRange, 'mixed static import');
  assert.equal(JSON.stringify(win.nect.document.layers[0].nodes[1]), variableBefore);
  assert.equal(win.nect.evaluate().error, null);
  q('[data-action="undo"]').click();
  await wait(() => snapshot() === mixedRange, 'mixed static import single undo');
  // Whole-text variable selection remains explicit and prepared, with no gesture preview commit.
  await win.nect.load(mixedBefore);
  await openPicker();
  row(variableId).click();
  assert.equal(snapshot(), mixedBefore);
  q('.picker-cancel').click();
  assert.equal(snapshot(), mixedBefore, 'variable row cancellation leaves static Text untouched');
  await openPicker();
  row(variableId).click();
  await apply();
  await wait(() => !!current().variation, 'mixed whole variable switch');
  assert.equal(current().font, variableId);
  assert.equal(win.nect.evaluate().error, null);
  q('[data-action="undo"]').click();
  await wait(() => snapshot() === mixedBefore, 'mixed variable switch single undo');
  win.nect.load(serialize(doc));
});
