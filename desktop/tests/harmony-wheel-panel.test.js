import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createSharedColorsPanel } from '../src/shared-colors-panel.js';
import { createHarmonyWheelPanel } from '../src/harmony-wheel-panel.js';
import {
  DEFAULT_HARMONY_WHEEL,
  buildHarmonyWheel,
  resolveHarmonyPalette,
  wheelPoint,
} from '../src/harmony-wheel.js';
import { colorChromaCoordinates } from '../src/auto-chroma-panel.js';
import { captureFocus, restoreFocus } from '../src/focus.js';
import { proposeAutoChroma } from '../src/auto-chroma.js';
import { blankDocument, CommandSession, serialize, deserialize } from '../src/core.js';

function fixture() {
  return {
    layers: [],
    colorSlots: [
      { id: 'primary', name: 'Same', value: { hex: '#A44799', alpha: 0.123456789 } },
      { id: 'secondary', name: 'Same', value: { hex: '#254360', alpha: 0 } },
      { id: 'accent', name: 'Accent', value: { hex: '#991100', alpha: 0.8 } },
      { id: 'background', name: 'Paper', value: { hex: '#ffffff', alpha: 1 } },
      { id: 'text', name: 'Ink', value: { hex: '#000000', alpha: 0.2 } },
      { id: 'unmanaged', name: 'Other', value: { hex: '#33aa77', alpha: 0.5 } },
    ],
    colorDesign: {
      version: 2,
      profile: 'balanced',
      colorfulness: 0.72,
      areaAware: true,
      lockedSlotIds: [],
      wheel: { ...DEFAULT_HARMONY_WHEEL },
      palette: [
        { id: 'primary', kind: 'chromatic', position: 0, ring: 2, role: 'Primary' },
        { id: 'secondary', kind: 'chromatic', position: 1, ring: 3, role: 'Secondary' },
        { id: 'accent', kind: 'chromatic', position: 2, ring: 4, role: 'Accent' },
        { id: 'background', kind: 'neutral', L: 0.94, role: 'Background' },
        { id: 'text', kind: 'neutral', L: 0.1, role: 'Text' },
      ],
    },
  };
}
function setup(
  t,
  {
    realSession = false,
    empty = false,
    restoreRenderFocus = false,
    live = false,
    nativeChangeOnRemoval = false,
    deferAdvancedToggle = false,
  } = {},
) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<main></main>';
  const app = win.document.querySelector('main');
  if (deferAdvancedToggle)
    app.addEventListener(
      'toggle',
      (event) => {
        // Model the browser's interval before its queued toggle is delivered.
        if (event.target.matches?.('[data-color-advanced]')) event.stopImmediatePropagation();
      },
      true,
    );
  let model = fixture(),
    session = { state: { sessionId: 'session-a', expectedRevision: 3 } },
    coverage = null;
  if (realSession) {
    const document = blankDocument();
    if (!empty) {
      document.schemaVersion = 56;
      document.colorSlots = model.colorSlots;
      document.colorDesign = model.colorDesign;
    }
    session = new CommandSession(document);
  }
  const currentDocument = () => (realSession ? session.snapshot() : model);
  const calls = [],
    notices = [];
  let removalChanges = 0;
  const render = () => {
    const focus = restoreRenderFocus ? captureFocus(app) : null;
    const html = panel.html();
    const active = win.document.activeElement;
    if (
      nativeChangeOnRemoval &&
      removalChanges === 0 &&
      active?.matches('input[type="number"]') &&
      active.value !== active.defaultValue
    ) {
      removalChanges++;
      active.dispatchEvent(new win.Event('change', { bubbles: true }));
    }
    app.innerHTML = html;
    if (restoreRenderFocus) restoreFocus(app, focus);
  };
  const panel = createSharedColorsPanel({
    app,
    live,
    getSession: () => session,
    getDocument: currentDocument,
    getCoverageSnapshot: () => coverage,
    dispatch: (type, payload, state) => {
      calls.push({ type, payload: structuredClone(payload), state });
      return realSession ? session.dispatch({ ...state, type, payload }) : undefined;
    },
    render,
    notify: (...args) => notices.push(args),
  });
  const query = (s) => app.querySelector(s);
  const fire = (target, type = 'change', init = {}) => {
    assert.ok(target, `Missing ${type} target`);
    const Event =
      type.startsWith('pointer') || type === 'lostpointercapture'
        ? win.PointerEvent
        : type === 'keydown'
          ? win.KeyboardEvent
          : win.Event;
    const event = new Event(type, { bubbles: true, cancelable: true, isPrimary: true, ...init });
    target.dispatchEvent(event);
    return event;
  };
  const click = (selector) => fire(query(selector), 'click');
  const change = (selector, value) => {
    const el = query(selector);
    assert.ok(el, selector);
    if (el.type === 'checkbox') el.checked = value;
    else el.value = String(value);
    fire(el);
  };
  panel.toggle();
  return {
    win,
    app,
    panel,
    calls,
    notices,
    render,
    query,
    fire,
    click,
    change,
    get removalChanges() {
      return removalChanges;
    },
    get model() {
      return currentDocument();
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
    set coverage(value) {
      coverage = value;
    },
    preview: () => click('[data-chroma-preview]'),
    apply: () => click('[data-chroma-apply]'),
    drag(id = 'primary') {
      const marker = query(`[data-wheel-marker="${id}"]`);
      marker.closest('svg').getBoundingClientRect = () => ({
        left: 0,
        top: 0,
        width: 230,
        height: 230,
      });
      fire(marker, 'pointerdown', { pointerId: 7, button: 0, clientX: 115, clientY: 75 });
    },
    move(x, y) {
      fire(app, 'pointermove', { pointerId: 7, clientX: x, clientY: y });
    },
  };
}

test('v2 wheel precedes controls/swatches and renders exact discrete centered markers, not a polygon', (t) => {
  const s = setup(t);
  const html = s.app.innerHTML;
  assert.ok(html.indexOf('harmony-wheel-panel') < html.indexOf('data-chroma-profile'));
  assert.ok(html.indexOf('harmony-wheel-panel') < html.indexOf('shared-color-list'));
  assert.ok(!s.query('polygon'));
  assert.equal(s.app.querySelectorAll('.harmony-wheel-grid path').length, 84);
  assert.equal(s.app.querySelectorAll('.harmony-wheel-spokes line').length, 3);
  for (const item of s.model.colorDesign.palette.filter((p) => p.kind === 'chromatic')) {
    const p = wheelPoint(DEFAULT_HARMONY_WHEEL, item.position, item.ring);
    assert.equal(
      s.query(`[data-wheel-marker="${item.id}"]`).getAttribute('transform'),
      `translate(${p.x * 100} ${p.y * 100})`,
    );
  }
  assert.deepEqual(s.calls, []);
});

