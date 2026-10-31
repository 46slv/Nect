import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createSharedColorsPanel } from '../src/shared-colors-panel.js';
import { DEFAULT_HARMONY_WHEEL } from '../src/harmony-wheel.js';
import { analyzePaletteContrast } from '../src/palette-contrast.js';
import { blankDocument, CommandSession } from '../src/core.js';

function fixture() {
  const document = blankDocument();
  document.schemaVersion = 56;
  document.colorSlots = [
    { id: 'text', name: 'Named Text', value: { hex: '#000000', alpha: 1 } },
    { id: 'background', name: 'Named Background', value: { hex: '#ffffff', alpha: 1 } },
  ];
  document.colorDesign = {
    version: 2,
    profile: 'balanced',
    colorfulness: 0.72,
    areaAware: true,
    lockedSlotIds: [],
    wheel: { ...DEFAULT_HARMONY_WHEEL },
    palette: [
      { id: 'text', kind: 'neutral', L: 0.15, role: 'Text' },
      { id: 'background', kind: 'neutral', L: 0.98, role: 'Background' },
    ],
  };
  return document;
}

function setup(t, document = fixture()) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<main></main>';
  const app = win.document.querySelector('main');
  const session = new CommandSession(document);
  const calls = [],
    notices = [];
  const render = () => {
    app.innerHTML = panel.html();
  };
  const panel = createSharedColorsPanel({
    app,
    getSession: () => session,
    getDocument: () => session.snapshot(),
    dispatch: (type, payload, state) => {
      calls.push({ type, payload: structuredClone(payload), state: { ...state } });
      return session.dispatch({ ...state, type, payload });
    },
    render,
    notify: (...args) => notices.push(args),
  });
  const query = (selector) => app.querySelector(selector);
  const fire = (target, type) => {
    assert.ok(target, `Missing ${type} target`);
    target.dispatchEvent(new win.Event(type, { bubbles: true, cancelable: true }));
  };
  const click = (selector) => fire(query(selector), 'click');
  const change = (selector, value) => {
    const element = query(selector);
    assert.ok(element, selector);
    element.value = String(value);
    fire(element, 'change');
  };
  panel.toggle();
  return {
    app,
    session,
    calls,
    notices,
    render,
    query,
    click,
    change,
    document: () => session.snapshot(),
    current: () => query('[data-palette-contrast="current"]'),
    preview: () => query('[data-palette-contrast="preview"]'),
  };
}

function assertStatus(report, expected) {
  assert.ok(report, 'Contrast report is visible');
  assert.equal(report.dataset.contrastStatus, expected);
}
function assertPair(report, expected) {
  const row = report.querySelector(
    `[data-contrast-text-id="${expected.textId}"][data-contrast-background-id="${expected.backgroundId}"]`,
  );
  assert.ok(row, 'Semantic candidate retains stable Text/Background IDs');
  assert.equal(row.dataset.contrastStatus, expected.status);
  if (expected.ratio === null) {
    assert.doesNotMatch(row.textContent, /\d+(?:\.\d+)?\s*:\s*1/);
    assert.match(row.textContent, /unknown/i);
  } else {
    const ratio = row.textContent.match(/\b(\d+(?:\.\d{1,3})?)\s*:\s*1\b/);
    assert.ok(ratio, `Bounded readable contrast ratio: ${row.textContent}`);
    assert.ok(Math.abs(Number(ratio[1]) - expected.ratio) <= 0.01);
    assert.match(row.textContent, new RegExp(expected.status, 'i'));
  }
  return row;
}
function proposedDocument(s, design) {
  const proposed = [
    ...s.app.querySelectorAll('.chroma-preview .chroma-swatch[aria-label^="Proposed "]'),
  ];
  const document = s.document();
  assert.equal(proposed.length, document.colorSlots.length);
  document.colorSlots = document.colorSlots.map((slot, index) => ({
    ...slot,
    value: {
      ...slot.value,
      hex: proposed[index].getAttribute('aria-label').slice('Proposed '.length),
    },
  }));
  document.colorDesign = design;
  return document;
}

test('saved contrast uses authored literals and semantic roles, with an explicitly bounded advisory scope', (t) => {
  const s = setup(t),
    before = s.document(),
    state = { ...s.session.state };
  const report = s.current(),
    expected = analyzePaletteContrast(before);
  assertStatus(report, 'pass');
  assert.equal(expected.pairs[0].ratio, 21);
  const row = assertPair(report, expected.pairs[0]);
  assert.match(row.textContent, /Named Text/);
  assert.match(row.textContent, /Named Background/);
  assert.match(report.textContent, /normal[ -]text/i);
  assert.match(report.textContent, /4\.5\s*:\s*1/);
  assert.match(report.textContent, /Text\s*[×x]\s*Background/i);
  assert.match(report.textContent, /rendered|artwork|WCAG/i);
  assert.equal(s.preview(), null);
  assert.deepEqual(s.document(), before);
  assert.deepEqual(s.session.state, state);
  assert.deepEqual(s.calls, []);
});

