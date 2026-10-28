import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createSharedColorsPanel } from '../src/shared-colors-panel.js';
import { proposeAutoChroma } from '../src/auto-chroma.js';
import { colorChromaCoordinates } from '../src/auto-chroma-panel.js';
import { captureFocus, restoreFocus } from '../src/focus.js';
import { blankDocument, CommandSession, serialize, deserialize } from '../src/core.js';

const initialSlots = () => [
  { id: 'slot-dark', name: 'Duplicate', value: { hex: '#151019', alpha: 0 } },
  { id: 'slot-mid', name: 'Duplicate', value: { hex: '#A44799', alpha: 0.123456789 } },
  { id: 'slot-light', name: 'Paper', value: { hex: '#f5eefb', alpha: 1 } },
];

// Independent encoded-sRGB → OKLab transform, rather than relying on a panel helper.
function proposalInput(slot) {
  const [r, g, b] = slot.value.hex.match(/[a-f\d]{2}/gi).map((byte) => {
    const encoded = parseInt(byte, 16) / 255;
    return encoded <= 0.04045 ? encoded / 12.92 : ((encoded + 0.055) / 1.055) ** 2.4;
  });
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  const L = 0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s;
  const a = 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s;
  const bLab = 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s;
  return {
    id: slot.id,
    L,
    h: ((Math.atan2(bLab, a) * 180) / Math.PI + 360) % 360,
    alpha: slot.value.alpha,
    role: L < 0.25 ? 'Text' : L > 0.9 ? 'Background' : 'Primary',
  };
}
function expectedUpdates(slots, settings = {}, locked = []) {
  const proposals = proposeAutoChroma(
    slots.map((slot) => ({
      ...proposalInput(slot),
      ...(locked.includes(slot.id) ? { anchor: slot.value } : {}),
    })),
    settings,
  );
  return proposals.map(({ id, value }) => ({ slotId: id, value }));
}

function setup(t, { realSession = false, restoreRenderFocus = false } = {}) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<main id="app"></main>';
  const app = document.querySelector('#app');
  let model = { colorSlots: initialSlots(), layers: [] };
  let session = { state: { sessionId: 'session-a', expectedRevision: 3 } };
  if (realSession) {
    model = blankDocument();
    model.schemaVersion = 54;
    model.colorSlots = initialSlots();
    session = new CommandSession(model);
  }
  const calls = [],
    notices = [],
    handlers = new Map();
  const addEventListener = app.addEventListener.bind(app);
  app.addEventListener = (type, handler, ...options) => {
    handlers.set(type, [...(handlers.get(type) || []), handler]);
    addEventListener(type, handler, ...options);
  };
  let dispatchError;
  const currentDocument = () => (realSession ? session.snapshot() : model);
  const render = () => {
    const focus = restoreRenderFocus ? captureFocus(app) : null;
    app.innerHTML = `<button data-workspace-panel="colors">Colors</button>${panel.html()}`;
    if (restoreRenderFocus) restoreFocus(app, focus);
  };
  const panel = createSharedColorsPanel({
    app,
    getSession: () => session,
    getDocument: currentDocument,
    dispatch: (type, payload, state) => {
      if (dispatchError) throw dispatchError;
      calls.push({ type, payload: structuredClone(payload), state });
      return realSession ? session.dispatch({ ...state, type, payload }) : undefined;
    },
    render,
    notify: (...args) => notices.push(args),
  });
  const fire = (target, type = 'change', options = {}) => {
    assert.ok(target, `missing ${type} target`);
    const Event = type === 'keydown' ? win.KeyboardEvent : win.Event;
    const event = new Event(type, { bubbles: true, cancelable: true, ...options });
    target.dispatchEvent(event);
    return event;
  };
  const query = (selector) => app.querySelector(selector);
  const click = (selector) => fire(query(selector), 'click');
  render();
  panel.toggle();
  return {
    win,
    document,
    app,
    panel,
    calls,
    notices,
    handlers,
    fire,
    query,
    click,
    render,
    currentDocument,
    get model() {
      return model;
    },
    set model(value) {
      model = value;
    },
    get session() {
      return session;
    },
    set session(value) {
      session = value;
    },
    failDispatch(error) {
      dispatchError = error;
    },
    preview() {
      click('[data-chroma-preview]');
    },
    apply() {
      click('[data-chroma-apply]');
    },
    change(selector, value) {
      const el = query(selector);
      assert.ok(el, `missing ${selector}`);
      if (el.type === 'checkbox') el.checked = value;
      else el.value = value;
      fire(el);
    },
  };
}