test('empty Shared Colors still offers one explicit Create palette action with stamped Session', (t) => {
  const s = setup(t);
  s.model = { layers: [], colorSlots: [] };
  s.render();
  assert.ok(s.query('[data-harmony-create]'));
  s.click('[data-harmony-create]');
  assert.deepEqual(s.calls, [
    { type: 'create-harmony-palette', payload: {}, state: s.session.state },
  ]);
  assert.notEqual(s.calls[0].state, s.session.state);
  assert.deepEqual(s.model.colorSlots, []);
});

test('preview/cancel are local; Apply sends all stable IDs and exact v2 recipe once with current alpha/Keep', (t) => {
  const s = setup(t),
    before = structuredClone(s.model);
  s.change('[data-chroma-lock="primary"]', true);
  s.change('[data-wheel-setting="hueRotation"]', 48);
  s.change('[data-wheel-entry]', 'text');
  s.change('[data-wheel-lightness]', 0.2);
  s.preview();
  assert.deepEqual(s.calls, []);
  s.click('[data-chroma-cancel]');
  assert.deepEqual(s.model, before);
  assert.ok(!s.query('[data-chroma-apply]'));
  s.preview();
  s.apply();
  assert.equal(s.calls.length, 1);
  const { type, payload, state } = s.calls[0];
  assert.equal(type, 'set-color-slots');
  assert.deepEqual(state, s.session.state);
  assert.equal(payload.design.version, 2);
  assert.deepEqual(payload.design.wheel, { ...DEFAULT_HARMONY_WHEEL, hueRotation: 48 });
  assert.deepEqual(payload.design.lockedSlotIds, ['primary']);
  assert.equal(payload.design.palette[4].L, 0.2);
  assert.ok(
    payload.design.palette.every((p) => !Object.hasOwn(p, 'alpha') && !Object.hasOwn(p, 'anchor')),
  );
  const expected = resolveHarmonyPalette(
    payload.design.wheel,
    payload.design.palette.map((p) => ({
      ...p,
      alpha: before.colorSlots.find((slot) => slot.id === p.id).value.alpha,
      ...(p.id === 'primary' ? { anchor: before.colorSlots[0].value } : {}),
    })),
  ).colors;
  for (const row of expected)
    assert.deepEqual(payload.updates.find((u) => u.slotId === row.id).value, row.value);
  assert.deepEqual(payload.updates[0].value, before.colorSlots[0].value);
  assert.deepEqual(
    payload.updates.map((u) => u.slotId),
    before.colorSlots.map((slot) => slot.id),
  );
  const other = before.colorSlots[5],
    { L, h } = colorChromaCoordinates(other.value.hex);
  assert.deepEqual(
    payload.updates[5].value,
    proposeAutoChroma([{ id: other.id, L, h, role: 'Primary', alpha: other.value.alpha }])[0].value,
  );
  assert.deepEqual(s.model, before);
});

test('hue rotation changes grid colors but leaves spoke geometry fixed; incompatible combinations are disabled and rejected', (t) => {
  const s = setup(t);
  const transforms = [...s.app.querySelectorAll('[data-wheel-marker]')].map((m) =>
    m.getAttribute('transform'),
  );
  const firstColor = s.query('.harmony-wheel-grid path').getAttribute('fill');
  s.change('[data-wheel-setting="hueRotation"]', 91);
  assert.notEqual(s.query('.harmony-wheel-grid path').getAttribute('fill'), firstColor);
  assert.deepEqual(
    [...s.app.querySelectorAll('[data-wheel-marker]')].map((m) => m.getAttribute('transform')),
    transforms,
  );
  assert.ok(s.query('[data-wheel-setting="hueSteps"] option[value="13"]').disabled);
  s.preview();
  s.change('[data-wheel-setting="hueSteps"]', 13);
  assert.match(s.notices.at(-1)[0], /Harmony does not fit/);
  assert.ok(!s.query('[data-chroma-apply]'));
  assert.ok(s.query('[data-wheel-setting="hueSteps"] option[value="12"]').hasAttribute('selected'));
  s.change('[data-wheel-setting="hueSteps"]', 9);
  assert.ok(s.query('[data-wheel-setting="harmony"] option[value="split"]').disabled);
  assert.ok(s.query('[data-wheel-setting="harmony"] option[value="complementary"]').disabled);
  assert.deepEqual(s.calls, []);
});

test('ring-count changes resnap every chromatic entry to the nearest new center, preserving IDs/roles/neutrals', (t) => {
  const s = setup(t),
    before = structuredClone(s.model.colorDesign.palette);
  s.change('[data-wheel-setting="lightnessSteps"]', 4);
  s.preview();
  s.apply();
  const after = s.calls[0].payload.design.palette;
  for (let i = 0; i < before.length; i++) {
    assert.deepEqual(
      after[i],
      before[i].kind === 'neutral'
        ? before[i]
        : { ...before[i], ring: Math.round(((before[i].ring + 0.5) * 4) / 7 - 0.5) },
    );
  }
});

test('all overlapping marker IDs are reachable through selection and keyboard/ring/role controls', (t) => {
  const s = setup(t);
  s.change('[data-wheel-setting="harmony"]', 'complementary');
  s.change('[data-wheel-entry]', 'accent');
  s.change('[data-wheel-ring]', 4);
  assert.equal(
    s.query('[data-wheel-marker="accent"]').getAttribute('transform'),
    s.query('[data-wheel-marker="secondary"]').getAttribute('transform'),
  );
  assert.equal(s.app.querySelectorAll('[data-wheel-entry] option').length, 5);
  s.change('[data-wheel-role]', 'Background');
  s.fire(s.query('[data-wheel-marker="accent"]'), 'keydown', { key: 'ArrowDown' });
  assert.equal(Number(s.query('[data-wheel-ring]').value), 5);
  s.fire(s.query('[data-wheel-marker="accent"]'), 'keydown', { key: 'Home' });
  assert.equal(Number(s.query('[data-wheel-ring]').value), 1);
  s.fire(s.query('[data-wheel-marker="accent"]'), 'keydown', { key: 'End' });
  assert.equal(Number(s.query('[data-wheel-ring]').value), 7);
  s.change('[data-wheel-position]', 0);
  s.preview();
  s.apply();
  assert.deepEqual(s.calls[0].payload.design.palette[2], {
    id: 'accent',
    kind: 'chromatic',
    position: 0,
    ring: 6,
    role: 'Background',
  });
});

