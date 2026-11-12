import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createEffectsPanel, EFFECT_FAVORITES_KEY } from '../src/effects-panel.js';

function fixture(t, storage) {
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
    storage,
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

test('favorite effects persist by ID, sort first, and do not author the selected artwork', (t) => {
  const data = new Map([[EFFECT_FAVORITES_KEY, JSON.stringify(['temporarily-unavailable'])]]);
  const storage = {
    getItem: (key) => data.get(key) ?? null,
    setItem: (key, value) => data.set(key, value),
  };
  const f = fixture(t, storage);
  let view = f.mount();
  view.querySelector('[data-effect-id="provider:other"]').click();
  const favorite = view.querySelector('[data-effects-favorite]');
  favorite.click();
  assert.equal(favorite.getAttribute('aria-pressed'), 'true');
  assert.deepEqual(JSON.parse(data.get(EFFECT_FAVORITES_KEY)), [
    'temporarily-unavailable',
    'provider:other',
  ]);
  assert.deepEqual(
    [...view.querySelectorAll('[data-effect-id]')].map((row) => row.dataset.effectId),
    ['provider:other', 'builtin:transform'],
  );
  assert.deepEqual(f.actions, []);
  view = f.mount();
  assert.equal(view.querySelector('[data-effects-favorite]').getAttribute('aria-pressed'), 'true');
  view.querySelector('[data-effects-apply]').click();
  assert.equal(f.actions[0].id, 'provider:other');
  view.querySelector('[data-effects-favorite]').click();
  assert.deepEqual(JSON.parse(data.get(EFFECT_FAVORITES_KEY)), ['temporarily-unavailable']);
});

test('favorite writes preserve unreadable data and refuse hidden or composing actions', (t) => {
  let raw = '{bad json',
    writes = 0;
  const storage = {
    getItem: () => raw,
    setItem: (_key, value) => {
      writes++;
      raw = value;
    },
  };
  const f = fixture(t, storage);
  let view = f.mount();
  assert.equal(view.querySelector('[data-effects-favorite]').disabled, false);
  assert.match(
    view.querySelector('[data-effects-favorite-status]').textContent,
    /could not be read/,
  );
  view.querySelector('[data-effects-favorite]').click();
  assert.equal(writes, 0);
  assert.equal(raw, '{bad json');
  raw = '[]';
  view = f.mount();
  const button = view.querySelector('[data-effects-favorite]');
  view.hidden = true;
  button.click();
  view.hidden = false;
  view.setAttribute('inert', '');
  button.click();
  view.removeAttribute('inert');
  const search = view.querySelector('[data-effects-search]');
  f.fire(search, 'compositionstart');
  button.click();
  f.fire(search, 'compositionend');
  assert.equal(writes, 0);
  button.click();
  assert.equal(writes, 1);
});

test('failed favorite writes leave preferences unchanged and can be retried in place', (t) => {
  let failing = true,
    raw = '[]';
  const f = fixture(t, {
    getItem: () => raw,
    setItem: (_key, value) => {
      if (failing) throw Error('storage unavailable');
      raw = value;
    },
  });
  const view = f.mount();
  view.querySelector('[data-effects-favorite]').click();
  assert.equal(view.querySelector('[data-effects-favorite]').getAttribute('aria-pressed'), 'false');
  assert.match(
    view.querySelector('[data-effects-favorite-status]').textContent,
    /could not be saved/,
  );
  failing = false;
  view.querySelector('[data-effects-favorite]').click();
  assert.equal(view.querySelector('[data-effects-favorite]').getAttribute('aria-pressed'), 'true');
  assert.equal(view.querySelector('[data-effects-favorite-status]').textContent, '');
  view.querySelector('[data-effects-apply]').click();
  assert.equal(f.actions[0].id, 'builtin:transform');
});

test('a stale favorite button never reverses a change made in another window', (t) => {
  let raw = '[]';
  const f = fixture(t, {
    getItem: () => raw,
    setItem: (_key, value) => (raw = value),
  });
  const view = f.mount(),
    button = view.querySelector('[data-effects-favorite]');
  raw = '["builtin:transform","elsewhere"]';
  button.click();
  assert.deepEqual(JSON.parse(raw), ['builtin:transform', 'elsewhere']);
  assert.equal(button.getAttribute('aria-pressed'), 'true');
  button.click();
  assert.deepEqual(JSON.parse(raw), ['elsewhere']);
});

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
  const row = view.querySelector('[data-effect-id="builtin:transform"]');
  f.fire(search, 'compositionstart');
  search.value = '移動';
  f.fire(search, 'input', { isComposing: true });
  assert.equal(apply.disabled, true);
  const down = new f.win.PointerEvent('pointerdown', { bubbles: true, cancelable: true });
  apply.dispatchEvent(down);
  assert.equal(down.defaultPrevented, true);
  f.fire(search, 'keydown', { key: 'Enter', isComposing: true });
  apply.click();
  assert.equal(f.actions.length, 0);
  f.fire(search, 'compositionend');
  assert.equal(view.querySelector('[data-effect-id="builtin:transform"]'), row);
  assert.equal(apply.disabled, false);
  f.fire(search, 'keydown', { key: 'Enter' });
  assert.equal(f.actions.length, 0);
  apply.click();
  assert.equal(f.actions.length, 1);
});

