import test from 'node:test';
import assert from 'node:assert/strict';
import { createColorRangeEdit } from '../src/color-range-edit.js';

function fixture() {
  const listeners = new Map(),
    windowListeners = new Map();
  let value = 0.62,
    commits = 0,
    cancels = 0,
    renders = 0;
  const input = {
    value: '0.62',
    closest() {
      return this;
    },
  };
  const app = {
    addEventListener: (type, listener) => listeners.set(type, listener),
    ownerDocument: {
      defaultView: {
        addEventListener: (type, listener) => windowListeners.set(type, listener),
      },
    },
  };
  const controller = createColorRangeEdit({
    app,
    selector: 'input',
    snapshot: () => value,
    restore: (before) => {
      value = before;
    },
    onCommit: () => commits++,
    onCancel: () => cancels++,
    render: () => renders++,
  });
  return {
    controller,
    fire(type, init = {}) {
      listeners.get(type)?.({
        target: input,
        button: 0,
        isPrimary: true,
        pointerId: 1,
        preventDefault() {},
        stopImmediatePropagation() {},
        ...init,
      });
    },
    blur: () => windowListeners.get('blur')(),
    set value(next) {
      value = next;
      input.value = String(next);
    },
    get value() {
      return value;
    },
    get commits() {
      return commits;
    },
    get cancels() {
      return cancels;
    },
    get renders() {
      return renders;
    },
  };
}

test('native range pointer movement finishes as one edit', () => {
  const f = fixture();
  f.fire('pointerdown');
  for (const value of [0.3, 0.4, 0.5]) f.value = value;
  assert.equal(f.controller.active, true);
  assert.equal(f.commits, 0);
  f.fire('pointerup');
  assert.equal(f.value, 0.5);
  assert.equal(f.commits, 1);
  assert.equal(f.controller.active, false);
});

for (const interruption of ['pointercancel', 'lostpointercapture', 'Escape', 'blur'])
  test(`native range ${interruption} restores its baseline without a commit`, () => {
    const f = fixture();
    f.fire('pointerdown');
    f.value = 0.2;
    if (interruption === 'Escape') f.fire('keydown', { key: 'Escape' });
    else if (interruption === 'blur') f.blur();
    else f.fire(interruption);
    f.fire('pointerup');
    assert.equal(f.value, 0.62);
    assert.equal(f.commits, 0);
    assert.equal(f.cancels, 1);
    assert.equal(f.controller.active, false);
  });

test('native range keyboard repeats share a baseline until keyup', () => {
  const f = fixture();
  f.fire('keydown', { key: 'ArrowRight' });
  f.value = 0.63;
  f.fire('keydown', { key: 'ArrowRight', repeat: true });
  f.value = 0.64;
  assert.equal(f.commits, 0);
  f.fire('keyup', { key: 'ArrowRight' });
  assert.equal(f.commits, 1);
  assert.equal(f.value, 0.64);
});

test('Tab settles a held keyboard range edit before focus navigation', () => {
  const f = fixture();
  f.fire('keydown', { key: 'ArrowRight' });
  f.value = 0.63;
  f.fire('keydown', { key: 'Tab' });
  assert.equal(f.commits, 1);
  assert.equal(f.controller.active, false);
});

test('another pointer cannot finish an active native range edit', () => {
  const f = fixture();
  f.fire('pointerdown');
  f.value = 0.3;
  f.fire('pointerup', { pointerId: 9 });
  assert.equal(f.commits, 0);
  assert.equal(f.controller.active, true);
  f.fire('pointerup');
  assert.equal(f.commits, 1);
});


test('native range samples can restore their fixed gesture baseline before applying the next value', () => {
  const f = fixture();
  f.fire('pointerdown');
  f.value = 0.3;
  f.controller.restoreBaseline();
  assert.equal(f.value, 0.62);
  f.value = 0.5;
  f.fire('pointerup');
  assert.equal(f.value, 0.5);
  assert.equal(f.commits, 1);
});