test('radial drag changes only ring, not angle, and authors nothing until Apply', (t) => {
  const s = setup(t),
    before = structuredClone(s.model);
  s.preview();
  s.drag();
  assert.ok(!s.query('[data-chroma-apply]'));
  s.move(210, 115); // Right-hand pointer still moves the upward primary spoke radially.
  assert.equal(Number(s.query('[data-wheel-ring]').value), 7);
  const p = wheelPoint(DEFAULT_HARMONY_WHEEL, 0, 6);
  assert.equal(
    s.query('[data-wheel-marker="primary"]').getAttribute('transform'),
    `translate(${p.x * 100} ${p.y * 100})`,
  );
  s.fire(s.app, 'pointerup', { pointerId: 7 });
  assert.deepEqual(s.calls, []);
  assert.deepEqual(s.model, before);
  s.preview();
  s.apply();
  assert.equal(s.calls[0].payload.design.palette[0].ring, 6);
  assert.equal(s.calls[0].payload.design.palette[0].position, 0);
});

for (const type of ['pointercancel', 'lostpointercapture', 'Escape', 'blur'])
  test(`interrupted drag ${type} rolls back its ring without authored changes`, (t) => {
    const s = setup(t);
    s.drag();
    s.move(210, 115);
    if (type === 'Escape')
      s.fire(s.query('[data-wheel-marker="primary"]'), 'keydown', { key: 'Escape' });
    else if (type === 'blur') s.win.dispatchEvent(new s.win.Event('blur'));
    else s.fire(s.app, type, { pointerId: 7 });
    assert.equal(Number(s.query('[data-wheel-ring]').value), 3);
    assert.equal(s.panel.open, true);
    assert.deepEqual(s.calls, []);
  });

test('IME Escape does not interrupt drag or close the panel', (t) => {
  const s = setup(t);
  s.drag();
  s.move(210, 115);
  s.fire(s.query('[data-wheel-marker="primary"]'), 'keydown', { key: 'Escape', isComposing: true });
  assert.equal(Number(s.query('[data-wheel-ring]').value), 7);
  assert.equal(s.panel.open, true);
  s.fire(s.app, 'pointercancel', { pointerId: 7 });
});

test('a changed Session/revision cancels an in-flight pointer, and a new design reloads drafts', (t) => {
  const s = setup(t);
  s.drag();
  s.move(210, 115);
  s.session.state.expectedRevision++;
  s.move(115, 115);
  assert.equal(Number(s.query('[data-wheel-ring]').value), 3);
  s.change('[data-wheel-setting="hueRotation"]', 90);
  s.session = { state: { sessionId: 'replacement', expectedRevision: 0 } };
  s.render();
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 18);
  assert.deepEqual(s.calls, []);
});

test('Add is explicit, supports either kind and refuses to discard any unapplied wheel/settings draft', (t) => {
  const s = setup(t);
  s.change('[data-wheel-setting="hueRotation"]', 60);
  s.click('[data-harmony-add]');
  assert.deepEqual(s.calls, []);
  assert.match(s.notices.at(-1)[0], /Apply.*before adding/);
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 60);
  s.click('[data-chroma-reload]');
  s.click('[data-harmony-add]');
  assert.deepEqual(s.calls[0], {
    type: 'add-harmony-color',
    payload: { kind: 'chromatic' },
    state: s.session.state,
  });
  s.change('[data-wheel-add-kind]', 'neutral');
  s.click('[data-harmony-add]');
  assert.deepEqual(s.calls[1].payload, { kind: 'neutral' });
  s.change('[data-chroma-strength]', 0.2);
  s.click('[data-harmony-create]');
  assert.equal(s.calls.length, 2);
});

test('literal edits invalidate preview without persisting/resetting v2 drafts; saved-design changes reload them', (t) => {
  const s = setup(t);
  s.change('[data-wheel-setting="hueRotation"]', 60);
  s.change('[data-wheel-entry]', 'accent');
  s.change('[data-wheel-role]', 'Background');
  s.preview();
  s.model.colorSlots[0].value.hex = '#001122';
  s.session.state.expectedRevision++;
  s.render();
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 60);
  assert.ok(!s.query('[data-chroma-apply]'));
  assert.equal(s.model.colorDesign.wheel.hueRotation, 18);
  s.preview();
  s.apply();
  assert.equal(s.calls[0].payload.design.palette[2].role, 'Background');
  s.model.colorDesign.wheel.hueRotation = 33;
  s.render();
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 33);
});

test('stale wheel controls/Create/Add are rejected and events do not reach later app handlers', (t) => {
  const s = setup(t);
  let leaked = 0;
  s.app.addEventListener('change', () => leaked++);
  s.app.addEventListener('click', () => leaked++);
  s.change('[data-wheel-role]', 'Accent');
  assert.equal(leaked, 0);
  s.session.state.expectedRevision++;
  s.change('[data-wheel-setting="hueRotation"]', 90);
  assert.match(s.notices.at(-1)[0], /stale/);
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 18);
  s.session.state.expectedRevision++;
  s.click('[data-harmony-create]');
  assert.match(s.notices.at(-1)[0], /stale/);
  s.session.state.expectedRevision++;
  s.click('[data-harmony-add]');
  assert.match(s.notices.at(-1)[0], /stale/);
  assert.deepEqual(s.calls, []);
  assert.equal(leaked, 0);
});

test('full-registry coverage identity is passed with the existing stale scene/font guard', (t) => {
  const s = setup(t);
  s.coverage = {
    key: 'scene1:font1',
    value: {
      slots: s.model.colorSlots.map((slot) => ({ id: slot.id, percent: 100 / 6 })),
      other: { percent: 0 },
    },
  };
  s.preview();
  assert.ok(s.query('[data-chroma-apply]'));
  s.coverage = { key: 'scene2:font1', value: null };
  s.apply();
  assert.deepEqual(s.calls, []);
  assert.match(s.notices.at(-1)[0], /stale/);
});

test('grid rendering reuses unchanged settings for pointer/entry edits, and retains at most four grids', (t) => {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<main></main>';
  const app = win.document.querySelector('main'),
    doc = fixture();
  let builds = 0;
  const controller = createHarmonyWheelPanel({
    app,
    envelope: () => ({}),
    render: () => {},
    notify: () => {},
    onEdit: () => {},
    getState: () => ({}),
    buildGrid: (...args) => {
      builds++;
      return buildHarmonyWheel(...args);
    },
  });
  controller.synchronize(doc.colorDesign, 'a');
  const html = (colorfulness) => {
    app.innerHTML = controller.html({ slots: doc.colorSlots, profile: 'balanced', colorfulness });
  };
  html(0.72);
  html(0.72);
  assert.equal(builds, 1);
  const ring = app.querySelector('[data-wheel-ring]');
  ring.value = '6';
  ring.dispatchEvent(new win.Event('change', { bubbles: true }));
  html(0.72);
  assert.equal(builds, 1, 'entry coordinate updates must not recompute the grid');
  for (const strength of [0.1, 0.2, 0.3, 0.4]) html(strength);
  assert.equal(builds, 5);
  html(0.72);
  assert.equal(builds, 6, 'oldest of five keys was evicted');
});