test('workspace refresh updates a changed target without rebuilding an unchanged one', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const owner = {};
  let revision = 1,
    calls = 0,
    current = { label: 'Circle', snapshot: { revision } };
  const panel = createEffectsPanel({
    document: win.document,
    getTarget: () => {
      calls++;
      return current;
    },
    getTargetVersion: () => [owner, revision],
    getEntries: () => [{ id: 'transform', label: 'Transform' }],
    onApply: () => assert.fail('No artwork is selected'),
  });
  const view = panel.createView();
  win.document.body.append(view);
  const row = view.querySelector('[data-effect-id]');
  panel.refreshTarget();
  assert.equal(calls, 1);
  current = null;
  revision++;
  panel.refreshTarget();
  assert.equal(calls, 2);
  assert.equal(view.querySelector('[data-effects-apply]').disabled, true);
  assert.equal(view.querySelector('[data-effects-status]').textContent, 'Select artwork');
  assert.equal(view.querySelector('[data-effect-id]'), row);
});

test('entry selection captures its own target and refreshes registered entries on revision change', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  let revision = 1,
    entries = [
      { id: 'transform', label: 'Transform' },
      { id: 'macro:one', label: 'Macro one' },
    ];
  const applied = [],
    panel = createEffectsPanel({
      document: win.document,
      getTargetVersion: () => [revision],
      getTarget: (id) => ({ label: 'Rectangle', snapshot: { id, revision } }),
      getEntries: () => entries,
      onApply: (id, snapshot) => applied.push({ id, snapshot }),
    }),
    view = panel.createView();
  win.document.body.append(view);
  view.querySelector('[data-effect-id="macro:one"]').click();
  view.querySelector('[data-effects-apply]').click();
  assert.deepEqual(applied[0], { id: 'macro:one', snapshot: { id: 'macro:one', revision: 1 } });
  entries = [
    { id: 'transform', label: 'Transform' },
    { id: 'macro:two', label: 'Macro two' },
  ];
  revision++;
  panel.refreshTarget();
  assert.equal(view.querySelector('[data-effect-id="macro:one"]'), null);
  view.querySelector('[data-effect-id="macro:two"]').click();
  view.querySelector('[data-effects-apply]').click();
  assert.deepEqual(applied[1], { id: 'macro:two', snapshot: { id: 'macro:two', revision: 2 } });
});

test('hidden or inert effects views cannot be activated through a retained control', (t) => {
  const f = fixture(t);
  const view = f.mount(),
    apply = view.querySelector('[data-effects-apply]');
  view.hidden = true;
  apply.click();
  view.hidden = false;
  view.setAttribute('inert', '');
  apply.click();
  assert.deepEqual(f.actions, []);
  view.removeAttribute('inert');
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