function assertUnchanged(s, before) {
  assert.deepEqual(s.currentDocument(), before);
  assert.deepEqual(s.calls, [], 'preview controls are not authored commands');
}

function assertBatch(s, slots, settings = {}, locked = []) {
  assert.equal(s.calls.length, 1, 'apply is one atomic authored command');
  assert.equal(s.calls[0].type, 'set-color-slots');
  assert.deepEqual(s.calls[0].payload, {
    updates: expectedUpdates(slots, settings, locked),
    design: {
      version: 1,
      profile: settings.profile ?? 'balanced',
      colorfulness: settings.colorfulness ?? 0.72,
      areaAware: settings.areaAware ?? true,
      lockedSlotIds: locked,
    },
  });
  assert.deepEqual(s.calls[0].state, s.session.state);
  assert.notEqual(s.calls[0].state, s.session.state, 'capture an independent Session envelope');
}

test('Auto Chroma preview and cancel are local, with explicit before/after colors', (t) => {
  const s = setup(t);
  const before = structuredClone(s.model);
  assert.equal(s.query('[data-chroma-apply]'), null);
  s.preview();
  assert.ok(s.query('[data-chroma-apply]'));
  assert.ok(s.query('[data-chroma-cancel]'));
  const preview = s.query('.chroma-preview');
  const swatches = [...preview.querySelectorAll('.chroma-swatch')];
  for (const { value } of before.colorSlots)
    assert.ok(swatches.some((el) => el.getAttribute('aria-label') === `Current ${value.hex}`));
  for (const { value } of expectedUpdates(before.colorSlots)) {
    assert.ok(swatches.some((el) => el.getAttribute('aria-label') === `Proposed ${value.hex}`));
    assert.ok([...preview.querySelectorAll('output')].some((el) => el.textContent === value.hex));
  }
  assertUnchanged(s, before);
  s.click('[data-chroma-cancel]');
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.equal(s.panel.open, true, 'cancel closes only the preview');
  assertUnchanged(s, before);
  s.preview();
  assertUnchanged(s, before);
});

test('default preview applies all exact stable IDs and alpha in one current stamped batch', (t) => {
  const s = setup(t);
  const slots = structuredClone(s.model.colorSlots);
  assert.deepEqual(
    slots.map(proposalInput).map(({ role }) => role),
    ['Text', 'Primary', 'Background'],
  );
  assert.equal(s.query('[data-chroma-profile]').value, 'balanced');
  assert.equal(Number(s.query('[data-chroma-strength]').value), 0.72);
  s.preview();
  s.apply();
  assertBatch(s, slots);
  assert.deepEqual(s.model.colorSlots, slots, 'the panel never mutates its document directly');
  assert.equal(s.query('[data-chroma-apply]'), null);
});

test('profile and strength controls recompute or discard only the local preview', async (t) => {
  for (const profile of ['balanced', 'fidelity', 'vibrant', 'soft', 'uniform']) {
    for (const strength of [0, 0.38, 1]) {
      await t.test(`${profile}: ${strength}`, (t) => {
        const s = setup(t);
        const before = structuredClone(s.model);
        const strengthControl = s.query('[data-chroma-strength]');
        assert.equal(strengthControl.getAttribute('min'), '0');
        assert.equal(strengthControl.getAttribute('max'), '1');
        s.preview();
        s.change('[data-chroma-profile]', profile);
        s.change('[data-chroma-strength]', String(strength));
        assertUnchanged(s, before);
        if (!s.query('[data-chroma-apply]')) s.preview();
        s.apply();
        assertBatch(s, before.colorSlots, { profile, colorfulness: strength });
      });
    }
  }
});

