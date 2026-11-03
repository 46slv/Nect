import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createKeyColorDeltaScrub } from '../src/key-color-delta-scrub.js';

function fixture(t, raw = '-2.1234567890123457') {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const doc = win.document;
  doc.body.innerHTML =
    '<dialog open><input type="number" step="any" data-key-color-input="deltaL" data-key-color-slot="a"></dialog>';
  const dialog = doc.querySelector('dialog'),
    input = dialog.querySelector('input');
  input.value = raw;
  let valid = true,
    blocked = false,
    inputs = 0;
  const control = createKeyColorDeltaScrub(dialog, { valid: () => valid, blocked: () => blocked });
  dialog.addEventListener('input', () => inputs++);
  const fire = (target, type, x, id = 1) =>
    target.dispatchEvent(
      new win.PointerEvent(type, {
        bubbles: true,
        cancelable: true,
        pointerId: id,
        isPrimary: true,
        button: 0,
        clientX: x,
      }),
    );
  return {
    win,
    doc,
    dialog,
    input,
    control,
    fire,
    get inputs() {
      return inputs;
    },
    set valid(value) {
      valid = value;
    },
    set blocked(value) {
      blocked = value;
    },
  };
}

test('detached delta scrub has a dead zone and preserves the exact original on return', (t) => {
  const f = fixture(t),
    original = f.input.value;
  f.fire(f.input, 'pointerdown', 100);
  f.fire(f.doc, 'pointermove', 104);
  assert.equal(f.inputs, 0);
  f.fire(f.doc, 'pointermove', 124);
  assert.ok(Math.abs(Number(f.input.value) - (Number(original) + 0.2)) < 1e-12);
  f.fire(f.doc, 'pointermove', 100);
  f.fire(f.doc, 'pointerup', 100);
  assert.equal(f.input.value, original);
  assert.equal(f.inputs, 2);
});

for (const reason of ['pointercancel', 'blur', 'stale', 'dispose'])
  test(`${reason} restores the original and rejects late pointer events`, (t) => {
    const f = fixture(t),
      original = f.input.value;
    f.fire(f.input, 'pointerdown', 100);
    f.fire(f.doc, 'pointermove', 124);
    if (reason === 'pointercancel') f.fire(f.doc, 'pointercancel', 124);
    else if (reason === 'blur') f.win.dispatchEvent(new f.win.Event('blur'));
    else if (reason === 'stale') {
      f.valid = false;
      f.fire(f.doc, 'pointermove', 130);
    } else f.control.dispose();
    assert.equal(f.input.value, original);
    f.fire(f.doc, 'pointermove', 180);
    f.fire(f.doc, 'pointerup', 180);
    assert.equal(f.input.value, original);
  });

test('other input, wrong pointer and composition cannot overwrite a newer draft', (t) => {
  const f = fixture(t);
  f.blocked = true;
  f.fire(f.input, 'pointerdown', 100);
  f.fire(f.doc, 'pointermove', 124);
  assert.equal(f.inputs, 0);
  f.blocked = false;
  f.fire(f.input, 'pointerdown', 100);
  assert.equal(
    f.fire(f.input, 'pointerdown', 100, 2),
    false,
    'a second pointer cannot start another control',
  );
  f.fire(f.doc, 'pointermove', 124, 2);
  assert.equal(f.inputs, 0);
  f.fire(f.doc, 'pointermove', 124);
  f.input.value = '7.125';
  f.input.dispatchEvent(new f.win.Event('input', { bubbles: true }));
  f.fire(f.doc, 'pointerup', 124);
  assert.equal(f.input.value, '7.125');
});

test('an empty draft remains invalid and is not silently converted by a drag', (t) => {
  const f = fixture(t, '');
  f.fire(f.input, 'pointerdown', 100);
  f.fire(f.doc, 'pointermove', 124);
  f.fire(f.doc, 'pointerup', 124);
  assert.equal(f.input.value, '');
  assert.equal(f.inputs, 0);
});
