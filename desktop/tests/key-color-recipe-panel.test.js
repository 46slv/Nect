import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { blankDocument, CommandSession, serialize, deserialize, validate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createKeyColorRecipePanel } from '../src/key-color-recipe-panel.js';
import { createColorCoverageView } from '../src/color-coverage-view.js';
import { researchPlans } from '../src/key-color-research-plans.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';

function setup(
  t,
  kind = 'document',
  doc = retainedRecipeFixture(),
  getCoverageSnapshot = () => null,
) {
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<button data-key-color-recipe-open>Key color</button>';
  let session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const calls = [],
    notices = [];
  let dispatchResult = 'real';
  const panel = createKeyColorRecipePanel({
    document,
    getSession: () => session,
    getDocument: () => session.historyView.doc,
    getCoverageSnapshot,
    dispatch: (type, payload, envelope) => {
      calls.push({ type, payload: structuredClone(payload), envelope: { ...envelope } });
      if (dispatchResult !== 'real') return dispatchResult;
      return session.dispatch({ ...envelope, type, payload });
    },
    notify: (...args) => notices.push(args),
  });
  const query = (selector) => document.querySelector(selector);
  const fire = (el, type, options = {}) =>
    el.dispatchEvent(
      new (type === 'keydown' ? win.KeyboardEvent : win.Event)(type, {
        bubbles: true,
        cancelable: true,
        ...options,
      }),
    );
  const input = (kind, value) => {
    const el = query(`[data-key-color-input="${kind}"]`);
    assert.ok(el);
    el.value = String(value);
    fire(el, 'input');
    return el;
  };
  return {
    win,
    document,
    panel,
    query,
    fire,
    input,
    calls,
    notices,
    open: () => panel.open(query('[data-key-color-recipe-open]')),
    click: (selector) => query(selector).click(),
    snapshot: () => session.snapshot(),
    captured: () => ({
      doc: session.snapshot(),
      state: session.state,
      past: structuredClone(session.historyView.past),
      future: structuredClone(session.historyView.future),
    }),
    send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    get session() {
      return session;
    },
    set session(value) {
      session = value;
    },
    set dispatchResult(value) {
      dispatchResult = value;
    },
  };
}

