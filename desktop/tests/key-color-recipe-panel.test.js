import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { blankDocument, CommandSession, serialize, deserialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createKeyColorRecipePanel } from '../src/key-color-recipe-panel.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';
import { retainedRecipeFixture } from './helpers/key-color-recipe-fixture.js';

function setup(t, kind = 'document', doc = retainedRecipeFixture()) {
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
  assert.match(s.query('[data-key-color-empty]').textContent, /does not create a preset/);
  assert.equal(s.query('[data-key-color-input]'), null);
  assert.equal(s.query('[data-key-color-apply]'), null);
  s.click('[data-key-color-cancel]');
  assert.deepEqual(s.captured(), before);
  assert.deepEqual(s.calls, []);
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
