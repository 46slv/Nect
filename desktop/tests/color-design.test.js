import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  CommandSession,
  validate,
  serialize,
  deserialize,
} from '../src/core.js';
import { applyColorSlotCommand } from '../src/color-slots.js';
import {
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const original = { hex: '#A44799', alpha: 0.123456789 };
const changed = { hex: '#123456', alpha: 0 };
const designFor = (lockedSlotIds = ['slot-b'], overrides = {}) => ({
  version: 1,
  profile: 'soft',
  colorfulness: 0.38,
  areaAware: false,
  lockedSlotIds,
  ...overrides,
});
function documentFixture() {
  const doc = blankDocument();
  doc.schemaVersion = 54;
  doc.colorSlots = ['slot-a', 'slot-b'].map((id) => ({
    id,
    name: 'Duplicate',
    value: { ...original },
  }));
  return doc;
}
function fixture(kind) {
  const doc = documentFixture();
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  return {
    session,
    send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
  };
}
function stateOf(session) {
  return {
    document: serialize(session.snapshot()),
    project: session.projectSnapshot
      ? serializeCompositionProject(session.projectSnapshot())
      : null,
    state: structuredClone(session.state),
    projectRevision: session.projectRevision,
    past: structuredClone(session.historyView.past),
    future: structuredClone(session.historyView.future),
  };
}
function assertSavedDocument(session, saved) {
  // A Composition owner reconstructs the active legacy projection on history moves;
  // root key order is not authored state. Its native project bytes remain exact.
  if (session.projectSnapshot) {
    assert.deepEqual(session.snapshot(), JSON.parse(saved.document));
    assert.equal(serializeCompositionProject(session.projectSnapshot()), saved.project);
  } else assert.equal(serialize(session.snapshot()), saved.document);
}
const updates = () => [{ slotId: 'slot-a', value: { ...changed } }];
function invalidDesigns() {
  const sparse = ['slot-a'];
  sparse.length = 2;
  return [
    null,
    [],
    'soft',
    1,
    ...Object.keys(designFor()).map((key) => {
      const value = designFor();
      delete value[key];
      return value;
    }),
    designFor([], { version: 0 }),
    designFor([], { version: 2 }),
    designFor([], { version: '1' }),
    designFor([], { profile: 'unknown' }),
    designFor([], { profile: null }),
    designFor([], { colorfulness: -0.01 }),
    designFor([], { colorfulness: 1.01 }),
    designFor([], { colorfulness: NaN }),
    designFor([], { colorfulness: Infinity }),
    designFor([], { colorfulness: '0.38' }),
    designFor([], { areaAware: 0 }),
    designFor([], { areaAware: 'false' }),
    designFor([], { extra: true }),
    designFor([], { harmony: 'triad' }),
    designFor(null),
    designFor({}),
    designFor('slot-a'),
    designFor([1]),
    designFor(['slot-a', 'slot-a']),
    designFor(['Duplicate']),
    designFor(['missing-slot']),
    designFor(sparse),
    designFor(Array(257).fill('slot-a')),
  ];
}

for (const kind of ['document', 'composition']) {
  test(`${kind}: Apply authors colors and design atomically, with one Undo/Redo and native reopen`, () => {
    const f = fixture(kind);
    const before = stateOf(f.session);
    const design = designFor();
    const result = f.send('set-color-slots', { updates: updates(), design });
    const after = stateOf(f.session);
    assert.equal(result.changed, true);
    assert.equal(f.session.snapshot().schemaVersion, 55);
    assert.deepEqual(f.session.snapshot().colorDesign, design);
    assert.deepEqual(
      f.session.snapshot().colorSlots.map(({ value }) => value),
      [changed, original],
    );
    assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
    assert.equal(after.past.length, before.past.length + 1);
    f.send('undo');
    assertSavedDocument(f.session, before);
    if (before.project)
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), before.project);
    f.send('redo');
    assertSavedDocument(f.session, after);
    if (after.project) {
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), after.project);
      const reopened = createCompositionSessionOwner(deserializeCompositionProject(after.project));
      assert.equal(serializeCompositionProject(reopened.projectSnapshot()), after.project);
      assert.deepEqual(reopened.snapshot().colorDesign, design);
    } else {
      const reopened = new CommandSession(deserialize(after.document));
      assert.equal(serialize(reopened.snapshot()), after.document);
      assert.deepEqual(reopened.snapshot().colorDesign, design);
    }
  });

  test(`${kind}: unchanged colors plus newly saved settings are authored, identical repeats are no-ops`, () => {
    const f = fixture(kind);
    const before = stateOf(f.session);
    const unchanged = f.session
      .snapshot()
      .colorSlots.map(({ id, value }) => ({ slotId: id, value }));
    const design = designFor(['slot-a', 'slot-b']);
    assert.equal(f.send('set-color-slots', { updates: unchanged, design }).changed, true);
    const after = stateOf(f.session);
    assert.deepEqual(f.session.snapshot().colorSlots, JSON.parse(before.document).colorSlots);
    assert.equal(after.past.length, before.past.length + 1);
    assert.equal(
      f.send('set-color-slots', { updates: unchanged, design: structuredClone(design) }).changed,
      false,
    );
    assert.deepEqual(stateOf(f.session), after);
    assert.equal(
      f.send('set-color-slots', { updates: [], design: designFor([], { colorfulness: 0 }) })
        .changed,
      true,
    );
    assert.equal(f.session.snapshot().colorDesign.colorfulness, 0);
    f.send('undo');
    assertSavedDocument(f.session, after);
    f.send('undo');
    assertSavedDocument(f.session, before);
  });

  test(`${kind}: invalid design or later invalid color row rolls back both and preserves pending Redo`, () => {
    const f = fixture(kind);
    f.send('set-color-slots', { updates: updates(), design: designFor() });
    f.send('undo');
    const before = stateOf(f.session);
    for (const design of invalidDesigns()) {
      assert.throws(() => f.send('set-color-slots', { updates: updates(), design }));
      assert.deepEqual(stateOf(f.session), before);
    }
    for (const invalidUpdates of [
      undefined,
      null,
      {},
      [...updates(), { slotId: 'missing-slot', value: changed }],
      [...updates(), { slotId: 'slot-b', value: { hex: '#bad', alpha: 1 } }],
    ]) {
      assert.throws(() =>
        f.send('set-color-slots', { updates: invalidUpdates, design: designFor() }),
      );
      assert.deepEqual(stateOf(f.session), before);
    }
    f.send('redo');
    assert.deepEqual(f.session.snapshot().colorDesign, designFor());
    assert.deepEqual(f.session.snapshot().colorSlots[0].value, changed);
  });

  test(`${kind}: immutable caller payloads and returned snapshots cannot alter saved design or history`, () => {
    const f = fixture(kind);
    const payload = { updates: updates(), design: designFor(['slot-b', 'slot-a']) };
    const expected = structuredClone(payload);
    Object.freeze(payload.design.lockedSlotIds);
    Object.freeze(payload.design);
    Object.freeze(payload.updates[0].value);
    Object.freeze(payload.updates[0]);
    Object.freeze(payload.updates);
    Object.freeze(payload);
    f.send('set-color-slots', payload);
    assert.deepEqual(payload, expected);
    const after = stateOf(f.session);
    const snapshot = f.session.snapshot();
    snapshot.colorDesign.lockedSlotIds.pop();
    snapshot.colorDesign.profile = 'vibrant';
    assert.deepEqual(stateOf(f.session), after);
    const mutable = { updates: [], design: designFor(['slot-a'], { colorfulness: 1 }) };
    f.send('set-color-slots', mutable);
    mutable.design.lockedSlotIds.push('missing-slot');
    mutable.design.profile = 'fidelity';
    f.send('undo');
    assertSavedDocument(f.session, after);
    f.send('redo');
    assert.deepEqual(f.session.snapshot().colorDesign, designFor(['slot-a'], { colorfulness: 1 }));
  });

  test(`${kind}: old no-design batches preserve schema and no-op/Redo behavior, saved settings survive literal edits`, () => {
    const f = fixture(kind);
    f.send('set-color-slots', { updates: updates() });
    assert.equal(f.session.snapshot().schemaVersion, 54);
    assert.equal(Object.hasOwn(f.session.snapshot(), 'colorDesign'), false);
    f.send('undo');
    const before = stateOf(f.session);
    for (const rows of [[], [{ slotId: 'slot-a', value: { ...original } }]]) {
      assert.equal(f.send('set-color-slots', { updates: rows }).changed, false);
      assert.deepEqual(stateOf(f.session), before);
    }
    f.send('redo');
    f.send('set-color-slots', { updates: [], design: designFor() });
    f.send('set-color-slots', { updates: [{ slotId: 'slot-b', value: changed }] });
    f.send('set-color-slot', { slotId: 'slot-a', value: original });
    f.send('rename-color-slot', { slotId: 'slot-b', name: 'Renamed' });
    assert.deepEqual(f.session.snapshot().colorDesign, designFor());
  });
}