for (const kind of ['document', 'project']) {
  test(`${kind}: saturated dark HEX supports H/L edits after opening and direct HEX input`, (t) => {
    for (const source of ['retained', 'direct']) {
      const doc = retainedRecipeFixture(),
        saved = doc.keyColorRecipe.committedSnapshot;
      if (source === 'retained')
        doc.keyColorRecipe.committedSnapshot = editKeyColorRecipe(saved, {
          type: 'key-color',
          expectedRevision: saved.revision,
          value: { hex: '#000011', alpha: 0 },
        });
      const s = setup(t, kind, doc),
        before = s.captured();
      for (const [channel, value, hex] of [
        ['h', 120, '#001100'],
        ['l', 50, '#0000ff'],
      ]) {
        s.open();
        if (source === 'direct') s.input('hex', '#000011');
        assert.equal(s.query('[data-key-color-input="hex"]').value, '#000011');
        assert.equal(s.query('[data-key-color-input="hsl-s"]').value, '100');
        s.input(`hsl-${channel}`, value);
        assert.equal(s.query('[data-key-color-input="hex"]').value, hex, `${source} ${channel}`);
        s.click('[data-key-color-preview]');
        assert.equal(
          s.query('[data-key-color-compare="key"] [data-key-color-after] code').textContent,
          hex,
        );
        assert.equal(s.query('[data-key-color-input="alpha"]').value, '0');
        assert.deepEqual(s.captured(), before);
        s.click('[data-key-color-cancel]');
        assert.deepEqual(s.captured(), before);
      }
      assert.deepEqual(s.calls, []);
    }
  });

  test(`${kind}: HSL edits stay in the C1 draft until one atomic Apply`, (t) => {
    const s = setup(t, kind),
      before = s.captured();
    s.open();
    assert.equal(s.document.activeElement, s.query('[data-key-color-input="hsl-h"]'));
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#aBcD09');
    for (const [channel, value] of [
      ['h', 240],
      ['s', 100],
      ['l', 50],
    ])
      s.input(`hsl-${channel}`, value);
    s.fire(s.query('[data-key-color-input="hsl-l"]'), 'change');
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#0000ff');
    assert.equal(s.query('[data-key-color-input="alpha"]').value, '0');
    assert.deepEqual(s.captured(), before);
    s.click('[data-key-color-preview]');
    assert.equal(
      s.query('[data-key-color-compare="key"] [data-key-color-after] code').textContent,
      '#0000ff',
    );
    assert.deepEqual(s.captured(), before);
    s.click('[data-key-color-apply]');
    const after = s.captured();
    assert.deepEqual(after.doc.colorSlots.find((slot) => slot.id === 'key').value, {
      hex: '#0000ff',
      alpha: 0,
    });
    assert.equal(after.past.length, before.past.length + 1);
    assert.deepEqual(after.doc.colorDesign, before.doc.colorDesign);
    assert.deepEqual(after.doc.colorSlots.slice(2), before.doc.colorSlots.slice(2));
    assert.deepEqual(
      s.calls.map((call) => call.type),
      ['apply-key-color-recipe-snapshot'],
    );
    s.send('undo');
    assert.deepEqual(s.snapshot(), before.doc);
    s.send('redo');
    assert.deepEqual(s.snapshot(), after.doc);
  });

  test(`${kind}: gray hue is local, keeps exact C1 case/alpha, and Cancel preserves Redo`, (t) => {
    const doc = retainedRecipeFixture(),
      saved = doc.keyColorRecipe.committedSnapshot;
    doc.keyColorRecipe.committedSnapshot = editKeyColorRecipe(saved, {
      type: 'key-color',
      expectedRevision: saved.revision,
      value: { hex: '#aAaAaA', alpha: 0 },
    });
    const s = setup(t, kind, doc);
    s.send('set-color-slot', { slotId: 'other', value: { hex: '#010203', alpha: 0 } });
    s.send('undo');
    const before = s.captured();
    s.open();
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#aAaAaA');
    s.input('hsl-h', 240);
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#aAaAaA');
    s.click('[data-key-color-preview]');
    assert.deepEqual(s.captured(), before);
    assert.equal(
      s.query('[data-key-color-compare="key"] [data-key-color-after] code').textContent,
      '#aAaAaA',
    );
    s.input('hsl-s', 100);
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#5555ff');
    assert.equal(s.query('[data-key-color-apply]').disabled, true);
    s.input('hex', '#808080');
    assert.equal(s.query('[data-key-color-input="hsl-h"]').value, '240');
    assert.equal(s.query('[data-key-color-input="alpha"]').value, '0');
    s.click('[data-key-color-cancel]');
    assert.deepEqual(s.captured(), before);
    assert.deepEqual(s.calls, []);
  });

  test(`${kind}: equal Apply preserves Session revision, History and pending Redo`, (t) => {
    const doc = retainedRecipeFixture();
    for (const row of doc.keyColorRecipe.committedSnapshot.resolved.filter((row) => row.active))
      doc.colorSlots.find((slot) => slot.id === row.id).value = { ...row.value };
    const s = setup(t, kind, doc);
    s.send('set-color-slot', { slotId: 'other', value: { hex: '#010203', alpha: 0 } });
    s.send('undo');
    const before = s.captured();
    s.open();
    s.click('[data-key-color-preview]');
    s.click('[data-key-color-apply]');
    assert.equal(s.query('dialog'), null);
    assert.equal(s.calls.length, 1);
    assert.deepEqual(s.captured(), before);
  });

  test(`${kind}: detached Preview preserves literal case/Alpha0, finite intent and one-Undo native Apply`, (t) => {
    const s = setup(t, kind),
      before = s.captured(),
      saved = before.doc.keyColorRecipe.committedSnapshot;
    s.open();
    assert.equal(s.query('[data-key-color-input="hex"]').type, 'text');
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#aBcD09');
    assert.equal(s.query('[data-key-color-input="alpha"]').value, '0');
    assert.equal(s.query('[data-key-color-dormant] input'), null);
    assert.match(s.query('details').textContent, /test-only-coefficients-not-defaults/);
    assert.equal(s.query('details input,details select'), null);
    s.input('hex', '#DeAd09');
    s.input('alpha', 0);
    s.input('deltaL', -2.125);
    s.input('deltaRatioCmax', 3.375);
    s.fire(s.query('[data-key-color-input="deltaL"]'), 'change');
    s.query('[data-key-color-input="deltaL"]').blur();
    assert.deepEqual(s.captured(), before, 'input/change/blur never author');
    assert.equal(s.query('[data-key-color-apply]').disabled, true);
    s.click('[data-key-color-preview]');
    assert.deepEqual(s.captured(), before, 'Preview is detached');
    assert.equal(s.query('[data-key-color-apply]').disabled, false);
    assert.equal(
      s.query('[data-key-color-compare="key"] [data-key-color-before] code').textContent,
      '#123456',
    );
    assert.equal(
      s.query('[data-key-color-compare="key"] [data-key-color-after] code').textContent,
      '#DeAd09',
    );
    assert.match(
      s.query('[data-key-color-compare="key"] [data-key-color-after]').textContent,
      /α 0$/,
    );
    assert.equal(s.query('[data-key-color-compare="dormant"]'), null);
    s.click('[data-key-color-apply]');
    const after = s.captured(),
      next = after.doc.keyColorRecipe.committedSnapshot;
    assert.equal(s.query('[data-key-color-recipe-panel]'), null);
    assert.deepEqual(
      s.calls.map((c) => c.type),
      ['apply-key-color-recipe-snapshot'],
    );
    assert.deepEqual(Object.keys(s.calls[0].payload), ['snapshot']);
    assert.deepEqual(s.calls[0].envelope, before.state);
    assert.deepEqual(after.doc.colorSlots.find((x) => x.id === 'key').value, {
      hex: '#DeAd09',
      alpha: 0,
    });
    assert.equal(next.intent.slots[0].deltaL, -2.125);
    assert.equal(next.intent.slots[0].deltaRatioCmax, 3.375);
    assert.equal(next.resolved.find((x) => x.id === 'derived').value.alpha, 1);
    assert.deepEqual(
      next.resolved.find((x) => x.id === 'dormant'),
      saved.resolved.find((x) => x.id === 'dormant'),
    );
    assert.deepEqual(after.doc.colorSlots.slice(2), before.doc.colorSlots.slice(2));
    assert.deepEqual(after.doc.colorDesign, before.doc.colorDesign);
    assert.deepEqual(after.doc.layers, before.doc.layers);
    assert.equal(after.past.length, before.past.length + 1);
    assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
    s.send('undo');
    assert.deepEqual(s.snapshot(), before.doc);
    s.send('redo');
    assert.deepEqual(s.snapshot(), after.doc);
    if (kind === 'project') {
      const text = serializeCompositionProject(s.session.projectSnapshot());
      assert.equal(
        serializeCompositionProject(
          createCompositionSessionOwner(deserializeCompositionProject(text)).projectSnapshot(),
        ),
        text,
      );
    } else {
      const text = serialize(s.snapshot());
      assert.equal(serialize(new CommandSession(deserialize(text)).snapshot()), text);
    }
    // Receipt is historical; ordinary manual colors are not a new recipe input.
    s.send('set-color-slot', { slotId: 'key', value: { hex: '#010203', alpha: 0.7 } });
    assert.deepEqual(s.snapshot().keyColorRecipe, after.doc.keyColorRecipe);
    s.open();
    assert.equal(s.query('[data-key-color-input="hex"]').value, '#DeAd09');
    s.click('[data-key-color-preview]');
    assert.equal(s.query('[data-key-color-before] code').textContent, '#010203');
  });
}