test('per-slot lock keeps exact original spelling and alpha while other duplicate-name IDs change', (t) => {
  const s = setup(t);
  const before = structuredClone(s.model);
  s.preview();
  s.change('[data-chroma-lock="slot-mid"]', true);
  assertUnchanged(s, before);
  if (!s.query('[data-chroma-apply]')) s.preview();
  s.apply();
  assertBatch(s, before.colorSlots, {}, ['slot-mid']);
  assert.deepEqual(s.calls[0].payload.updates.find(({ slotId }) => slotId === 'slot-mid').value, {
    hex: '#A44799',
    alpha: 0.123456789,
  });
});

test('unlocking restores automatic chroma without authoring a command before Apply', (t) => {
  const s = setup(t);
  const before = structuredClone(s.model);
  s.change('[data-chroma-lock="slot-mid"]', true);
  s.preview();
  s.change('[data-chroma-lock="slot-mid"]', false);
  assertUnchanged(s, before);
  if (!s.query('[data-chroma-apply]')) s.preview();
  s.apply();
  assertBatch(s, before.colorSlots);
});

test('stale Session and revision reject preview creation and applying an existing preview', async (t) => {
  for (const stale of ['session', 'revision']) {
    for (const phase of ['preview', 'apply']) {
      await t.test(`${stale}: ${phase}`, (t) => {
        const s = setup(t);
        const before = structuredClone(s.model);
        if (phase === 'apply') s.preview();
        if (stale === 'session')
          s.session = { state: { sessionId: 'replacement-session', expectedRevision: 3 } };
        else s.session.state.expectedRevision++;
        s.click(phase === 'apply' ? '[data-chroma-apply]' : '[data-chroma-preview]');
        assertUnchanged(s, before);
        assert.equal(s.query('[data-chroma-apply]'), null);
        assert.ok(s.notices.length, 'stale action should explain rejection');
        assert.equal(s.notices.at(-1)[1], true);
      });
    }
  }
});

test('rendering after a changed Session invalidates rather than restamping an old preview', (t) => {
  const s = setup(t);
  const before = structuredClone(s.model);
  s.preview();
  s.session.state.expectedRevision++;
  s.render();
  const apply = s.query('[data-chroma-apply]');
  if (apply) s.fire(apply, 'click');
  assert.equal(s.query('[data-chroma-apply]'), null);
  assertUnchanged(s, before);
  s.preview();
  s.apply();
  assertBatch(s, before.colorSlots);
});

test('changed document colors reject an old preview even without a revision update', async (t) => {
  for (const change of ['hex', 'alpha', 'remove', 'add', 'replace-document']) {
    await t.test(change, (t) => {
      const s = setup(t);
      s.preview();
      if (change === 'hex') s.model.colorSlots[1].value.hex = '#ffffff';
      if (change === 'alpha') s.model.colorSlots[1].value.alpha = 0.987654321;
      if (change === 'remove') s.model.colorSlots.splice(1, 1);
      if (change === 'add')
        s.model.colorSlots.push({ id: 'added', name: 'New', value: { hex: '#123456', alpha: 1 } });
      if (change === 'replace-document')
        s.model = {
          ...s.model,
          colorSlots: [{ id: 'other', name: 'Other', value: { hex: '#123456', alpha: 1 } }],
        };
      const before = structuredClone(s.model);
      s.apply();
      assertUnchanged(s, before);
      assert.equal(s.query('[data-chroma-apply]'), null);
    });
  }
});

test('close, toggle-close and Escape discard previews across reopening without authored changes', async (t) => {
  for (const close of ['button', 'toggle', 'escape']) {
    await t.test(close, (t) => {
      const s = setup(t);
      const before = structuredClone(s.model);
      s.preview();
      if (close === 'button') s.click('[data-shared-color-close]');
      if (close === 'toggle') s.panel.toggle();
      if (close === 'escape') s.fire(s.query('[data-chroma-apply]'), 'keydown', { key: 'Escape' });
      assert.equal(s.panel.open, false);
      s.panel.toggle();
      assert.equal(s.query('[data-chroma-apply]'), null);
      assertUnchanged(s, before);
      s.preview();
      s.apply();
      assertBatch(s, before.colorSlots);
    });
  }
});

