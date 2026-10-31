import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as newDocument, node, serialize } from '../src/core.js';
const buffer = async (path) => {
  const b = await fs.readFile(new URL(path, import.meta.url));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
};
test('installed discovery never copies bytes; explicit file import remains atomic, reusable and stale-safe', async () => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'DOMPoint', 'requestAnimationFrame'])
    globalThis[key] = key === 'requestAnimationFrame' ? () => 0 : win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: () => buffer('../public/fonts/DejaVuSans.ttf'),
  });
  let calls = 0;
  globalThis.readInstalledTestFont = async (id) => {
    calls++;
    return buffer(
      id === 'system-2'
        ? '../public/fonts/DejaVuSans.ttf'
        : './fixtures/fonts/NectJPFixture-Regular.otf',
    );
  };
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /import \{[^}]*\} from '.\/storage.js';/,
      `import { DocumentLifecycle } from '${new URL('../src/document-lifecycle.js', import.meta.url)}'; const releaseProject = async () => {}, writeProject = async () => null, chooseUnsaved = async () => 'cancel', registerClose = async () => {}; const desktop = true, saveFile = async () => true, openFile = async () => null, listSystemFonts = async () => [['system-0', 'Japanese fixture', null], ['system-1', 'Variable face', 'Variable fonts unsupported'], ['system-2', 'DejaVu Sans', null]], readSystemFont = (id) => globalThis.readInstalledTestFont(id); const beginRecovery=async()=> 'test-recovery', writeRecovery=async()=>Date.now(), listRecovery=async()=>[], readRecovery=async()=>'', acknowledgeRecovery=async()=>{};`,
    );
  for (const name of [
    'artwork-creation',
    'artwork-move-target',
    'artwork-multi-move',
    'artwork-alignment',
    'artwork-distribution',
    'artwork-gap-distribution',
    'artwork-deletion',
    'artwork-ordering',
    'core',
    'character-snapping',
    'focus',
    'layout',
    'svg-import',
    'fonts',
    'images',
    'raster',
  ])
    source = source.replace(
      `from './${name}.js'`,
      `from '${new URL(`../src/${name}.js`, import.meta.url)}'`,
    );
  source = resolveMainRelativeImports(source);
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const until = async (predicate) => {
    const deadline = Date.now() + 5000;
    while (!predicate()) {
      assert.ok(Date.now() < deadline, 'Timed out waiting for font operation');
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  };
  await until(() => !!win.nect);
  assert.equal(calls, 0);
  const doc = newDocument(),
    text = node('Text'),
    out = node('Output');
  text.text = 'Abg 123';
  out.params.path.source = text.id;
  doc.layers[0].nodes = [text, out];
  doc.layers[0].output = out.id;
  win.nect.load(serialize(doc));
  openDOMPanel(document, 'inspector');
  const snapshot = () => JSON.stringify(win.nect.document);
  const q = (selector) => document.querySelector(selector);
  const before = snapshot();
  q('.inspector-body [data-change-font]').dispatchEvent(
    new win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true }),
  );
  await until(() => q('dialog[open] .picker-option'));
  for (const group of [...document.querySelectorAll('.picker-family')]) {
    const current = [...document.querySelectorAll('.picker-family')].find(
      (g) => g.dataset.fontFamily === group.dataset.fontFamily,
    );
    if (current?.getAttribute('aria-expanded') === 'false')
      current.querySelector('.picker-family-heading').click();
  }
  const rows = () => [...document.querySelectorAll('.picker-option')];
  const row = (id) =>
    rows().find((entry) => entry.dataset.id === JSON.stringify(['installed', id]));
  assert.equal(document.querySelectorAll('.inspector-body [data-change-font]').length, 1);
  assert.equal(document.querySelectorAll('[data-system-font]').length, 0);
  for (const id of ['system-0', 'system-1', 'system-2']) {
    assert.equal(row(id).getAttribute('aria-disabled'), 'true');
    assert.match(row(id).textContent, /Font file import is required; use Advanced/);
    row(id).click();
  }
  assert.match(row('system-1').textContent, /Variable fonts unsupported/);
  assert.equal(snapshot(), before);
  q('.picker-cancel').click();
  assert.equal(calls, 0);
  assert.equal(snapshot(), before);

  const start = (file) => {
    q('.inspector-body [data-load-font]').click();
    const input = q('#font-file');
    Object.defineProperty(input, 'files', { configurable: true, value: file ? [file] : [] });
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
  };
  const bytes = await buffer('./fixtures/fonts/NectJPFixture-Regular.otf');
  const file = {
    name: 'licensed-fixture.otf',
    size: bytes.byteLength,
    arrayBuffer: async () => bytes,
  };
  start(file);
  await until(() => /Loaded /.test(q('.message').textContent));
  assert.equal(calls, 0, 'explicit import does not use the installed catalog reader');
  assert.equal(win.nect.evaluate().error, null);
  assert.equal(Object.keys(win.nect.document.fontAssets).length, 1);
  const saved = snapshot(),
    svg = win.nect.export();
  q('.message').textContent = '';
  start(file);
  await until(() => /Loaded /.test(q('.message').textContent));
  assert.equal(snapshot(), saved);
  q('[data-action="undo"]').click();
  assert.equal(snapshot(), before);
  q('[data-action="redo"]').click();
  assert.equal(win.nect.export(), svg);
  win.nect.load(saved);
  assert.equal(win.nect.export(), svg);

  // Explicitly imported licensed faces change actual outlines, not only labels.
  const firstPath = JSON.stringify(win.nect.evaluate().path);
  const latin = await buffer('../public/fonts/DejaVuSans.ttf');
  q('.message').textContent = '';
  start({ name: 'licensed-latin.ttf', size: latin.byteLength, arrayBuffer: async () => latin });
  await until(() => /Loaded DejaVu/.test(q('.message').textContent));
  const second = snapshot(),
    secondSvg = win.nect.export();
  assert.notEqual(JSON.stringify(win.nect.evaluate().path), firstPath);
  assert.notEqual(secondSvg, svg);
  assert.match(q('.inspector-body .text-font-choice').textContent, /DejaVu/);
  q('[data-action="undo"]').click();
  assert.equal(win.nect.export(), svg);
  q('[data-action="redo"]').click();
  assert.equal(win.nect.export(), secondSvg);
  win.nect.load(second);
  assert.equal(win.nect.export(), secondSvg);

  // A delayed explicit file read cannot overwrite a later authored Text edit.
  let finish;
  start({
    ...file,
    arrayBuffer: () =>
      new Promise((resolve) => {
        finish = resolve;
      }),
  });
  const textarea = q('.inspector-body [data-text]');
  textarea.value = 'Changed';
  textarea.dispatchEvent(new win.Event('change', { bubbles: true }));
  const changed = snapshot();
  finish(bytes);
  await until(() => /Document changed/.test(q('.message').textContent));
  assert.equal(snapshot(), changed);
  assert.match(q('.message').textContent, /Document changed/);
  q('.message').textContent = '';
  start({
    ...file,
    arrayBuffer: async () => {
      throw Error('Font file was removed');
    },
  });
  await until(() => /Font file was removed/.test(q('.message').textContent));
  assert.equal(snapshot(), changed);
  assert.equal(q('.inspector-body [data-change-font]').disabled, false);
  q('.message').textContent = '';
  start({ ...file, arrayBuffer: async () => new ArrayBuffer(12) });
  await until(() => /Font rejected/.test(q('.message').textContent));
  assert.equal(snapshot(), changed);
  assert.equal(calls, 0);
  win.happyDOM.abort();
});