test('absent retained recipe is explanatory only, with no bootstrap/default/history change', (t) => {
  const s = setup(t, 'document', blankDocument()),
    before = s.captured();
  s.open();
  assert.match(s.query('[data-key-color-empty]').textContent, /five trial colors/);
  assert.equal(s.query('[data-research-create]').disabled, true);
  assert.equal(s.query('[data-key-color-input="hex"]').value, '');
  assert.equal(s.query('[data-key-color-input="hsl-h"]').disabled, true);
  assert.equal(s.query('[data-key-color-apply]'), null);
  s.click('[data-key-color-cancel]');
  assert.deepEqual(s.captured(), before);
  assert.deepEqual(s.calls, []);
});

test('research draft participates in the edit barrier and never rebases after composition switch', (t) => {
  const s = setup(t, 'project', blankDocument());
  const first = s.session.activeCompositionId,
    other = s.session.createComposition();
  s.session.switchComposition(first);
  s.open();
  assert.equal(s.panel.editing, true);
  const canvas = s.query('canvas');
  let captures = 0;
  canvas.setPointerCapture = () => {
    captures++;
  };
  canvas.dispatchEvent(
    new s.win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 7 }),
  );
  assert.equal(captures, 0);
  assert.equal(s.query('[data-research-hex]').value, '');
  for (const [selector, value, event] of [
    ['[data-research-hex]', '#3273c7', 'input'],
    ['[data-research-alpha]', '1', 'input'],
    ['[data-research-plan]', 'research-neutral-start', 'change'],
  ]) {
    const el = s.query(selector);
    el.value = value;
    s.fire(el, event);
  }
  s.click('[data-research-preview]');
  assert.ok(s.query('[data-research-row]'));
  s.session.switchComposition(other);
  s.panel.reconcile();
  assert.equal(s.query('[data-research-preview]').disabled, true);
  assert.equal(s.query('[data-research-row]'), null);
  const hexBeforeStaleInput = s.query('[data-research-hex]').value;
  const hue = s.query('[data-key-color-input="hsl-h"]');
  hue.value = '123';
  s.fire(hue, 'input');
  canvas.dispatchEvent(
    new s.win.PointerEvent('pointerdown', { bubbles: true, button: 0, pointerId: 8 }),
  );
  assert.equal(captures, 0);
  assert.equal(s.query('[data-research-hex]').value, hexBeforeStaleInput);
  s.session.switchComposition(first);
  s.panel.reconcile();
  assert.equal(s.query('[data-research-preview]').disabled, true);
  const beforeClose = s.captured();
  s.click('[data-key-color-cancel]');
  assert.equal(s.panel.editing, false);
  assert.equal(s.calls.length, 0);
  assert.deepEqual(s.captured(), beforeClose);
});