test('design validation preflights before mutating even the private color-slot candidate', () => {
  const doc = documentFixture();
  const before = structuredClone(doc);
  assert.throws(() =>
    applyColorSlotCommand(
      doc,
      'set-color-slots',
      {
        updates: updates(),
        design: designFor(['missing-slot']),
      },
      () => 'unused',
    ),
  );
  assert.deepEqual(doc, before);
  assert.throws(() =>
    applyColorSlotCommand(
      doc,
      'set-color-slots',
      {
        updates: [...updates(), { slotId: 'slot-b', value: null }],
        design: designFor(),
      },
      () => 'unused',
    ),
  );
  assert.deepEqual(doc, before);
});

test('native Document and Composition reject malformed designs, schema-version lies and dangling locks', () => {
  for (const kind of ['document', 'composition']) {
    const base =
      kind === 'document'
        ? documentFixture()
        : migrateLegacyDocumentToCompositionProject(documentFixture());
    const check = kind === 'document' ? validate : validateCompositionProject;
    const decode = kind === 'document' ? deserialize : deserializeCompositionProject;
    for (const design of invalidDesigns()) {
      const invalid = { ...structuredClone(base), schemaVersion: 55, colorDesign: design };
      assert.throws(() => check(invalid));
      // JSON itself cannot preserve NaN/Infinity/sparse arrays; the decoded null is invalid too.
      assert.throws(() => decode(JSON.stringify(invalid)));
    }
    for (const schemaVersion of [1, 53, 54]) {
      const invalid = { ...structuredClone(base), schemaVersion, colorDesign: designFor() };
      assert.throws(() => check(invalid));
      assert.throws(() => decode(JSON.stringify(invalid)));
    }
    const dangling = { ...structuredClone(base), schemaVersion: 55, colorDesign: designFor() };
    dangling.colorSlots = dangling.colorSlots.filter(({ id }) => id !== 'slot-b');
    assert.throws(() => check(dangling));
    assert.throws(() => decode(JSON.stringify(dangling)));
  }
});

