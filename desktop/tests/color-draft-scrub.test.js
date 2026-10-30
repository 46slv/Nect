import test from 'node:test';
import assert from 'node:assert/strict';
import { createColorDraftScrub } from '../src/color-draft-scrub.js';

// A small event/capture harness; actual rendered fields are covered by DOM/Chrome tests.
function fixture() {
  const listeners = new Map(),
    blur = new Map();
  let value = 0.5,
    input,
    captured = null,
    edits = 0,
    renders = 0;
  const state = { sessionId: 'a', expectedRevision: 3 };
  const app = {
    ownerDocument: { defaultView: { Event, addEventListener: (key, fn) => blur.set(key, fn) } },
    addEventListener(key, fn) {
      listeners.set(key, fn);
    },
    querySelectorAll() {
      return [input];
    },
    setPointerCapture(id) {
      captured = id;
    },
    hasPointerCapture(id) {
      return captured === id;
    },
    releasePointerCapture() {
      captured = null;
    },
  };
  function render() {
    renders++;
    input = {
      value: String(value),
      defaultValue: String(value),
      min: '0',
      max: '1',
      dataset: { colorScrubStep: '0.01' },
      getAttribute: () => 'Colorfulness',
      closest() {
        return this;
      },
      getBoundingClientRect: () => ({ width: 100, right: 100 }),
      dispatchEvent() {
        value = Number(this.value);
        render();
      },
    };
  }
  render();
  const control = createColorDraftScrub({
    app,
    selector: 'input',
    envelope: () => ({ ...state }),
    getState: () => state,
    snapshot: () => value,
    restore: (before) => {
      value = before;
    },
    update: (_el, next) => {
      value = next;
    },
    onEdit: () => {
      edits++;
    },
    render,
  });
  function fire(type, init = {}) {
    const event = {
      target: input,
      pointerId: 1,
      button: 0,
      isPrimary: true,
      clientX: 20,
      preventDefault() {
        this.prevented = true;
      },
      stopImmediatePropagation() {},
      ...init,
    };
    listeners.get(type)?.(event);
    return event;
  }
  return {
    fire,
    state,
    control,
    blur: () => blur.get('blur')(),
    get value() {
      return value;
    },
    get input() {
      return input;
    },
    get captured() {
      return captured;
    },
    get edits() {
      return edits;
    },
    get renders() {
      return renders;
    },
  };
}

test('color scrubs use a four-pixel dead zone, fixed baseline and bounded draft values', () => {
  const f = fixture();
  const first = f.input;
  f.fire('pointerdown');
  assert.equal(f.captured, 1);
  f.fire('pointermove', { clientX: 24 });
  assert.equal(f.value, 0.5);
  assert.equal(f.edits, 0);
  f.fire('pointermove', { clientX: 34 });
  assert.equal(f.value, 0.6);
  assert.notEqual(f.input, first, 'app capture survives input replacement');
  f.fire('pointermove', { clientX: 44 });
  assert.equal(f.value, 0.7, 'not a delta from the previous move');
  f.fire('pointermove', { clientX: 999 });
  assert.equal(f.value, 1);
  f.fire('pointermove', { clientX: 20 });
  assert.equal(f.value, 0.5);
  f.fire('pointerup');
  assert.equal(f.captured, null);
});

for (const interruption of ['pointercancel', 'lostpointercapture', 'Escape', 'blur'])
  test(`color scrub ${interruption} restores the exact baseline and ignores late moves`, () => {
    const f = fixture();
    f.fire('pointerdown');
    f.fire('pointermove', { clientX: 34 });
    if (interruption === 'Escape') f.fire('keydown', { key: 'Escape' });
    else if (interruption === 'blur') f.blur();
    else f.fire(interruption);
    assert.equal(f.value, 0.5);
    assert.equal(f.input.value, '0.5');
    assert.equal(f.captured, null);
    f.fire('pointermove', { clientX: 99 });
    f.fire('pointerup');
    assert.equal(f.value, 0.5);
  });

test('wrong pointers cannot move or complete an active color scrub', () => {
  const f = fixture();
  f.fire('pointerdown');
  f.fire('pointermove', { clientX: 44, pointerId: 9 });
  f.fire('pointerup', { pointerId: 9 });
  assert.equal(f.value, 0.5);
  assert.equal(f.captured, 1);
  f.fire('pointermove', { clientX: 34 });
  f.fire('pointerup');
  assert.equal(f.value, 0.6);
});

test('native spinner, non-primary and disabled controls retain native input behavior', () => {
  for (const init of [
    { clientX: 90 },
    { button: 2 },
    { isPrimary: false },
    { disabled: true },
    { readOnly: true },
  ]) {
    const f = fixture();
    Object.assign(f.input, init);
    f.fire('pointerdown', init);
    f.fire('pointermove', { clientX: 44 });
    assert.equal(f.value, 0.5);
    assert.equal(f.captured, null);
  }
});

test('pending typed value settles before the same press starts a new draft scrub', () => {
  const f = fixture();
  f.input.value = '0.2';
  f.fire('pointerdown');
  f.fire('pointermove', { clientX: 34 });
  assert.equal(f.value, 0.3);
  f.fire('pointercancel');
  assert.equal(f.value, 0.2);
});

test('a changed Session envelope cancels rather than applying a stale draft movement', () => {
  const f = fixture();
  f.fire('pointerdown');
  f.fire('pointermove', { clientX: 34 });
  f.state.expectedRevision++;
  f.fire('pointermove', { clientX: 44 });
  assert.equal(f.value, 0.5);
  assert.equal(f.captured, null);
});
