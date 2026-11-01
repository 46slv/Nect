import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import {
  blankDocument,
  node,
  CommandSession,
  validate,
  serialize,
  deserialize,
  exportSVG,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
  activeCompositionDocument,
  applyActiveCompositionDocument,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  createKeyColorRecipe,
  editKeyColorRecipe,
  restoreKeyColorRecipe,
} from '../src/key-color-recipe.js';
import { KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION } from '../src/key-color-recipe-document.js';
import { DEFAULT_HARMONY_WHEEL } from '../src/harmony-wheel.js';
import { readSVGSource } from '../src/svg-tree.js';

const retain = 'retain-key-color-recipe-snapshot';
const replace = 'replace-key-color-recipe-snapshot';
const clear = 'clear-key-color-recipe-snapshot';
const original = { hex: '#aBcD09', alpha: 0.37 };
const changed = { hex: '#123456', alpha: 0 };
function legacyDocument() {
  const doc = blankDocument();
  doc.schemaVersion = 57;
  doc.colorSlots = [
    { id: 'legacy-chromatic', name: 'Legacy color', value: { ...original } },
    { id: 'legacy-neutral', name: 'Legacy neutral', value: { hex: '#DdCcBb', alpha: 0.2 } },
    { id: 'dormant', name: 'Unused referenced identity', value: { hex: '#010203', alpha: 0 } },
  ];
  doc.colorDesign = {
    version: 2,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: ['legacy-neutral'],
    wheel: { ...DEFAULT_HARMONY_WHEEL },
    palette: [
      {
        id: 'legacy-chromatic',
        kind: 'chromatic',
        position: 0,
        ring: 3,
        role: 'Primary',
        overrideC: 0.12,
      },
      { id: 'legacy-neutral', kind: 'neutral', L: 0.91, role: 'Background' },
    ],
  };
  const color = node('Color');
  color.params.value.colorSlotId = 'legacy-chromatic';
  doc.layers[0].nodes.push(color);
  return doc;
}
function receipt() {
  const active = createKeyColorRecipe({
    paletteId: 'retained-palette',
    keyColor: { id: 'legacy-chromatic', role: 'Identity', value: { ...original } },
    preset: {
      id: 'explicit-fixture-only',
      version: 1,
      harmony: 'triadic',
      hueSteps: 12,
      slots: [
        {
          id: 'legacy-neutral',
          role: 'Background',
          position: 0,
          lightnessScale: 0,
          lightnessOffset: 0.83,
          chromaScale: 0,
          chromaOffset: 0,
          alpha: 0.2,
        },
        {
          id: 'dormant',
          role: 'Secondary',
          position: 1,
          lightnessScale: 0.7,
          lightnessOffset: 0.15,
          chromaScale: 0.8,
          chromaOffset: 0,
          alpha: 0,
        },
      ],
    },
    slots: [
      { id: 'legacy-neutral', role: 'Background', deltaL: 0.04, deltaRatioCmax: 0.02 },
      { id: 'dormant', role: 'Secondary', deltaL: -0.03, deltaRatioCmax: 0.05 },
    ],
    settings: { areaAware: false },
  });
  return editKeyColorRecipe(active, {
    type: 'preset',
    expectedRevision: active.revision,
    preset: { ...active.intent.preset, slots: active.intent.preset.slots.slice(0, 1) },
  });
}
const packet = (snapshot = receipt()) => ({
  version: 1,
  mode: 'retained-only',
  committedSnapshot: snapshot,
});
function fixture(kind, doc = legacyDocument()) {
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  return {
    session,
    send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
  };
}
function authored(session) {
  return {
    document: session.snapshot(),
    ...(session.projectSnapshot ? { project: session.projectSnapshot() } : {}),
  };
}
const authoredFrom = ({ document, project }) => ({ document, ...(project ? { project } : {}) });
function captured(session) {
  return {
    ...authored(session),
    state: structuredClone(session.state),
    projectRevision: session.projectRevision,
    past: structuredClone(session.historyView.past),
    future: structuredClone(session.historyView.future),
  };
}
function reopen(session) {
  const expected = authored(session),
    reopened = session.projectSnapshot
      ? createCompositionSessionOwner(
          deserializeCompositionProject(serializeCompositionProject(session.projectSnapshot())),
          { activeCompositionId: session.activeCompositionId },
        )
      : new CommandSession(deserialize(serialize(session.snapshot())));
  assert.deepEqual(authored(reopened), expected);
}
function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}
const Parser = new Window().DOMParser;
const svgSource = () =>
  readSVGSource(
    '<svg width="100" height="100"><rect width="50" height="50" fill="#123456"/></svg>',
    Parser,
  );

