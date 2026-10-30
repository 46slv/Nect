import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { document as newDocument, node, serialize } from '../src/core.js';

// These synthetic adapters exercise the app and Session wiring only. They do
// not prove actual DirectWrite, font fidelity, file permissions or WebView2.
const face = {
  family: 'Synthetic private collection',
  name: 'Regular',
  weight: 400,
  style: 0,
  stretch: 5,
  format: 1,
  color: false,
};
const sha256 = 'a'.repeat(64);
const source = (faceIndex) => ({ kind: 'file-native-v1', sha256, faceIndex });
const file = {
  token: 'runtime-only-token',
  sha256,
  faces: [
    { faceIndex: 0, face },
    { faceIndex: 1, face },
  ],
};
const outline = (request) => ({
  ...request,
  engine: request.axes === undefined ? 'directwrite-v1' : 'directwrite-v2',
  ...(request.axes === undefined ? {} : { axes: [], advanceHeight: request.size * 4 }),
  ascent: request.size * 0.8,
  advanceWidth: request.size * 2,
  lineCount: request.text.split('\n').length,
  glyphCount: [...request.text].length,
  points: [
    [0, 0],
    [20, 0],
    [20, 10],
    [0, 10],
  ],
  types: [0, 1, 1, 129],
});
const deferred = () => {
  let resolve;
  return {
    promise: new Promise((done) => {
      resolve = done;
    }),
    get resolve() {
      return resolve;
    },
  };
};

