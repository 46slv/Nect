import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { resolveMainRelativeImports } from './helpers/main-data-url.js';
import { pngDimensions } from '../src/raster.js';

test('PNG PPI scrubbing keeps export settings out of document history', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  globalThis.requestAnimationFrame = () => 0;
  Object.defineProperty(globalThis, 'localStorage', {
    configurable: true,
    value: win.localStorage,
  });
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
  let source = await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8');
  source = resolveMainRelativeImports(source.replace("import './style.css';", ''));
  source +=
    '\nwindow.ppiState = () => ({ppi:pngPpi,preset:selectedPngExportPreset,past:history.past.length,future:history.future.length,gesture:session.hasGesture});';
  source += '\nwindow.ppiRender = render;';
  source += '\nfor(let i=0;!bundledFont && i<500;i++) await new Promise(r=>setTimeout(r,10));';
  t.after(() => win.happyDOM.abort());
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const $ = (s) => document.querySelector(s),
    state = () => win.ppiState();
  const event = (el, type) => el.dispatchEvent(new win.Event(type, { bubbles: true }));
  const change = (selector, value) => {
    const el = $(selector);
    el.value = value;
    event(el, 'change');
  };
  const open = () => {
    const input = $('#png-ppi');
    for (let el = input.parentElement; el; el = el.parentElement)
      if (el.matches('details')) el.open = true;
    return input;
  };
  const fixture = structuredClone(win.nect.document);
  fixture.width = 100;
  fixture.height = 50;
  win.nect.load(JSON.stringify(fixture));
  change('#png-export-mode', 'ppi');
  const set = (value) => {
    const el = open();
    el.value = String(value);
    event(el, 'input');
    return el;
  };
  const fire = (el, type, dx = 0) =>
    el.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 9,
        clientX: 100 + dx,
        clientY: 20,
      }),
    );
  const begin = (dx) => {
    const el = open();
    fire(el, 'pointerdown');
    fire(document, 'pointermove', dx);
    return el;
  };
  const end = (dx = 24) => fire(document, 'pointerup', dx);
  const cancel = () =>
    document.dispatchEvent(new win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  const saved = () => JSON.stringify(win.nect.document);
  // Seed a real document Redo entry, then keep it through every local export operation.
  change('[data-dimension="width"]', '650');
  win.ppiRender();
  $('[data-action="undo"]').click();
  const original = saved(),
    before = state();
  assert.equal(before.future, 1);
  const unchanged = () => {
    assert.equal(saved(), original);
    assert.deepEqual(
      [state().past, state().future, state().gesture],
      [before.past, before.future, false],
    );
  };

  await t.test('drag, typing, dimensions and local settings reopening', () => {
    set(300);
    begin(24);
    end();
    assert.equal(state().ppi, 320);
    assert.equal(open().value, '320');
    const size = pngDimensions(win.nect.document, { mode: 'ppi', ppi: 320 });
    assert.equal($('[data-png-dimensions]').textContent, `${size.width} × ${size.height} px`);
    change('#png-export-mode', 'scale');
    change('#png-export-mode', 'ppi');
    assert.equal(open().value, '320');
    set(1200);
    begin(24);
    end();
    assert.equal(state().ppi, 1220, 'no invented 1200 PPI ceiling');
    unchanged();
  });
  await t.test('precision, no-op, cancel and exact origin preserve raw baseline', () => {
    const baseline = 300.1234567890123;
    set(baseline);
    begin(3);
    end(3);
    assert.equal(state().ppi, baseline);
    begin(24);
    cancel();
    end();
    assert.equal(state().ppi, baseline);
    begin(24);
    fire(document, 'pointermove', 0);
    end(0);
    assert.equal(state().ppi, baseline);
    assert.equal(open().value, String(baseline));
    unchanged();
  });
  await t.test('invalid typing and requests retain existing validator authority', () => {
    for (const invalid of ['', '0', '-1', '0.001', '1e300']) {
      const input = set(invalid),
        raw = state().ppi;
      assert.equal($('[data-png-dimensions]').dataset.valid, 'false');
      begin(24);
      end();
      assert.equal(state().ppi, raw);
      assert.equal(input.value, invalid);
    }
    set(300);
    begin(24);
    fire(document, 'pointermove', -1000);
    end(-1000);
    assert.equal(state().ppi, 320, 'invalid request retains the last accepted preview');
    assert.equal($('[data-png-dimensions]').dataset.valid, 'true');
    unchanged();
  });
  await t.test('menu close and disconnected control cancel without late commit', () => {
    set(300);
    const input = begin(24);
    input.closest('[data-output-settings]').open = false;
    end();
    assert.equal(state().ppi, 300);
    begin(24);
    open().remove();
    end();
    assert.equal(state().ppi, 300);
    change('#png-export-mode', 'ppi');
    open();
    unchanged();
  });
  await t.test('new typing, mode and preset supersede the old gesture', () => {
    set(300);
    begin(24);
    set(450);
    end();
    assert.equal(state().ppi, 450);
    begin(24);
    change('#png-export-mode', 'scale');
    end();
    change('#png-export-mode', 'ppi');
    assert.equal(open().value, '450');
    set(300);
    $('#png-export-preset-name').value = 'Print';
    event($('#png-export-preset-name'), 'input');
    $('[data-action="save-png-export-preset"]').click();
    open();
    assert.equal(state().preset, 'Print');
    begin(24);
    cancel();
    end();
    assert.equal(state().preset, 'Print');
    assert.equal($('#png-export-preset').value, 'Print');
    begin(24);
    fire(document, 'pointermove', 0);
    end(0);
    assert.equal(state().preset, 'Print');
    begin(24);
    change('#png-export-preset', 'Print');
    end();
    assert.equal(state().ppi, 300);
    assert.equal(state().preset, 'Print');
    unchanged();
  });
  await t.test('existing edit guard and document replacement prevent a late commit', () => {
    set(300);
    begin(24);
    change('[data-dimension="width"]', '700');
    assert.equal(win.nect.document.width, 100);
    assert.match($('.status .message').textContent, /GESTURE_ACTIVE/);
    assert.equal(state().past, before.past);
    const replacement = structuredClone(win.nect.document);
    replacement.width = 700;
    win.nect.load(JSON.stringify(replacement));
    end();
    assert.equal(win.nect.document.width, 700);
    assert.equal(state().ppi, 300);
    assert.equal(state().past, 0);
    assert.equal(state().future, 0);
  });
});
