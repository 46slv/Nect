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
import {
  createKeyColorRecipe,
  editKeyColorRecipe,
  restoreKeyColorRecipe,
  KEY_COLOR_NEUTRAL_SOLVER_VERSION,
} from '../src/key-color-recipe.js';
import { createKeyColorRecipePanel } from '../src/key-color-recipe-panel.js';

// Only a development fixture, never a product palette or default.
function request(solverVersion = KEY_COLOR_NEUTRAL_SOLVER_VERSION) {
  const slots = [
    {
      id: 'neutral-test',
      role: 'Neutral',
      position: 0,
      lightnessScale: 0,
      lightnessOffset: 0.8,
      chromaScale: 0,
      chromaOffset: 0,
      alpha: 1,
    },
    {
      id: 'text-test',
      role: 'Text',
      position: 0,
      lightnessScale: 0,
      lightnessOffset: 0.2,
      chromaScale: 0,
      chromaOffset: 0,
      alpha: 1,
    },
    {
      id: 'accent-test',
      role: 'Accent',
      position: 1,
      lightnessScale: 0.7,
      lightnessOffset: 0.1,
      chromaScale: 0.8,
      chromaOffset: 0,
      alpha: 1,
    },
  ];
  return {
    version: 1,
    modelVersion: 2,
    solverVersion,
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'test-policy-palette',
    intent: {
      keyColor: {
        id: 'key-test',
        role: 'Identity',
        value: { hex: '#aBcD09', alpha: 0 },
        hueHint: 17,
      },
      preset: { id: 'test-policy-plan', version: 1, harmony: 'triadic', hueSteps: 12, slots },
      slots: slots.map((slot) => ({
        id: slot.id,
        role: slot.role,
        deltaL: 0,
        deltaRatioCmax: slot.role === 'Neutral' ? 0.25 : slot.role === 'Text' ? -0.15 : 0.1,
      })),
      settings: { areaAware: false },
    },
    allocations: ['key-test', ...slots.map((slot) => slot.id)].map((slotId) => ({
      slotId,
      name: slotId,
    })),
  };
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const make = (payload) =>
  createKeyColorRecipe({
    paletteId: payload.paletteId,
    ...payload.intent,
    solverVersion: payload.solverVersion,
  });
const row = (snapshot, id) => snapshot.resolved.find((entry) => entry.id === id);

test('new policy validates achromatic supports, preserves intent and retains dormant output', () => {
  const p = request(),
    snapshot = make(p);
  for (const id of ['neutral-test', 'text-test']) {
    assert.equal(row(snapshot, id).C, 0);
    assert.equal(row(snapshot, id).rTarget, 0);
    assert.ok(row(snapshot, id).diagnostics.includes('support-chroma-intent-unapplied'));
  }
  assert.equal(snapshot.intent.slots[0].deltaRatioCmax, 0.25);
  assert.ok(row(snapshot, 'accent-test').rTarget > 0);
  assert.deepEqual(restoreKeyColorRecipe(JSON.parse(JSON.stringify(snapshot))), snapshot);
  const bad = structuredClone(p);
  bad.intent.preset.slots[0].chromaScale = 0.1;
  assert.throws(() => make(bad), /zero chroma/);
  const forged = structuredClone(snapshot);
  forged.resolved[1].rTarget = 0.25;
  assert.throws(() => restoreKeyColorRecipe(forged), /ratio metadata/);
  const detached = editKeyColorRecipe(snapshot, {
    type: 'preset',
    expectedRevision: 0,
    preset: {
      ...snapshot.intent.preset,
      slots: snapshot.intent.preset.slots.filter((s) => s.id !== 'neutral-test'),
    },
  });
  assert.deepEqual(row(detached, 'neutral-test'), {
    ...row(snapshot, 'neutral-test'),
    active: false,
  });
  assert.equal(detached.intent.slots[0].deltaRatioCmax, 0.25);
  const reopened = restoreKeyColorRecipe(JSON.parse(JSON.stringify(detached)));
  assert.deepEqual(reopened, detached);
  const active = editKeyColorRecipe(reopened, {
    type: 'preset',
    expectedRevision: reopened.revision,
    preset: snapshot.intent.preset,
  });
  assert.deepEqual(row(active, 'neutral-test'), row(snapshot, 'neutral-test'));
  const old = make(request('experimental-affine-v1'));
  assert.equal(old.solverVersion, 'experimental-affine-v1');
  assert.equal(row(old, 'neutral-test').rTarget, 0.25);
  assert.ok(row(old, 'neutral-test').C > 0);
  assert.deepEqual(restoreKeyColorRecipe(old), old);
});

test('typed initialization, retained panel Apply, native roundtrip and Undo/Redo preserve policy and IDs', async (t) => {
  const session = new CommandSession(blankDocument()),
    before = session.snapshot();
  send(session, 'initialize-key-color-recipe', request());
  const initialized = session.snapshot();
  assert.equal(
    initialized.keyColorRecipe.committedSnapshot.solverVersion,
    KEY_COLOR_NEUTRAL_SOLVER_VERSION,
  );
  assert.deepEqual(
    initialized.colorSlots.map((s) => s.id),
    ['key-test', 'neutral-test', 'text-test', 'accent-test'],
  );
  assert.deepEqual(deserialize(serialize(initialized)), initialized);
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  const document = win.document;
  document.body.innerHTML = '<button data-key-color-recipe-open>Key color</button>';
  const panel = createKeyColorRecipePanel({
    document,
    getSession: () => session,
    getDocument: () => session.historyView.doc,
    dispatch: (type, payload, envelope) => session.dispatch({ ...envelope, type, payload }),
    notify: () => {},
  });
  panel.open(document.querySelector('button'));
  for (const id of ['neutral-test', 'text-test']) {
    assert.ok(
      document.querySelector(`[data-key-color-input="deltaL"][data-key-color-slot="${id}"]`),
    );
    assert.equal(
      document.querySelector(
        `[data-key-color-input="deltaRatioCmax"][data-key-color-slot="${id}"]`,
      ),
      null,
    );
  }
  assert.match(
    document.querySelector('[data-key-color-support-intent]').textContent,
    /0.25.*not applied.*support-chroma-intent-unapplied/,
  );
  assert.ok(
    document.querySelector(
      '[data-key-color-input="deltaRatioCmax"][data-key-color-slot="accent-test"]',
    ),
  );
  const input = document.querySelector(
    '[data-key-color-input="deltaL"][data-key-color-slot="neutral-test"]',
  );
  input.value = '0.05';
  input.dispatchEvent(new win.Event('input', { bubbles: true }));
  document.querySelector('[data-key-color-preview]').click();
  assert.equal(document.querySelector('[data-key-color-apply]').disabled, false);
  document.querySelector('[data-key-color-apply]').click();
  const edited = session.snapshot(),
    saved = edited.keyColorRecipe.committedSnapshot;
  assert.equal(saved.intent.slots[0].deltaRatioCmax, 0.25);
  assert.equal(saved.intent.slots[0].deltaL, 0.05);
  assert.equal(row(saved, 'neutral-test').C, 0);
  assert.ok(row(saved, 'neutral-test').diagnostics.includes('support-chroma-intent-unapplied'));
  assert.deepEqual(deserialize(serialize(edited)), edited);
  const dormant = editKeyColorRecipe(saved, {
    type: 'preset',
    expectedRevision: saved.revision,
    preset: {
      ...saved.intent.preset,
      slots: saved.intent.preset.slots.filter((slot) => slot.id !== 'neutral-test'),
    },
  });
  send(session, 'apply-key-color-recipe-snapshot', { snapshot: dormant });
  const withDormant = session.snapshot();
  assert.equal(row(withDormant.keyColorRecipe.committedSnapshot, 'neutral-test').active, false);
  assert.equal(withDormant.keyColorRecipe.committedSnapshot.intent.slots[0].deltaRatioCmax, 0.25);
  assert.deepEqual(deserialize(serialize(withDormant)), withDormant);
  assert.deepEqual(
    withDormant.colorSlots.find((s) => s.id === 'neutral-test'),
    edited.colorSlots.find((s) => s.id === 'neutral-test'),
  );
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), edited);
  send(session, 'redo');
  assert.deepEqual(session.snapshot(), withDormant);
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), edited);
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), initialized);
  send(session, 'redo');
  assert.deepEqual(session.snapshot(), edited);
  send(session, 'undo');
  send(session, 'undo');
  assert.deepEqual(session.snapshot(), before);
  send(session, 'redo');
  assert.deepEqual(session.snapshot(), initialized);
});