for (const kind of ['document', 'composition']) {
  test(`${kind}: explicit retention is one metadata-only Undo and native reopen preserves every legacy value`, () => {
    const f = fixture(kind),
      before = captured(f.session),
      snapshot = freeze(receipt());
    assert.equal(snapshot.resolved.find((r) => r.id === 'dormant').active, false);
    assert.notDeepEqual(
      snapshot.resolved.find((r) => r.id === 'legacy-neutral').value,
      before.document.colorSlots[1].value,
    );
    assert.equal(Object.hasOwn(before.document, 'keyColorRecipe'), false);
    assert.equal(before.document.schemaVersion, 57);
    const result = f.send(retain, freeze({ snapshot })),
      after = captured(f.session);
    assert.equal(result.changed, true);
    assert.equal(after.document.schemaVersion, KEY_COLOR_RECIPE_DOCUMENT_SCHEMA_VERSION);
    assert.deepEqual(after.document.keyColorRecipe, packet(snapshot));
    assert.equal(
      JSON.stringify(after.document.colorDesign),
      JSON.stringify(before.document.colorDesign),
    );
    assert.equal(
      JSON.stringify(after.document.colorSlots),
      JSON.stringify(before.document.colorSlots),
    );
    assert.deepEqual(after.document.layers, before.document.layers);
    assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
    assert.equal(after.past.length, before.past.length + 1);
    assert.deepEqual(after.future, []);
    const detached = f.session.snapshot();
    detached.keyColorRecipe.committedSnapshot.resolved[0].value.hex = '#000000';
    assert.deepEqual(f.session.snapshot().keyColorRecipe, packet(snapshot));
    assert.throws(() => {
      f.session.historyView.doc.keyColorRecipe.mode = 'active';
    }, TypeError);
    reopen(f.session);
    f.send('undo');
    assert.deepEqual(authored(f.session), authoredFrom(before));
    f.send('redo');
    assert.deepEqual(authored(f.session), {
      document: after.document,
      ...(after.project ? { project: after.project } : {}),
    });
  });

  test(`${kind}: retained history never drives or blocks legacy set/rename/bind/unbind or Absolute-C edits`, () => {
    const f = fixture(kind);
    f.send(retain, { snapshot: receipt() });
    const retained = structuredClone(f.session.snapshot().keyColorRecipe),
      layerId = f.session.snapshot().layers[0].id,
      nodeId = f.session.snapshot().layers[0].nodes.find((n) => n.type === 'Color').id;
    f.send('set-color-slot', { slotId: 'legacy-chromatic', value: changed });
    f.send('rename-color-slot', {
      slotId: 'legacy-neutral',
      name: 'Renamed without changing recipe role',
    });
    f.send('set-color-slots', {
      updates: [{ slotId: 'dormant', value: { hex: '#998877', alpha: 0.88 } }],
    });
    f.send('unbind-color-slot', { layerId, nodeId, key: 'value' });
    assert.deepEqual(
      f.session.snapshot().layers[0].nodes.find((n) => n.id === nodeId).params.value.value,
      changed,
    );
    f.send('bind-color-slot', { layerId, nodeId, key: 'value', slotId: 'legacy-neutral' });
    const design = structuredClone(f.session.snapshot().colorDesign);
    design.palette[0].overrideC = 0.23;
    design.palette[1].L = 0.95;
    f.send('set-color-slots', { updates: [], design });
    const current = f.session.snapshot();
    assert.deepEqual(current.keyColorRecipe, retained);
    assert.deepEqual(current.colorSlots[0].value, changed);
    assert.equal(current.colorSlots[1].name, 'Renamed without changing recipe role');
    assert.equal(
      current.layers[0].nodes.find((n) => n.id === nodeId).params.value.colorSlotId,
      'legacy-neutral',
    );
    assert.deepEqual(current.colorDesign, design);
    reopen(f.session); // Deliberate current-value/name mismatch is historical, not a load error.
  });

  test(`${kind}: explicit replace/clear, equal replacement and absent clear preserve no-op Redo semantics`, () => {
    const f = fixture(kind),
      a = receipt();
    f.send(retain, { snapshot: a });
    const b = editKeyColorRecipe(a, {
      type: 'slot-delta',
      expectedRevision: a.revision,
      slotId: 'legacy-neutral',
      deltaL: 0.09,
    });
    f.send(replace, { snapshot: b });
    f.send('undo');
    const withRedo = captured(f.session);
    assert.equal(f.send(replace, { snapshot: structuredClone(a) }).changed, false);
    assert.deepEqual(captured(f.session), withRedo);
    const reorder = (value) =>
      Array.isArray(value)
        ? value.map(reorder)
        : value && typeof value === 'object'
          ? Object.fromEntries(
              Object.entries(value)
                .reverse()
                .map(([key, child]) => [key, reorder(child)]),
            )
          : value;
    const oldFormat = {
      ...structuredClone(a),
      version: 1,
      resolved: a.resolved.map(({ rTarget, rRealized, ...row }) => row),
    };
    for (const equivalent of [reorder(a), oldFormat]) {
      assert.equal(f.send(replace, { snapshot: equivalent }).changed, false);
      assert.deepEqual(captured(f.session), withRedo);
    }
    f.send('redo');
    assert.deepEqual(f.session.snapshot().keyColorRecipe, packet(b));
    const beforeClear = authored(f.session);
    f.send(clear);
    const afterClear = captured(f.session);
    assert.equal(Object.hasOwn(afterClear.document, 'keyColorRecipe'), false);
    assert.equal(afterClear.document.schemaVersion, 58);
    assert.deepEqual(afterClear.document.colorDesign, beforeClear.document.colorDesign);
    assert.deepEqual(afterClear.document.colorSlots, beforeClear.document.colorSlots);
    f.send('undo');
    assert.deepEqual(authored(f.session), beforeClear);
    f.send('redo');
    const empty = captured(f.session);
    assert.equal(f.send(clear).changed, false);
    assert.deepEqual(captured(f.session), empty);
    reopen(f.session);
  });

  test(`${kind}: invalid/stale/ambiguous packet operations preserve authored state, History, Redo and revisions`, () => {
    const f = fixture(kind),
      a = receipt();
    f.send(retain, { snapshot: a });
    f.send(clear);
    f.send('undo');
    const before = captured(f.session),
      missing = structuredClone(a);
    missing.intent.slots.find((r) => r.id === 'dormant').id = 'missing-registry-id';
    missing.resolved.find((r) => r.id === 'dormant').id = 'missing-registry-id';
    assert.doesNotThrow(() => restoreKeyColorRecipe(missing));
    const invalidSnapshots = [
      null,
      {},
      missing,
      { ...a, version: 99 },
      (() => {
        const v = structuredClone(a);
        v.resolved[0].linear[0] = NaN;
        return v;
      })(),
      (() => {
        const v = structuredClone(a);
        v.resolved[0].rRealized = 0;
        return v;
      })(),
    ];
    for (const snapshot of invalidSnapshots) {
      assert.throws(() => f.send(replace, { snapshot }));
      assert.deepEqual(captured(f.session), before);
    }
    for (const [type, payload] of [
      [retain, { snapshot: a }],
      [replace, {}],
      [clear, { snapshot: a }],
      [replace, { snapshot: a, mode: 'active' }],
    ]) {
      assert.throws(() => f.send(type, payload));
      assert.deepEqual(captured(f.session), before);
    }
    for (const type of [retain, replace, clear])
      for (const envelope of [
        { ...f.session.state, expectedRevision: f.session.state.expectedRevision - 1 },
        { ...f.session.state, sessionId: 'stale-session' },
      ]) {
        assert.throws(() =>
          f.session.dispatch({ ...envelope, type, payload: type === clear ? {} : { snapshot: a } }),
        );
        assert.deepEqual(captured(f.session), before);
      }
    f.send('redo');
    assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
    const noPacket = captured(f.session);
    assert.throws(() => f.send(replace, { snapshot: a }));
    assert.deepEqual(captured(f.session), noPacket);
  });

  test(`${kind}: retained FK includes inactive IDs; explicit clear releases only metadata`, () => {
    const f = fixture(kind);
    f.send(retain, { snapshot: receipt() });
    const withPacket = f.session.snapshot();
    withPacket.colorSlots = withPacket.colorSlots.filter((r) => r.id !== 'dormant');
    assert.throws(() => validate(withPacket), /retained key-color slot reference/);
    f.send(clear);
    const released = f.session.snapshot();
    released.colorSlots = released.colorSlots.filter((r) => r.id !== 'dormant');
    assert.doesNotThrow(() => validate(released));
    assert.equal(f.session.snapshot().colorSlots.length, 3); // No new slot-delete command.
  });

  test(`${kind}: SVG replacement deep-preserves both old recipe and new root packet; export is nonmutating`, () => {
    const f = fixture(kind);
    f.send(retain, { snapshot: receipt() });
    const before = authored(f.session),
      bytes = serialize(f.session.snapshot());
    exportSVG(f.session.snapshot());
    assert.equal(serialize(f.session.snapshot()), bytes);
    f.send('import-svg', { source: svgSource() });
    const after = authored(f.session);
    for (const key of ['colorSlots', 'colorDesign', 'keyColorRecipe'])
      assert.deepEqual(after.document[key], before.document[key]);
    assert.equal(after.document.schemaVersion, 58);
    assert.notDeepEqual(after.document.layers, before.document.layers);
    reopen(f.session);
    f.send('undo');
    assert.deepEqual(authored(f.session), before);
    f.send('redo');
    assert.deepEqual(authored(f.session), after);
  });

  test(`${kind}: foreign import remaps only imported bindings and never adopts/replaces source packet`, () => {
    for (const retainReceiver of [false, true]) {
      const source = fixture('document');
      source.send(retain, { snapshot: receipt() });
      source.send('set-color-slot', { slotId: 'legacy-chromatic', value: changed });
      const sourceDoc = source.session.snapshot(),
        sourceBytes = serialize(sourceDoc);
      const f = fixture(kind);
      if (retainReceiver) f.send(retain, { snapshot: receipt() });
      const before = authored(f.session);
      if (kind === 'document')
        f.send('import-composition', {
          sourceText: sourceBytes,
          compositionId: sourceDoc.layers[0].id,
        });
      else {
        const project = migrateLegacyDocumentToCompositionProject(sourceDoc);
        f.send('import-project-compositions', {
          sourceProject: project,
          compositionIds: [project.compositions[0].id],
        });
      }
      const after = f.session.snapshot(),
        layers = f.session.projectSnapshot
          ? f.session.projectSnapshot().compositions.at(-1).layers
          : after.layers,
        importedId = layers.at(-1).nodes.find((n) => n.type === 'Color').params.value.colorSlotId;
      assert.notEqual(importedId, 'legacy-chromatic');
      assert.deepEqual(after.colorSlots.find((r) => r.id === importedId).value, changed);
      assert.equal(
        after.colorSlots.length,
        4,
        'source dormant and unused identities do not get implicitly adopted',
      );
      assert.deepEqual(after.colorDesign, before.document.colorDesign);
      assert.deepEqual(after.keyColorRecipe, before.document.keyColorRecipe);
      assert.equal(Object.hasOwn(after, 'keyColorRecipe'), retainReceiver);
      assert.equal(serialize(source.session.snapshot()), sourceBytes);
      reopen(f.session);
    }
  });

  test(`${kind}: ordinary legacy files do not opt in or change schema on load/save`, () => {
    for (const version of [1, 2]) {
      const doc = legacyDocument();
      if (version === 1) {
        doc.schemaVersion = 55;
        doc.colorDesign = {
          version: 1,
          profile: 'soft',
          colorfulness: 0.38,
          areaAware: false,
          lockedSlotIds: ['legacy-neutral'],
        };
      }
      const f = fixture(kind, doc),
        before = authored(f.session);
      reopen(f.session);
      assert.deepEqual(authored(f.session), before);
      assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
      assert.equal(f.session.snapshot().schemaVersion, version === 1 ? 55 : 57);
      const oldDesignBytes = JSON.stringify(f.session.snapshot().colorDesign);
      f.send(retain, { snapshot: receipt() });
      assert.equal(JSON.stringify(f.session.snapshot().colorDesign), oldDesignBytes);
    }
  });

  test(`${kind}: retain respects existing active-gesture barriers without committing a packet`, () => {
    const f = fixture(kind);
    const gesture = f.send('begin-gesture', {
      target: { kind: 'color-slots', slotIds: ['legacy-chromatic'] },
    });
    const before = captured(f.session);
    assert.throws(() => f.send(retain, { snapshot: receipt() }), /GESTURE_ACTIVE/);
    assert.deepEqual(captured(f.session), before);
    f.send('cancel-gesture', { gestureId: gesture.gestureId });
  });
}

