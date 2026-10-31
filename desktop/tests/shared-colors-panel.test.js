import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createSharedColorsPanel } from '../src/shared-colors-panel.js';
import { captureFocus, restoreFocus } from '../src/focus.js';

function setup(t) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<main id="app"></main>';
  const app = document.querySelector('#app');
  const handlers = new Map();
  const addEventListener = app.addEventListener.bind(app);
  app.addEventListener = (type, handler, ...options) => {
    handlers.set(type, handler);
    addEventListener(type, handler, ...options);
  };
  const model = {
    colorSlots: [
      { id: 'slot-a', name: 'Primary', value: { hex: '#112233', alpha: 0.4 } },
      { id: 'slot-b', name: 'Primary', value: { hex: '#aabbcc', alpha: 1 } },
    ],
    layers: [{ id: 'layer-a' }, { id: 'layer-b' }],
  };
  let session = { state: { sessionId: 'session-a', expectedRevision: 3 } };
  const calls = [],
    notices = [];
  let renders = 0,
    bindingHTML = '',
    dispatchError;
  const render = () => {
    renders++;
    app.innerHTML = `<button data-workspace-panel="colors">Colors</button><input id="unrelated">${panel.html()}${bindingHTML}`;
  };
  const panel = createSharedColorsPanel({
    app,
    getSession: () => session,
    getDocument: () => model,
    dispatch: (type, payload, state) => {
      if (dispatchError) throw dispatchError;
      calls.push({ type, payload, state });
    },
    render,
    notify: (...args) => notices.push(args),
  });
  render();
  const fire = (target, type = 'change', options = {}) => {
    const Event = type === 'keydown' ? win.KeyboardEvent : win.Event;
    const event = new Event(type, { bubbles: true, cancelable: true, ...options });
    target.dispatchEvent(event);
    return event;
  };
  const bind = (
    q = {},
    node = { id: 'node-b', name: 'Paint' },
    key = 'stroke',
    layer = 'layer-b',
  ) => {
    bindingHTML = panel.bindingControl(node, key, q, layer);
    render();
    return app.querySelector('[data-shared-color-binding]');
  };
  return {
    win,
    document,
    app,
    panel,
    model,
    calls,
    notices,
    handlers,
    fire,
    bind,
    render,
    get renders() {
      return renders;
    },
    get session() {
      return session;
    },
    set session(value) {
      session = value;
    },
    failDispatch: (error) => (dispatchError = error),
  };
}

test('panel starts closed and add dispatches a typed command with the current Session envelope', (t) => {
  const s = setup(t);
  assert.equal(s.panel.open, false);
  assert.equal(s.panel.html(), '');
  s.panel.toggle();
  assert.equal(s.panel.open, true);
  assert.equal(
    s.app.querySelector('.shared-colors-panel').getAttribute('aria-label'),
    'Shared colors',
  );
  s.fire(s.app.querySelector('[data-shared-color-add]'), 'click');
  assert.deepEqual(s.calls, [
    {
      type: 'create-color-slot',
      payload: { name: 'Color 3', value: { hex: '#808080', alpha: 1 } },
      state: { sessionId: 'session-a', expectedRevision: 3 },
    },
  ]);
  assert.notEqual(
    s.calls[0].state,
    s.session.state,
    'capture the envelope rather than passing mutable state',
  );
  assert.deepEqual(s.model.colorSlots[0].value, { hex: '#112233', alpha: 0.4 });
});

test('name, hex and alpha edits preserve exact slot identity and untouched color components', (t) => {
  const s = setup(t);
  s.panel.toggle();
  const row = s.app.querySelector('[data-shared-color-row="slot-b"]');
  for (const [kind, input, payload] of [
    ['name', 'Renamed', { slotId: 'slot-b', name: 'Renamed' }],
    ['hex', '#abcdef', { slotId: 'slot-b', value: { hex: '#abcdef', alpha: 1 } }],
    ['alpha', '0', { slotId: 'slot-b', value: { hex: '#aabbcc', alpha: 0 } }],
  ]) {
    const el = row.querySelector(`[data-shared-color-edit="${kind}"]`);
    el.value = input;
    s.fire(el);
    assert.deepEqual(s.calls.at(-1), {
      type: kind === 'name' ? 'rename-color-slot' : 'set-color-slot',
      payload,
      state: { sessionId: 'session-a', expectedRevision: 3 },
    });
  }
  assert.deepEqual(s.model.colorSlots[1], {
    id: 'slot-b',
    name: 'Primary',
    value: { hex: '#aabbcc', alpha: 1 },
  });
});

