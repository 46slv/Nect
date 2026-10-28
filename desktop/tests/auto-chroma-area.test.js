import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createColorCoverageView } from '../src/color-coverage-view.js';
import { createSharedColorsPanel } from '../src/shared-colors-panel.js';
import { proposeAutoChroma } from '../src/auto-chroma.js';
import { blankDocument, CommandSession, serialize } from '../src/core.js';

const flush = () => new Promise((resolve) => setImmediate(resolve));
const initialSlots = () => [
  { id: 'red', name: 'Duplicate', value: { hex: '#FF0000', alpha: 0.123456789 } },
  { id: 'green', name: 'Duplicate', value: { hex: '#00ff00', alpha: 1 } },
  { id: 'blue', name: 'Blue', value: { hex: '#0000ff', alpha: 0.75 } },
  { id: 'neutral', name: 'Neutral', value: { hex: '#AbAbAb', alpha: 0 } },
];
// Intentionally not palette order: measurement joins must use stable IDs.
const coverage = () => ({
  slots: [
    { id: 'blue', percent: 2 },
    { id: 'neutral', percent: 0 },
    { id: 'red', percent: 90 },
    { id: 'green', percent: 8 },
  ],
  other: { percent: 0 },
  sampleSize: { width: 256, height: 128 },
});

function coverageHarness(t) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML =
    '<main><output data-color-coverage-id="red">—</output><p data-color-coverage-status>Initial</p></main>';
  const app = win.document.querySelector('main');
  const requests = [];
  let key = 'document-a:revision-1:font-1',
    open = true;
  const view = createColorCoverageView({
    app,
    getKey: () => key,
    isOpen: () => open,
    measure: (isCurrent) =>
      new Promise((resolve, reject) => requests.push({ isCurrent, resolve, reject })),
  });
  return {
    app,
    view,
    requests,
    get key() {
      return key;
    },
    set key(value) {
      key = value;
    },
    set open(value) {
      open = value;
    },
  };
}

test('coverage snapshot is a pure read: unavailable before and during measurement', async (t) => {
  const s = coverageHarness(t);
  const initialDOM = s.app.innerHTML;
  for (let i = 0; i < 3; i++) assert.equal(s.view.snapshot(), null);
  await flush();
  assert.equal(s.requests.length, 0, 'reading a snapshot must not initiate measurement');
  assert.equal(s.app.innerHTML, initialDOM, 'reading must not paint or rerender');
  s.view.refresh();
  const pendingDOM = s.app.innerHTML;
  assert.equal(s.view.snapshot(), null);
  await flush();
  assert.equal(s.requests.length, 1);
  assert.equal(s.view.snapshot(), null);
  assert.equal(s.app.innerHTML, pendingDOM);
  s.requests[0].resolve(coverage());
  await flush();
});

test('settled coverage snapshots deep-copy every nested value without changing cache or DOM', async (t) => {
  const s = coverageHarness(t);
  s.view.refresh();
  await flush();
  const measured = coverage();
  const original = structuredClone(measured);
  s.requests[0].resolve(measured);
  await flush();
  const dom = s.app.innerHTML;
  const first = s.view.snapshot();
  assert.deepEqual(first, { key: s.key, value: original });
  assert.notEqual(first.value, measured);
  assert.notEqual(first.value.slots, measured.slots);
  assert.notEqual(first.value.slots[0], measured.slots[0]);
  assert.notEqual(first.value.other, measured.other);
  assert.notEqual(first.value.sampleSize, measured.sampleSize);
  first.key = 'consumer-edit';
  first.value.slots[0].percent = 999;
  first.value.slots.push({ id: 'consumer-edit', percent: 1 });
  first.value.other.percent = 999;
  first.value.sampleSize.width = 1;
  const second = s.view.snapshot();
  assert.deepEqual(second, { key: s.key, value: original });
  assert.notEqual(second, first);
  assert.notEqual(second.value, first.value);
  assert.deepEqual(measured, original);
  assert.equal(s.app.innerHTML, dom);
  assert.equal(s.requests.length, 1);
});

test('snapshot invalidates immediately on key changes and never exposes a stale completion', async (t) => {
  const s = coverageHarness(t);
  s.view.refresh();
  await flush();
  s.requests[0].resolve(coverage());
  await flush();
  assert.ok(s.view.snapshot());
  s.key = 'document-a:revision-1:font-2';
  const dom = s.app.innerHTML;
  assert.equal(s.view.snapshot(), null, 'font changes invalidate without a Session revision');
  assert.equal(s.requests.length, 1, 'snapshot must not schedule replacement work');
  assert.equal(s.app.innerHTML, dom);
  s.view.refresh();
  await flush();
  assert.equal(s.requests.length, 2);
  s.key = 'document-b:revision-1:font-2';
  s.requests[1].resolve(coverage());
  await flush();
  assert.equal(s.view.snapshot(), null);
  assert.equal(s.requests[1].isCurrent(), false);
  assert.equal(s.requests.length, 3);
  const latest = coverage();
  latest.slots[0].percent = 12;
  latest.slots[2].percent = 80;
  s.requests[2].resolve(latest);
  await flush();
  assert.deepEqual(s.view.snapshot(), { key: s.key, value: latest });
});