test('all profiles, range endpoints and the exact 256-lock budget round-trip without reordering stable IDs', () => {
  const doc = documentFixture();
  doc.colorSlots = Array.from({ length: 256 }, (_, i) => ({
    id: `slot-${i}`,
    name: 'Duplicate',
    value: { ...original },
  }));
  const session = new CommandSession(doc);
  const lockedSlotIds = doc.colorSlots.map(({ id }) => id).reverse();
  for (const profile of ['balanced', 'fidelity', 'vibrant', 'soft', 'uniform']) {
    for (const colorfulness of [0, 1]) {
      const design = designFor(lockedSlotIds, { profile, colorfulness, areaAware: true });
      session.dispatch({
        ...session.state,
        type: 'set-color-slots',
        payload: { updates: [], design },
      });
      assert.deepEqual(deserialize(serialize(session.snapshot())).colorDesign, design);
    }
  }
});

test('migration and inactive Composition switching retain one project-owned saved design', () => {
  const doc = documentFixture();
  doc.schemaVersion = 55;
  doc.colorDesign = designFor();
  const originalDocument = structuredClone(doc);
  const project = migrateLegacyDocumentToCompositionProject(doc);
  assert.deepEqual(doc, originalDocument, 'migration does not consume or mutate the source');
  assert.deepEqual(project.colorDesign, designFor());
  assert.equal(Object.hasOwn(project.compositions[0], 'colorDesign'), false);
  const owner = createCompositionSessionOwner(project);
  const firstId = owner.activeCompositionId;
  owner.createComposition({ name: 'Sibling' });
  const siblingId = owner.activeCompositionId;
  const before = owner.projectSnapshot();
  const design = designFor(['slot-a'], { profile: 'vibrant', colorfulness: 1 });
  owner.dispatch({
    ...owner.state,
    type: 'set-color-slots',
    payload: { updates: updates(), design },
  });
  assert.deepEqual(owner.projectSnapshot().compositions, before.compositions);
  owner.switchComposition(firstId);
  assert.deepEqual(owner.snapshot().colorDesign, design);
  owner.switchComposition(siblingId);
  assert.deepEqual(owner.snapshot().colorDesign, design);
  const saved = serializeCompositionProject(owner.projectSnapshot());
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(saved));
  assert.deepEqual(reopened.projectSnapshot().colorDesign, design);
  assert.equal(serializeCompositionProject(reopened.projectSnapshot()), saved);
});