test('shared colors use HSL by default while preserving alpha and slot identity', (t) => {
  const s = setup(t);
  s.panel.toggle();
  const row = s.app.querySelector('[data-shared-color-row="slot-b"]');
  const hue = row.querySelector('[data-shared-color-edit="hsl-h"]');
  assert.ok(hue);
  assert.ok(row.querySelector('[data-shared-color-edit="hsl-s"]'));
  assert.ok(row.querySelector('[data-shared-color-edit="hsl-l"]'));
  hue.value = '120';
  s.fire(hue);
  const value = s.calls.at(-1).payload.value;
  assert.equal(s.calls.at(-1).type, 'set-color-slot');
  assert.equal(s.calls.at(-1).payload.slotId, 'slot-b');
  assert.equal(value.alpha, 1);
  assert.equal(value.hex, '#aaccaa');
  assert.deepEqual(s.model.colorSlots[1].value, { hex: '#aabbcc', alpha: 1 });
  const before = s.calls.length;
  hue.value = '400';
  s.fire(hue);
  assert.equal(s.calls.length, before, 'out-of-range HSL must not dispatch');
});

test('focus restoration keeps duplicate-name slot B drafts on B across structural renders', async (t) => {
  for (const [kind, draft] of [
    ['name', 'Slot B uncommitted draft'],
    ['alpha', '0.25'],
  ]) {
    await t.test(kind, (t) => {
      const s = setup(t);
      s.panel.toggle();
      const input = (id) =>
        s.app.querySelector(`[data-shared-color-row="${id}"] [data-shared-color-edit="${kind}"]`);
      const beforeModel = structuredClone(s.model);
      const slotAValue = input('slot-a').value;
      assert.equal(input('slot-a').dataset.sharedColorId, 'slot-a');
      assert.equal(input('slot-b').dataset.sharedColorId, 'slot-b');
      const previousB = input('slot-b');
      previousB.focus();
      previousB.value = draft;
      if (kind === 'name') previousB.setSelectionRange(3, 9);
      const focus = captureFocus(s.app);
      assert.ok(focus);
      s.render();
      assert.equal(previousB.isConnected, false, 'exercise real DOM replacement');
      restoreFocus(s.app, focus);
      assert.equal(s.document.activeElement, input('slot-b'));
      assert.equal(input('slot-b').value, draft);
      assert.equal(input('slot-a').value, slotAValue, 'restoration must not overwrite slot A');
      if (kind === 'name') {
        assert.equal(input('slot-b').selectionStart, 3);
        assert.equal(input('slot-b').selectionEnd, 9);
      }
      assert.deepEqual(s.model, beforeModel, 'a restored draft is not an authored command');
      assert.deepEqual(s.calls, []);
    });
  }
});

test('negative control: stripping per-input slot IDs reproduces duplicate-name focus collision', async (t) => {
  for (const [kind, draft] of [
    ['name', 'Slot B draft'],
    ['alpha', '0.25'],
  ]) {
    await t.test(kind, (t) => {
      const s = setup(t);
      s.panel.toggle();
      const stripInputIdentity = () =>
        s.app
          .querySelectorAll('[data-shared-color-id]')
          .forEach((el) => el.removeAttribute('data-shared-color-id'));
      const input = (id) =>
        s.app.querySelector(`[data-shared-color-row="${id}"] [data-shared-color-edit="${kind}"]`);
      stripInputIdentity();
      const slotBValue = input('slot-b').value;
      input('slot-b').focus();
      input('slot-b').value = draft;
      const focus = captureFocus(s.app);
      s.render();
      stripInputIdentity();
      restoreFocus(s.app, focus);
      assert.equal(
        s.document.activeElement,
        input('slot-a'),
        'without identity, first matching A wins',
      );
      assert.equal(input('slot-a').value, draft, 'the old bug leaks B draft into A');
      assert.equal(
        input('slot-b').value,
        slotBValue,
        'B loses its draft without the stable input ID',
      );
      assert.deepEqual(s.calls, []);
    });
  }
});

test('binding and unbinding use exact layer, node, key and slot IDs, not names or selection', (t) => {
  const s = setup(t);
  const el = s.bind({ colorSlotId: 'slot-a' });
  assert.equal(el.value, 'slot-a');
  el.value = 'slot-b';
  s.fire(el);
  el.value = '';
  s.fire(el);
  assert.deepEqual(s.calls, [
    {
      type: 'bind-color-slot',
      payload: { layerId: 'layer-b', nodeId: 'node-b', key: 'stroke', slotId: 'slot-b' },
      state: { sessionId: 'session-a', expectedRevision: 3 },
    },
    {
      type: 'unbind-color-slot',
      payload: { layerId: 'layer-b', nodeId: 'node-b', key: 'stroke' },
      state: { sessionId: 'session-a', expectedRevision: 3 },
    },
  ]);
});