test('real Session v2 Apply is one atomic Undo/Redo step and native round-trip restores wheel/roles', (t) => {
  const s = setup(t, { realSession: true });
  const before = serialize(s.model),
    past = s.session.historyView.past.length;
  s.change('[data-wheel-setting="hueRotation"]', 72);
  s.change('[data-wheel-entry]', 'accent');
  s.change('[data-wheel-role]', 'Secondary');
  s.change('[data-chroma-strength]', 0.45);
  s.preview();
  s.click('[data-chroma-cancel]');
  assert.equal(serialize(s.model), before);
  assert.equal(s.session.historyView.past.length, past);
  s.preview();
  s.apply();
  const after = serialize(s.model);
  assert.notEqual(after, before);
  assert.equal(s.session.historyView.past.length, past + 1);
  assert.equal(s.model.colorDesign.version, 2);
  assert.equal(s.model.colorDesign.wheel.hueRotation, 72);
  assert.equal(s.model.colorDesign.palette[2].role, 'Secondary');
  assert.equal(s.model.colorDesign.colorfulness, 0.45);
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), before);
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 18);
  const future = structuredClone(s.session.historyView.future);
  s.preview();
  s.click('[data-chroma-cancel]');
  assert.deepEqual(s.session.historyView.future, future);
  s.session.dispatch({ ...s.session.state, type: 'redo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), after);
  s.change('[data-wheel-setting="hueRotation"]', 99);
  s.session = new CommandSession(deserialize(after));
  s.render();
  assert.equal(Number(s.query('[data-wheel-setting="hueRotation"]').value), 72);
  assert.ok(s.query('[data-wheel-role] option[value="Secondary"]').hasAttribute('selected'));
});

test('real Session Create and Add each add one undoable palette update without replacing prior slots or bindings', (t) => {
  const s = setup(t, { realSession: true, empty: true });
  const initial = serialize(s.model);
  s.click('[data-harmony-create]');
  assert.equal(s.model.colorSlots.length, 5);
  assert.equal(s.model.colorDesign.palette.length, 5);
  const created = serialize(s.model),
    firstIds = s.model.colorSlots.map((slot) => slot.id),
    past = s.session.historyView.past.length;
  s.click('[data-harmony-add]');
  assert.equal(s.model.colorSlots.length, 6);
  assert.equal(s.model.colorDesign.palette.length, 6);
  assert.equal(s.model.colorDesign.palette[5].kind, 'chromatic');
  assert.deepEqual(
    s.model.colorSlots.slice(0, 5).map((slot) => slot.id),
    firstIds,
  );
  assert.equal(s.session.historyView.past.length, past + 1);
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), created);
  s.change('[data-wheel-add-kind]', 'neutral');
  s.click('[data-harmony-add]');
  assert.equal(s.model.colorDesign.palette[5].kind, 'neutral');
  assert.equal(s.model.colorDesign.palette[5].L, 0.5);
  const retained = structuredClone(s.model.colorSlots);
  s.click('[data-harmony-create]');
  assert.equal(s.model.colorSlots.length, 11);
  assert.deepEqual(s.model.colorSlots.slice(0, 6), retained);
  assert.equal(s.model.colorDesign.palette.length, 5);
  assert.deepEqual(s.model.layers, deserialize(initial).layers);
});

test('keyboard marker focus survives repeated structural renders; Delete/tool keys cannot reach graph handlers', (t) => {
  const s = setup(t, { restoreRenderFocus: true });
  const marker = () => s.query('[data-wheel-marker="secondary"]');
  marker().focus();
  let leaked = 0;
  s.app.addEventListener('keydown', () => leaked++);
  s.app.addEventListener('change', () => leaked++);
  s.fire(marker(), 'keydown', { key: 'ArrowDown' });
  assert.ok(s.win.document.activeElement === marker());
  assert.equal(Number(s.query('[data-wheel-ring]').value), 5);
  s.fire(marker(), 'keydown', { key: 'ArrowRight' });
  assert.ok(s.win.document.activeElement === marker());
  assert.equal(Number(s.query('[data-wheel-ring]').value), 6);
  for (const key of ['Delete', 'Backspace', 'r', 'v']) s.fire(marker(), 'keydown', { key });
  assert.equal(leaked, 0);
  assert.equal(s.fire(marker(), 'keydown', { key: 'Tab' }).defaultPrevented, false);
  s.change('[data-wheel-add-kind]', 'neutral');
  assert.equal(leaked, 0);
  assert.deepEqual(s.calls, []);
});

test('a retained palette field cannot retarget the newly selected stable color', (t) => {
  const s = setup(t);
  const oldRole = s.query('[data-wheel-role]');
  assert.equal(oldRole.dataset.wheelId, 'primary');
  s.change('[data-wheel-entry]', 'secondary');
  s.query('.harmony-entry-controls').append(oldRole);
  oldRole.value = 'Text';
  s.fire(oldRole);
  assert.match(s.notices.at(-1)?.[0] || '', /stale/);
  s.click('[data-chroma-preview]');
  s.click('[data-chroma-apply]');
  assert.equal(
    s.calls.at(-1).payload.design.palette.find((p) => p.id === 'primary').role,
    'Primary',
  );
  assert.equal(
    s.calls.at(-1).payload.design.palette.find((p) => p.id === 'secondary').role,
    'Secondary',
  );
});