test('legacy and project Composition imports remap source color identities without importing its saved controls', () => {
  for (const kind of ['document', 'composition']) {
    for (const savedTarget of [false, true]) {
      const source = documentFixture();
      source.schemaVersion = 55;
      source.colorDesign = designFor(['slot-b'], { profile: 'vibrant', colorfulness: 1 });
      source.colorSlots[0].value = { ...changed };
      const color = node('Color');
      color.params.value.colorSlotId = 'slot-a';
      source.layers[0].nodes.push(color);
      const target = documentFixture();
      const targetDesign = designFor(['slot-a'], { profile: 'fidelity', colorfulness: 0 });
      if (savedTarget) {
        target.schemaVersion = 55;
        target.colorDesign = targetDesign;
      }
      const sourceBefore = serialize(source);
      const targetBefore = serialize(target);
      let result;
      if (kind === 'document') {
        const session = new CommandSession(target);
        session.dispatch({
          ...session.state,
          type: 'import-composition',
          payload: {
            sourceText: sourceBefore,
            compositionId: source.layers[0].id,
          },
        });
        result = session.snapshot();
        const imported = result.layers.at(-1).nodes.find(({ type }) => type === 'Color');
        assert.notEqual(imported.params.value.colorSlotId, 'slot-a');
        assert.deepEqual(
          result.colorSlots.find(({ id }) => id === imported.params.value.colorSlotId).value,
          changed,
        );
        const saved = serialize(result);
        assert.equal(serialize(deserialize(saved)), saved);
        session.dispatch({ ...session.state, type: 'undo' });
        assert.equal(serialize(session.snapshot()), targetBefore);
        session.dispatch({ ...session.state, type: 'redo' });
        assert.equal(serialize(session.snapshot()), saved);
      } else {
        const sourceProject = migrateLegacyDocumentToCompositionProject(source);
        const targetProject = migrateLegacyDocumentToCompositionProject(target);
        const sourceProjectBefore = serializeCompositionProject(sourceProject);
        const targetProjectBefore = serializeCompositionProject(targetProject);
        result = prepareProjectCompositionsImport(targetProject, sourceProject, [
          sourceProject.compositions[0].id,
        ]).project;
        const imported = result.compositions
          .at(-1)
          .layers[0].nodes.find(({ type }) => type === 'Color');
        assert.notEqual(imported.params.value.colorSlotId, 'slot-a');
        assert.deepEqual(
          result.colorSlots.find(({ id }) => id === imported.params.value.colorSlotId).value,
          changed,
        );
        const saved = serializeCompositionProject(result);
        assert.equal(serializeCompositionProject(deserializeCompositionProject(saved)), saved);
        assert.equal(serializeCompositionProject(sourceProject), sourceProjectBefore);
        assert.equal(serializeCompositionProject(targetProject), targetProjectBefore);
      }
      assert.equal(result.schemaVersion, 55);
      assert.equal(
        result.colorSlots.length,
        target.colorSlots.length + 1,
        'only the used source slot imports',
      );
      assert.deepEqual(result.colorSlots.slice(0, target.colorSlots.length), target.colorSlots);
      if (savedTarget) assert.deepEqual(result.colorDesign, targetDesign);
      else
        assert.equal(
          Object.hasOwn(result, 'colorDesign'),
          false,
          'source settings must not become receiving-project controls',
        );
      assert.equal(serialize(source), sourceBefore);
      assert.equal(serialize(target), targetBefore);
    }
  }
});
