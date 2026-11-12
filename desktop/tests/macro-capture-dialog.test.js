import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { openMacroCaptureDialog } from '../src/macro-capture-dialog.js';

const candidates = () => [
  { id: 'first-scale', nodeId: 'first', key: 'scale', label: 'First scale' },
  { id: 'first-rotation', nodeId: 'first', key: 'rotation', label: 'First rotation' },
  { id: 'second-scale', nodeId: 'second', key: 'scale', label: 'Second scale' },
];
function fixture(t, values = candidates()) {
  const win = new Window(),
    document = win.document,
    anchor = document.createElement('button');
  t.after(() => win.happyDOM.abort());
  document.body.append(anchor);
  anchor.focus();
  const controller = openMacroCaptureDialog({ document, candidates: values, anchor });
  const dialog = document.querySelector('dialog');
  const fire = (element, type, options = {}) => {
    const EventType = type === 'keydown' ? win.KeyboardEvent : win.Event;
    const event = new EventType(type, { bubbles: true, cancelable: true, ...options });
    element.dispatchEvent(event);
    return event;
  };
  return { win, document, anchor, dialog, controller, fire };
}

test('Macro draft returns only selected stable identities and editable display names', async (t) => {
  const values = candidates(),
    f = fixture(t, values),
    name = f.dialog.querySelector('[data-macro-name]'),
    first = f.dialog.querySelector('[data-macro-control="first-scale"]'),
    second = f.dialog.querySelector('[data-macro-control="second-scale"]'),
    label = f.dialog.querySelector('[data-macro-control-label="first-scale"]'),
    submit = f.dialog.querySelector('[type="submit"]');
  assert.equal(f.document.activeElement, name);
  assert.equal(submit.disabled, true);
  assert.equal(label.disabled, true);
  values[0].nodeId = 'changed-by-caller';
  values[0].label = 'changed-by-caller';
  name.value = 'My Macro';
  f.fire(name, 'input');
  first.click();
  second.click();
  label.value = 'Size';
  f.fire(label, 'input');
  assert.equal(submit.disabled, false);
  submit.click();
  assert.deepEqual(await f.controller.result, {
    name: 'My Macro',
    parameters: [
      { id: 'first-scale', nodeId: 'first', key: 'scale', label: 'Size' },
      { id: 'second-scale', nodeId: 'second', key: 'scale', label: 'Second scale' },
    ],
  });
  assert.equal(f.dialog.isConnected, false);
  assert.equal(f.document.activeElement, f.anchor);
});

test('empty or silently changed inputs cannot submit, and labels are text', async (t) => {
  const values = candidates();
  values[0].label = '<img src=x onerror=alert(1)>';
  const f = fixture(t, values),
    name = f.dialog.querySelector('[data-macro-name]'),
    toggle = f.dialog.querySelector('[data-macro-control="first-scale"]'),
    label = f.dialog.querySelector('[data-macro-control-label="first-scale"]'),
    submit = f.dialog.querySelector('[type="submit"]');
  assert.equal(f.dialog.querySelector('img'), null);
  name.value = 'Macro';
  f.fire(name, 'input');
  toggle.click();
  label.value = '  ';
  f.fire(label, 'input');
  assert.equal(submit.disabled, true);
  assert.equal(label.getAttribute('aria-invalid'), 'true');
  label.value = 'Control';
  f.fire(label, 'input');
  assert.equal(submit.disabled, false);
  name.value = ' ';
  f.fire(f.dialog.querySelector('form'), 'submit');
  assert.equal(f.dialog.isConnected, true);
  assert.equal(submit.disabled, true);
  f.controller.cancel();
  assert.equal(await f.controller.result, null);
});

test('IME Enter, Escape, pointer and native cancel cannot finish the draft', async (t) => {
  const f = fixture(t),
    name = f.dialog.querySelector('[data-macro-name]'),
    cancel = f.dialog.querySelector('[data-macro-cancel]');
  name.value = 'Macro';
  f.fire(name, 'input');
  f.dialog.querySelector('[data-macro-control="first-scale"]').click();
  f.fire(name, 'compositionstart');
  assert.equal(f.dialog.querySelector('[type="submit"]').disabled, true);
  assert.equal(f.fire(name, 'keydown', { key: 'Enter', isComposing: true }).defaultPrevented, true);
  assert.equal(
    f.fire(name, 'keydown', { key: 'Escape', isComposing: true }).defaultPrevented,
    true,
  );
  assert.equal(f.fire(f.dialog, 'cancel').defaultPrevented, true);
  assert.equal(f.fire(cancel, 'pointerdown').defaultPrevented, true);
  cancel.click();
  assert.equal(f.dialog.isConnected, true);
  f.fire(name, 'compositionend');
  f.fire(name, 'keydown', { key: 'Escape' });
  assert.equal(await f.controller.result, null);
  assert.equal(f.document.activeElement, f.anchor);
});

test('invalid or duplicate candidate identities are rejected before opening', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  for (const bad of [
    [],
    [{ id: undefined, nodeId: 'first', key: 'scale', label: 'Scale' }],
    [{ id: 'id', nodeId: undefined, key: 'scale', label: 'Scale' }],
    [candidates()[0], candidates()[0]],
    [{ id: 'id', nodeId: 'first', key: 'path', label: 'Path' }],
    [{ id: 'id', nodeId: 'first', key: 'scale', label: ' ' }],
  ])
    assert.throws(() => openMacroCaptureDialog({ document: win.document, candidates: bad }));
  assert.equal(win.document.querySelector('dialog'), null);
});
