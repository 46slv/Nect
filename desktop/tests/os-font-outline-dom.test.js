import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { document as makeDoc, node, serialize } from '../src/core.js';

test('OS font picker commits geometry only and rejects cancel, pending duplicates and stale document/selection', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement']) globalThis[key] = win[key];
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
  const raw = (intent) => ({
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
  let calls = 0,
    promptCount = 0,
    finish;
  globalThis.nativeOsTest = async (intent) => {
    calls++;
    return raw(intent);
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
    const desktop=true,releaseProject=async()=>{},writeProject=async()=>null,chooseUnsaved=async()=>globalThis.testUnsavedChoice(),registerClose=async(handler)=>{globalThis.testNativeClose=handler;},saveFile=async()=>true,openFile=async()=>null,listSystemFonts=async()=>[],readSystemFont=async()=>{throw Error('Font bytes must not be requested');},listOsOutlineFonts=async()=>[{family:'DIN Alternate',styles:[0,1]}],outlineOsText=(intent)=>globalThis.nativeOsTest(intent);`,
    )
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, path) => `from '${new URL('../src/' + path.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.osTestState=()=>({past:history.past.length,selected,layer:activeLayer,pending:!!osOutlinePending}); const fontDeadline=Date.now()+5000; while(!bundledFont){if(Date.now()>fontDeadline) throw Error("Font timeout");await new Promise(r=>setTimeout(r,10));}';
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
  const setup = () => {
    const d = makeDoc(),
      n = node('Text'),
      fill = node('Fill');
    n.text = 'DIN';
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
  const open = async () => {
    $('.inspector-body [data-os-text-outline]').click();
    await until(() => $('dialog[aria-label="OS font outlines"]'));
    await new Promise((r) => setTimeout(r, 0));
    return $('dialog[aria-label="OS font outlines"]');
  };
  const saved = () => serialize(win.nect.document);
  const key = (name, extra = {}) =>
    $('.canvas-stage').dispatchEvent(
      new win.KeyboardEvent('keydown', { bubbles: true, cancelable: true, key: name, ...extra }),
    );
  let n = setup(),
    before = saved(),
    picker = await open();
  assert.match(picker.textContent, /DIN Alternate.*OS Regular/s);
  assert.equal(calls, 0);
  picker.querySelector('.picker-cancel').click();
  assert.equal(saved(), before);
  picker = await open();
  picker.querySelector('.picker-apply').click();
  await until(() => !$('dialog[aria-label="OS font outlines"]'));
  assert.equal(win.nect.document.layers[0].nodes.find((x) => x.id === n.id).type, 'Multi Path');
  assert.equal(win.osTestState().past, 1);
  assert.equal(Object.keys(win.nect.document.fontAssets || {}).length, 0);
  key('z', { ctrlKey: true });
  assert.equal(saved(), before);

  // Cancel an in-flight native request, then resolve it: no late adoption.
  n = setup();
  before = saved();
  globalThis.nativeOsTest = (intent) => {
    calls++;
    return new Promise((resolve) => {
      finish = () => resolve(raw(intent));
    });
  };
  picker = await open();
  picker.querySelector('.picker-apply').click();
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
  picker.querySelector('.picker-apply').click();
  win.nect.load(before);
  finish();
  await new Promise((r) => setTimeout(r, 20));
  assert.equal(saved(), before);

  n = setup();
  before = saved();
  picker = await open();
  picker.querySelector('.picker-apply').click();
  picker.querySelector('input[type="search"]').value = 'OS Bold';
  picker
    .querySelector('input[type="search"]')
    .dispatchEvent(new win.Event('input', { bubbles: true }));
  finish();
  await new Promise((r) => setTimeout(r, 20));
  assert.equal(saved(), before);
  assert.match(picker.querySelector('output').textContent, /Font choice changed/);
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
  globalThis.nativeOsTest = async (intent) => ({ ...raw(intent), family: 'Unmatched family' });
  picker = await open();
  picker.querySelector('.picker-apply').click();
  await until(() => /Unmatched OS font/.test(picker.querySelector('output').textContent));
  assert.equal(saved(), before);
  picker.querySelector('.picker-cancel').click();
  // Retained OS font Text edits use native preparation without replacing Text.
  n = setup();
  globalThis.nativeOsTest = async (intent) => raw(intent);
  $('.inspector-body [data-os-text-font]').click();
  await until(() => $('dialog[aria-label="OS font outlines"]'));
  await new Promise((r) => setTimeout(r, 0));
  picker = $('dialog[aria-label="OS font outlines"]');
  assert.equal(picker.querySelector('.picker-apply').textContent, 'Use font for Text');
  picker.querySelector('.picker-apply').click();
  await until(() => !$('dialog[aria-label="OS font outlines"]'));
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
  $('.inspector-body [data-clear-os-text]').click();
  assert.equal($('.inspector-body [data-clear-os-text]').textContent, 'Use DejaVu Sans');
  assert.equal(live().osFont.family, 'DIN Alternate');
  assert.equal(serialize(win.nect.document), good);
  assert.match($('.message').textContent, /OS font retained:.*Missing glyphs/);
  // Restore a geometry-only saved document even when preparation cannot run.
  win.nect.load(good);
  assert.equal(
    win.nect.document.layers[0].nodes.find((x) => x.id === n.id).osFont.family,
    'DIN Alternate',
  );
  assert.equal(win.nect.evaluate().error, null);
});