test('measurement failure and missing result never become usable coverage snapshots', async (t) => {
  for (const outcome of ['error', 'missing']) {
    await t.test(outcome, async (t) => {
      const s = coverageHarness(t);
      s.view.refresh();
      await flush();
      if (outcome === 'error') s.requests[0].reject(Error('decoder failed'));
      else s.requests[0].resolve(undefined);
      await flush();
      const dom = s.app.innerHTML;
      assert.equal(s.view.snapshot(), null);
      assert.equal(s.view.snapshot(), null);
      await flush();
      assert.equal(s.requests.length, 1);
      assert.equal(s.app.innerHTML, dom);
      assert.match(s.app.textContent, /Coverage unavailable/);
    });
  }
});

test('closed coverage views expose no snapshot and reopening can reuse a current settled cache', async (t) => {
  const s = coverageHarness(t);
  s.view.refresh();
  await flush();
  s.requests[0].resolve(coverage());
  await flush();
  const snapshot = s.view.snapshot();
  s.open = false;
  assert.equal(s.view.snapshot(), null);
  s.open = true;
  assert.deepEqual(s.view.snapshot(), snapshot);
  assert.equal(s.requests.length, 1);
});

// Independent primary-color OKLab reference vectors; no panel conversion helper.
const coordinates = {
  red: { L: 0.6279553606145516, h: 29.233885192342633 },
  green: { L: 0.8664396115356694, h: 142.49533888780996 },
  blue: { L: 0.4520137183853429, h: 264.052020638055 },
  neutral: { L: 0.741533, h: 0 },
};
function expectedUpdates(areaAware, measured = null, locked = [], settings = {}) {
  return proposeAutoChroma(
    initialSlots().map((slot) => ({
      id: slot.id,
      ...coordinates[slot.id],
      role: 'Primary',
      alpha: slot.value.alpha,
      ...(slot.id === 'neutral' || locked.includes(slot.id) ? { anchor: slot.value } : {}),
    })),
    { areaAware, ...settings },
    measured,
  ).map(({ id, value }) => ({ slotId: id, value }));
}

function panelHarness(t, { omitCallback = false } = {}) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<main></main>';
  const app = document.querySelector('main');
  const model = blankDocument();
  model.schemaVersion = 54;
  model.colorSlots = initialSlots();
  let session = new CommandSession(model),
    fontGeneration = 1;
  const calls = [],
    notices = [],
    requests = [];
  let callbackReads = 0;
  const render = () => {
    app.innerHTML = panel.html();
  };
  const panel = createSharedColorsPanel({
    app,
    getSession: () => session,
    getDocument: () => session.snapshot(),
    dispatch: (type, payload, state) => {
      calls.push({ type, payload: structuredClone(payload), state });
      return session.dispatch({ ...state, type, payload });
    },
    render,
    notify: (...args) => notices.push(args),
    ...(!omitCallback
      ? {
          getCoverageSnapshot: () => {
            callbackReads++;
            return view.snapshot();
          },
        }
      : {}),
  });
  const getKey = () => JSON.stringify([session.state, fontGeneration]);
  const view = createColorCoverageView({
    app,
    getKey,
    isOpen: () => panel.open,
    measure: (isCurrent) =>
      new Promise((resolve, reject) => requests.push({ isCurrent, resolve, reject })),
  });
  const query = (selector) => app.querySelector(selector);
  const fire = (el, type = 'change') => {
    assert.ok(el, `missing ${type} target`);
    el.dispatchEvent(new win.Event(type, { bubbles: true, cancelable: true }));
  };
  const click = (selector) => fire(query(selector), 'click');
  panel.toggle();
  return {
    app,
    panel,
    view,
    calls,
    notices,
    requests,
    query,
    fire,
    click,
    render,
    getKey,
    get session() {
      return session;
    },
    get callbackReads() {
      return callbackReads;
    },
    replaceFont() {
      fontGeneration++;
    },
    replaceSession() {
      session = new CommandSession(model);
    },
    change(selector, value) {
      const el = query(selector);
      assert.ok(el, `missing ${selector}`);
      if (el.type === 'checkbox') el.checked = value;
      else el.value = value;
      fire(el);
    },
    preview() {
      click('[data-chroma-preview]');
    },
    apply() {
      click('[data-chroma-apply]');
    },
    async settle(value = coverage()) {
      view.refresh();
      await flush();
      requests.at(-1).resolve(value);
      await flush();
    },
  };
}