test('an already-queued detached or foreign Apply cannot author a second batch', (t) => {
  const s = setup(t);
  s.preview();
  const apply = s.query('[data-chroma-apply]');
  s.apply();
  assert.equal(s.calls.length, 1);
  const click = (target) => {
    let stopped = false;
    const event = {
      target,
      stopImmediatePropagation() {
        stopped = true;
      },
    };
    for (const handler of s.handlers.get('click')) {
      handler(event);
      if (stopped) break;
    }
  };
  click(apply);
  s.document.body.append(apply);
  click(apply);
  assert.equal(s.calls.length, 1);
});

test('a rejected batch reports the error without changing authored colors', (t) => {
  const s = setup(t);
  const before = structuredClone(s.model);
  s.preview();
  s.failDispatch(Error('typed Session rejected this batch'));
  s.apply();
  assertUnchanged(s, before);
  assert.match(s.notices.at(-1)[0], /typed Session rejected this batch/);
  assert.equal(s.notices.at(-1)[1], true);
});

test('real Session Apply is one Undo/Redo step and preview/cancel preserve pending Redo', (t) => {
  const s = setup(t, { realSession: true });
  const before = serialize(s.currentDocument());
  const state = s.session.state;
  const past = s.session.historyView.past.length;
  s.preview();
  assert.equal(s.session.historyView.past.length, past);
  s.apply();
  const after = serialize(s.currentDocument());
  assert.notEqual(after, before);
  assert.equal(s.calls.length, 1);
  assert.equal(s.calls[0].type, 'set-color-slots');
  assert.deepEqual(s.calls[0].state, state);
  assert.equal(s.session.historyView.past.length, past + 1);
  assert.equal(s.session.state.expectedRevision, state.expectedRevision + 1);
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  assert.equal(serialize(s.currentDocument()), before);
  s.render();
  const undoneState = structuredClone(s.session.state);
  const future = structuredClone(s.session.historyView.future);
  s.preview();
  s.click('[data-chroma-cancel]');
  assert.deepEqual(s.session.state, undoneState);
  assert.deepEqual(s.session.historyView.future, future);
  s.session.dispatch({ ...s.session.state, type: 'redo', payload: {} });
  assert.equal(serialize(s.currentDocument()), after);
});

test('HEX conversion agrees with primary-color OKLCH vectors and rejects malformed input', () => {
  const close = (actual, expected, tolerance) =>
    assert.ok(Math.abs(actual - expected) < tolerance, `${actual} != ${expected}`);
  for (const [hex, L, C, h] of [
    ['#ff0000', 0.6279553606, 0.2576833077, 29.2338851923],
    ['#00ff00', 0.8664396115, 0.2948272403, 142.4953388878],
    ['#0000ff', 0.4520137184, 0.3132143717, 264.0520206381],
  ]) {
    const actual = colorChromaCoordinates(hex);
    close(actual.L, L, 1e-9);
    close(actual.C, C, 1e-9);
    close(actual.h, h, 1e-8);
    assert.deepEqual(colorChromaCoordinates(hex.toUpperCase()), actual);
  }
  for (const hex of [null, 12, '#abc', '#12345678', '#12gg56', '123456', '#123456 '])
    assert.throws(() => colorChromaCoordinates(hex), /Invalid shared color/);
});

test('every neutral sRGB byte remains an exact protected original, not invented hue', (t) => {
  const s = setup(t);
  s.model.colorSlots = Array.from({ length: 256 }, (_, i) => {
    const byte = i.toString(16).padStart(2, '0');
    const hex = `#${byte.repeat(3)}`;
    const coordinates = colorChromaCoordinates(hex);
    assert.ok(coordinates.L >= 0 && coordinates.L <= 1);
    assert.ok(coordinates.C < 1e-6);
    assert.equal(coordinates.h, 0);
    return {
      id: `gray-${i}`,
      name: `Gray ${i}`,
      value: { hex: i % 2 ? hex.toUpperCase() : hex, alpha: i / 255 },
    };
  });
  s.render();
  const before = structuredClone(s.model);
  s.change('[data-chroma-profile]', 'vibrant');
  s.change('[data-chroma-strength]', '1');
  s.preview();
  assertUnchanged(s, before);
  s.apply();
  assert.equal(s.calls.length, 1);
  assert.deepEqual(
    s.calls[0].payload.updates,
    before.colorSlots.map(({ id, value }) => ({ slotId: id, value })),
  );
});