test('research preview and cancel preserve document revision and redo', (t) => {
  const s = setup(t, 'document', blankDocument());
  s.send('set-document-settings', { key: 'width', value: 1001 });
  s.send('undo');
  const before = s.captured();
  assert.equal(before.future.length, 1);
  s.open();
  for (const [selector, value, event] of [
    ['[data-research-hex]', '#aBcD09', 'input'],
    ['[data-research-alpha]', '0', 'input'],
    ['[data-research-plan]', 'research-neutral-start', 'change'],
  ]) {
    const el = s.query(selector);
    el.value = value;
    s.fire(el, event);
  }
  s.click('[data-research-preview]');
  assert.equal(s.document.querySelectorAll('[data-research-row]').length, 5);
  assert.match(s.query('[data-trial-status]').textContent, /Usage is not measured before creation/);
  assert.match(
    s.query('[data-trial-status]').textContent,
    /Readability and procedure not evaluated/,
  );
  s.click('[data-key-color-cancel]');
  assert.deepEqual(s.captured(), before);
  assert.deepEqual(s.calls, []);
});

test('retained B artwork projection expires on composition change and never rebases', (t) => {
  const s = setup(t, 'project', blankDocument());
  const first = s.session.activeCompositionId;
  const other = s.session.createComposition();
  s.session.switchComposition(first);
  s.open();
  for (const [selector, value, event] of [
    ['[data-research-hex]', '#3273c7', 'input'],
    ['[data-research-alpha]', '1', 'input'],
    ['[data-research-plan]', 'research-neutral-start', 'change'],
  ]) {
    s.query(selector).value = value;
    s.fire(s.query(selector), event);
  }
  s.click('[data-research-preview]');
  s.click('[data-research-create]');
  s.open();
  s.click('[data-key-color-preview]');
  s.click('[data-artwork-show]');
  assert.ok(s.panel.previewDocument);
  const before = s.session.projectSnapshot();
  const calls = s.calls.length;
  s.session.switchComposition(other);
  s.panel.reconcile();
  assert.equal(s.panel.previewDocument, null);
  assert.equal(s.query('[data-key-color-apply]').disabled, true);
  assert.match(s.query('[data-key-color-status]').textContent, /stale/i);
  s.session.switchComposition(first);
  s.panel.reconcile();
  assert.equal(s.panel.previewDocument, null);
  assert.equal(s.query('[data-key-color-preview]').disabled, true);
  assert.deepEqual(s.session.projectSnapshot(), before);
  assert.equal(s.calls.length, calls);
});

test('linked trial artwork preview is a valid detached projection and never authors history', (t) => {
  const s = setup(t, 'project', blankDocument());
  s.open();
  assert.equal(s.query('[data-research-live-option]').hidden, true);
  assert.equal(s.query('[data-research-live-warning]').hidden, true);
  for (const [selector, value, event] of [
    ['[data-research-hex]', '#3273c7', 'input'],
    ['[data-research-alpha]', '1', 'input'],
    ['[data-research-plan]', 'research-neutral-start', 'change'],
  ]) {
    s.query(selector).value = value;
    s.fire(s.query(selector), event);
  }
  assert.equal(s.query('[data-research-live-option]').hidden, false);
  assert.equal(s.query('[data-research-live-warning]').hidden, true);
  s.query('[data-research-live]').checked = true;
  s.fire(s.query('[data-research-live]'), 'change');
  assert.equal(s.query('[data-research-live-warning]').hidden, false);
  assert.match(
    s.query('[data-research-live-warning]').textContent,
    /Older Nect versions cannot open/,
  );
  assert.match(
    s.query('[data-research-live-warning]').textContent,
    /Detaching the palette is not yet available/,
  );
  s.query('[data-research-plan]').value = 'research-triadic-offset';
  s.fire(s.query('[data-research-plan]'), 'change');
  assert.equal(s.query('[data-research-live-option]').hidden, true);
  assert.equal(s.query('[data-research-live]').checked, false);
  assert.equal(s.query('[data-research-live-warning]').hidden, true);
  s.query('[data-research-plan]').value = 'research-neutral-start';
  s.fire(s.query('[data-research-plan]'), 'change');
  s.query('[data-research-live]').checked = true;
  s.fire(s.query('[data-research-live]'), 'change');
  s.click('[data-research-preview]');
  const beforeGetter = s.captured();
  assert.equal(s.panel.previewDocument, null);
  assert.equal(s.query('[data-research-create]').disabled, false);
  assert.ok(s.query('[data-research-row]'));
  assert.deepEqual(s.captured(), beforeGetter);
  s.click('[data-research-create]');
  assert.equal(s.snapshot().keyColorRecipe.mode, 'live');
  const before = s.captured();
  s.open();
  s.input('hex', '#e43f74');
  s.click('[data-key-color-preview]');
  s.click('[data-artwork-show]');
  const projected = s.panel.previewDocument;
  assert.ok(projected);
  assert.doesNotThrow(() => validate(projected));
  assert.doesNotThrow(() => deserialize(serialize(projected)));
  assert.equal(projected.keyColorRecipe.committedSnapshot.intent.keyColor.value.hex, '#e43f74');
  assert.deepEqual(s.captured(), before);
  s.click('[data-artwork-restore]');
  assert.equal(s.panel.previewDocument, null);
  assert.deepEqual(s.captured(), before);
});