function authoredState(s) {
  return {
    document: serialize(s.session.snapshot()),
    state: structuredClone(s.session.state),
    history: structuredClone(s.session.historyView),
  };
}
function assertLocal(s, before) {
  assert.deepEqual(
    authoredState(s),
    before,
    'temporary settings and previews preserve document, revision and Undo/Redo',
  );
  assert.deepEqual(s.calls, [], 'no authored command before Apply');
}
function previewHexes(s) {
  return [...s.app.querySelectorAll('.chroma-preview output')].map((el) => el.textContent);
}

test('default area-aware preview uses current measurement and Apply saves the actual weighted stable-ID values', async (t) => {
  const s = panelHarness(t);
  const before = authoredState(s);
  const measured = coverage();
  await s.settle(measured);
  assert.equal(s.query('[data-chroma-area]').checked, true);
  s.preview();
  assert.match(
    s.query('[data-chroma-area-status]').textContent,
    /current visible area.*soft weight/i,
  );
  const weighted = expectedUpdates(true, measured);
  const unweighted = expectedUpdates(false);
  assert.notDeepEqual(weighted, unweighted, 'fixture must visibly distinguish area weighting');
  assert.deepEqual(
    previewHexes(s),
    weighted.map(({ value }) => value.hex),
  );
  assert.ok(s.callbackReads > 0, 'shared panel forwards the coverage snapshot callback');
  assertLocal(s, before);
  s.apply();
  assert.equal(s.calls.length, 1);
  assert.equal(s.calls[0].type, 'set-color-slots');
  assert.deepEqual(s.calls[0].payload, { updates: weighted });
  assert.deepEqual(s.calls[0].state, before.state);
  assert.equal(s.session.historyView.past.length, before.history.past.length + 1);
  assert.deepEqual(
    s.session.snapshot().colorSlots.map(({ id, value }) => ({ slotId: id, value })),
    weighted,
  );
  assert.deepEqual(
    s.session.snapshot().colorSlots.map(({ id, name }) => ({ id, name })),
    initialSlots().map(({ id, name }) => ({ id, name })),
  );
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  assert.equal(serialize(s.session.snapshot()), before.document);
});

test('weighted preview keeps exact locked spelling, alpha and neutral anchors', async (t) => {
  const s = panelHarness(t);
  await s.settle();
  const before = authoredState(s);
  s.change('[data-chroma-lock="red"]', true);
  s.preview();
  const expected = expectedUpdates(true, coverage(), ['red']);
  assert.deepEqual(
    previewHexes(s),
    expected.map(({ value }) => value.hex),
  );
  assertLocal(s, before);
  s.apply();
  assert.deepEqual(s.calls[0].payload.updates, expected);
  for (const id of ['red', 'neutral'])
    assert.deepEqual(
      s.session.snapshot().colorSlots.find((slot) => slot.id === id).value,
      initialSlots().find((slot) => slot.id === id).value,
    );
});

test('weighted Apply rejects a changed coverage key even when Session revision and colors are unchanged', async (t) => {
  for (const refresh of [false, true]) {
    await t.test(
      refresh ? 'replacement measurement ready' : 'replacement measurement unavailable',
      async (t) => {
        const s = panelHarness(t);
        await s.settle();
        s.preview();
        const before = authoredState(s);
        const oldKey = s.getKey();
        s.replaceFont();
        assert.notEqual(s.getKey(), oldKey);
        assert.equal(s.view.snapshot(), null);
        if (refresh) await s.settle();
        s.apply();
        assertLocal(s, before);
        assert.equal(s.query('[data-chroma-apply]'), null);
        assert.match(s.notices.at(-1)[0], /stale|changed|preview again/i);
        assert.equal(s.notices.at(-1)[1], true);
        // Recover through a fresh preview, never by silently restamping old results.
        if (!refresh) await s.settle();
        s.preview();
        s.apply();
        assert.equal(s.calls.length, 1);
        assert.deepEqual(s.calls[0].payload.updates, expectedUpdates(true, coverage()));
      },
    );
  }
});

test('unavailable measurement is explicitly unweighted, never an invented area estimate', async (t) => {
  for (const availability of ['no measurement', 'pending', 'stale', 'failed', 'callback omitted']) {
    await t.test(availability, async (t) => {
      const s = panelHarness(t, { omitCallback: availability === 'callback omitted' });
      if (availability === 'stale') {
        await s.settle();
        s.replaceFont();
      }
      if (availability === 'pending' || availability === 'failed') {
        s.view.refresh();
        await flush();
        if (availability === 'failed') {
          s.requests[0].reject(Error('decoder failed'));
          await flush();
        }
      }
      const before = authoredState(s);
      s.preview();
      assert.equal(s.query('[data-chroma-area]').checked, true);
      assert.match(
        s.query('.chroma-preview').textContent,
        /no current.*measurement|measurement.*(?:unavailable|not ready)/i,
      );
      assert.deepEqual(
        previewHexes(s),
        expectedUpdates(false).map(({ value }) => value.hex),
      );
      assertLocal(s, before);
      s.apply();
      assert.equal(s.calls.length, 1);
      assert.deepEqual(s.calls[0].payload.updates, expectedUpdates(false));
      if (availability === 'pending') {
        s.requests[0].resolve(coverage());
        await flush();
      }
    });
  }
});