test('binding availability follows locked, sourced and expression-owned parameters and layers', (t) => {
  const s = setup(t);
  for (const q of [{ locked: true }, { source: { nodeId: 'other' } }, { expression: '1' }])
    assert.equal(s.bind(q).disabled, true);
  s.model.layers[1].locked = true;
  assert.equal(s.bind().disabled, true);
  s.model.layers[1].locked = false;
  assert.equal(s.bind().disabled, false);
  s.model.colorSlots = [];
  assert.equal(s.bind(), null);
  assert.ok(
    s.bind({ colorSlotId: 'missing-slot' }),
    'a persisted binding still exposes an unlink control',
  );
  s.panel.toggle();
  assert.match(s.app.querySelector('.shared-colors-panel').textContent, /Add a color/);
});

test('stale Session and revision controls reject add, edit and bind without dispatch', async (t) => {
  for (const stale of ['session', 'revision']) {
    for (const operation of ['add', 'edit', 'bind']) {
      await t.test(`${stale}: ${operation}`, (t) => {
        const s = setup(t);
        s.panel.toggle();
        const control =
          operation === 'bind'
            ? s.bind()
            : s.app.querySelector(
                operation === 'add' ? '[data-shared-color-add]' : '[data-shared-color-edit="name"]',
              );
        if (stale === 'session')
          s.session = { state: { sessionId: 'replacement', expectedRevision: 3 } };
        else s.session.state.expectedRevision++;
        if (operation === 'bind') control.value = 'slot-b';
        s.fire(control, operation === 'add' ? 'click' : 'change');
        assert.deepEqual(s.calls, []);
        assert.match(s.notices[0][0], /stale/);
        assert.equal(s.notices[0][1], true);
      });
    }
  }
});

test('detached or foreign stamped DOM cannot be used by a queued delegated change handler', (t) => {
  const s = setup(t);
  const detached = s.bind();
  detached.value = 'slot-b';
  detached.remove();
  // Exercise the handler directly to model an already-queued callback after DOM replacement.
  const change = (target) => s.handlers.get('change')({ target, stopImmediatePropagation() {} });
  change(detached);
  assert.deepEqual(s.calls, []);
  assert.match(s.notices.at(-1)[0], /stale/);
  s.document.body.append(detached);
  assert.equal(detached.isConnected, true);
  change(detached);
  assert.deepEqual(s.calls, []);
  assert.match(s.notices.at(-1)[0], /stale/);
});

test('blank alpha, missing slots and command failures notify instead of mutating state', (t) => {
  const s = setup(t);
  s.panel.toggle();
  const alpha = s.app.querySelector('[data-shared-color-edit="alpha"]');
  alpha.value = '';
  s.fire(alpha);
  assert.deepEqual(s.calls, []);
  assert.match(s.notices.at(-1)[0], /Enter an alpha/);
  assert.equal(s.model.colorSlots[0].value.alpha, 0.4);
  s.model.colorSlots = [];
  s.fire(s.app.querySelector('[data-shared-color-edit="name"]'));
  assert.match(s.notices.at(-1)[0], /no longer available/);
  s.failDispatch(Error('typed Session rejected this edit'));
  s.fire(s.app.querySelector('[data-shared-color-add]'), 'click');
  assert.match(s.notices.at(-1)[0], /typed Session rejected/);
  assert.deepEqual(s.calls, []);
});

test('HTML escapes names, identities and binding labels without creating executable markup', (t) => {
  const s = setup(t);
  const hostile = '\"><img src=x onerror="bad()"> & \' <script>bad()</script>';
  s.model.colorSlots[0].id = hostile;
  s.model.colorSlots[0].name = hostile;
  s.session.state.sessionId = hostile;
  s.panel.toggle();
  const row = s.app.querySelector('[data-shared-color-row]');
  assert.equal(row.dataset.sharedColorRow, hostile);
  assert.equal(row.querySelector('[data-shared-color-edit="name"]').value, hostile);
  assert.equal(
    row.querySelector('[data-shared-color-edit="hex"]').getAttribute('aria-label'),
    `${hostile} color`,
  );
  const el = s.bind({ colorSlotId: hostile }, { id: hostile, name: hostile }, hostile, hostile);
  assert.equal(el.value, hostile);
  assert.equal(el.dataset.sharedColorLayer, hostile);
  assert.equal(el.dataset.sharedColorNode, hostile);
  assert.equal(el.dataset.sharedColorKey, hostile);
  assert.equal(el.dataset.sharedColorSession, hostile);
  assert.equal(el.getAttribute('aria-label'), `${hostile} ${hostile} shared color`);
  assert.equal(el.selectedOptions[0].textContent, hostile);
  assert.equal(s.app.querySelector('img,script,[onerror]'), null);
});

