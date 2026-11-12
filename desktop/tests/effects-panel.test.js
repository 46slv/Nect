import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createEffectsPanel } from '../src/effects-panel.js';

function fixture(t) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  const actions = [],
    errors = [];
  let current = { label: '<Artwork>', snapshot: { session: 'a', revision: 3, layer: 'one' } };
  const entries = [
    { id: 'builtin:transform', label: 'Transform', aliases: ['移動', '回転', '拡大縮小'] },
    { id: 'provider:other', label: 'Other effect', aliases: [] },
  ];
  const controller = createEffectsPanel({
    document,
    getTarget: () => current,
    getEntries: () => entries,
    onApply: (id, snapshot) => {
      if (snapshot !== current?.snapshot) throw Error('Artwork changed');
      actions.push({ id, snapshot });
    },
    onError: (error) => errors.push(error.message),
  });
  const mount = () => {
    const view = controller.createView();
    document.body.replaceChildren(view);
    return view;
  };
  const fire = (element, type, options = {}) => {
    const EventType = type === 'keydown' ? win.KeyboardEvent : win.Event;
    const event = new EventType(type, { bubbles: true, cancelable: true, ...options });
    element.dispatchEvent(event);
    return event;
  };
  return {
    win,
    controller,
    actions,
    errors,
    mount,
    fire,
    setTarget: (target) => (current = target),
  };
}

test('effect search uses labels and aliases, and applies a stable entry ID to the captured artwork', (t) => {
  const f = fixture(t);
  const view = f.mount();
  assert.equal(view.querySelector('[data-effects-target]').textContent, '<Artwork>');
  assert.equal(view.querySelector('[data-effects-target] artwork'), null);
  const search = view.querySelector('[data-effects-search]');
  search.focus();
  search.value = '　移動　';
  f.fire(search, 'input');
  assert.equal(f.win.document.activeElement, search);
  assert.deepEqual(
    [...view.querySelectorAll('[data-effect-id]')].map((row) => row.textContent),
    ['Transform'],
  );
  view.querySelector('[data-effects-apply]').click();
  assert.deepEqual(f.actions, [
    { id: 'builtin:transform', snapshot: { session: 'a', revision: 3, layer: 'one' } },
  ]);
  search.value = 'ＴＲＡＮＳＦＯＲＭ';
  f.fire(search, 'input');
  assert.equal(view.querySelectorAll('[data-effect-id]').length, 1);
  search.value = 'missing';
  f.fire(search, 'input');
  assert.equal(view.querySelector('[data-effects-apply]').disabled, true);
  assert.equal(view.querySelector('[data-effects-status]').textContent, 'No matching effects');
});

test('IME confirmation and search Enter do not apply, while an explicit Apply does', (t) => {
  const f = fixture(t);
  const view = f.mount();
  const search = view.querySelector('[data-effects-search]');
  const apply = view.querySelector('[data-effects-apply]');
  f.fire(search, 'compositionstart');
  search.value = '移動';
  f.fire(search, 'input', { isComposing: true });
  assert.equal(apply.disabled, true);
  f.fire(search, 'keydown', { key: 'Enter', isComposing: true });
  apply.click();
  assert.equal(f.actions.length, 0);
  f.fire(search, 'compositionend');
  assert.equal(apply.disabled, false);
  f.fire(search, 'keydown', { key: 'Enter' });
  assert.equal(f.actions.length, 0);
  apply.click();
  assert.equal(f.actions.length, 1);
});

test('a stale displayed artwork is passed to the owner for refusal, and a new view captures the replacement', (t) => {
  const f = fixture(t);
  const mutableTarget = { label: 'Before', snapshot: { session: 'a', revision: 3, layer: 'one' } };
  f.setTarget(mutableTarget);
  const old = f.mount();
  const replacement = {
    label: 'Replacement',
    snapshot: { session: 'b', revision: 3, layer: 'one' },
  };
  // Even if a caller reuses its outer object, the displayed action keeps its original snapshot.
  mutableTarget.snapshot = replacement.snapshot;
  old.querySelector('[data-effects-apply]').click();
  assert.deepEqual(f.actions, []);
  assert.deepEqual(f.errors, ['Artwork changed']);
  const fresh = f.mount();
  fresh.querySelector('[data-effects-apply]').click();
  assert.equal(f.actions[0].snapshot, replacement.snapshot);
  old.querySelector('[data-effects-apply]').click();
  assert.equal(f.actions.length, 1);
});

test('selection and unavailable artwork control the action without editing the document', (t) => {
  const f = fixture(t);
  f.setTarget({ label: 'Locked', reason: 'Unlock artwork' });
  let view = f.mount();
  assert.equal(view.querySelector('[data-effects-apply]').disabled, true);
  assert.equal(view.querySelector('[data-effects-status]').textContent, 'Unlock artwork');
  view.querySelector('[data-effect-id="provider:other"]').click();
  assert.equal(view.querySelector('[data-effects-apply]').disabled, true);
  f.setTarget(null);
  view = f.mount();
  assert.equal(view.querySelector('[data-effects-status]').textContent, 'Select artwork');
  assert.deepEqual(f.actions, []);
});