test('invalid strength, profile and missing lock identities discard preview without authorship', async (t) => {
  for (const value of ['', '-0.01', '1.01', 'not-a-number', 'Infinity']) {
    await t.test(`strength ${JSON.stringify(value)}`, (t) => {
      const s = setup(t);
      const before = structuredClone(s.model);
      s.preview();
      s.change('[data-chroma-strength]', value);
      assertUnchanged(s, before);
      assert.equal(s.query('[data-chroma-apply]'), null);
      assert.match(s.notices.at(-1)[0], /between zero and one/);
    });
  }
  for (const kind of ['profile', 'lock']) {
    await t.test(kind, (t) => {
      const s = setup(t);
      const before = structuredClone(s.model);
      s.preview();
      if (kind === 'profile') s.change('[data-chroma-profile]', 'not-a-profile');
      else {
        const el = s.query('[data-chroma-lock]');
        el.dataset.chromaLock = 'removed-slot';
        el.checked = true;
        s.fire(el);
      }
      assertUnchanged(s, before);
      assert.equal(s.query('[data-chroma-apply]'), null);
      assert.ok(s.notices.length);
      assert.equal(s.notices.at(-1)[1], true);
    });
  }
});

test('empty palettes offer no broken preview actions and no authored change', (t) => {
  const s = setup(t);
  s.model.colorSlots = [];
  s.render();
  assert.equal(s.query('[data-chroma-preview]'), null);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.deepEqual(s.calls, []);
});

test('duplicate-name lock focus survives structural renders on the same stable slot', (t) => {
  const s = setup(t, { restoreRenderFocus: true });
  const before = structuredClone(s.model);
  const middle = s.query('[data-chroma-lock="slot-mid"]');
  middle.focus();
  middle.checked = true;
  s.fire(middle);
  assert.equal(middle.isConnected, false, 'exercise replacement rather than in-place DOM edits');
  assert.equal(s.document.activeElement, s.query('[data-chroma-lock="slot-mid"]'));
  assert.equal(s.query('[data-chroma-lock="slot-mid"]').checked, true);
  assert.equal(s.query('[data-chroma-lock="slot-dark"]').checked, false);
  const profile = s.query('[data-chroma-profile]');
  profile.focus();
  profile.value = 'soft';
  s.fire(profile);
  assert.equal(s.document.activeElement, s.query('[data-chroma-profile]'));
  // happy-dom misreports .value for a parsed nonfirst selected option. Verify
  // serialized selection and the actual proposal below instead.
  assert.ok(s.query('[data-chroma-profile] option[value="soft"]').hasAttribute('selected'));
  const strength = s.query('[data-chroma-strength]');
  strength.focus();
  strength.value = '0.38';
  s.fire(strength);
  assert.equal(s.document.activeElement, s.query('[data-chroma-strength]'));
  assert.equal(s.query('[data-chroma-strength]').value, '0.38');
  assertUnchanged(s, before);
  s.preview();
  s.apply();
  assertBatch(s, before.colorSlots, { profile: 'soft', colorfulness: 0.38 }, ['slot-mid']);
});

test('hostile names remain text in lock labels and previews, never executable markup', (t) => {
  const s = setup(t);
  const hostile = '\"><img src=x onerror="bad()"> & <script>bad()</script>';
  s.model.colorSlots[1].name = hostile;
  s.render();
  s.preview();
  assert.ok(s.query('.chroma-preview').textContent.includes(hostile));
  assert.ok(
    s.query('[data-chroma-lock="slot-mid"]').closest('label').textContent.includes(hostile),
  );
  assert.equal(s.query('img,script,[onerror]'), null);
  assert.deepEqual(s.calls, []);
});

test('Auto Chroma events stop before later application handlers and ancestors', (t) => {
  const s = setup(t);
  let appEvents = 0,
    bodyEvents = 0;
  for (const type of ['click', 'change']) {
    s.app.addEventListener(type, () => appEvents++);
    s.document.body.addEventListener(type, () => bodyEvents++);
  }
  s.change('[data-chroma-profile]', 'soft');
  s.change('[data-chroma-strength]', '0.5');
  s.change('[data-chroma-lock="slot-mid"]', true);
  s.preview();
  s.click('[data-chroma-cancel]');
  s.preview();
  s.apply();
  assert.equal(appEvents, 0);
  assert.equal(bodyEvents, 0);
  assert.equal(s.calls.length, 1);
});