test('disabling area awareness discards the weighted preview and ignores later measurement changes', async (t) => {
  const s = panelHarness(t);
  await s.settle();
  const before = authoredState(s);
  s.preview();
  s.change('[data-chroma-area]', false);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.equal(s.query('[data-chroma-area]').checked, false);
  s.preview();
  assert.deepEqual(
    previewHexes(s),
    expectedUpdates(false).map(({ value }) => value.hex),
  );
  assert.match(
    s.query('.chroma-preview').textContent,
    /(?:area.*(?:off|disabled)|unweighted|without area)/i,
  );
  s.replaceFont();
  assertLocal(s, before);
  s.apply();
  assert.equal(s.calls.length, 1, 'unused coverage keys do not stale an unweighted preview');
  assert.deepEqual(s.calls[0].payload.updates, expectedUpdates(false));
});

test('reenabling area weighting after an unweighted preview uses the current measured output', async (t) => {
  const s = panelHarness(t);
  await s.settle();
  const before = authoredState(s);
  s.change('[data-chroma-area]', false);
  s.preview();
  assert.deepEqual(
    previewHexes(s),
    expectedUpdates(false).map(({ value }) => value.hex),
  );
  s.change('[data-chroma-area]', true);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.equal(s.query('[data-chroma-area]').checked, true);
  s.preview();
  assert.deepEqual(
    previewHexes(s),
    expectedUpdates(true, coverage()).map(({ value }) => value.hex),
  );
  assertLocal(s, before);
  s.apply();
  assert.deepEqual(s.calls[0].payload.updates, expectedUpdates(true, coverage()));
});

test('measurement arriving after fallback preview does not silently replace its proposed colors', async (t) => {
  const s = panelHarness(t);
  const before = authoredState(s);
  s.preview();
  const unweightedHexes = previewHexes(s);
  await s.settle();
  s.render();
  assert.deepEqual(previewHexes(s), unweightedHexes);
  assert.match(
    s.query('[data-chroma-area-status]').textContent,
    /no current.*measurement.*unweighted/i,
  );
  assertLocal(s, before);
  s.apply();
  assert.deepEqual(s.calls[0].payload.updates, expectedUpdates(false));
});

test('rendering after a coverage-key change discards weighted preview without silently restamping it', async (t) => {
  const s = panelHarness(t);
  await s.settle();
  s.preview();
  const before = authoredState(s);
  s.replaceFont();
  await s.settle();
  s.render();
  assert.equal(s.query('[data-chroma-apply]'), null);
  assertLocal(s, before);
  s.preview();
  s.apply();
  assert.equal(s.calls.length, 1);
  assert.deepEqual(s.calls[0].payload.updates, expectedUpdates(true, coverage()));
});

test('area setting is temporary, preserves pending Redo, and resets for a new Session', async (t) => {
  const s = panelHarness(t);
  await s.settle();
  s.preview();
  s.apply();
  s.session.dispatch({ ...s.session.state, type: 'undo', payload: {} });
  s.calls.length = 0;
  s.render();
  const before = authoredState(s);
  s.change('[data-chroma-area]', false);
  s.change('[data-chroma-profile]', 'soft');
  s.change('[data-chroma-strength]', '0.38');
  s.preview();
  s.click('[data-chroma-cancel]');
  assertLocal(s, before);
  assert.ok(s.session.historyView.future.length, 'pending Redo survives temporary controls');
  s.replaceSession();
  s.render();
  assert.equal(s.query('[data-chroma-area]').checked, true);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.equal(s.query('[data-chroma-profile]').value, 'balanced');
  assert.equal(Number(s.query('[data-chroma-strength]').value), 0.72);
  assert.deepEqual(s.calls, []);
});

test('queued area-toggle changes reject stale Session envelopes without authored changes', async (t) => {
  const s = panelHarness(t);
  await s.settle();
  s.preview();
  const toggle = s.query('[data-chroma-area]');
  s.replaceSession();
  const before = authoredState(s);
  toggle.checked = false;
  s.fire(toggle);
  assertLocal(s, before);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assert.match(s.notices.at(-1)[0], /stale/i);
  assert.equal(s.notices.at(-1)[1], true);
});