test('B status uses current coverage by stable ID and keeps proposed L separate from saved state', async (t) => {
  let coverage,
    key = 'current',
    open = true;
  const s = setup(t, 'document', blankDocument(), () => coverage.snapshot());
  const plan = structuredClone(researchPlans.find((item) => item.id === 'research-neutral-start'));
  const payload = {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-neutral-text-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'test-palette',
    intent: {
      keyColor: { id: 'key', role: 'Identity', value: { hex: '#EbEbEb', alpha: 0 }, hueHint: 0 },
      preset: plan,
      slots: plan.slots.map(({ id, role }) => ({ id, role, deltaL: 0, deltaRatioCmax: 0 })),
      settings: { areaAware: true },
    },
    allocations: [
      { slotId: 'key', name: 'C1' },
      ...plan.slots.map(({ id, role }) => ({ slotId: id, name: role })),
    ],
  };
  s.send('initialize-key-color-recipe', payload);
  s.send('set-document-settings', { key: 'width', value: 1001 });
  s.send('undo');
  const before = s.captured();
  coverage = createColorCoverageView({
    app: s.document.body,
    getKey: () => key,
    isOpen: () => open,
    measure: async () => ({
      slots: [
        { id: 'key', percent: 37 },
        { id: 'neutral', percent: 0 },
      ],
      sampleSize: { width: 32, height: 32 },
      other: { percent: 63 },
    }),
  });
  s.open();
  assert.match(s.query('[data-trial-usage="key"]').textContent, /unavailable/i);
  coverage.refresh();
  await new Promise((resolve) => setImmediate(resolve));
  s.click('[data-trial-refresh]');
  assert.match(s.query('[data-trial-usage="key"]').textContent, /37%/);
  assert.match(s.query('[data-trial-usage="neutral"]').textContent, /0%/);
  assert.match(s.query('[data-trial-usage="accent"]').textContent, /unavailable/i);
  assert.doesNotMatch(s.query('[data-trial-row="key"]').textContent, /Same literal/); // RGB matches Neutral but alpha differs.
  const saved = s.query('[data-trial-status="Saved recipe"]');
  assert.match(saved.textContent, /Readability and procedure not evaluated/);
  assert.equal(saved.textContent.split('Readability and procedure not evaluated').length - 1, 1);
  const keyDetails = saved.querySelector('[data-trial-row="key"] [data-trial-details]');
  assert.equal(keyDetails.open, false);
  assert.equal(keyDetails.querySelector('code').textContent, 'key');
  const exactKeyL = before.doc.keyColorRecipe.committedSnapshot.resolved.find(
    (row) => row.id === 'key',
  ).L;
  assert.ok(keyDetails.textContent.includes(`C1 OKLCH L ${exactKeyL}`));
  assert.match(saved.querySelector('[data-trial-row="key"] [data-trial-l]').textContent, /^≈ /);
  const savedOrder = s.query('[data-trial-status="Saved recipe"] [data-model-l-order]').textContent;
  assert.match(savedOrder, /Derived model L order/);
  assert.match(savedOrder, /maintained/);
  assert.doesNotMatch(savedOrder, /invalid metadata/);
  s.input('deltaL', 2);
  s.click('[data-key-color-preview]');
  const comparison = s.query('[data-key-color-comparison]');
  assert.match(comparison.textContent, /Proposed colors · not applied/);
  assert.match(comparison.querySelector('[data-model-l-order]').textContent, /reversed/);
  assert.equal(s.query('[data-key-color-apply]').disabled, false);
  assert.equal(
    s.query('[data-trial-status="Saved recipe"] [data-model-l-order]').textContent,
    savedOrder,
  );
  assert.match(
    comparison.querySelector('[data-trial-row="assort"] [data-trial-notice]').textContent,
    /L range limited/,
  );
  assert.match(
    comparison.querySelector('[data-trial-row="assort"] [data-trial-details]').textContent,
    /ΔL 2/,
  );
  for (const id of ['key', 'accent', 'neutral', 'text']) {
    const row = comparison.querySelector(`[data-key-color-compare="${id}"]`);
    assert.equal(
      row.querySelector('[data-key-color-before] code').textContent,
      row.querySelector('[data-key-color-after] code').textContent,
    );
  }
  const changed = comparison.querySelector('[data-key-color-compare="assort"]');
  assert.notEqual(
    changed.querySelector('[data-key-color-before] code').textContent,
    changed.querySelector('[data-key-color-after] code').textContent,
  );
  assert.deepEqual(s.captured(), before);
  key = 'new-key';
  s.click('[data-trial-refresh]');
  assert.match(s.query('[data-trial-usage="key"]').textContent, /unavailable/i);
  assert.doesNotMatch(s.query('[data-trial-usage="key"]').textContent, /0%/);
  open = false;
  s.click('[data-trial-refresh]');
  assert.match(s.query('[data-trial-usage="neutral"]').textContent, /unavailable/i);
  s.click('[data-key-color-cancel]');
  assert.deepEqual(s.captured(), before);
  const matching = setup(t, 'document', blankDocument());
  const sameAlpha = structuredClone(payload);
  sameAlpha.intent.keyColor.value.alpha = 1;
  matching.send('initialize-key-color-recipe', sameAlpha);
  matching.open();
  assert.match(
    matching.query('[data-trial-row="key"]').textContent,
    /Same literal and alpha as Neutral/,
  );
});

