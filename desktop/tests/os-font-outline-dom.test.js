import { openDOMPanel } from './helpers/dom-workspace-controls.js';
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, serialize } from '../src/core.js';

test('unified Font keeps native Text editable and explicit outlines reject cancel, pending duplicates and stale document/selection', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'Event', 'localStorage'])
    globalThis[key] = win[key];
  globalThis.MutationObserver = win.MutationObserver;
  globalThis.requestAnimationFrame = () => 0;
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
  globalThis.DOMPoint = class {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }
    matrixTransform() {
      return this;
    }
  };
  win.SVGElement.prototype.getScreenCTM = () => matrix;
  win.SVGSVGElement.prototype.getScreenCTM = () => matrix;
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  const face = {
    family: 'DIN Alternate',
    name: 'Regular',
    weight: 400,
    style: 0,
    stretch: 5,
    format: 1,
    color: false,
  };
  const boldFace = { ...face, name: 'Bold', weight: 700 };
  // Exact-face synthetic DirectWrite snapshots prove routing, not real font fidelity.
  const raw = (intent) => ({
    ...intent,
    engine: 'directwrite-v1',
    ascent: intent.size * 0.8,
    advanceWidth: intent.size * 2,
    lineCount: intent.text.split('\n').length,
    glyphCount: [...intent.text].length,
    points: [
      [0, 0],
      [10, 0],
      [10, 10],
      [0, 10],
    ],
    types: [0, 1, 1, 129],
  });
  let calls = 0,
    promptCount = 0,
    finish;
  globalThis.osTestFaces = [face, boldFace];
  globalThis.nativeOsTest = async (intent) => raw(intent);
  globalThis.osTestShape = (intent) => {
    calls++;
    return globalThis.nativeOsTest(intent);
  };
  globalThis.legacyOsTest = async () => {
    throw Error('Legacy family-name fallback must not run');
  };
  globalThis.testUnsavedChoice = () => {
    promptCount++;
    return 'cancel';
  };
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = source
    .replace("import './style.css';", '')
    .replace(
      /import \{[^}]*\} from '.\/storage.js';/,
      `import {DocumentLifecycle} from '${new URL('../src/document-lifecycle.js', import.meta.url)}';
    const beginRecovery=async()=> 'test-recovery',writeRecovery=async()=>Date.now(),listRecovery=async()=>[],readRecovery=async()=>{throw Error('No recovery fixture');},acknowledgeRecovery=async()=>{};
    const desktop=true,releaseProject=async()=>{},writeProject=async()=>null,chooseUnsaved=async()=>globalThis.testUnsavedChoice(),registerClose=async(handler)=>{globalThis.testNativeClose=handler;},saveFile=async()=>true,openFile=async()=>null,listSystemFonts=async()=>[],readSystemFont=async()=>{throw Error('Font bytes must not be requested');},listOsOutlineFonts=async()=>[],listDesktopFontFaces=async()=>globalThis.osTestFaces,listDesktopFontAxes=async()=>[],shapeDesktopText=(intent)=>globalThis.osTestShape(intent),outlineOsText=(intent)=>globalThis.legacyOsTest(intent);`,
    )
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.osTestState=()=>({past:history.past.length,selected,layer:activeLayer,picker:!!editorPicker}); const fontDeadline=Date.now()+5000; while(!bundledFont){if(Date.now()>fontDeadline) throw Error("Font timeout");await new Promise(r=>setTimeout(r,10));}';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  t.after(() => win.happyDOM.abort());
  const $ = (s) => document.querySelector(s);
  const until = async (check) => {
    const end = Date.now() + 5000;
    while (!check()) {
      assert.ok(Date.now() < end, 'UI operation timed out');
      await new Promise((r) => setTimeout(r, 10));
    }
  };
  // The initial new document is intentionally unsaved. Close waits for the
  // existing user choice; a three-second process wait alone is not an app hang.
  assert.equal(await globalThis.testNativeClose(), false);
  assert.equal(promptCount, 1);
  const setup = (legacyOutline) => {
    const d = makeDoc(),
      n = node('Text'),
      fill = node('Fill');
    n.text = legacyOutline?.text ?? 'DIN';
    if (legacyOutline) {
      d.schemaVersion = Math.max(d.schemaVersion, 28);
      n.params.size.value = legacyOutline.size;
      n.osFont = {
        version: 1,
        family: legacyOutline.family,
        style: legacyOutline.style,
        outline: legacyOutline,
      };
    }
    fill.params.path.source = n.id;
    d.layers[0].nodes.push(n, fill);
    d.layers[0].nodes.find((x) => x.id === d.layers[0].output).params.path.source = fill.id;
    win.nect.load(serialize(d));
    const head = $(`.node[data-id="${n.id}"] .node-head`);
    head.dispatchEvent(
      new win.PointerEvent('pointerdown', { bubbles: true, pointerId: 1, button: 0 }),
    );
    document.dispatchEvent(
      new win.PointerEvent('pointerup', { bubbles: true, pointerId: 1, button: 0 }),
    );
    return n;
  };
  const search = async (picker, query) => {
    const input = picker.querySelector('input[type="search"]');
    input.value = query;
    input.dispatchEvent(new win.Event('input', { bubbles: true }));
    await new Promise((r) => setTimeout(r, 0));
    const row = picker.querySelector('.picker-option[aria-disabled="false"]');
    assert.ok(row, `enabled exact font choice for ${query}`);
    row.click();
    await new Promise((r) => setTimeout(r, 0));
    return row;
  };
  const open = async () => {
    openDOMPanel(document, 'inspector');
    $('.inspector-body [data-change-font]').click();
    await until(() => $('dialog[aria-label="Font"]'));
    const picker = $('dialog[aria-label="Font"]');
    await search(picker, 'DIN Alternate Regular');
    return picker;
  };
  const saved = () => serialize(win.nect.document);
  const key = (name, extra = {}) =>
    $('.canvas-stage').dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name, ...extra }),
    );
  let n = setup(),
    before = saved(),
    picker = await open();
  assert.match(picker.textContent, /DIN Alternate.*Regular.*400/s);
  assert.equal(picker.querySelector('.picker-apply').textContent, 'Apply font');
  assert.equal($('.inspector-body [data-os-text-outline]'), null);
  assert.equal($('.inspector-body [data-os-text-font]'), null);
  assert.equal(calls, 0);
  picker.querySelector('.picker-cancel').click();
  assert.equal(saved(), before);
  picker = await open();
  picker.querySelector('.picker-apply').click();
  await until(() => !$('dialog[aria-label="Font"]'));
  assert.equal(win.nect.document.layers[0].nodes.find((x) => x.id === n.id).type, 'Text');
  assert.deepEqual(win.nect.document.layers[0].nodes.find((x) => x.id === n.id).osFont.face, face);
  assert.equal(win.osTestState().past, 1);
  assert.equal(Object.keys(win.nect.document.fontAssets || {}).length, 0);
  const native = saved(),
    nativeSvg = win.nect.export(),
    nativeCalls = calls;
  const actions = $('.inspector-body .text-actions');
  assert.ok(actions, 'outline conversion is an explicit Text action');
  actions.open = true;
  actions.querySelector('[data-bake-text-outline]').click();
  assert.equal(win.nect.document.layers[0].nodes.find((x) => x.id === n.id).type, 'Multi Path');
  assert.equal(win.osTestState().past, 2);
  assert.equal(calls, nativeCalls, 'current-appearance conversion does not prepare a new font');
  assert.equal(win.nect.export(), nativeSvg);
  key('z', { ctrlKey: true });
  assert.equal(saved(), native, 'Undo conversion restores the exact saved native Text');
  key('z', { ctrlKey: true });
  assert.equal(saved(), before, 'Undo font selection restores the original static Text');

  // Cancel an in-flight native request, then resolve it: no late adoption.
  n = setup();
  before = saved();
  globalThis.nativeOsTest = (intent) => {
    return new Promise((resolve) => {
      finish = () => resolve(raw(intent));
    });
  };
  picker = await open();
  finish = undefined;
  picker.querySelector('.picker-apply').click();
  await until(() => typeof finish === 'function');
  picker.querySelector('.picker-apply').click();
  const pendingCalls = calls;
  picker.querySelector('.picker-cancel').click();
  finish();
  await new Promise((r) => setTimeout(r, 20));
  assert.equal(calls, pendingCalls);
  assert.equal(saved(), before);

  // A new document invalidates the owner/revision even when IDs are reused.
  n = setup();
  before = saved();
  picker = await open();
  finish = undefined;
  picker.querySelector('.picker-apply').click();
  await until(() => typeof finish === 'function');
  win.nect.load(before);
  finish();
  await new Promise((r) => setTimeout(r, 20));
  assert.equal(saved(), before);

  n = setup();
  before = saved();
  picker = await open();
  finish = undefined;
  picker.querySelector('.picker-apply').click();
  await until(() => typeof finish === 'function');
  await search(picker, 'DIN Alternate Bold');
  finish();
  await new Promise((r) => setTimeout(r, 20));
  assert.equal(saved(), before, 'an older completion cannot commit after the exact choice changes');
  assert.equal(win.osTestState().past, 0);
  assert.match(
    picker.querySelector('.picker-option[aria-selected="true"]').textContent,
    /Bold.*700/,
  );
  assert.equal(picker.querySelector('.picker-apply').disabled, false);
  picker.querySelector('.picker-cancel').click();

  n = setup();
  before = saved();
  globalThis.nativeOsTest = async () => {
    throw Error('Missing glyphs refused');
  };
  picker = await open();
  picker.querySelector('.picker-apply').click();
  await until(() => /Missing glyphs/.test(picker.querySelector('output').textContent));
  assert.equal(saved(), before);
  picker.querySelector('.picker-cancel').click();
  n = setup();
  before = saved();
  globalThis.nativeOsTest = async (intent) => ({
    ...raw(intent),
    face: { ...intent.face, family: 'Unmatched family' },
  });
  picker = await open();
  picker.querySelector('.picker-apply').click();
  await until(() =>
    /did not match the selected face/.test(picker.querySelector('output').textContent),
  );
  assert.equal(saved(), before);
  picker.querySelector('.picker-cancel').click();
  // Retained OS font Text edits use native preparation without replacing Text.
  n = setup();
  globalThis.nativeOsTest = async (intent) => raw(intent);
  picker = await open();
  assert.equal(picker.querySelector('.picker-apply').textContent, 'Apply font');
  picker.querySelector('.picker-apply').click();
  await until(() => !$('dialog[aria-label="Font"]'));
  let live = () => win.nect.document.layers[0].nodes.find((x) => x.id === n.id);
  assert.equal(live().type, 'Text');
  assert.equal(live().osFont.family, 'DIN Alternate');
  const retained = serialize(win.nect.document);
  let editor = $('.inspector-body [data-os-text]');
  const beforeIme = calls;
  editor.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
  editor.value = '日本語';
  editor.dispatchEvent(new win.InputEvent('input', { bubbles: true, isComposing: true }));
  editor.dispatchEvent(new win.Event('change', { bubbles: true }));
  await new Promise((r) => setTimeout(r, 10));
  assert.equal(calls, beforeIme);
  assert.equal(live().text, 'DIN');
  editor.dispatchEvent(
    new win.CompositionEvent('compositionend', { bubbles: true, data: '日本語' }),
  );
  editor.dispatchEvent(new win.Event('change', { bubbles: true }));
  await until(() => live().text === '日本語');
  assert.equal(live().type, 'Text');
  assert.equal(live().osFont.family, 'DIN Alternate');
  key('z', { ctrlKey: true });
  assert.equal(serialize(win.nect.document), retained);
  key('z', { ctrlKey: true, shiftKey: true });
  assert.equal(live().text, '日本語');
  const good = serialize(win.nect.document);
  globalThis.nativeOsTest = async () => {
    throw Error('Requested OS font is no longer active');
  };
  editor = $('.inspector-body [data-os-text]');
  editor.value = '漢字';
  editor.dispatchEvent(new win.InputEvent('input', { bubbles: true }));
  editor.dispatchEvent(new win.Event('change', { bubbles: true }));
  await until(() => /no longer active/.test($('.message').textContent));
  assert.equal(serialize(win.nect.document), good);
  assert.equal($('.inspector-body [data-os-text]').value, '漢字');
  assert.equal(await globalThis.testNativeClose(), false);
  $('.inspector-body [data-cancel-os-text]').click();
  assert.equal($('.inspector-body [data-os-text]').value, '日本語');
  assert.equal($('.inspector-body [data-clear-os-text]'), null, 'no default-font bypass remains');
  picker = await open();
  await search(picker, 'DejaVu Sans');
  picker.querySelector('.picker-apply').click();
  await until(() =>
    /OS font retained:.*Missing glyphs/s.test(picker.querySelector('output').textContent),
  );
  assert.equal(live().osFont.family, 'DIN Alternate');
  assert.equal(serialize(win.nect.document), good);
  assert.match(picker.querySelector('output').textContent, /OS font retained:.*Missing glyphs/s);
  picker.querySelector('.picker-cancel').click();
  // Restore a geometry-only saved document even when preparation cannot run.
  win.nect.load(good);
  assert.equal(
    win.nect.document.layers[0].nodes.find((x) => x.id === n.id).osFont.family,
    'DIN Alternate',
  );
  assert.equal(win.nect.evaluate().error, null);

  // Existing saved version-1 GDI Text still has its own edit preparation path.
  // It is not a new Font choice and never uses modern face preparation.
  const legacyRaw = (intent) => ({
    ...intent,
    ascent: 80,
    points: [
      [0, 0],
      [10, 0],
      [10, 10],
      [0, 10],
    ],
    types: [0, 1, 1, 161],
  });
  n = setup(legacyRaw({ family: 'Saved legacy GDI fixture', style: 0, text: 'DIN', size: 100 }));
  const legacyBefore = saved(),
    modernCalls = calls;
  let legacyCalls = 0,
    legacyPrepare = async (intent) => legacyRaw(intent);
  globalThis.legacyOsTest = (intent) => {
    legacyCalls++;
    return legacyPrepare(intent);
  };
  assert.equal(live().osFont.version, 1);
  editor = $('.inspector-body [data-os-text]');
  editor.dispatchEvent(new win.CompositionEvent('compositionstart', { bubbles: true }));
  editor.value = '日本語';
  editor.dispatchEvent(new win.InputEvent('input', { bubbles: true, isComposing: true }));
  editor.dispatchEvent(new win.Event('change', { bubbles: true }));
  await new Promise((r) => setTimeout(r, 10));
  assert.equal(legacyCalls, 0, 'legacy preparation waits for IME composition to finish');
  assert.equal(live().text, 'DIN');
  editor.dispatchEvent(
    new win.CompositionEvent('compositionend', { bubbles: true, data: '日本語' }),
  );
  editor.dispatchEvent(new win.Event('change', { bubbles: true }));
  await until(() => live().text === '日本語');
  assert.equal(legacyCalls, 1);
  assert.equal(live().type, 'Text');
  assert.equal(live().osFont.version, 1);
  assert.equal(live().osFont.family, 'Saved legacy GDI fixture');
  const legacyEdited = saved();
  key('z', { ctrlKey: true });
  assert.equal(saved(), legacyBefore);
  key('z', { ctrlKey: true, shiftKey: true });
  assert.equal(saved(), legacyEdited);
  legacyPrepare = async () => {
    throw Error('Saved legacy font is no longer active');
  };
  editor = $('.inspector-body [data-os-text]');
  editor.value = '漢字';
  editor.dispatchEvent(new win.InputEvent('input', { bubbles: true }));
  editor.dispatchEvent(new win.Event('change', { bubbles: true }));
  await until(() => /Saved legacy font is no longer active/.test($('.message').textContent));
  assert.equal(legacyCalls, 2);
  assert.equal(saved(), legacyEdited, 'failed legacy preparation preserves the saved geometry');
  assert.equal($('.inspector-body [data-os-text]').value, '漢字');
  assert.equal(await globalThis.testNativeClose(), false);
  $('.inspector-body [data-cancel-os-text]').click();
  assert.equal($('.inspector-body [data-os-text]').value, '日本語');
  assert.equal(saved(), legacyEdited);
  assert.equal(win.osTestState().past, 1);
  assert.equal(calls, modernCalls, 'saved legacy edits never invoke modern face preparation');
});