test('native packet validation is schema58/canonical-v2/retained-only and project-root-only', () => {
  for (const kind of ['document', 'composition']) {
    const base = legacyDocument();
    base.schemaVersion = 58;
    base.keyColorRecipe = packet();
    const doc = kind === 'document' ? base : migrateLegacyDocumentToCompositionProject(base),
      check = kind === 'document' ? validate : validateCompositionProject,
      decode = kind === 'document' ? deserialize : deserializeCompositionProject;
    for (const change of [
      (d) => (d.schemaVersion = 57),
      (d) => (d.keyColorRecipe = null),
      (d) => (d.keyColorRecipe.version = 2),
      (d) => (d.keyColorRecipe.mode = 'active'),
      (d) => (d.keyColorRecipe.extra = true),
      (d) => (d.keyColorRecipe.committedSnapshot.version = 1),
      (d) => (d.keyColorRecipe.committedSnapshot.resolved[0].rRealized = 0),
    ]) {
      const invalid = structuredClone(doc);
      change(invalid);
      assert.throws(() => check(invalid));
      assert.throws(() => decode(JSON.stringify(invalid)));
    }
    assert.doesNotThrow(() => decode(JSON.stringify(doc)));
  }
  const project = migrateLegacyDocumentToCompositionProject(legacyDocument());
  project.compositions[0].keyColorRecipe = packet();
  assert.throws(() => validateCompositionProject(project), /root only/);
  assert.throws(() => createCompositionSessionOwner(project), /root only/);
});