test('per-color Chroma inherits global by default; an explicit override is a cancellable stable-ID draft', (t) => {
  const s = setup(t),
    before = structuredClone(s.model);
  assert.equal(s.query('[data-wheel-chroma-mode]').value, 'auto');
  assert.equal(s.query('[data-wheel-chroma]'), null);
  s.change('[data-wheel-chroma-mode]', 'override');
  assert.equal(
    Number(s.query('[data-wheel-chroma]').value),
    colorChromaCoordinates(before.colorSlots[0].value.hex).C,
  );
  s.change('[data-wheel-chroma]', 0.02);
  s.change('[data-chroma-strength]', 0);
  const grid = s.query('.harmony-wheel-grid').innerHTML;
  s.preview();
  assert.deepEqual(s.calls, []);
  assert.deepEqual(s.model, before);
  const proposed = () =>
    s.query('[data-wheel-marker="primary"] [data-wheel-marker-color]').style.fill;
  const manual = proposeAutoChroma(
    [
      {
        id: 'primary',
        ...wheelPoint(DEFAULT_HARMONY_WHEEL, 0, 2),
        role: 'Primary',
        overrideC: 0.02,
        alpha: before.colorSlots[0].value.alpha,
      },
    ],
    { colorfulness: 0 },
  )[0];
  assert.equal(proposed(), manual.value.hex);
  assert.equal(
    s.query('.harmony-wheel-grid').innerHTML,
    grid,
    'individual color never repaints the global wheel grid',
  );
  s.click('[data-chroma-cancel]');
  assert.deepEqual(s.model, before);
  assert.equal(
    s.query('[data-wheel-chroma-mode]').value,
    'override',
    'Cancel preview retains the local recipe draft',
  );
  assert.equal(
    proposed(),
    before.colorSlots[0].value.hex,
    'Cancel restores the saved marker swatch',
  );
  s.change('[data-wheel-entry]', 'secondary');
  assert.equal(s.query('[data-wheel-chroma-mode]').value, 'auto');
  s.change('[data-wheel-entry]', 'primary');
  assert.equal(Number(s.query('[data-wheel-chroma]').value), 0.02);
  s.preview();
  s.apply();
  const { design, updates } = s.calls.at(-1).payload;
  assert.equal(design.palette.find(({ id }) => id === 'primary').overrideC, 0.02);
  assert.ok(
    design.palette
      .filter(({ id }) => id !== 'primary')
      .every((entry) => !Object.hasOwn(entry, 'overrideC')),
  );
  assert.deepEqual(updates.find(({ slotId }) => slotId === 'primary').value, manual.value);
  assert.equal(updates.find(({ slotId }) => slotId === 'secondary').value.hex, '#636363');
  s.click('[data-chroma-reload]');
  assert.equal(s.query('[data-wheel-chroma-mode]').value, 'auto');
  assert.equal(s.query('[data-wheel-chroma]'), null);
});

test('override follows stable ID across palette/registry reorder and preserves Keep and neutral colors', (t) => {
  const s = setup(t);
  s.model.colorDesign.palette[2].overrideC = 0.04;
  s.model.colorDesign.palette.reverse();
  s.model.colorSlots.reverse();
  s.render();
  s.change('[data-wheel-entry]', 'accent');
  assert.equal(Number(s.query('[data-wheel-chroma]').value), 0.04);
  const anchor = structuredClone(s.model.colorSlots.find(({ id }) => id === 'accent').value);
  s.change('[data-chroma-lock="accent"]', true);
  s.change('[data-wheel-chroma]', 0.45);
  s.change('[data-wheel-setting="hueRotation"]', 136);
  s.preview();
  s.apply();
  const { design, updates } = s.calls.at(-1).payload;
  assert.equal(design.palette.find(({ id }) => id === 'accent').overrideC, 0.45);
  assert.deepEqual(updates.find(({ slotId }) => slotId === 'accent').value, anchor);
  assert.deepEqual(design.lockedSlotIds, ['accent']);
  s.change('[data-chroma-lock="accent"]', false);
  s.preview();
  assert.ok(s.query('[data-chroma-gamut-limited]'));
  s.apply();
  assert.notDeepEqual(
    s.calls.at(-1).payload.updates.find(({ slotId }) => slotId === 'accent').value,
    anchor,
  );
  for (const id of ['background', 'text']) {
    s.change('[data-wheel-entry]', id);
    assert.equal(s.query('[data-wheel-chroma-mode]'), null);
    assert.equal(s.query('[data-wheel-chroma]'), null);
    assert.equal(s.query('[data-wheel-chroma-neutral]').textContent, 'Chroma 0');
    const neutral = s.calls.at(-1).payload.updates.find(({ slotId }) => slotId === id).value.hex;
    assert.equal(neutral.slice(1, 3), neutral.slice(3, 5));
    assert.equal(neutral.slice(3, 5), neutral.slice(5, 7));
  }
});

test('Chroma fields reject malformed, stale and neutral-target edits without corrupting the last valid draft', (t) => {
  const s = setup(t);
  s.change('[data-wheel-chroma-mode]', 'override');
  s.change('[data-wheel-chroma]', 0);
  for (const value of ['', '-0.001', '0.450001', 'NaN', 'Infinity', 'text']) {
    s.preview();
    s.change('[data-wheel-chroma]', value);
    assert.match(s.notices.at(-1)[0], /Enter a palette coordinate|Chroma must/);
    assert.equal(s.query('[data-chroma-apply]'), null);
    assert.equal(Number(s.query('[data-wheel-chroma]').value), 0);
  }
  s.change('[data-wheel-chroma]', 0.45);
  const stale = s.query('[data-wheel-chroma]');
  s.change('[data-wheel-entry]', 'secondary');
  s.query('.harmony-entry-controls').append(stale);
  stale.value = '0.1';
  s.fire(stale);
  assert.match(s.notices.at(-1)[0], /stale/);
  s.change('[data-wheel-entry]', 'text');
  stale.dataset.wheelId = 'text';
  s.query('.harmony-entry-controls').append(stale);
  s.fire(stale);
  assert.match(s.notices.at(-1)[0], /Choose Chroma override/);
  s.change('[data-wheel-entry]', 'primary');
  assert.equal(Number(s.query('[data-wheel-chroma]').value), 0.45);
  s.session.state.expectedRevision++;
  s.change('[data-wheel-chroma]', 0.2);
  assert.match(s.notices.at(-1)[0], /stale/);
  assert.equal(Number(s.query('[data-wheel-chroma]').value), 0.45);
  assert.deepEqual(s.calls, []);
});

test('real Session Chroma Apply/Auto reset round-trip in one history step without upgrading untouched schema56', (t) => {
  const s = setup(t, { realSession: true });
  const before = serialize(s.model),
    past = s.session.historyView.past.length;
  s.change('[data-wheel-entry]', 'accent');
  s.change('[data-wheel-chroma-mode]', 'override');
  s.change('[data-wheel-chroma]', 0.031);
  s.preview();
  s.click('[data-chroma-cancel]');
  assert.equal(serialize(s.model), before);
  assert.equal(s.model.schemaVersion, 56);
  assert.equal(s.session.historyView.past.length, past);
  s.preview();
  s.apply();
  const after = serialize(s.model);
  assert.equal(s.model.schemaVersion, 57);
  assert.equal(s.model.colorDesign.palette.find(({ id }) => id === 'accent').overrideC, 0.031);
  assert.equal(s.session.historyView.past.length, past + 1);
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), before);
  assert.equal(s.query('[data-wheel-chroma-mode]').value, 'auto');
  const future = structuredClone(s.session.historyView.future);
  s.change('[data-wheel-chroma-mode]', 'override');
  s.change('[data-wheel-chroma]', 0.12);
  s.preview();
  s.click('[data-chroma-cancel]');
  assert.deepEqual(
    s.session.historyView.future,
    future,
    'local override drafts never consume Redo',
  );
  s.session.dispatch({ ...s.session.state, type: 'redo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), after);
  s.session = new CommandSession(deserialize(after));
  s.render();
  assert.equal(s.query('[data-wheel-chroma-mode]').value, 'override');
  assert.equal(Number(s.query('[data-wheel-chroma]').value), 0.031);
  s.change('[data-wheel-chroma-mode]', 'auto');
  s.preview();
  s.apply();
  assert.equal(s.model.schemaVersion, 57, 'returning to Auto never downgrades the native document');
  assert.ok(
    !Object.hasOwn(
      s.model.colorDesign.palette.find(({ id }) => id === 'accent'),
      'overrideC',
    ),
  );
  assert.deepEqual(deserialize(serialize(s.model)), s.model);
});

