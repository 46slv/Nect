import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createGraphPanJoystick } from '../src/graph-pan-joystick.js';
function setup(t, canPan = () => true) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const doc = win.document;
  doc.body.innerHTML =
    '<div class="body"><div class="viewport" tabindex="0"></div></div><input type="number">';
  const container = doc.querySelector('.body'),
    viewport = doc.querySelector('.viewport');
  let width = 800,
    height = 300,
    resize;
  Object.defineProperties(viewport, {
    clientWidth: { get: () => width },
    clientHeight: { get: () => height },
    offsetWidth: { get: () => width + 17 },
    offsetHeight: { get: () => height + 17 },
  });
  win.ResizeObserver = class {
    constructor(fn) {
      resize = fn;
    }
    observe() {}
    disconnect() {}
  };
  const frames = new Map();
  let nextFrame = 0;
  win.requestAnimationFrame = (fn) => {
    frames.set(++nextFrame, fn);
    return nextFrame;
  };
  win.cancelAnimationFrame = (id) => frames.delete(id);
  const moves = [];
  const controller = createGraphPanJoystick({
    container,
    viewport,
    canPan,
    pan: (delta) => moves.push(delta),
  });
  const element = controller.element;
  const captured = new Set();
  element.setPointerCapture = (id) => captured.add(id);
  element.hasPointerCapture = (id) => captured.has(id);
  element.releasePointerCapture = (id) => captured.delete(id);
  element.getBoundingClientRect = () => ({
    left: 100,
    top: 100,
    width: controller.layout.size,
    height: controller.layout.size,
  });
  const fire = (target, type, x = 140, y = 140, extra = {}) => {
    const e = new win.PointerEvent(type, {
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
    target.dispatchEvent(e);
    return e;
  };
  const key = (type, name, target = element, extra = {}) => {
    const e = new win.KeyboardEvent(type, { bubbles: true, cancelable: true, key: name, ...extra });
    target.dispatchEvent(e);
    return e;
  };
  const flush = (time) => {
    const jobs = [...frames.values()];
    frames.clear();
    jobs.forEach((fn) => fn(time));
  };
  return {
    win,
    doc,
    element,
    controller,
    frames,
    moves,
    captured,
    fire,
    key,
    flush,
    resize: (w, h) => {
      width = w;
      height = h;
      resize();
    },
  };
}
test('pointer mouse/touch continuous motion, bounded knob and idle scheduling', async (t) => {
  const s = setup(t);
  assert.equal(s.frames.size, 0);
  assert.equal(s.element.type, 'button');
  assert.equal(s.element.getAttribute('aria-label'), 'Pan node graph');
  assert.ok(s.doc.getElementById(s.element.getAttribute('aria-describedby')));
  s.fire(s.element, 'pointerdown');
  assert.equal(s.controller.active, true);
  assert.equal(s.frames.size, 0, 'neutral hold needs no render loop');
  s.fire(s.doc, 'pointermove', 1140, -860);
  assert.equal(s.frames.size, 1);
  s.flush(0);
  s.flush(1000 / 60);
  assert.ok(s.moves[0][0] > 0 && s.moves[0][1] < 0);
  const coords = s.element
    .querySelector('span')
    .style.transform.match(/-?\d+(?:\.\d+)?/g)
    .map(Number);
  assert.ok(Math.hypot(...coords) <= 18 + 1e-8);
  s.fire(s.doc, 'pointermove', 140, 140);
  assert.equal(s.frames.size, 0, 'returning to center cancels scheduled frame');
  s.fire(s.doc, 'pointerup');
  assert.equal(s.controller.active, false);
  s.fire(s.element, 'pointerdown', 140, 140, { pointerType: 'touch' });
  s.fire(s.doc, 'pointermove', 100, 160, { pointerType: 'touch' });
  s.flush(100);
  s.flush(120);
  assert.ok(s.moves.at(-1)[0] < 0 && s.moves.at(-1)[1] > 0);
  s.controller.destroy();
});
test('all stop paths recenter immediately, ignore wrong pointers and never resurrect RAF', async (t) => {
  for (const reason of [
    'pointerup',
    'pointercancel',
    'lostpointercapture',
    'blur',
    'focus',
    'hidden',
    'composition',
    'escape',
    'buttons',
    'resize',
    'destroy',
    'blocked',
  ]) {
    await t.test(reason, (t) => {
      let allowed = true;
      const s = setup(t, () => allowed);
      s.fire(s.element, 'pointerdown', 120, 120);
      s.flush(0);
      s.flush(16);
      for (const type of ['pointerup', 'pointercancel', 'lostpointercapture'])
        s.fire(s.doc, type, 0, 0, { pointerId: 8 });
      assert.equal(s.controller.active, true, 'another pointer cannot cancel this drag');
      if (reason === 'blur') s.win.dispatchEvent(new s.win.Event('blur'));
      else if (reason === 'focus') s.doc.querySelector('input').focus();
      else if (reason === 'hidden') {
        Object.defineProperty(s.doc, 'hidden', { value: true, configurable: true });
        s.doc.dispatchEvent(new s.win.Event('visibilitychange'));
      } else if (reason === 'composition')
        s.doc.dispatchEvent(new s.win.Event('compositionstart', { bubbles: true }));
      else if (reason === 'escape') s.key('keydown', 'Escape');
      else if (reason === 'buttons') s.fire(s.doc, 'pointermove', 120, 120, { buttons: 0 });
      else if (reason === 'resize') s.resize(400, 60);
      else if (reason === 'destroy') s.controller.destroy();
      else if (reason === 'blocked') {
        allowed = false;
        s.flush(32);
      } else s.fire(s.doc, reason);
      const count = s.moves.length;
      assert.equal(s.controller.active, false);
      assert.equal(s.element.querySelector('span').style.transform, 'translate(0px, 0px)');
      assert.equal(s.frames.size, 0);
      s.flush(100);
      s.flush(200);
      assert.equal(s.moves.length, count);
      assert.equal(s.captured.size, 0);
      if (reason !== 'destroy') s.controller.destroy();
    });
  }
});
test('focused held-arrow keys support diagonal pan and respect unrelated/IME inputs', async (t) => {
  const s = setup(t);
  s.element.focus();
  assert.equal(s.key('keydown', 'ArrowRight').defaultPrevented, true);
  s.key('keydown', 'ArrowUp');
  s.key('keydown', 'ArrowUp', s.element, { repeat: true });
  assert.equal(s.frames.size, 1, 'key repeat never creates a second loop');
  s.flush(0);
  s.flush(16);
  assert.ok(s.moves[0][0] > 0 && s.moves[0][1] < 0);
  s.key('keyup', 'ArrowRight');
  s.flush(32);
  assert.equal(s.moves.at(-1)[0], 0);
  s.key('keyup', 'ArrowUp');
  assert.equal(s.frames.size, 0);
  for (const extra of [
    { isComposing: true },
    { keyCode: 229 },
    { ctrlKey: true },
    { metaKey: true },
    { altKey: true },
  ]) {
    assert.equal(s.key('keydown', 'ArrowRight', s.element, extra).defaultPrevented, false);
    assert.equal(s.controller.active, false);
  }
  s.key('keydown', 'ArrowLeft');
  s.key('keydown', 'Escape');
  s.key('keydown', 'ArrowLeft', s.element, { repeat: true });
  assert.equal(s.controller.active, false, 'held-key repeats cannot restart after Escape');
  assert.equal(s.frames.size, 0);
  s.key('keyup', 'ArrowLeft');
  s.key('keydown', 'ArrowLeft');
  assert.equal(s.controller.active, true, 'a fresh key press starts again');
  s.key('keyup', 'ArrowLeft');
  const input = s.doc.querySelector('input');
  input.focus();
  assert.equal(s.key('keydown', 'ArrowRight', input).defaultPrevented, false);
  assert.equal(s.key('keydown', 'Tab', s.element).defaultPrevented, false);
  assert.equal(s.key('keydown', 'x', s.element).defaultPrevented, false);
  assert.equal(s.frames.size, 0);
  s.controller.destroy();
});
test('window capture guard prevents document handlers and editable blur before rejected press', async (t) => {
  let allowed = false;
  const s = setup(t, () => allowed);
  const input = s.doc.querySelector('input');
  input.focus();
  input.value = '123.456';
  let documentPresses = 0,
    blurs = 0,
    clicks = 0;
  s.doc.addEventListener('pointerdown', () => documentPresses++, true);
  input.addEventListener('blur', () => blurs++);
  s.doc.addEventListener('click', () => clicks++);
  assert.equal(s.fire(s.element, 'pointerdown', 120, 120).defaultPrevented, true);
  assert.equal(s.doc.activeElement, input);
  assert.equal(input.value, '123.456');
  assert.equal(documentPresses, 0);
  assert.equal(blurs, 0);
  assert.equal(s.controller.active, false);
  input.blur();
  allowed = true;
  s.fire(s.element, 'pointerdown', 120, 120);
  s.fire(s.doc, 'pointerup');
  s.element.click();
  assert.equal(clicks, 0);
  s.controller.destroy();
});

test('zero-area viewport stops, hides, then restores the control without an old loop', (t) => {
  const s = setup(t);
  s.fire(s.element, 'pointerdown', 120, 120);
  s.flush(0);
  s.flush(16);
  s.resize(400, 0);
  assert.equal(s.controller.active, false);
  assert.equal(s.element.hidden, true);
  assert.equal(s.controller.layout.size, 0);
  assert.equal(s.frames.size, 0);
  s.fire(s.element, 'pointerdown', 120, 120);
  assert.equal(s.controller.active, false, 'a collapsed body cannot restart via a stale event');
  s.resize(400, 60);
  assert.equal(s.element.hidden, false);
  assert.equal(s.controller.layout.size, 44);
  s.fire(s.element, 'pointerdown', 110, 110);
  assert.equal(s.controller.active, true);
  s.fire(s.doc, 'pointerup');
  s.controller.destroy();
});

test('explicit hide stops pointer and key pan, releases capture and stays hidden through resize', (t) => {
  const s = setup(t);
  for (const input of ['pointer', 'keyboard']) {
    s.controller.setVisible(true);
    s.element.focus();
    if (input === 'pointer') s.fire(s.element, 'pointerdown', 120, 120);
    else s.key('keydown', 'ArrowRight');
    s.flush(0);
    s.flush(16);
    assert.equal(s.controller.active, true);
    s.controller.setVisible(false);
    assert.equal(s.controller.visible, false);
    assert.equal(s.controller.active, false);
    assert.equal(s.element.hidden, true);
    assert.equal(s.element.querySelector('span').style.transform, 'translate(0px, 0px)');
    assert.equal(s.captured.size, 0);
    assert.equal(s.frames.size, 0);
    assert.equal(s.doc.activeElement, s.doc.querySelector('.viewport'));
    const count = s.moves.length;
    s.fire(s.doc, 'pointermove', 120, 120);
    s.key('keydown', 'ArrowRight', s.element, { repeat: true });
    s.flush(32);
    assert.equal(s.moves.length, count, 'no held input survives hiding');
    s.resize(400, 60);
    assert.equal(s.element.hidden, true, 'normal layout does not undo user hide');
    s.resize(400, 0);
    s.controller.setVisible(true);
    assert.equal(s.element.hidden, true, 'show never overrides zero-area safety');
    s.resize(800, 300);
    assert.equal(s.element.hidden, false);
    assert.deepEqual(s.controller.layout, { size: 80, right: 25, bottom: 25 });
    s.element.focus();
    if (input === 'pointer') s.fire(s.element, 'pointerdown', 120, 120);
    else s.key('keydown', 'ArrowRight');
    s.flush(48);
    s.flush(64);
    assert.ok(s.moves.length > count, 'fresh input works after showing');
    s.controller.stop();
  }
  s.controller.destroy();
});