test('preview contrast uses actual proposed colors and draft lightness; Cancel authors nothing', (t) => {
  const s = setup(t),
    before = s.document(),
    state = { ...s.session.state };
  s.change('[data-wheel-entry]', 'text');
  s.change('[data-wheel-lightness]', 0.9);
  s.click('[data-chroma-preview]');
  const draft = structuredClone(before.colorDesign);
  draft.palette[0].L = 0.9;
  const proposed = proposedDocument(s, draft),
    expected = analyzePaletteContrast(proposed);
  assert.equal(
    expected.status,
    'fail',
    'Fixture distinguishes proposed colors from saved black/white',
  );
  assert.notDeepEqual(proposed.colorSlots, before.colorSlots);
  assertStatus(s.current(), 'pass');
  assertStatus(s.preview(), expected.status);
  assert.ok(s.query('.chroma-preview').contains(s.preview()));
  assertPair(s.preview(), expected.pairs[0]);
  assert.equal(s.query('[data-chroma-apply]').disabled, false, 'Contrast failure is advisory');
  assert.deepEqual(s.document(), before);
  assert.deepEqual(s.session.state, state);
  assert.deepEqual(s.calls, []);
  s.click('[data-chroma-cancel]');
  assert.equal(s.preview(), null);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assertStatus(s.current(), 'pass');
  assert.deepEqual(s.document(), before);
  assert.deepEqual(s.session.state, state);
  assert.deepEqual(s.calls, []);
});

test('draft roles drive preview applicability without changing saved-role contrast; failed advisory can Apply', (t) => {
  const s = setup(t),
    before = s.document();
  s.change('[data-wheel-entry]', 'text');
  s.change('[data-wheel-role]', 'Accent');
  s.click('[data-chroma-preview]');
  assertStatus(s.current(), 'pass');
  assertStatus(s.preview(), 'not-applicable');
  assert.equal(s.preview().querySelector('[data-contrast-text-id]'), null);
  assert.deepEqual(s.document(), before);
  s.click('[data-chroma-cancel]');
  s.change('[data-wheel-role]', 'Text');
  s.change('[data-wheel-lightness]', 0.9);
  s.click('[data-chroma-preview]');
  const draft = structuredClone(before.colorDesign);
  draft.palette[0].L = 0.9;
  const proposed = proposedDocument(s, draft);
  assertStatus(s.preview(), 'fail');
  assert.equal(s.query('[data-chroma-apply]').disabled, false);
  s.click('[data-chroma-apply]');
  assert.equal(s.calls.length, 1);
  assert.equal(s.calls[0].type, 'set-color-slots');
  assert.deepEqual(s.document().colorSlots, proposed.colorSlots);
  assert.deepEqual(s.document().colorDesign, draft);
  assert.equal(s.preview(), null);
  assertStatus(s.current(), 'fail');
  assertPair(s.current(), analyzePaletteContrast(proposed).pairs[0]);
  assert.deepEqual(s.notices, []);
});

test('changing a literal invalidates a passing preview; transparent backdrop stays unknown and never stale pass', (t) => {
  const s = setup(t);
  s.click('[data-chroma-preview]');
  assertStatus(s.preview(), 'pass');
  s.change('[data-shared-color-id="background"][data-shared-color-edit="alpha"]', 0.5);
  s.render();
  assert.equal(s.calls.length, 1);
  assert.equal(s.calls[0].type, 'set-color-slot');
  assert.equal(s.document().colorSlots[1].value.alpha, 0.5);
  assert.equal(s.preview(), null);
  assert.equal(s.query('[data-chroma-apply]'), null);
  assertStatus(s.current(), 'unknown');
  assertPair(s.current(), analyzePaletteContrast(s.document()).pairs[0]);
  s.click('[data-chroma-preview]');
  assertStatus(s.preview(), 'unknown');
  assertPair(s.preview(), {
    textId: 'text',
    backgroundId: 'background',
    ratio: null,
    status: 'unknown',
  });
  assert.equal(
    s.query('[data-chroma-apply]').disabled,
    false,
    'Unknown contrast does not block Apply',
  );
  assert.equal(s.app.querySelector('[data-contrast-status="pass"]'), null);
});

test('missing semantic pairs are not applicable even when slot names suggest Text and Background', (t) => {
  const document = fixture();
  document.colorDesign.palette[0].role = 'Primary';
  document.colorDesign.palette[1].role = 'Accent';
  const s = setup(t, document);
  assertStatus(s.current(), 'not-applicable');
  assert.equal(s.current().querySelector('[data-contrast-text-id]'), null);
  s.click('[data-chroma-preview]');
  assertStatus(s.preview(), 'not-applicable');
  assert.equal(s.preview().querySelector('[data-contrast-text-id]'), null);
  assert.deepEqual(s.calls, []);
});

test('contrast pair names are escaped as text in saved and preview reports', (t) => {
  const document = fixture();
  document.colorSlots[0].name = 'Text <img src=x onerror="evil()"> & "ink"';
  document.colorSlots[1].name = "Background <script>evil()</script> & 'paper'";
  const s = setup(t, document);
  s.click('[data-chroma-preview]');
  for (const report of [s.current(), s.preview()]) {
    const row = report.querySelector(
      '[data-contrast-text-id="text"][data-contrast-background-id="background"]',
    );
    assert.ok(row);
    assert.ok(row.textContent.includes(document.colorSlots[0].name));
    assert.ok(row.textContent.includes(document.colorSlots[1].name));
    assert.equal(report.querySelector('img,script,[onerror]'), null);
  }
  assert.deepEqual(s.calls, []);
});