test('B trial preview rejects shared color budget before enabling Create', (t) => {
  const doc = blankDocument();
  doc.schemaVersion = 54;
  doc.colorSlots = Array.from({ length: 252 }, (_, i) => ({
    id: `existing-${i}`,
    name: `Existing ${i}`,
    value: { hex: '#123456', alpha: 1 },
  }));
  const s = setup(t, 'document', doc),
    before = s.captured();
  s.open();
  for (const [selector, value, event] of [
    ['[data-research-hex]', '#aBcD09', 'input'],
    ['[data-research-alpha]', '0', 'input'],
    ['[data-research-plan]', 'research-neutral-start', 'change'],
  ]) {
    const el = s.query(selector);
    el.value = value;
    s.fire(el, event);
  }
  s.click('[data-research-preview]');
  assert.equal(s.query('[data-research-create]').disabled, true);
  assert.match(s.query('[data-key-color-status]').textContent, /budget/i);
  assert.deepEqual(s.captured(), before);
  assert.deepEqual(s.calls, []);
});

test('B trial disambiguates duplicate roles and names without changing stable targets', (t) => {
  const s = setup(t, 'document', blankDocument());
  const plan = structuredClone(researchPlans.find((item) => item.id === 'research-neutral-start'));
  plan.slots[0].id = 'a-1111';
  plan.slots[1].id = 'b-1111';
  plan.slots[1].role = plan.slots[0].role;
  plan.slots[2].role = 'Assort · a-1111';
  const payload = {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-neutral-text-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'duplicate-label-palette',
    intent: {
      keyColor: { id: 'key', role: 'Identity', value: { hex: '#3273c7', alpha: 1 }, hueHint: 0 },
      preset: plan,
      slots: plan.slots.map(({ id, role }) => ({ id, role, deltaL: 0, deltaRatioCmax: 0 })),
      settings: { areaAware: false },
    },
    allocations: [
      { slotId: 'key', name: 'C1' },
      { slotId: 'a-1111', name: 'Shared' },
      { slotId: 'b-1111', name: 'Shared' },
      { slotId: plan.slots[2].id, name: 'Shared · a-1111' },
      { slotId: plan.slots[3].id, name: 'Text' },
    ],
  };
  s.send('initialize-key-color-recipe', payload);
  const before = s.captured();
  s.open();
  const options = [...s.document.querySelectorAll('[data-pair-foreground] option')].slice(1);
  assert.equal(new Set(options.map((option) => option.textContent)).size, options.length);
  assert.deepEqual(
    options.map((option) => option.value),
    ['key', ...plan.slots.map((slot) => slot.id)],
  );
  assert.equal(
    s.query('[data-trial-row="a-1111"] [data-trial-details] code').textContent,
    'a-1111',
  );
  assert.notEqual(
    s.query('[data-key-color-slot="a-1111"]').getAttribute('aria-label'),
    s.query('[data-key-color-slot="b-1111"]').getAttribute('aria-label'),
  );
  s.click('[data-key-color-preview]');
  const names = [...s.document.querySelectorAll('[data-key-color-compare] th')].map(
    (heading) => heading.firstChild.textContent,
  );
  assert.equal(new Set(names).size, names.length);
  assert.equal(s.query('[data-key-color-compare="a-1111"] details code').textContent, 'a-1111');
  assert.equal(s.query('[data-key-color-compare="b-1111"] details code').textContent, 'b-1111');
  const foreground = s.query('[data-pair-foreground]');
  const background = s.query('[data-pair-background]');
  foreground.value = 'a-1111';
  background.value = 'b-1111';
  s.click('[data-pair-run]');
  assert.match(s.query('[data-pair-result]').textContent, /Current shared colors:/);
  assert.deepEqual(s.captured(), before);
});