test('stale profile, strength and lock controls reject queued changes and clear previews', async (t) => {
  for (const stale of ['session', 'revision']) {
    for (const selector of [
      '[data-chroma-profile]',
      '[data-chroma-strength]',
      '[data-chroma-lock]',
    ]) {
      await t.test(`${stale}: ${selector}`, (t) => {
        const s = setup(t);
        const before = structuredClone(s.model);
        s.preview();
        const el = s.query(selector);
        if (el.type === 'checkbox') el.checked = true;
        else el.value = el.tagName === 'SELECT' ? 'soft' : '0.5';
        if (stale === 'session')
          s.session = { state: { sessionId: 'replacement-session', expectedRevision: 3 } };
        else s.session.state.expectedRevision++;
        s.fire(el);
        assertUnchanged(s, before);
        assert.equal(s.query('[data-chroma-apply]'), null);
        assert.match(s.notices.at(-1)[0], /stale/);
      });
    }
  }
});

test('a new Session resets temporary profile, strength and locks rather than leaking prior settings', (t) => {
  const s = setup(t);
  const before = structuredClone(s.model);
  s.change('[data-chroma-profile]', 'soft');
  s.change('[data-chroma-strength]', '0.38');
  s.change('[data-chroma-lock="slot-mid"]', true);
  s.preview();
  s.session = { state: { sessionId: 'replacement-session', expectedRevision: 9 } };
  s.render();
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.equal(s.query('[data-chroma-profile]').value, 'balanced');
  assert.equal(Number(s.query('[data-chroma-strength]').value), 0.72);
  assert.ok([...s.app.querySelectorAll('[data-chroma-lock]')].every((el) => !el.checked));
  assertUnchanged(s, before);
  s.preview();
  s.apply();
  assertBatch(s, before.colorSlots);
});

function assertControls(s, design) {
  // happy-dom's select.value can misreport a parsed nonfirst selected option.
  assert.ok(
    s.query(`[data-chroma-profile] option[value="${design.profile}"]`).hasAttribute('selected'),
  );
  assert.equal(Number(s.query('[data-chroma-strength]').value), design.colorfulness);
  assert.equal(s.query('[data-chroma-area]').checked, design.areaAware);
  for (const el of s.app.querySelectorAll('[data-chroma-lock]'))
    assert.equal(el.checked, design.lockedSlotIds.includes(el.dataset.chromaLock));
}
const defaultDesign = () => ({
  version: 1,
  profile: 'balanced',
  colorfulness: 0.72,
  areaAware: true,
  lockedSlotIds: [],
});

test('Apply saves controls; Undo/Redo and a native reopen restore every saved setting, not local drafts', (t) => {
  const s = setup(t, { realSession: true });
  const initial = serialize(s.currentDocument());
  const design = {
    version: 1,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: ['slot-mid'],
  };
  s.change('[data-chroma-profile]', design.profile);
  s.change('[data-chroma-strength]', String(design.colorfulness));
  s.change('[data-chroma-area]', false);
  s.change('[data-chroma-lock="slot-mid"]', true);
  s.preview();
  s.apply();
  const saved = serialize(s.currentDocument());
  assert.deepEqual(s.currentDocument().colorDesign, design);
  assertControls(s, design);
  s.change('[data-chroma-profile]', 'vibrant');
  s.change('[data-chroma-strength]', '1');
  s.change('[data-chroma-area]', true);
  s.change('[data-chroma-lock="slot-mid"]', false);
  s.preview();
  s.click('[data-chroma-cancel]');
  assert.equal(serialize(s.currentDocument()), saved, 'cancel never saves draft settings');
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.currentDocument()), initial);
  assertControls(s, defaultDesign());
  s.session.dispatch({ ...s.session.state, type: 'redo', payload: {} });
  s.render();
  assertControls(s, design);
  s.change('[data-chroma-strength]', '0.11');
  s.session = new CommandSession(deserialize(saved));
  s.render();
  assertControls(s, design);
  assert.equal(s.query('[data-chroma-apply]'), null);
  // A second panel has no in-memory setting cache from the original Apply.
  const reopened = setup(t, { realSession: true });
  reopened.session = new CommandSession(deserialize(saved));
  reopened.render();
  assertControls(reopened, design);
  assert.deepEqual(reopened.calls, []);
});