test('preset, ring resnap and role edits retain independent per-ID override values', (t) => {
  const s = setup(t);
  s.change('[data-wheel-chroma-mode]', 'override');
  s.change('[data-wheel-chroma]', 0.03);
  s.change('[data-wheel-entry]', 'secondary');
  s.change('[data-wheel-chroma-mode]', 'override');
  s.change('[data-wheel-chroma]', 0.08);
  s.change('[data-wheel-setting="harmony"]', 'analogous');
  s.change('[data-wheel-setting="lightnessSteps"]', 4);
  s.change('[data-wheel-role]', 'Accent');
  s.preview();
  s.apply();
  const entries = s.calls.at(-1).payload.design.palette;
  assert.equal(entries.find(({ id }) => id === 'primary').overrideC, 0.03);
  assert.equal(entries.find(({ id }) => id === 'secondary').overrideC, 0.08);
  assert.ok(
    !Object.hasOwn(
      entries.find(({ id }) => id === 'accent'),
      'overrideC',
    ),
  );
  assert.deepEqual(
    entries.map(({ id }) => id),
    s.model.colorDesign.palette.map(({ id }) => id),
  );
});

test('accepted empty, v1 and neutral-only states never require a missing marker hex', (t) => {
  const s = setup(t, { realSession: true, empty: true });
  assert.equal(s.query('[data-wheel-marker]'), null);
  s.click('[data-harmony-create]');
  const doc = s.model;
  doc.colorDesign.palette = doc.colorDesign.palette.filter(({ kind }) => kind === 'neutral');
  doc.colorSlots = doc.colorSlots.filter(({ id }) =>
    doc.colorDesign.palette.some((entry) => entry.id === id),
  );
  s.session = new CommandSession(doc);
  s.render();
  assert.equal(s.query('[data-wheel-marker]'), null);
  assert.equal(s.query('[data-wheel-chroma-neutral]').textContent, 'Chroma 0');
  s.preview();
  s.apply();
  assert.equal(s.notices.length, 0);
  const v1 = s.model;
  delete v1.colorDesign.wheel;
  delete v1.colorDesign.palette;
  v1.colorDesign.version = 1;
  s.session = new CommandSession(v1);
  s.render();
  assert.equal(s.query('.harmony-wheel-panel'), null);
  assert.equal(s.notices.length, 0);
});

test('palette status and one existing action row lead the draft; saved colors remain separate', (t) => {
  const s = setup(t, { realSession: true });
  const original = serialize(s.model);
  const state = () => s.query('[data-chroma-state]').dataset.chromaState;
  const uniqueActions = (preview) => {
    const bar = s.query('.chroma-draft-bar');
    assert.equal(s.query('.auto-chroma-controls').firstElementChild, bar);
    for (const attr of ['preview', ...(preview ? ['apply', 'cancel'] : [])]) {
      assert.equal(s.app.querySelectorAll(`[data-chroma-${attr}]`).length, 1);
      assert.ok(bar.querySelector(`[data-chroma-${attr}]`));
    }
    assert.equal(s.app.querySelectorAll('[data-chroma-reload]').length, 1);
    assert.equal(
      bar.querySelector('[data-chroma-reload]'),
      null,
      'Reload stays a secondary settings action',
    );
    assert.equal(s.app.querySelectorAll('[data-chroma-apply]').length, preview ? 1 : 0);
    assert.equal(s.app.querySelectorAll('[data-chroma-cancel]').length, preview ? 1 : 0);
  };
  assert.equal(state(), 'saved');
  assert.equal(s.query('[data-chroma-state]').getAttribute('role'), 'status');
  assert.equal(s.query('.shared-color-list').getAttribute('aria-label'), 'Saved shared colors');
  assert.equal(s.query('.shared-color-list').firstElementChild.textContent, 'Saved colors');
  uniqueActions(false);
  s.change('[data-wheel-setting="hueRotation"]', 75);
  assert.equal(state(), 'draft');
  assert.match(s.query('[data-chroma-state]').textContent, /not applied/);
  assert.equal(serialize(s.model), original);
  s.preview();
  assert.equal(state(), 'preview');
  assert.match(s.query('[data-chroma-state]').textContent, /Artwork still uses saved colors/);
  assert.match(s.query('.chroma-preview').textContent, /Proposed colors · not applied/);
  uniqueActions(true);
  assert.equal(serialize(s.model), original);
  s.click('[data-chroma-cancel]');
  assert.equal(state(), 'draft', 'Cancel discards the proposal, not its settings draft');
  assert.equal(serialize(s.model), original);
  s.click('[data-chroma-reload]');
  assert.equal(state(), 'saved');
  assert.equal(serialize(s.model), original);
  assert.equal(s.calls.length, 0);
  s.change('[data-wheel-setting="hueRotation"]', 75);
  s.preview();
  s.apply();
  assert.equal(state(), 'saved');
  assert.equal(s.calls.length, 1);
  assert.equal(s.calls[0].type, 'set-color-slots');
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), original, 'Apply is still one Undo');
  assert.equal(state(), 'saved');
});

test('status follows settings, not selection; invalidated previews show the remaining draft', (t) => {
  const s = setup(t);
  const state = () => s.query('[data-chroma-state]').dataset.chromaState;
  s.change('[data-wheel-entry]', 'accent');
  assert.equal(state(), 'saved', 'Selecting a marker is not a settings edit');
  s.change('[data-chroma-strength]', 0.4);
  assert.equal(state(), 'draft');
  s.preview();
  assert.equal(state(), 'preview');
  s.model.colorSlots[0].value.hex = '#abcdef';
  s.render();
  assert.equal(state(), 'draft', 'An invalidated proposal must not remain labeled Preview');
  assert.equal(s.query('[data-chroma-apply]'), null);
  s.click('[data-chroma-reload]');
  assert.equal(state(), 'saved');
  s.preview();
  s.session = { state: { sessionId: 'new-session', expectedRevision: 0 } };
  s.render();
  assert.equal(state(), 'saved', 'Session changes discard local proposals/settings');
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.deepEqual(s.calls, []);
});