test('new policy cannot be installed over legacy or through retain of arbitrary existing colors', async (t) => {
  const session = new CommandSession(blankDocument());
  const oldRequest = request('experimental-affine-v1');
  send(session, 'initialize-key-color-recipe', oldRequest);
  const before = session.snapshot(),
    state = { ...session.state },
    history = structuredClone(session.historyView);
  const next = make(request());
  const win = new Window();
  t.after(() => win.happyDOM.abort());
  win.document.body.innerHTML = '<button>Key color</button>';
  const panel = createKeyColorRecipePanel({
    document: win.document,
    getSession: () => session,
    getDocument: () => session.historyView.doc,
    dispatch: (type, payload, envelope) => session.dispatch({ ...envelope, type, payload }),
    notify: () => {},
  });
  panel.open(win.document.querySelector('button'));
  assert.ok(
    win.document.querySelector(
      '[data-key-color-input="deltaRatioCmax"][data-key-color-slot="neutral-test"]',
    ),
  );
  assert.equal(win.document.querySelector('[data-key-color-support-intent]'), null);
  panel.close();
  assert.deepEqual(session.snapshot(), before);
  for (const type of ['apply-key-color-recipe-snapshot', 'replace-key-color-recipe-snapshot']) {
    assert.throws(() => send(session, type, { snapshot: next }), /create-only initialization/);
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    assert.deepEqual(session.historyView.past, history.past);
    assert.deepEqual(session.historyView.future, history.future);
  }
  send(session, 'clear-key-color-recipe-snapshot');
  const noReceipt = session.snapshot();
  assert.throws(
    () => send(session, 'retain-key-color-recipe-snapshot', { snapshot: next }),
    /create-only initialization/,
  );
  assert.deepEqual(session.snapshot(), noReceipt);
});

test('Composition Session retains new policy atomically and rejects invalid support without losing redo', () => {
  const session = createCompositionSessionOwner(
    migrateLegacyDocumentToCompositionProject(blankDocument()),
  );
  const before = session.projectSnapshot();
  send(session, 'initialize-key-color-recipe', request());
  const after = session.projectSnapshot();
  assert.equal(
    after.keyColorRecipe.committedSnapshot.solverVersion,
    KEY_COLOR_NEUTRAL_SOLVER_VERSION,
  );
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(after)), after);
  send(session, 'undo');
  assert.deepEqual(session.projectSnapshot(), before);
  const state = { ...session.state },
    future = structuredClone(session.historyView.future);
  const invalid = request();
  invalid.intent.preset.slots[0].chromaOffset = 0.1;
  assert.throws(() => send(session, 'initialize-key-color-recipe', invalid), /zero chroma/);
  assert.deepEqual(session.projectSnapshot(), before);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.historyView.future, future);
  send(session, 'redo');
  assert.deepEqual(session.projectSnapshot(), after);
});