test('switching between saved documents and an unsaved new Session never leaks another document draft', (t) => {
  const s = setup(t, { realSession: true });
  const docA = s.currentDocument();
  docA.schemaVersion = 55;
  docA.colorDesign = {
    version: 1,
    profile: 'fidelity',
    colorfulness: 0,
    areaAware: false,
    lockedSlotIds: ['slot-light'],
  };
  const docB = structuredClone(docA);
  docB.colorDesign = {
    version: 1,
    profile: 'uniform',
    colorfulness: 1,
    areaAware: true,
    lockedSlotIds: ['slot-dark', 'slot-mid'],
  };
  for (const doc of [docA, docB, docA]) {
    s.session = new CommandSession(deserialize(serialize(doc)));
    s.render();
    assertControls(s, doc.colorDesign);
    s.change('[data-chroma-profile]', 'vibrant');
    s.change('[data-chroma-strength]', '0.25');
    s.preview();
    assert.equal(serialize(s.currentDocument()), serialize(doc));
  }
  const fresh = blankDocument();
  fresh.schemaVersion = 54;
  fresh.colorSlots = initialSlots();
  s.session = new CommandSession(fresh);
  s.render();
  assertControls(s, defaultDesign());
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.deepEqual(s.calls, []);
});

test('literal color edits neither persist nor reset current draft controls', (t) => {
  const s = setup(t, { realSession: true });
  s.preview();
  s.apply();
  const savedDesign = structuredClone(s.currentDocument().colorDesign);
  const draft = {
    version: 1,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: ['slot-mid'],
  };
  s.change('[data-chroma-profile]', draft.profile);
  s.change('[data-chroma-strength]', String(draft.colorfulness));
  s.change('[data-chroma-area]', false);
  s.change('[data-chroma-lock="slot-mid"]', true);
  s.preview();
  const priorCommands = s.calls.length;
  s.session.dispatch({
    ...s.session.state,
    type: 'set-color-slot',
    payload: {
      slotId: 'slot-dark',
      value: { hex: '#abcdef', alpha: 0.5 },
    },
  });
  s.render();
  assertControls(s, draft);
  assert.deepEqual(s.currentDocument().colorDesign, savedDesign);
  assert.equal(
    s.query('[data-chroma-apply]'),
    null,
    'literal edit invalidates the old color preview',
  );
  assert.equal(s.calls.length, priorCommands, 'rerender must not author a design save');
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assertControls(s, draft);
  assert.deepEqual(s.currentDocument().colorDesign, savedDesign);
});

test('all locked colors still save newly applied settings in one authored step; identical repeat is a no-op', (t) => {
  const s = setup(t, { realSession: true });
  const before = serialize(s.currentDocument());
  const colors = structuredClone(s.currentDocument().colorSlots);
  const past = s.session.historyView.past.length;
  for (const { id } of colors) s.change(`[data-chroma-lock="${id}"]`, true);
  s.change('[data-chroma-profile]', 'soft');
  s.change('[data-chroma-strength]', '0.38');
  s.change('[data-chroma-area]', false);
  s.preview();
  s.apply();
  const design = {
    version: 1,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: colors.map(({ id }) => id),
  };
  assert.deepEqual(s.currentDocument().colorSlots, colors);
  assert.deepEqual(s.currentDocument().colorDesign, design);
  assert.equal(s.session.historyView.past.length, past + 1);
  const saved = serialize(s.currentDocument());
  const state = structuredClone(s.session.state);
  s.preview();
  s.apply();
  assert.equal(serialize(s.currentDocument()), saved);
  assert.deepEqual(s.session.state, state);
  assert.equal(s.session.historyView.past.length, past + 1);
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.currentDocument()), before);
  assertControls(s, defaultDesign());
  s.session.dispatch({ ...s.session.state, type: 'redo', payload: {} });
  s.render();
  assert.equal(serialize(s.currentDocument()), saved);
  assertControls(s, design);
});