test('moved actions keep stable keyboard focus, returning to Preview when Apply/Cancel disappear', (t) => {
  const s = setup(t, { realSession: true, restoreRenderFocus: true });
  const preview = () => s.query('[data-chroma-preview]');
  const active = () => s.win.document.activeElement;
  s.change('[data-wheel-setting="hueRotation"]', 75);
  preview().focus();
  s.preview();
  assert.ok(active() === preview(), 'Preview remains the same focus identity after rendering');
  assert.deepEqual(
    [...s.query('.chroma-draft-actions').children].map((el) => el.textContent),
    ['Preview', 'Apply', 'Cancel'],
    'Natural DOM Tab order reaches Apply and Cancel without entering the wheel',
  );
  assert.deepEqual(
    [...s.query('.chroma-draft-actions').children].map((el) => el.getAttribute('aria-label')),
    ['Preview palette', 'Apply colors', 'Cancel preview'],
    'Compact visual labels retain full accessible action names',
  );
  s.query('[data-chroma-cancel]').focus();
  s.click('[data-chroma-cancel]');
  assert.ok(active() === preview(), 'Focus returns to Preview');
  s.preview();
  s.query('[data-chroma-apply]').focus();
  s.apply();
  assert.ok(active() === preview(), 'Focus returns to Preview');
  s.preview();
  assert.equal(s.app.querySelectorAll('[data-chroma-apply]').length, 1);
  s.click('[data-chroma-cancel]');
  assert.equal(s.calls.length, 1, 'Repeated Preview/Cancel adds no history');
  assert.deepEqual(s.notices, []);
});

test('palette swatches immediately follow the wheel and select exact stable IDs without authoring', (t) => {
  const s = setup(t, { realSession: true });
  const before = serialize(s.model);
  const palette = s.query('.harmony-palette-swatches');
  assert.equal(palette.previousElementSibling, s.query('.harmony-wheel'));
  assert.deepEqual(
    [...palette.querySelectorAll('button')].map((el) => el.dataset.wheelSelect),
    s.model.colorDesign.palette.map((item) => item.id),
  );
  s.click('[data-wheel-select="text"]');
  assert.equal(s.query('[data-wheel-entry]').value, 'text');
  assert.equal(s.query('[data-wheel-lightness]').dataset.wheelId, 'text');
  assert.equal(serialize(s.model), before);
  assert.ok(s.query('.chroma-settings-scroll .shared-color-list'));
  assert.ok(!s.query('.chroma-settings-scroll .harmony-wheel'));
});

for (const [selector, original, expected] of [
  ['[data-wheel-setting="hueRotation"]', 0, 10],
  ['[data-wheel-setting="lightnessRange"]', 0.7, 0.8],
  ['[data-chroma-strength]', 0.72, 0.82],
])
  test(`rendered ${selector} scrubs as a view draft and Apply is exactly one Undo`, (t) => {
    const s = setup(t, { realSession: true, restoreRenderFocus: true });
    s.change(selector, original);
    const before = serialize(s.model),
      past = s.session.historyView.past.length;
    const input = s.query(selector);
    input.focus();
    s.fire(input, 'pointerdown', { pointerId: 21, button: 0, clientX: 20 });
    s.fire(s.app, 'pointermove', { pointerId: 21, clientX: 34 });
    assert.equal(Number(s.query(selector).value), expected);
    s.fire(s.app, 'pointerup', { pointerId: 21 });
    assert.equal(Number(s.query(selector).value), expected);
    assert.equal(serialize(s.model), before);
    assert.equal(s.session.historyView.past.length, past);
    s.preview();
    s.click('[data-chroma-cancel]');
    assert.equal(serialize(s.model), before);
    s.preview();
    s.apply();
    assert.equal(s.session.historyView.past.length, past + 1);
    s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
    s.render();
    assert.equal(serialize(s.model), before);
  });

for (const interruption of ['pointercancel', 'lostpointercapture', 'Escape', 'blur'])
  test(`wheel numeric ${interruption} restores the whole recipe and input`, (t) => {
    const s = setup(t, { realSession: true, restoreRenderFocus: true });
    const before = serialize(s.model),
      past = s.session.historyView.past.length;
    const input = s.query('[data-wheel-setting="lightnessSteps"]');
    const original = input.value;
    const markers = [...s.app.querySelectorAll('[data-wheel-marker]')].map((el) =>
      el.getAttribute('transform'),
    );
    input.focus();
    s.fire(input, 'pointerdown', { pointerId: 21, button: 0, clientX: 20 });
    s.fire(s.app, 'pointermove', { pointerId: 21, clientX: 26 });
    assert.notEqual(s.query('[data-wheel-setting="lightnessSteps"]').value, original);
    if (interruption === 'Escape') s.fire(s.app, 'keydown', { key: 'Escape' });
    else if (interruption === 'blur') s.win.dispatchEvent(new s.win.Event('blur'));
    else s.fire(s.app, interruption, { pointerId: 21 });
    assert.equal(s.query('[data-wheel-setting="lightnessSteps"]').value, original);
    assert.deepEqual(
      [...s.app.querySelectorAll('[data-wheel-marker]')].map((el) => el.getAttribute('transform')),
      markers,
    );
    assert.equal(serialize(s.model), before);
    assert.equal(s.session.historyView.past.length, past);
    assert.equal(s.panel.open, true, 'Escape cancels the gesture, not the whole panel');
  });

test('color scroll offsets survive repeated recipe edits and swatch selection', (t) => {
  const s = setup(t);
  s.query('.chroma-settings-scroll').scrollTop = 190;
  s.query('.harmony-palette-swatches').scrollLeft = 17;
  s.change('[data-wheel-setting="hueRotation"]', 22);
  assert.equal(s.query('.chroma-settings-scroll').scrollTop, 190);
  assert.equal(s.query('.harmony-palette-swatches').scrollLeft, 17);
  s.click('[data-wheel-select="background"]');
  assert.equal(s.query('.chroma-settings-scroll').scrollTop, 190);
  assert.equal(s.query('.harmony-palette-swatches').scrollLeft, 17);
});

test('live alpha drag uses stable IDs even when two saved colors have the same name', (t) => {
  const s = setup(t, { realSession: true, live: true });
  const before = serialize(s.model),
    originalFirst = s.model.colorSlots.find((slot) => slot.id === 'primary').value.alpha,
    past = s.session.historyView.past.length;
  const input = s.query('[data-shared-color-id="secondary"][data-shared-color-edit="alpha"]');
  s.fire(input, 'pointerdown', { pointerId: 31, button: 0, clientX: 20 });
  s.fire(s.app, 'pointermove', { pointerId: 31, clientX: 34 });
  s.fire(s.app, 'pointerup', { pointerId: 31 });
  assert.equal(s.model.colorSlots.find((slot) => slot.id === 'secondary').value.alpha, 0.1);
  assert.equal(s.model.colorSlots.find((slot) => slot.id === 'primary').value.alpha, originalFirst);
  assert.equal(s.session.historyView.past.length, past + 1);
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.render();
  assert.equal(serialize(s.model), before);
});