test('private file picker preserves identity, atomic history, missing state and stale work in the real app', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'Event', 'localStorage'])
    globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
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
  const matrix = {
    a: 1,
    b: 0,
    c: 0,
    d: 1,
    e: 0,
    f: 0,
    inverse() {
      return this;
    },
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const bytes = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    },
  });
  t.after(() => win.happyDOM.abort());
  let chooser = null,
    shape = async (token, index, request) => ({ source: source(index), outline: outline(request) });
  const calls = [];
  globalThis.privateOpenFixture = () => Promise.resolve(chooser);
  globalThis.privateShapeFixture = (token, index, request) => {
    calls.push({ token, index, request: structuredClone(request) });
    return shape(token, index, request);
  };
  let main = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  main = main.replace("import './style.css';", '').replace(
    /import \{[^}]*\} from '.\/storage.js';/,
    `
import { DocumentLifecycle } from '${new URL('../src/document-lifecycle.js', import.meta.url)}';
const desktop = true, releaseProject = async () => {}, writeProject = async () => null,
  saveFile = async () => true, openFile = async () => null, openSVGSource = async () => null,
  chooseUnsaved = async () => 'cancel', registerClose = async () => {},
  beginRecovery = async () => 'private-font-test', writeRecovery = async () => Date.now(),
  listRecovery = async () => [], readRecovery = async () => '', acknowledgeRecovery = async () => {},
  listSystemFonts = async () => [], readSystemFont = async () => { throw Error('Unexpected font-byte read'); },
  listOsOutlineFonts = async () => [], listDesktopFontFaces = async () => [],
  listDesktopFontAxes = async () => [], shapeDesktopText = async () => { throw Error('Installed fallback must not run'); },
  outlineOsText = async () => { throw Error('Legacy fallback must not run'); },
  openPrivateFontFile = () => globalThis.privateOpenFixture(), listPrivateFontFiles = async () => [],
  listPrivateFontAxes = async () => [], shapePrivateFontText = (...args) => globalThis.privateShapeFixture(...args);`,
  );
  main = resolveMainRelativeImports(main);
  main +=
    '\nwindow.privateFontDebug = () => ({ past: history.past.length, bundled: !!bundledFont });';
  await import('data:text/javascript;base64,' + Buffer.from(main).toString('base64'));
  const q = (selector) => document.querySelector(selector);
  const tick = () => new Promise((done) => setTimeout(done, 20));
  const until = async (predicate, label) => {
    const deadline = Date.now() + 6000;
    while (!predicate()) {
      assert.ok(Date.now() < deadline, `Timed out: ${label}`);
      await tick();
    }
  };
  await until(() => win.nect && win.privateFontDebug().bundled, 'startup');
  const doc = newDocument(),
    text = node('Text'),
    output = node('Output');
  text.text = 'ABC';
  output.params.path.source = text.id;
  doc.layers[0].nodes = [text, output];
  doc.layers[0].output = output.id;
  const select = () => {
    q(`.graph .node[data-id="${text.id}"] .node-head`).dispatchEvent(
      new win.PointerEvent('pointerdown', { bubbles: true, pointerId: 1, button: 0 }),
    );
    document.dispatchEvent(
      new win.PointerEvent('pointerup', { bubbles: true, pointerId: 1, button: 0 }),
    );
  };
  win.nect.load(serialize(doc));
  select();
  const snapshot = () => JSON.stringify(win.nect.document);
  const live = () => win.nect.document.layers[0].nodes.find((n) => n.id === text.id);
  const openFile = () => q('.inspector-body [data-open-private-font]').click();
  const picker = () => q('dialog[aria-label="Font"]');
  const attach = async () => {
    chooser = file;
    openFile();
    await until(picker, 'Font after file open');
  };
  const choose = async (index) => {
    const input = picker().querySelector('input[type="search"]');
    input.value = face.family;
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
    await tick();
    const rows = [...picker().querySelectorAll('.picker-option')];
    assert.equal(rows.length, 2, 'same descriptor collection members remain distinct rows');
    rows[index].click();
    await tick();
  };
  const cancel = () => picker().querySelector('.picker-cancel').click();
  const apply = () => picker().querySelector('.picker-apply').click();

  const before = snapshot();
  chooser = null;
  openFile();
  await tick();
  assert.equal(snapshot(), before, 'native chooser cancel is non-mutating');
  assert.equal(picker(), null);
  await attach();
  await choose(1);
  apply();
  await until(() => !picker(), 'private apply');
  assert.deepEqual(live().osFont.source, source(1));
  assert.equal(calls.at(-1).index, 1);
  assert.equal(win.privateFontDebug().past, 1);
  const saved = snapshot();
  assert.ok(!saved.includes(file.token), 'project does not contain process capability');
  q('[data-action="undo"]').click();
  assert.equal(snapshot(), before);
  q('[data-action="redo"]').click();
  assert.equal(snapshot(), saved);

  win.nect.load(serialize(win.nect.document));
  select();
  assert.match(q('.inspector-body .text-font-choice').textContent, /Font file missing/);
  const callsBeforeMissingEdit = calls.length;
  const field = q('.inspector-body [data-os-text]');
  field.value = 'Changed';
  field.dispatchEvent(new win.Event('input', { bubbles: true }));
  q('.inspector-body [data-apply-os-text]').click();
  await tick();
  assert.equal(
    calls.length,
    callsBeforeMissingEdit,
    'missing token cannot fall back to same-name installed face',
  );
  assert.equal(snapshot(), saved, 'failed edit retains saved geometry and source');
  q('.inspector-body [data-cancel-os-text]').click();
  chooser = { ...file, token: 'different-token', sha256: 'b'.repeat(64) };
  openFile();
  await until(picker, 'different file opened');
  cancel();
  assert.match(
    q('.inspector-body .text-font-choice').textContent,
    /Font file missing/,
    'equal face names on a different file do not reattach the saved source',
  );
  assert.match(q('.status .message').textContent, /Font file missing/);
  await attach();
  cancel();
  assert.match(q('.inspector-body .text-font-choice').textContent, /attached for this session/);
  assert.match(q('.status .message').textContent, /attached for this session/);
  assert.equal(q('.status').classList.contains('error'), false);
  assert.equal(snapshot(), saved, 'reattachment is view capability, not document history');
  win.nect.load(serialize(win.nect.document));
  select();
  await attach();
  cancel();
  assert.match(q('.inspector-body .text-font-choice').textContent, /attached for this session/);

  // A wrapper from another collection member cannot be committed, even if the
  // face descriptor is identical. Cancellation of the picker restores the UI.
  const privateBeforeMismatch = snapshot();
  shape = async (_token, _index, request) => ({ source: source(0), outline: outline(request) });
  q('.inspector-body [data-change-font]').dispatchEvent(
    new win.PointerEvent('pointerdown', { bubbles: true, button: 0 }),
  );
  await until(picker, 'private picker');
  await choose(1);
  apply();
  await tick();
  assert.equal(snapshot(), privateBeforeMismatch);
  assert.match(picker().querySelector('.picker-status').textContent, /match/i);
  // Selecting the same row again after a failed preparation re-arms Apply.
  shape = async (_token, index, request) => ({ source: source(index), outline: outline(request) });
  await choose(1);
  apply();
  await until(() => !picker(), 'same-row retry');
  assert.deepEqual(live().osFont.source, source(1));

  // A late preparation cannot overwrite a newer Session revision.
  win.nect.load(serialize(doc));
  select();
  await attach();
  await choose(0);
  const pending = deferred();
  let request;
  shape = (_token, _index, r) => {
    request = r;
    return pending.promise;
  };
  apply();
  await until(() => request, 'pending shape');
  win.nect.dispatch({
    ...win.nect.commandState,
    type: 'set-text-content',
    payload: { layerId: doc.layers[0].id, nodeId: text.id, text: 'Newer edit' },
  });
  const newer = snapshot();
  pending.resolve({ source: source(0), outline: outline(request) });
  await tick();
  assert.equal(snapshot(), newer);
  assert.equal(live().osFont, undefined);
  if (picker()) cancel();
});