test('B policy exposes Δr only for Assort and Accent, including with a custom role', (t) => {
  const s = setup(t, 'document', blankDocument());
  const plan = structuredClone(researchPlans.find((item) => item.id === 'research-neutral-start'));
  plan.slots.push({
    id: 'custom',
    role: 'Custom role',
    position: 0,
    lightnessScale: 1,
    lightnessOffset: 0,
    chromaScale: 0,
    chromaOffset: 0,
    alpha: 1,
  });
  const slots = plan.slots.map(({ id, role }) => ({
    id,
    role,
    deltaL: 0,
    deltaRatioCmax: id === 'custom' ? 0.2 : 0,
  }));
  const request = {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-neutral-text-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'custom-fixture',
    intent: {
      keyColor: { id: 'key', role: 'Identity', value: { hex: '#3273c7', alpha: 1 }, hueHint: 0 },
      preset: plan,
      slots,
      settings: { areaAware: true },
    },
    allocations: [
      { slotId: 'key', name: 'C1' },
      ...slots.map(({ id, role }) => ({ slotId: id, name: role })),
    ],
  };
  s.send('initialize-key-color-recipe', request);
  const before = s.captured();
  s.open();
  for (const id of ['assort', 'accent'])
    assert.ok(s.query(`[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${id}"]`));
  for (const id of ['neutral', 'text', 'custom'])
    assert.equal(
      s.query(`[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${id}"]`),
      null,
    );
  assert.ok(s.query('[data-key-color-input="deltaL"][data-key-color-slot="custom"]'));
  s.input('hex', '#aAaAaA');
  assert.equal(s.query('[data-key-color-preview]').disabled, true);
  assert.match(s.query('[data-key-color-status]').textContent, /gray.*no defined hue/i);
  s.click('[data-key-color-cancel]');
  assert.deepEqual(s.captured(), before);
  const offsetCase = setup(t, 'document', blankDocument());
  const offsetRequest = structuredClone(request);
  offsetRequest.intent.slots.find((slot) => slot.id === 'custom').deltaRatioCmax = 0;
  offsetRequest.intent.preset.slots.find((slot) => slot.id === 'custom').chromaOffset = 0.1;
  offsetCase.send('initialize-key-color-recipe', offsetRequest);
  const offsetBefore = offsetCase.captured();
  offsetCase.open();
  offsetCase.input('hex', '#808080');
  assert.equal(offsetCase.query('[data-key-color-preview]').disabled, true);
  offsetCase.click('[data-key-color-cancel]');
  assert.deepEqual(offsetCase.captured(), offsetBefore);
});

test('invalid input anywhere revokes an old preview, and unobserved edits cannot apply it', (t) => {
  const s = setup(t),
    before = s.captured();
  s.open();
  for (const [kind, value] of [
    ['hex', '#bad'],
    ['alpha', ''],
    ['alpha', -0.1],
    ['alpha', 1.1],
    ['hsl-h', ''],
    ['hsl-h', 361],
    ['hsl-s', -1],
    ['hsl-l', 101],
    ['deltaL', ''],
    ['deltaRatioCmax', 'Infinity'],
    ['deltaRatioCmax', '1e309'],
  ]) {
    s.click('[data-key-color-preview]');
    assert.equal(s.query('[data-key-color-apply]').disabled, false);
    const old = s.query(`[data-key-color-input="${kind}"]`).value;
    s.input(kind, value);
    assert.equal(s.query('[data-key-color-apply]').disabled, true, `${kind} ${value}`);
    assert.equal(s.query('[data-key-color-preview]').disabled, true);
    assert.equal(s.query('[data-key-color-comparison]').textContent, '');
    s.fire(s.query('[data-key-color-apply]'), 'click');
    assert.deepEqual(s.captured(), before);
    assert.deepEqual(s.calls, []);
    s.input(kind, old);
  }
  s.click('[data-key-color-preview]');
  s.query('[data-key-color-input="hex"]').value = '#aBcD08'; // No input event.
  s.click('[data-key-color-apply]');
  assert.deepEqual(s.calls, []);
  assert.equal(s.query('[data-key-color-apply]').disabled, true);
  assert.match(s.query('[data-key-color-status]').textContent, /Preview the current inputs/);
  s.input('hex', '#aBcD09');
  s.click('[data-key-color-preview]');
  s.query('[data-key-color-input="hsl-h"]').value = '120'; // No input event.
  s.click('[data-key-color-apply]');
  assert.deepEqual(s.calls, []);
  assert.deepEqual(s.captured(), before);
  assert.equal(s.query('[data-key-color-apply]').disabled, true);
});

