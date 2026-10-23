import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { Window } from 'happy-dom';
import { penMenuMutationObserver } from './helpers/pen-menu-mutation-observer.js';
import { document as makeDoc, node, serialize } from '../src/core.js';

test('shipped joystick preserves exact document/history and isolates editing through real event paths', async (t) => {
  const win = new Window({ url: 'http://localhost:5173' });
  t.after(() => win.happyDOM.abort());
  // Retain Happy DOM's weak delivery callbacks just as the Pen/menu fixture does.
  // Keep the real production observer responsible for building the menu.
  win.MutationObserver = penMenuMutationObserver(win.MutationObserver);
  win.document.body.innerHTML = '<div id="app"></div>';
  for (const key of ['window', 'document', 'HTMLElement', 'MutationObserver'])
    globalThis[key] = win[key];
  const frames = new Map();
  let seq = 0;
  globalThis.requestAnimationFrame = win.requestAnimationFrame = (fn) => {
    frames.set(++seq, fn);
    return seq;
  };
  win.cancelAnimationFrame = (id) => frames.delete(id);
  const flush = (time = 0) => {
    const jobs = [...frames.values()];
    frames.clear();
    jobs.forEach((fn) => fn(time));
  };
  for (const [key, read] of Object.entries({
    offsetLeft: (el) => parseFloat(el.style.left) || 0,
    offsetTop: (el) => parseFloat(el.style.top) || 0,
    offsetWidth: (el) => (el.matches('.graph-scroll') ? 817 : parseFloat(el.style.width) || 264),
    offsetHeight: (el) => (el.matches('.graph-scroll') ? 317 : parseFloat(el.style.height) || 180),
    clientWidth: () => 800,
    clientHeight: () => 300,
    scrollWidth: () => 5000,
    scrollHeight: () => 5000,
  }))
    Object.defineProperty(win.HTMLElement.prototype, key, {
      configurable: true,
      get() {
        return read(this);
      },
    });
  const $ = (s) => win.document.querySelector(s);
  win.HTMLElement.prototype.getBoundingClientRect = function () {
    if (this.matches('.graph-pan-joystick'))
      return { left: 600, top: 200, width: 80, height: 80, right: 680, bottom: 280 };
    const n = this.closest('.node');
    const x = n ? parseFloat(n.style.left) + (this.matches('.output') ? 244 : 0) : 0,
      y = n ? parseFloat(n.style.top) + 30 : 0;
    return { x, y, left: x, top: y, width: 800, height: 300, right: x + 800, bottom: y + 300 };
  };
  win.SVGElement.prototype.getScreenCTM = () => ({
    inverse() {
      return this;
    },
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
  globalThis.fetch = async () => ({
    ok: true,
    arrayBuffer: async () => {
      const b = await fs.readFile(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  let source = (await fs.readFile(new URL('../src/main.js', import.meta.url), 'utf8'))
    .replace("import './style.css';", '')
    .replace(
      /from '(\.\/[^']+)'/g,
      (_, p) => `from '${new URL('../src/' + p.slice(2), import.meta.url)}'`,
    );
  source +=
    '\nwindow.joystickUI = () => ({ past: history.past, future: history.future, revision: history.revision, selected, selection: [...selection], artworkTarget, artworkSelection: [...artworkSelection], tool, space, drag: drag?.type, wiring: !!wiring }); window.joystickRefresh = updateNavigation; window.joystickRender = render; window.joystickActive = () => graphPanJoystick.active;';
  source +=
    '\nconst deadline = Date.now() + 5000; while (!bundledFont) { if (Date.now() > deadline) throw Error("Font unavailable"); await new Promise(r => setTimeout(r, 10)); }';
  await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
  const doc = makeDoc(),
    a = node('Rectangle', 100, 100),
    out = node('Output', 1200, 900);
  out.params.path.source = a.id;
  doc.layers[0].nodes = [a, out];
  doc.layers[0].output = out.id;
  win.nect.load(serialize(doc));
  const click = (action) => $(`[data-action="${action}"]`).click();
  const fire = (el, type, x = 640, y = 240, extra = {}) => {
    const event = new win.PointerEvent(type, {
      bubbles: true,
      cancelable: true,
      pointerId: 7,
      pointerType: 'mouse',
      button: 0,
      buttons: 1,
      clientX: x,
      clientY: y,
      ...extra,
    });
    el.dispatchEvent(event);
    return event;
  };
  const key = (target, type, name, extra = {}) => {
    const event = new win.KeyboardEvent(type, {
      bubbles: true,
      cancelable: true,
      key: name,
      ...extra,
    });
    target.dispatchEvent(event);
    return event;
  };
  // Select via normal graph click, author two normal numeric edits, then undo
  // one to retain both a nonempty past and a nonempty redo branch.
  fire($(`.node[data-id="${a.id}"] .node-head`), 'pointerdown', 100, 100);
  fire(document, 'pointerup', 100, 100);
  const parameter = () =>
    $(`.inspector .field[data-node="${a.id}"][data-key="size"] input[type="number"]`);
  for (const value of ['220', '240']) {
    const input = parameter();
    input.value = value;
    input.dispatchEvent(new win.Event('change', { bubbles: true }));
    await new Promise((resolve) => setTimeout(resolve, 0));
  }
  click('undo');
  await new Promise((resolve) => setTimeout(resolve, 0));
  assert.equal(win.joystickUI().past.length, 1);
  assert.equal(win.joystickUI().future.length, 1);
  assert.equal(win.nect.fileState.dirty, true);
  const viewport = $('.graph-scroll');
  for (const axis of ['scrollLeft', 'scrollTop'])
    Object.defineProperty(viewport, axis, {
      configurable: true,
      get() {
        return this[`_${axis}`] || 0;
      },
      set(value) {
        this[`_${axis}`] = Math.max(0, Math.round(value));
      },
    });
  const saved = () =>
    JSON.stringify({
      project: win.nect.projectSnapshot(),
      doc: win.nect.document,
      svg: win.nect.export(),
      file: win.nect.fileState,
      command: win.nect.commandState,
      ui: win.joystickUI(),
    });
  const baseline = saved();
  const invariant = () => assert.equal(saved(), baseline);
  const stick = () => $('.graph-pan-joystick');
  assert.equal(stick().parentElement, $('.graph-body'));
  assert.equal($('.graph-scroll').contains(stick()), false);
  assert.equal($('.graph-pan-pad'), null);
  flush();
  flush();
  assert.equal(frames.size, 0, 'startup settles without joystick idle loop');
  document.activeElement?.blur();
  const element = stick();
  for (const [dx, dy] of [
    [18, 0],
    [-18, 0],
    [0, -18],
    [0, 18],
    [18, -18],
  ]) {
    viewport.scrollLeft = viewport.scrollTop = 500;
    fire(element, 'pointerdown');
    fire(document, 'pointermove', 640 + dx, 240 + dy);
    flush(0);
    flush(20);
    assert.equal(Math.sign(500 - viewport.scrollLeft), Math.sign(dx));
    assert.equal(Math.sign(500 - viewport.scrollTop), Math.sign(dy));
    win.joystickRefresh();
    assert.equal(stick(), element, 'scroll/navigation refresh never replaces active control');
    fire(document, 'pointerup');
    element.click();
    assert.equal(stick(), element, 'ending click does not close panels or rerender');
    invariant();
    assert.equal(frames.size, 0);
  }
  // Nonnegative origin is the same as wheel/direct pan: right/down content
  // travel clamps without moving nodes to invent room above/left of the origin.
  viewport.scrollLeft = viewport.scrollTop = 0;
  fire(element, 'pointerdown', 658, 258);
  flush(0);
  flush(20);
  fire(document, 'pointerup');
  assert.deepEqual([viewport.scrollLeft, viewport.scrollTop], [0, 0]);
  invariant();
  // Fractional intent accumulates despite rounded browser scroll offsets.
  viewport.scrollLeft = 500;
  fire(element, 'pointerdown', 643, 240);
  flush(0);
  for (let i = 1; i <= 60; i++) flush((i * 1000) / 60);
  fire(document, 'pointerup');
  assert.ok(
    viewport.scrollLeft >= 467 && viewport.scrollLeft <= 469,
    'low-speed movement is not rounded away each frame',
  );
  invariant();
  // A real input draft stays focused, byte-exact, and uncommitted. Window
  // capture rejects before existing document capture and native focus defaults.
  const input = parameter();
  input.focus();
  input.value = '321.456';
  let blurs = 0;
  input.addEventListener('blur', () => blurs++);
  assert.equal(fire(element, 'pointerdown', 620, 240).defaultPrevented, true);
  assert.equal(win.joystickActive(), false);
  assert.equal(blurs, 0);
  assert.equal(document.activeElement, input);
  assert.equal(input.value, '321.456');
  invariant();
  assert.equal(key(input, 'keydown', 'ArrowRight').defaultPrevented, false);
  invariant();
  assert.equal(key(input, 'keydown', 'ArrowRight', { isComposing: true }).defaultPrevented, false);
  invariant();
  input.blur();
  // Native command menu and a document-capture context menu cannot be
  // dismissed first and then accidentally grant joystick movement.
  await win.happyDOM.waitUntilComplete();
  const menu = $('.command-menu');
  assert.ok(menu, 'The production observer builds menus before the joystick guard scenario');
  assert.deepEqual(
    [...document.querySelectorAll('.command-menu > summary')].map((summary) => summary.textContent),
    ['Project', 'Import', 'Export'],
  );
  menu.open = true;
  fire(element, 'pointerdown', 620, 240);
  assert.equal(win.joystickActive(), false);
  assert.equal(menu.open, true);
  menu.open = false;
  const context = document.createElement('div');
  context.className = 'node-context-menu';
  document.body.append(context);
  fire(element, 'pointerdown', 620, 240);
  assert.equal(win.joystickActive(), false);
  context.remove();
  const dialog = document.createElement('dialog');
  dialog.open = true;
  document.body.append(dialog);
  fire(element, 'pointerdown', 620, 240);
  assert.equal(win.joystickActive(), false);
  dialog.remove();
  invariant();
  // Escape is consumed at the control before global tool/selection handlers.
  element.focus();
  key(element, 'keydown', 'ArrowLeft');
  flush(0);
  flush(20);
  key(element, 'keydown', 'Escape');
  flush(40);
  assert.equal(win.joystickActive(), false);
  invariant();
  key(element, 'keydown', ' ');
  assert.equal(win.joystickUI().space, false);
  invariant();
  for (const name of ['Delete', 'Backspace'])
    for (const modifier of ['ctrlKey', 'metaKey', 'altKey']) {
      assert.equal(key(element, 'keydown', name, { [modifier]: true }).defaultPrevented, true);
      invariant();
    }
  // The stick remains a graph context after pointer focus, including the
  // existing zoom/fit shortcuts, not a dead-end keyboard island.
  key(element, 'keydown', '+');
  assert.equal($('[data-zoom="graph"]').textContent, '120%');
  key(element, 'keydown', '1', { ctrlKey: true });
  assert.equal($('[data-zoom="graph"]').textContent, '100%');
  key(element, 'keydown', '1', { shiftKey: true, code: 'Digit1' });
  key(element, 'keydown', '2', { shiftKey: true, code: 'Digit2' });
  key(element, 'keydown', '1', { ctrlKey: true });
  invariant();
  // Structural rendering disposes the controller, cancels its RAF and recenters
  // the detached knob; load must not allow an old control to move the new doc.
  fire(element, 'pointerdown', 620, 240);
  flush(0);
  flush(20);
  win.joystickRender();
  flush(40);
  flush(60);
  assert.equal(element.isConnected, false);
  assert.equal(element.querySelector('span').style.transform, 'translate(0px, 0px)');
  assert.equal(win.joystickActive(), false);
  invariant();
  const newStick = stick();
  fire(newStick, 'pointerdown', 620, 240);
  flush(0);
  win.nect.load(serialize(doc));
  flush(20);
  flush(40);
  assert.equal(newStick.isConnected, false);
  assert.equal(win.joystickActive(), false);
  assert.equal(serialize(win.nect.document), serialize(doc));
  assert.equal(frames.size, 0, 'no old joystick loop survives document replacement');
  stick().focus();
  assert.equal(key(stick(), 'keydown', 'a', { ctrlKey: true }).defaultPrevented, true);
  assert.equal(win.joystickUI().selection.length, 2);
  assert.equal(key(stick(), 'keydown', 'c', { ctrlKey: true }).defaultPrevented, true);
  assert.equal(serialize(win.nect.document), serialize(doc));
});
