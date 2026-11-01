import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { createKeyColorRecipe, editKeyColorRecipe } from '../src/key-color-recipe.js';
import { DEFAULT_HARMONY_WHEEL } from '../src/harmony-wheel.js';

const apply = 'apply-key-color-recipe-snapshot';
const keyValue = { hex: '#aBcD09', alpha: 0.37 },
  manual = { hex: '#998877', alpha: 0.12 };
function recipe(value = keyValue) {
  const snapshot = createKeyColorRecipe({
    paletteId: 'explicit-experimental-palette',
    keyColor: { id: 'key', role: 'Identity', value: { ...value } },
    preset: {
      id: 'explicit-coefficients-not-defaults',
      version: 1,
      harmony: 'triadic',
      hueSteps: 12,
      slots: [
        {
          id: 'derived',
          role: 'Background',
          position: 1,
          lightnessScale: 0,
          lightnessOffset: 0.83,
          chromaScale: 0,
          chromaOffset: 0,
        },
        {
          id: 'dormant',
          role: 'Accent',
          position: 2,
          lightnessScale: 0.7,
          lightnessOffset: 0.1,
          chromaScale: 0.8,
          chromaOffset: 0,
          alpha: 0.22,
        },
      ],
    },
    slots: [
      { id: 'derived', role: 'Background', deltaL: 0.03, deltaRatioCmax: 0 },
      { id: 'dormant', role: 'Accent', deltaL: 0, deltaRatioCmax: 0.05 },
    ],
    settings: { areaAware: false },
  });
  return editKeyColorRecipe(snapshot, {
    type: 'preset',
    expectedRevision: snapshot.revision,
    preset: { ...snapshot.intent.preset, slots: snapshot.intent.preset.slots.slice(0, 1) },
  });
}
function legacyDocument() {
  const doc = blankDocument();
  doc.schemaVersion = 57;
  doc.colorSlots = ['key', 'derived', 'dormant', 'other'].map((id, i) => ({
    id,
    name: `Legacy ${i}`,
    value: { hex: '#123456', alpha: i / 4 },
  }));
  doc.colorDesign = {
    version: 2,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: ['dormant'],
    wheel: { ...DEFAULT_HARMONY_WHEEL },
    palette: [
      { id: 'key', kind: 'chromatic', position: 0, ring: 3, role: 'Primary', overrideC: 0.12 },
      { id: 'derived', kind: 'neutral', L: 0.91, role: 'Background' },
    ],
  };
  const l = doc.layers[0],
    shape = node('Rectangle'),
    direct = node('Fill'),
    color = node('Color'),
    sourced = node('Fill'),
    out = l.nodes[0];
  l.nodes = [shape, direct, color, sourced, out];
  connect(l, shape.id, direct.id, 'path');
  connect(l, shape.id, sourced.id, 'path');
  connect(l, color.id, sourced.id, 'color');
  connect(l, direct.id, out.id, 'path');
  direct.params.color.colorSlotId = 'key';
  color.params.value.colorSlotId = 'derived';
  return doc;
}
function fixture(kind, doc = legacyDocument()) {
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  return {
    session,
    send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    run: () => {
      const d = session.historyView.doc;
      return evaluate(d.layers[0], {}, d.imageAssets, d);
    },
  };
}
function captured(s) {
  return {
    document: serialize(s.snapshot()),
    project: s.projectSnapshot ? serializeCompositionProject(s.projectSnapshot()) : null,
    state: structuredClone(s.state),
    projectRevision: s.projectRevision,
    past: structuredClone(s.historyView.past),
    future: structuredClone(s.historyView.future),
  };
}
const valueOf = (s, id) => s.snapshot().colorSlots.find((x) => x.id === id).value;
const rowValue = (s, id) => s.resolved.find((x) => x.id === id).value;
function withRedo(f) {
  f.send('set-color-slot', { slotId: 'other', value: manual });
  f.send('undo');
  return captured(f.session);
}
function reordered(v) {
  if (Array.isArray(v)) return v.map(reordered);
  if (v && typeof v === 'object')
    return Object.fromEntries(
      Object.entries(v)
        .reverse()
        .map(([k, x]) => [k, reordered(x)]),
    );
  return v;
}
function oldFormat(v) {
  const s = structuredClone(v);
  s.version = 1;
  for (const r of s.resolved) {
    delete r.rTarget;
    delete r.rRealized;
  }
  return s;
}
function reopen(s) {
  if (s.projectSnapshot) {
    const text = serializeCompositionProject(s.projectSnapshot());
    const reopened = createCompositionSessionOwner(deserializeCompositionProject(text));
    assert.equal(serializeCompositionProject(reopened.projectSnapshot()), text);
  } else {
    const text = serialize(s.snapshot());
    assert.equal(serialize(new CommandSession(deserialize(text)).snapshot()), text);
  }
}
for (const kind of ['document', 'composition']) {
  test(`${kind}: one-shot Apply is one Undo for active values plus receipt, preserving dormant and all legacy metadata`, () => {
    for (const alpha of [0, 0.37]) {
      const f = fixture(kind),
        before = captured(f.session),
        snapshot = recipe({ ...keyValue, alpha });
      const result = f.send(apply, { snapshot }),
        after = captured(f.session),
        d = f.session.snapshot(),
        old = JSON.parse(before.document);
      assert.equal(result.changed, true);
      assert.deepEqual(result.slotIds, ['key', 'derived']);
      assert.equal(result.paletteRevision, snapshot.revision);
      assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
      if (kind === 'composition') assert.equal(after.projectRevision, before.projectRevision + 1);
      assert.equal(after.past.length, before.past.length + 1);
      assert.deepEqual(after.future, []);
      assert.equal(d.schemaVersion, 58);
      assert.deepEqual(d.keyColorRecipe, {
        version: 1,
        mode: 'retained-only',
        committedSnapshot: snapshot,
      });
      assert.deepEqual(valueOf(f.session, 'key'), { ...keyValue, alpha });
      assert.deepEqual(valueOf(f.session, 'derived'), rowValue(snapshot, 'derived'));
      assert.equal(valueOf(f.session, 'derived').alpha, 1);
      assert.equal(JSON.stringify(d.colorDesign), JSON.stringify(old.colorDesign));
      assert.deepEqual(d.layers, old.layers);
      assert.deepEqual(
        d.colorSlots.map(({ id, name }) => ({ id, name })),
        old.colorSlots.map(({ id, name }) => ({ id, name })),
      );
      assert.deepEqual(d.colorSlots.slice(2), old.colorSlots.slice(2));
      reopen(f.session);
      f.send('undo');
      // Composition Undo/Redo rebuilds the active Document projection, whose
      // root key order is not authored meaning; the whole project bytes below
      // must still restore exactly, and every Document value is checked here.
      assert.deepEqual(f.session.snapshot(), JSON.parse(before.document));
      if (before.project)
        assert.equal(serializeCompositionProject(f.session.projectSnapshot()), before.project);
      f.send('redo');
      assert.deepEqual(f.session.snapshot(), JSON.parse(after.document));
      if (after.project)
        assert.equal(serializeCompositionProject(f.session.projectSnapshot()), after.project);
      snapshot.resolved[0].value.hex = '#000000';
      assert.deepEqual(valueOf(f.session, 'key'), { ...keyValue, alpha });
    }
  });
  test(`${kind}: receipt replacement and values commit together without bumping palette revision`, () => {
    const f = fixture(kind),
      a = recipe();
    f.send('retain-key-color-recipe-snapshot', { snapshot: a });
    const b = editKeyColorRecipe(a, {
        type: 'slot-delta',
        expectedRevision: a.revision,
        slotId: 'derived',
        deltaL: -0.1,
      }),
      before = captured(f.session);
    assert.equal(f.send(apply, { snapshot: b }).paletteRevision, b.revision);
    assert.deepEqual(f.session.snapshot().keyColorRecipe.committedSnapshot, b);
    assert.deepEqual(valueOf(f.session, 'derived'), rowValue(b, 'derived'));
    assert.equal(f.session.historyView.past.length, before.past.length + 1);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), JSON.parse(before.document));
    if (before.project)
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), before.project);
  });
  test(`${kind}: equal values and receipt preserve literal property order, revision and pending Redo`, () => {
    const snapshot = recipe(),
      d = legacyDocument();
    d.schemaVersion = 58;
    d.keyColorRecipe = { version: 1, mode: 'retained-only', committedSnapshot: snapshot };
    for (const row of snapshot.resolved.filter((r) => r.active))
      d.colorSlots.find((s) => s.id === row.id).value = {
        alpha: row.value.alpha,
        hex: row.value.hex,
      };
    const f = fixture(kind, d),
      before = withRedo(f);
    for (const candidate of [snapshot, reordered(snapshot), oldFormat(snapshot)]) {
      assert.equal(f.send(apply, { snapshot: candidate }).changed, false);
      assert.deepEqual(captured(f.session), before);
    }
  });
  test(`${kind}: every active legacy lock rejects the whole Apply, including already equal values`, () => {
    for (const id of ['key', 'derived']) {
      const f = fixture(kind),
        snapshot = recipe();
      f.send(apply, { snapshot });
      f.send('set-color-slot', { slotId: id, value: manual });
      const design = structuredClone(f.session.snapshot().colorDesign);
      design.lockedSlotIds.push(id);
      f.send('set-color-slots', { updates: [], design });
      let before = withRedo(f);
      assert.throws(() => f.send(apply, { snapshot }), /KEY_COLOR_TARGET_LOCKED/);
      assert.deepEqual(captured(f.session), before);
      f.send('set-color-slot', { slotId: id, value: rowValue(snapshot, id) });
      before = withRedo(f);
      assert.throws(() => f.send(apply, { snapshot }), /KEY_COLOR_TARGET_LOCKED/);
      assert.deepEqual(captured(f.session), before);
    }
  });
  test(`${kind}: manual changes retain history; captured envelope is stale; fresh unlocked Apply is explicit`, () => {
    const f = fixture(kind),
      snapshot = recipe();
    f.send(apply, { snapshot });
    const envelope = f.session.state;
    for (const slotId of ['key', 'derived', 'dormant', 'other'])
      f.send('set-color-slot', { slotId, value: manual });
    const before = captured(f.session);
    assert.deepEqual(f.session.snapshot().keyColorRecipe.committedSnapshot, snapshot);
    assert.throws(
      () => f.session.dispatch({ ...envelope, type: apply, payload: { snapshot } }),
      /STALE_REVISION/,
    );
    assert.deepEqual(captured(f.session), before);
    f.send(apply, { snapshot });
    assert.deepEqual(valueOf(f.session, 'key'), keyValue);
    assert.deepEqual(valueOf(f.session, 'derived'), rowValue(snapshot, 'derived'));
    assert.deepEqual(valueOf(f.session, 'dormant'), manual);
    assert.deepEqual(valueOf(f.session, 'other'), manual);
    assert.deepEqual(f.session.snapshot().keyColorRecipe.committedSnapshot, snapshot);
  });
  test(`${kind}: legacy v1 lock blocks first Apply without opting in; manual unlock needs a fresh envelope`, () => {
    const doc = legacyDocument();
    doc.colorDesign.version = 1;
    delete doc.colorDesign.wheel;
    delete doc.colorDesign.palette;
    doc.colorDesign.lockedSlotIds = ['derived'];
    const f = fixture(kind, doc),
      snapshot = recipe(),
      envelope = f.session.state,
      before = captured(f.session);
    assert.throws(() => f.send(apply, { snapshot }), /KEY_COLOR_TARGET_LOCKED/);
    assert.deepEqual(captured(f.session), before);
    assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
    f.send('set-color-slot', { slotId: 'key', value: manual });
    const design = structuredClone(f.session.snapshot().colorDesign);
    design.lockedSlotIds = [];
    f.send('set-color-slots', { updates: [], design });
    const unlocked = captured(f.session);
    assert.throws(
      () => f.session.dispatch({ ...envelope, type: apply, payload: { snapshot } }),
      /STALE_REVISION/,
    );
    assert.deepEqual(captured(f.session), unlocked);
    assert.equal(f.send(apply, { snapshot }).changed, true);
    assert.deepEqual(valueOf(f.session, 'key'), keyValue);
    assert.deepEqual(f.session.snapshot().colorDesign, design);
  });
  test(`${kind}: malformed, foreign and non-JSON inputs leave values, receipt, History and Redo intact`, () => {
    const f = fixture(kind),
      snapshot = recipe();
    f.send(apply, { snapshot });
    const before = withRedo(f);
    const variant = (fn) => {
      const v = structuredClone(snapshot);
      fn(v);
      return v;
    };
    const bad = [
      variant((s) => {
        s.resolved[1].value.hex = '#bad';
      }),
      variant((s) => {
        s.intent.keyColor.value.alpha = NaN;
      }),
      variant((s) => {
        s.intent.preset.slots[0].alpha = undefined;
      }),
      variant((s) => {
        s.intent.slots[0].deltaL = Infinity;
      }),
      variant((s) => {
        s.diagnostics[0] = () => {};
      }),
      variant((s) => {
        s.resolved[1].id = 'missing-active';
      }),
      variant((s) => {
        s.intent.slots[1].id = 'missing-dormant';
        s.resolved[2].id = 'missing-dormant';
      }),
      variant((s) => {
        s.resolved[2].id = s.resolved[1].id;
      }),
      variant((s) => {
        delete s.resolved[1];
      }),
    ];
    for (const candidate of bad) {
      assert.throws(() => f.send(apply, { snapshot: candidate }));
      assert.deepEqual(captured(f.session), before);
    }
    for (const payload of [{}, { snapshot, extra: true }, { snapshot, updates: [] }]) {
      assert.throws(() => f.send(apply, payload));
      assert.deepEqual(captured(f.session), before);
    }
    assert.throws(
      () =>
        f.session.dispatch({
          ...f.session.state,
          sessionId: 'wrong',
          type: apply,
          payload: { snapshot },
        }),
      /STALE_SESSION/,
    );
    assert.deepEqual(captured(f.session), before);
  });
  test(`${kind}: opaque-derived Apply rejects old alpha intent but retention and dormant alpha stay unchanged`, () => {
    const snapshot = recipe(),
      nonopaque = editKeyColorRecipe(snapshot, {
        type: 'preset',
        expectedRevision: snapshot.revision,
        preset: {
          ...snapshot.intent.preset,
          slots: snapshot.intent.preset.slots.map((p) => ({ ...p, alpha: 0.2 })),
        },
      });
    const f = fixture(kind);
    f.send('retain-key-color-recipe-snapshot', { snapshot: nonopaque });
    const before = withRedo(f);
    assert.throws(() => f.send(apply, { snapshot: nonopaque }), /KEY_COLOR_DERIVED_ALPHA/);
    assert.deepEqual(captured(f.session), before);
    const badPlan = structuredClone(snapshot);
    badPlan.intent.preset.slots[0].alpha = 0.2;
    assert.throws(() => f.send(apply, { snapshot: badPlan }), /KEY_COLOR_DERIVED_ALPHA/);
    assert.deepEqual(captured(f.session), before);
    const explicit = editKeyColorRecipe(snapshot, {
      type: 'preset',
      expectedRevision: snapshot.revision,
      preset: {
        ...snapshot.intent.preset,
        slots: snapshot.intent.preset.slots.map((p) => ({ ...p, alpha: 1 })),
      },
    });
    f.send(apply, { snapshot: explicit });
    assert.equal(valueOf(f.session, 'derived').alpha, 1);
    assert.equal(
      f.session.snapshot().keyColorRecipe.committedSnapshot.resolved[2].value.alpha,
      0.22,
    );
  });
  test(`${kind}: deactivation never retargets or updates inactive live values; clear leaves all values`, () => {
    const f = fixture(kind),
      snapshot = recipe();
    f.send(apply, { snapshot });
    f.send('set-color-slot', { slotId: 'derived', value: manual });
    const inactive = editKeyColorRecipe(snapshot, {
      type: 'preset',
      expectedRevision: snapshot.revision,
      preset: { ...snapshot.intent.preset, slots: [] },
    });
    f.send(apply, { snapshot: inactive });
    assert.deepEqual(valueOf(f.session, 'derived'), manual);
    assert.equal(f.session.snapshot().keyColorRecipe.committedSnapshot.resolved[1].active, false);
    const values = f.session.snapshot().colorSlots;
    f.send('clear-key-color-recipe-snapshot');
    assert.deepEqual(f.session.snapshot().colorSlots, values);
    assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
    assert.equal(f.session.snapshot().schemaVersion, 58);
  });
  test(`${kind}: Apply and Undo/Redo invalidate warm direct Fill and upstream Color caches`, () => {
    const f = fixture(kind),
      snapshot = recipe(),
      nodes = f.session.snapshot().layers[0].nodes,
      direct = nodes.find((n) => n.type === 'Fill'),
      color = nodes.find((n) => n.type === 'Color'),
      sourced = nodes.filter((n) => n.type === 'Fill')[1],
      fallback = nodes.map((n) => n.params);
    f.run().run(direct.id);
    f.run().run(sourced.id);
    const warm = f.run();
    warm.run(direct.id);
    warm.run(sourced.id);
    assert.equal(warm.stats.computed, 0);
    const old = valueOf(f.session, 'key');
    f.send(apply, { snapshot });
    const updated = f.run();
    assert.deepEqual(updated.run(direct.id).fill, keyValue);
    assert.deepEqual(updated.run(color.id), rowValue(snapshot, 'derived'));
    assert.deepEqual(updated.run(sourced.id).fill, rowValue(snapshot, 'derived'));
    assert.ok(updated.stats.computed > 0);
    assert.deepEqual(
      f.session.snapshot().layers[0].nodes.map((n) => n.params),
      fallback,
    );
    f.send('undo');
    assert.deepEqual(f.run().run(direct.id).fill, old);
    f.send('redo');
    assert.deepEqual(f.run().run(direct.id).fill, keyValue);
  });
  test(`${kind}: failed pure previews stay detached and Apply refuses an active gesture`, () => {
    const f = fixture(kind),
      snapshot = recipe();
    f.send('retain-key-color-recipe-snapshot', { snapshot });
    const preview = editKeyColorRecipe(snapshot, {
        type: 'key-color',
        expectedRevision: snapshot.revision,
        value: { hex: '#4866D8', alpha: 0 },
      }),
      copy = structuredClone(preview),
      before = withRedo(f);
    assert.throws(() =>
      editKeyColorRecipe(preview, {
        type: 'slot-delta',
        expectedRevision: preview.revision,
        slotId: 'derived',
        deltaL: NaN,
      }),
    );
    assert.deepEqual(preview, copy);
    assert.deepEqual(captured(f.session), before);
    const gesture = f.send('begin-gesture', {
        target: { kind: 'color-slots', slotIds: ['key', 'derived'] },
      }),
      during = captured(f.session);
    assert.throws(() => f.send(apply, { snapshot: preview }), /GESTURE_ACTIVE/);
    assert.deepEqual(captured(f.session), during);
    f.send('cancel-gesture', { gestureId: gesture.gestureId });
    assert.deepEqual(captured(f.session), before);
  });
}
test('Composition Apply reaches inactive consumers without changing their authored nodes', () => {
  const f = fixture('composition'),
    snapshot = recipe(),
    boundId = f.session.activeCompositionId;
  f.session.createComposition({ name: 'Unbound active sibling' });
  const before = f.session.projectSnapshot();
  f.send(apply, { snapshot });
  assert.deepEqual(f.session.projectSnapshot().compositions, before.compositions);
  f.session.switchComposition(boundId);
  const direct = f.session.snapshot().layers[0].nodes.find((n) => n.type === 'Fill');
  assert.deepEqual(f.run().run(direct.id).fill, keyValue);
});