test('Cancel, Escape and native dialog cancel preserve authored state and pending Redo', (t) => {
  const s = setup(t);
  s.send('set-color-slot', { slotId: 'other', value: { hex: '#010203', alpha: 0 } });
  s.send('undo');
  const before = s.captured();
  for (const dismiss of ['button', 'escape', 'native']) {
    s.open();
    s.input('hex', '#fFaA00');
    s.click('[data-key-color-preview]');
    if (dismiss === 'button') s.click('[data-key-color-cancel]');
    if (dismiss === 'escape')
      s.fire(s.query('[data-key-color-input]'), 'keydown', { key: 'Escape' });
    if (dismiss === 'native') s.fire(s.query('dialog'), 'cancel');
    assert.equal(s.query('dialog'), null);
    assert.deepEqual(s.captured(), before);
    assert.deepEqual(s.calls, []);
    assert.equal(s.document.activeElement, s.query('[data-key-color-recipe-open]'));
  }
});

test('IME blocks Preview/Apply; Escape during composition does not discard the editor', (t) => {
  const s = setup(t),
    before = s.captured();
  s.open();
  const input = s.query('[data-key-color-input]');
  s.fire(input, 'compositionstart');
  s.input('hsl-h', 240);
  assert.equal(s.query('[data-key-color-input="hex"]').value, '#aBcD09');
  s.fire(input, 'keydown', { key: 'Escape', isComposing: true });
  s.fire(s.query('dialog'), 'cancel');
  assert.ok(s.query('dialog'));
  assert.equal(s.query('[data-key-color-preview]').disabled, true);
  s.fire(input, 'compositionend');
  assert.equal(s.query('[data-key-color-preview]').disabled, false);
  assert.deepEqual(s.captured(), before);
});

for (const kind of ['edit', 'undo', 'switch', 'replace-owner'])
  test(`${kind} invalidates the captured envelope instead of rebasing`, (t) => {
    const s = setup(t, 'project');
    const first = s.session.activeCompositionId;
    const other = s.session.createComposition();
    s.session.switchComposition(first);
    s.open();
    s.click('[data-key-color-preview]');
    if (kind === 'edit')
      s.send('set-color-slot', { slotId: 'other', value: { hex: '#010203', alpha: 1 } });
    if (kind === 'undo') s.send('undo');
    if (kind === 'switch') s.session.switchComposition(other);
    if (kind === 'replace-owner')
      s.session = createCompositionSessionOwner(s.session.projectSnapshot());
    const before = s.captured();
    s.panel.reconcile();
    assert.equal(s.query('[data-key-color-apply]').disabled, true);
    assert.equal(s.query('[data-key-color-preview]').disabled, true);
    assert.match(s.query('[data-key-color-status]').textContent, /stale/);
    s.fire(s.query('[data-key-color-apply]'), 'click');
    assert.deepEqual(s.captured(), before);
    assert.deepEqual(s.calls, []);
  });

test('locked active targets reject whole Preview even when every literal equals the receipt', (t) => {
  const doc = retainedRecipeFixture();
  for (const row of doc.keyColorRecipe.committedSnapshot.resolved.filter((x) => x.active))
    doc.colorSlots.find((x) => x.id === row.id).value = { ...row.value };
  doc.colorDesign.lockedSlotIds.push('derived');
  const s = setup(t, 'document', doc),
    before = s.captured();
  s.open();
  s.click('[data-key-color-preview]');
  assert.match(s.query('[data-key-color-status]').textContent, /KEY_COLOR_TARGET_LOCKED/);
  assert.equal(s.query('[data-key-color-apply]').disabled, true);
  assert.deepEqual(s.captured(), before);
  assert.deepEqual(s.calls, []);
});

test('old nonopaque active derived recipe is read-only at alpha and rejected, never coerced', (t) => {
  const doc = retainedRecipeFixture(),
    saved = doc.keyColorRecipe.committedSnapshot;
  doc.keyColorRecipe.committedSnapshot = editKeyColorRecipe(saved, {
    type: 'preset',
    expectedRevision: saved.revision,
    preset: {
      ...saved.intent.preset,
      slots: saved.intent.preset.slots.map((slot) => ({ ...slot, alpha: 0.5 })),
    },
  });
  const s = setup(t, 'document', doc),
    before = s.captured();
  s.open();
  assert.match(s.query('dialog').textContent, /Stored derived alpha · 0.5/);
  s.click('[data-key-color-preview]');
  assert.match(s.query('[data-key-color-status]').textContent, /KEY_COLOR_DERIVED_ALPHA/);
  assert.equal(s.query('[data-key-color-apply]').disabled, true);
  assert.deepEqual(s.captured(), before);
});

for (const result of [null, undefined, false])
  test(`${String(result)} dispatch result never reports success`, (t) => {
    const s = setup(t),
      before = s.captured();
    s.dispatchResult = result;
    s.open();
    s.click('[data-key-color-preview]');
    s.click('[data-key-color-apply]');
    assert.ok(s.query('dialog'));
    assert.equal(s.query('[data-key-color-apply]').disabled, true);
    assert.match(s.query('[data-key-color-status]').textContent, /not accepted/);
    assert.equal(s.notices.length, 1);
    assert.equal(s.notices[0][1], true);
    assert.deepEqual(s.captured(), before);
  });