test('live ring-count input samples preserve fixed stable-ID centers when returning to the origin', (t) => {
  const s = setup(t, { realSession: true, live: true });
  const before = serialize(s.model),
    past = s.session.historyView.past.length;
  const input = s.query('[data-wheel-setting="lightnessSteps"]');
  for (const value of ['1', '7']) {
    input.value = value;
    s.fire(input, 'input');
  }
  s.fire(input, 'change');
  assert.equal(serialize(s.model), before);
  assert.equal(s.session.historyView.past.length, past);
  assert.equal(s.panel.previewDocument, null);
  const range = s.query('[data-wheel-range="hueSteps"]');
  assert.equal(range.min, '3');
  assert.equal(range.step, '3');
});

test('a second primary pointer cannot settle or retarget the first live color gesture', (t) => {
  const s = setup(t, { realSession: true, live: true });
  const before = serialize(s.model),
    angle = s.model.colorDesign.wheel.hueRotation,
    strength = s.model.colorDesign.colorfulness,
    past = s.session.historyView.past.length;
  const first = s.query('[data-wheel-setting="hueRotation"]');
  s.fire(first, 'pointerdown', { pointerId: 41, button: 0, clientX: 20 });
  s.fire(s.app, 'pointermove', { pointerId: 41, clientX: 34 });
  const second = s.query('[data-chroma-strength]');
  const rejected = s.fire(second, 'pointerdown', { pointerId: 42, button: 0, clientX: 20 });
  assert.equal(rejected.defaultPrevented, true);
  s.fire(s.app, 'pointerup', { pointerId: 42 });
  assert.equal(serialize(s.model), before);
  assert.equal(s.session.historyView.past.length, past);
  s.fire(s.app, 'pointerup', { pointerId: 41 });
  assert.equal(s.model.colorDesign.wheel.hueRotation, angle + 10);
  assert.equal(s.model.colorDesign.colorfulness, strength);
  assert.equal(s.session.historyView.past.length, past + 1);
  assert.equal(s.app.querySelectorAll('[data-color-coverage-status]').length, 1);
});

test('invalid final numeric text rolls back rather than committing its earlier valid preview on navigation', (t) => {
  const s = setup(t, { realSession: true, live: true });
  const before = serialize(s.model),
    past = s.session.historyView.past.length;
  const input = s.query('[data-chroma-strength]');
  input.focus();
  input.value = '0.3';
  s.fire(input, 'input');
  input.value = '';
  s.fire(input, 'input');
  s.fire(s.query('[data-wheel-select="text"]'), 'pointerdown', { pointerId: 51, button: 0 });
  assert.equal(serialize(s.model), before);
  assert.equal(s.session.historyView.past.length, past);
  assert.equal(s.panel.previewDocument, null);
});

for (const selector of ['[data-wheel-setting="hueRotation"]', '[data-chroma-strength]'])
  test(`Escape ignores native removal change from ${selector}`, async (t) => {
    const s = setup(t, { realSession: true, live: true, nativeChangeOnRemoval: true });
    const before = serialize(s.model),
      state = s.session.state,
      past = s.session.historyView.past.length;
    const input = s.query(selector);
    input.focus();
    input.value = selector.includes('hueRotation') ? '75' : '0.3';
    s.fire(input, 'input');
    await new Promise((resolve) => setTimeout(resolve, 40));
    assert.ok(s.panel.previewDocument, 'a valid live preview exists before Escape');
    s.fire(input, 'keydown', { key: 'Escape' });
    await new Promise((resolve) => setTimeout(resolve, 40));
    assert.equal(s.removalChanges, 1, 'the connected old field emits native removal change');
    assert.equal(serialize(s.model), before);
    assert.deepEqual(s.session.state, state);
    assert.equal(s.session.historyView.past.length, past);
    assert.equal(s.panel.previewDocument, null);
    assert.equal(s.query(selector).value, s.query(selector).defaultValue);
    assert.deepEqual(s.notices, []);
  });

test('Advanced survives an immediate color commit before native toggle is delivered', async (t) => {
  const s = setup(t, { realSession: true, live: true, deferAdvancedToggle: true });
  const before = serialize(s.model),
    state = s.session.state,
    past = s.session.historyView.past.length;
  s.query('[data-color-advanced]').open = true;
  assert.equal(serialize(s.model), before, 'opening Advanced is view-only');
  assert.deepEqual(s.session.state, state);
  s.change('[data-wheel-chroma-mode]', 'override');
  assert.equal(s.query('[data-color-advanced]').open, true);
  assert.ok(s.query('[data-wheel-chroma]'));
  assert.equal(s.session.historyView.past.length, past + 1, 'only the color edit authors history');
  assert.equal(s.session.state.expectedRevision, state.expectedRevision + 1);
  await new Promise((resolve) => setTimeout(resolve, 40));
  assert.equal(s.query('[data-color-advanced]').open, true, 'old queued events cannot close it');
  const edited = serialize(s.model),
    editedState = s.session.state;
  s.query('[data-color-advanced]').open = false;
  assert.equal(serialize(s.model), edited, 'closing Advanced is also view-only');
  assert.deepEqual(s.session.state, editedState);
  s.change('[data-wheel-setting="hueRotation"]', 22);
  assert.equal(s.query('[data-color-advanced]').open, false);
  assert.equal(s.session.historyView.past.length, past + 2);
  assert.deepEqual(s.notices, []);
});

test('unmounted or unrelated details cannot replace the remembered Advanced view state', (t) => {
  const s = setup(t, { realSession: true, live: true });
  const before = serialize(s.model),
    state = s.session.state,
    past = s.session.historyView.past.length;
  const old = s.query('[data-color-advanced]');
  old.open = true;
  s.panel.html();
  s.query('.shared-colors-panel').remove();
  old.open = false;
  s.fire(old, 'toggle');
  const unrelated = s.win.document.createElement('details');
  unrelated.dataset.colorAdvanced = '';
  s.app.append(unrelated);
  s.fire(unrelated, 'toggle');
  s.render();
  assert.equal(s.query('[data-color-advanced]').open, true);
  assert.equal(serialize(s.model), before);
  assert.deepEqual(s.session.state, state);
  assert.equal(s.session.historyView.past.length, past);
  assert.deepEqual(s.notices, []);
});