test('explicit old-prototype input upgrades ratio metadata only on retain, not by legacy-file migration', () => {
  const current = receipt(),
    old = {
      ...structuredClone(current),
      version: 1,
      resolved: current.resolved.map(({ rTarget, rRealized, ...row }) => row),
    };
  const f = fixture('document');
  f.send(retain, { snapshot: freeze(old) });
  const stored = f.session.snapshot().keyColorRecipe.committedSnapshot;
  assert.equal(stored.version, 2);
  assert.equal(stored.revision, old.revision);
  assert.deepEqual(stored.intent, old.intent);
  assert.deepEqual(
    stored.resolved.map(({ rTarget, rRealized, ...row }) => row),
    old.resolved,
  );
  assert.equal(old.version, 1);
  reopen(f.session);
});

test('both SessionOwner and public projection propagate clear without resurrecting global metadata', () => {
  const f = fixture('composition');
  f.send(retain, { snapshot: receipt() });
  const first = f.session.activeCompositionId;
  f.session.createComposition({ name: 'Sibling' });
  const second = f.session.activeCompositionId,
    before = f.session.projectSnapshot();
  assert.ok(before.compositions.every((c) => !Object.hasOwn(c, 'keyColorRecipe')));
  const edited = activeCompositionDocument(before, second);
  delete edited.keyColorRecipe;
  const projected = applyActiveCompositionDocument(freeze(before), second, edited);
  assert.equal(Object.hasOwn(projected, 'keyColorRecipe'), false);
  assert.deepEqual(projected.colorDesign, before.colorDesign);
  assert.deepEqual(projected.colorSlots, before.colorSlots);
  assert.ok(before.keyColorRecipe);
  f.send(clear);
  assert.equal(Object.hasOwn(f.session.projectSnapshot(), 'keyColorRecipe'), false);
  for (const id of [first, second, first]) {
    f.session.switchComposition(id);
    assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
  }
  reopen(f.session);
});

test('SVG replacement also preserves an old recipe without any new packet or schema upgrade', () => {
  const f = fixture('document'),
    before = authored(f.session);
  f.send('import-svg', { source: svgSource() });
  assert.deepEqual(f.session.snapshot().colorDesign, before.document.colorDesign);
  assert.equal(f.session.snapshot().schemaVersion, 57);
  assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
});

test('retained packet owns detached historical bytes, not mutable command payload objects', () => {
  const f = fixture('document'),
    snapshot = receipt(),
    payload = { snapshot };
  f.send(retain, payload);
  const before = captured(f.session);
  snapshot.intent.keyColor.value.hex = '#000000';
  snapshot.resolved[0].linear[0] = 1;
  payload.snapshot = null;
  assert.deepEqual(captured(f.session), before);
  reopen(f.session);
});