test('close and Escape dismiss the panel and restore focus to its launcher', (t) => {
  const s = setup(t);
  s.panel.toggle();
  s.fire(s.app.querySelector('[data-shared-color-close]'), 'click');
  assert.equal(s.panel.open, false);
  assert.equal(s.document.activeElement, s.app.querySelector('[data-workspace-panel="colors"]'));
  s.panel.toggle();
  const el = s.app.querySelector('[data-shared-color-edit="name"]');
  el.focus();
  const event = s.fire(el, 'keydown', { key: 'Escape' });
  assert.equal(event.defaultPrevented, true);
  assert.equal(s.panel.open, false);
  assert.equal(s.app.querySelector('.shared-colors-panel'), null);
  assert.equal(s.document.activeElement, s.app.querySelector('[data-workspace-panel="colors"]'));
  assert.deepEqual(s.calls, []);
});

test('Escape respects unrelated controls, handled events and an open nested dialog', (t) => {
  const s = setup(t);
  s.panel.toggle();
  s.fire(s.app.querySelector('#unrelated'), 'keydown', { key: 'Escape' });
  const el = s.app.querySelector('[data-shared-color-edit="name"]');
  el.addEventListener('keydown', (event) => event.preventDefault(), { once: true });
  s.fire(el, 'keydown', { key: 'Escape' });
  const dialog = s.document.createElement('dialog');
  dialog.setAttribute('open', '');
  dialog.innerHTML = '<input>';
  s.app.querySelector('.shared-colors-panel').append(dialog);
  const event = s.fire(dialog.querySelector('input'), 'keydown', { key: 'Escape' });
  assert.equal(event.defaultPrevented, false);
  assert.equal(s.panel.open, true);
  assert.deepEqual(s.calls, []);
});

test('IME Escape keeps the panel and focused draft alive for composing and keyCode 229 events', async (t) => {
  for (const options of [{ isComposing: true }, { keyCode: 229 }]) {
    await t.test(JSON.stringify(options), (t) => {
      const s = setup(t);
      s.panel.toggle();
      const input = s.app.querySelector(
        '[data-shared-color-row="slot-b"] [data-shared-color-edit="name"]',
      );
      input.focus();
      input.value = '未確定の色名';
      const renders = s.renders;
      const composingEscape = s.fire(input, 'keydown', { key: 'Escape', ...options });
      assert.equal(composingEscape.defaultPrevented, false);
      assert.equal(s.panel.open, true);
      assert.equal(s.renders, renders);
      assert.equal(input.isConnected, true);
      assert.equal(s.document.activeElement, input);
      assert.equal(input.value, '未確定の色名');
      assert.deepEqual(s.calls, []);
      const ordinaryEscape = s.fire(input, 'keydown', { key: 'Escape' });
      assert.equal(ordinaryEscape.defaultPrevented, true);
      assert.equal(s.panel.open, false);
      assert.equal(input.isConnected, false);
      assert.equal(
        s.document.activeElement,
        s.app.querySelector('[data-workspace-panel="colors"]'),
      );
    });
  }
});

test('shared color clicks and changes are isolated from later application handlers and ancestors', (t) => {
  const s = setup(t);
  s.panel.toggle();
  const select = s.bind();
  let appClicks = 0,
    appChanges = 0,
    bodyEvents = 0;
  s.app.addEventListener('click', () => appClicks++);
  s.app.addEventListener('change', () => appChanges++);
  s.document.body.addEventListener('click', () => bodyEvents++);
  s.document.body.addEventListener('change', () => bodyEvents++);
  s.fire(s.app.querySelector('[data-shared-color-add]'), 'click');
  const name = s.app.querySelector('[data-shared-color-edit="name"]');
  name.value = 'Changed';
  s.fire(name);
  select.value = 'slot-b';
  s.fire(select);
  s.fire(s.app.querySelector('[data-shared-color-close]'), 'click');
  assert.equal(s.panel.open, false);
  assert.equal(s.calls.length, 3, 'closing is view state only');
  assert.equal(appClicks, 0);
  assert.equal(appChanges, 0);
  assert.equal(bodyEvents, 0);
  s.fire(s.app.querySelector('#unrelated'), 'click');
  s.fire(s.app.querySelector('#unrelated'));
  assert.equal(appClicks, 1, 'unrelated clicks are not swallowed');
  assert.equal(appChanges, 1, 'unrelated changes are not swallowed');
  assert.equal(bodyEvents, 2);
});
