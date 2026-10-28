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
import { DEFAULT_HARMONY_WHEEL } from '../src/harmony-wheel.js';
import {
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const original = { hex: '#A44799', alpha: 0.123456789 };
const changed = { hex: '#123456', alpha: 0 };
const v1 = () => ({
  version: 1,
  profile: 'soft',
  colorfulness: 0.38,
  areaAware: false,
  lockedSlotIds: ['slot-b'],
});
const designFor = (overrides = {}) => ({
  version: 2,
  profile: 'balanced',
  colorfulness: 0.72,
  areaAware: true,
  lockedSlotIds: ['slot-b'],
  wheel: { ...DEFAULT_HARMONY_WHEEL },
  palette: [
    { id: 'slot-a', kind: 'chromatic', position: 0, ring: 3, role: 'Primary' },
    { id: 'slot-b', kind: 'neutral', L: 0.5, role: 'Background' },
  ],
  ...overrides,
});
function documentFixture(count = 2) {
  const doc = blankDocument();
  doc.schemaVersion = 54;
  doc.colorSlots = Array.from({ length: count }, (_, i) => ({
    id: i < 2 ? ['slot-a', 'slot-b'][i] : `slot-${i}`,
    name: 'Existing',
    value: { ...original },
  }));
  const color = node('Color');
  color.name = 'Existing bound Color';
  color.params.value.colorSlotId = 'slot-a';
  doc.layers[0].nodes.push(color);
  return doc;
}
function fixture(kind, doc = documentFixture()) {
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
    activeCompositionId: session.activeCompositionId,
    past: structuredClone(session.historyView.past),
    future: structuredClone(session.historyView.future),
  };
}
function assertSaved(session, saved) {
  if (saved.project) {
    assert.equal(serializeCompositionProject(session.projectSnapshot()), saved.project);
    // Active Composition is view selection, not authored project history.
    if (session.activeCompositionId === saved.activeCompositionId)
      assert.deepEqual(session.snapshot(), JSON.parse(saved.document));
  } else assert.equal(serialize(session.snapshot()), saved.document);
}
function assertReopen(session) {
  const saved = stateOf(session);
  const reopened = saved.project
    ? createCompositionSessionOwner(deserializeCompositionProject(saved.project), {
        activeCompositionId: session.activeCompositionId,
      })
    : new CommandSession(deserialize(saved.document));
  assertSaved(reopened, saved);
}
function freezeTree(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freezeTree);
    Object.freeze(value);
  }
  return value;
}
function invalidDesigns() {
  const mutate = (change) => {
    const design = designFor();
    change(design);
    return design;
  };
  return [
    null,
    [],
    'wheel',
    ...Object.keys(designFor()).map((key) => mutate((d) => delete d[key])),
    ...[0, 3, '2', null].map((version) => designFor({ version })),
    designFor({ extra: true }),
    designFor({ profile: 'unknown' }),
    ...[-0.1, 1.1, NaN, Infinity, '0.72'].map((colorfulness) => designFor({ colorfulness })),
    designFor({ areaAware: 1 }),
    ...[null, {}, ['missing'], ['slot-a', 'slot-a'], Array(1)].map((lockedSlotIds) =>
      designFor({ lockedSlotIds }),
    ),
    ...[null, {}, [], Array(1), Array(257).fill(designFor().palette[0])].map((palette) =>
      designFor({ palette }),
    ),
    mutate((d) => d.palette.push({ ...d.palette[0] })),
    mutate((d) => (d.palette[0].id = 'missing')),
    mutate((d) => (d.palette[0].role = 'primary')),
    mutate((d) => (d.palette[0].kind = 'unknown')),
    mutate((d) => (d.palette[0].extra = true)),
    mutate((d) => delete d.palette[0].ring),
    mutate((d) => (d.palette[0].L = 0.5)),
    mutate((d) => (d.palette[1].ring = 0)),
    ...[-1, 3, 0.5, NaN, '1'].map((position) => mutate((d) => (d.palette[0].position = position))),
    ...[-1, 7, 0.5, NaN, '3'].map((ring) => mutate((d) => (d.palette[0].ring = ring))),
    ...[-0.1, 1.1, NaN, Infinity, '0.5'].map((L) => mutate((d) => (d.palette[1].L = L))),
    ...[null, {}, [], { ...DEFAULT_HARMONY_WHEEL, extra: true }].map((wheel) =>
      designFor({ wheel }),
    ),
    ...Object.keys(DEFAULT_HARMONY_WHEEL).map((key) => mutate((d) => delete d.wheel[key])),
    ...[
      { harmony: 'unknown' },
      { hueSteps: 11 },
      { hueSteps: 2 },
      { hueSteps: 75 },
      { hueSteps: 12.5 },
      { harmony: 'complementary', hueSteps: 9 },
      { harmony: 'split', hueSteps: 4 },
      { lightnessSteps: 0 },
      { lightnessSteps: 13 },
      { lightnessSteps: 3 }, // Existing ring 3 no longer fits.
      { hueRotation: Infinity },
      { hueRotation: 360001 },
      { lightnessRange: -0.1 },
      { lightnessRange: 1.1 },
    ].map((settings) => designFor({ wheel: { ...DEFAULT_HARMONY_WHEEL, ...settings } })),
  ];
}

for (const kind of ['document', 'composition']) {
  test(`${kind}: create harmony appends five identities and the default v2 recipe in one native Undo/Redo`, () => {
    const f = fixture(kind);
    const before = stateOf(f.session);
    const result = f.send('create-harmony-palette', freezeTree({}));
    const doc = f.session.snapshot();
    const added = doc.colorSlots.slice(2);
    assert.equal(result.changed, true);
    assert.deepEqual(
      result.slotIds,
      added.map(({ id }) => id),
    );
    assert.equal(new Set(doc.colorSlots.map(({ id }) => id)).size, 7);
    assert.deepEqual(
      added.map(({ name }) => name),
      ['C1', 'C2', 'C3', 'Background', 'Text'],
    );
    assert.deepEqual(doc.colorSlots.slice(0, 2), JSON.parse(before.document).colorSlots);
    assert.deepEqual(
      doc.layers,
      JSON.parse(before.document).layers,
      'existing bindings are untouched',
    );
    assert.equal(doc.schemaVersion, 56);
    assert.deepEqual(doc.colorDesign, {
      version: 2,
      profile: 'balanced',
      colorfulness: 0.72,
      areaAware: true,
      lockedSlotIds: [],
      wheel: {
        harmony: 'triadic',
        hueSteps: 12,
        hueRotation: 18,
        lightnessSteps: 7,
        lightnessRange: 0.54,
      },
      palette: [
        ...added.slice(0, 3).map(({ id }, position) => ({
          id,
          kind: 'chromatic',
          position,
          ring: position === 1 ? 4 : 3,
          role: ['Primary', 'Secondary', 'Accent'][position],
        })),
        { id: added[3].id, kind: 'neutral', L: 0.94, role: 'Background' },
        { id: added[4].id, kind: 'neutral', L: 0.1, role: 'Text' },
      ],
    });
    for (const { value } of added) {
      assert.match(value.hex, /^#[0-9a-f]{6}$/i);
      assert.equal(value.alpha, 1);
    }
    const after = stateOf(f.session);
    assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
    assert.equal(after.past.length, before.past.length + 1);
    assertReopen(f.session);
    f.send('undo');
    assertSaved(f.session, before);
    f.send('redo');
    assertSaved(f.session, after);
    f.send('create-harmony-palette');
    assert.deepEqual(
      f.session.snapshot().colorSlots.slice(0, 7),
      doc.colorSlots,
      'another recipe never replaces old slots',
    );
  });

  test(`${kind}: add chromatic/neutral appends one color without regenerating any old color or saved setting`, () => {
    const doc = documentFixture();
    doc.schemaVersion = 56;
    doc.colorDesign = designFor({
      profile: 'soft',
      colorfulness: 0.31,
      areaAware: false,
      wheel: { ...DEFAULT_HARMONY_WHEEL, hueRotation: 111, lightnessSteps: 10 },
    });
    const f = fixture(kind, freezeTree(doc));
    for (const [payload, expected] of [
      [{}, { kind: 'chromatic', position: 1, ring: 5, role: 'Secondary' }],
      [
        { kind: 'chromatic', name: 'Custom accent' },
        { kind: 'chromatic', position: 2, ring: 5, role: 'Secondary' },
      ],
      [
        { kind: 'neutral', name: 'New neutral' },
        { kind: 'neutral', L: 0.5, role: 'Background' },
      ],
    ]) {
      const before = stateOf(f.session);
      const old = f.session.snapshot();
      const result = f.send('add-harmony-color', freezeTree(payload));
      const next = f.session.snapshot();
      const added = next.colorSlots.at(-1);
      assert.equal(result.changed, true);
      assert.deepEqual(result.slotIds, [added.id]);
      assert.equal(result.slotId, added.id);
      assert.equal(next.colorSlots.length, old.colorSlots.length + 1);
      assert.deepEqual(next.colorSlots.slice(0, -1), old.colorSlots);
      assert.deepEqual(next.layers, old.layers);
      assert.deepEqual(next.colorDesign, {
        ...old.colorDesign,
        palette: [...old.colorDesign.palette, { id: added.id, ...expected }],
      });
      if (payload.name) assert.equal(added.name, payload.name);
      const after = stateOf(f.session);
      assert.equal(after.past.length, before.past.length + 1);
      assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
      assertReopen(f.session);
      f.send('undo');
      assertSaved(f.session, before);
      f.send('redo');
      assertSaved(f.session, after);
    }
  });

  test(`${kind}: v2 colors and settings apply atomically, freeze/copy inputs, preserve no-op Redo and reopen`, () => {
    const f = fixture(kind);
    const before = stateOf(f.session);
    const payload = freezeTree({
      updates: [{ slotId: 'slot-a', value: changed }],
      design: designFor(),
    });
    const originalPayload = structuredClone(payload);
    assert.equal(f.send('set-color-slots', payload).changed, true);
    assert.deepEqual(payload, originalPayload);
    const after = stateOf(f.session);
    assert.deepEqual(f.session.snapshot().colorSlots[0].value, changed);
    assert.deepEqual(f.session.snapshot().colorDesign, payload.design);
    assert.equal(f.session.snapshot().schemaVersion, 56);
    assert.equal(after.past.length, before.past.length + 1);
    assertReopen(f.session);
    const detached = f.session.snapshot();
    detached.colorDesign.wheel.hueSteps = 3;
    detached.colorDesign.palette[0].ring = 999;
    detached.colorDesign.lockedSlotIds.length = 0;
    assert.deepEqual(stateOf(f.session), after);
    f.send('undo');
    assertSaved(f.session, before);
    assert.equal(f.send('set-color-slots', { updates: [] }).changed, false);
    f.send('redo');
    assertSaved(f.session, after);
    const unchanged = stateOf(f.session);
    assert.equal(f.send('set-color-slots', structuredClone(payload)).changed, false);
    assert.deepEqual(stateOf(f.session), unchanged);
    const mutable = { updates: [], design: designFor({ colorfulness: 0 }) };
    f.send('set-color-slots', mutable);
    mutable.design.wheel.hueSteps = 3;
    mutable.design.palette.pop();
    f.send('undo');
    assertSaved(f.session, after);
    const pending = stateOf(f.session);
    assert.equal(f.send('set-color-slots', structuredClone(payload)).changed, false);
    assert.deepEqual(stateOf(f.session), pending);
    f.send('redo');
    assert.deepEqual(f.session.snapshot().colorDesign, designFor({ colorfulness: 0 }));
  });

  test(`${kind}: invalid v2 designs or later color updates preserve authored state, revision and pending Redo`, () => {
    const f = fixture(kind);
    f.send('set-color-slots', {
      updates: [{ slotId: 'slot-a', value: changed }],
      design: designFor(),
    });
    f.send('undo');
    const before = stateOf(f.session);
    for (const design of invalidDesigns()) {
      assert.throws(() =>
        f.send('set-color-slots', { updates: [{ slotId: 'slot-a', value: changed }], design }),
      );
      assert.deepEqual(stateOf(f.session), before);
    }
    for (const updates of [
      undefined,
      null,
      {},
      Array(1),
      [
        { slotId: 'slot-a', value: changed },
        { slotId: 'missing', value: changed },
      ],
      [
        { slotId: 'slot-a', value: changed },
        { slotId: 'slot-b', value: { hex: '#bad', alpha: 1 } },
      ],
    ]) {
      assert.throws(() => f.send('set-color-slots', { updates, design: designFor() }));
      assert.deepEqual(stateOf(f.session), before);
    }
    f.send('redo');
    assert.deepEqual(f.session.snapshot().colorDesign, designFor());
    assert.deepEqual(f.session.snapshot().colorSlots[0].value, changed);
  });

  test(`${kind}: create/add reject malformed payloads and require a v2 recipe without consuming Redo`, () => {
    const f = fixture(kind);
    f.send('create-harmony-palette');
    const future = stateOf(f.session);
    f.send('undo');
    const before = stateOf(f.session);
    for (const payload of [null, [], { wheel: DEFAULT_HARMONY_WHEEL }, { name: 'not supported' }]) {
      assert.throws(() => f.send('create-harmony-palette', payload));
      assert.deepEqual(stateOf(f.session), before);
    }
    assert.throws(() => f.send('add-harmony-color'));
    assert.deepEqual(stateOf(f.session), before);
    f.send('redo');
    assertSaved(f.session, future);
    f.send('add-harmony-color');
    f.send('undo');
    const ready = stateOf(f.session);
    for (const payload of [
      null,
      [],
      { kind: 'unknown' },
      { kind: 1 },
      { kind: null },
      { name: '' },
      { name: ' ' },
      { name: 1 },
      { name: 'n'.repeat(129) },
      { extra: true },
      { kind: 'neutral', L: 0.2 },
    ]) {
      assert.throws(() => f.send('add-harmony-color', payload));
      assert.deepEqual(stateOf(f.session), ready);
    }
    f.send('redo');
    assert.equal(f.session.snapshot().colorSlots.length, 8);
  });

  test(`${kind}: schema55 v1 remains readable and editable; add requires an explicit harmony upgrade`, () => {
    const doc = documentFixture();
    doc.schemaVersion = 55;
    doc.colorDesign = v1();
    const f = fixture(kind, doc);
    assertReopen(f.session);
    const before = stateOf(f.session);
    assert.throws(() => f.send('add-harmony-color'));
    assert.deepEqual(stateOf(f.session), before);
    f.send('set-color-slots', { updates: [{ slotId: 'slot-a', value: changed }], design: v1() });
    assert.equal(f.session.snapshot().schemaVersion, 55);
    assert.deepEqual(f.session.snapshot().colorDesign, v1());
    f.send('create-harmony-palette');
    assert.equal(f.session.snapshot().colorDesign.version, 2);
    assert.deepEqual(f.session.snapshot().colorSlots[0].value, changed);
    assertReopen(f.session);
  });

  test(`${kind}: 256-slot budget accepts exact create/add boundary and rejects overflow without consuming Redo`, () => {
    const f = fixture(kind, documentFixture(251));
    f.send('create-harmony-palette');
    assert.equal(f.session.snapshot().colorSlots.length, 256);
    assertReopen(f.session);
    const full = stateOf(f.session);
    for (const type of ['create-harmony-palette', 'add-harmony-color']) {
      assert.throws(() => f.send(type), /budget/i);
      assert.deepEqual(stateOf(f.session), full);
    }
    f.send('undo');
    const waiting = stateOf(f.session);
    assert.throws(() => f.send('add-harmony-color'));
    assert.deepEqual(stateOf(f.session), waiting);
    f.send('redo');
    assertSaved(f.session, full);
    const overflow = fixture(kind, documentFixture(252));
    const untouched = stateOf(overflow.session);
    assert.throws(() => overflow.send('create-harmony-palette'), /budget/i);
    assert.deepEqual(stateOf(overflow.session), untouched);
    const addDoc = documentFixture(255);
    addDoc.schemaVersion = 56;
    addDoc.colorDesign = designFor();
    const add = fixture(kind, addDoc);
    add.send('add-harmony-color');
    assert.equal(add.session.snapshot().colorSlots.length, 256);
    add.send('undo');
    const pending = stateOf(add.session);
    assert.throws(() => add.send('create-harmony-palette'), /budget/i);
    assert.deepEqual(stateOf(add.session), pending);
    add.send('redo');
    assertReopen(add.session);
  });
}

test('native Document and Composition reject malformed v2 recipes and schema55 v2 version lies', () => {
  for (const kind of ['document', 'composition']) {
    const base =
      kind === 'document'
        ? documentFixture()
        : migrateLegacyDocumentToCompositionProject(documentFixture());
    const check = kind === 'document' ? validate : validateCompositionProject;
    const decode = kind === 'document' ? deserialize : deserializeCompositionProject;
    for (const design of invalidDesigns()) {
      const invalid = { ...structuredClone(base), schemaVersion: 56, colorDesign: design };
      assert.throws(() => check(invalid));
      assert.throws(() => decode(JSON.stringify(invalid)));
    }
    for (const schemaVersion of [54, 55]) {
      const invalid = { ...structuredClone(base), schemaVersion, colorDesign: designFor() };
      assert.throws(() => check(invalid));
      assert.throws(() => decode(JSON.stringify(invalid)));
    }
  }
});

test('v2 exact 256-entry/lock budget, role vocabulary, wheel families and endpoint neutrals round-trip', () => {
  const doc = documentFixture(256);
  doc.schemaVersion = 56;
  doc.colorDesign = designFor({
    lockedSlotIds: doc.colorSlots.map(({ id }) => id).reverse(),
    palette: doc.colorSlots.map(({ id }, index) => ({
      id,
      kind: 'neutral',
      L: index % 2,
      role: ['Primary', 'Secondary', 'Accent', 'Background', 'Text'][index % 5],
    })),
  });
  for (const harmony of ['triadic', 'complementary', 'analogous', 'split']) {
    doc.colorDesign.wheel.harmony = harmony;
    const f = fixture('document', freezeTree(structuredClone(doc)));
    assert.deepEqual(deserialize(serialize(f.session.snapshot())).colorDesign, doc.colorDesign);
    assertReopen(f.session);
  }
});

test('migration and inactive Composition commands maintain exactly one global v2 recipe', () => {
  const doc = documentFixture();
  doc.schemaVersion = 56;
  doc.colorDesign = designFor();
  const before = serialize(doc);
  const project = migrateLegacyDocumentToCompositionProject(freezeTree(doc));
  assert.equal(serialize(doc), before);
  assert.deepEqual(project.colorDesign, designFor());
  const owner = createCompositionSessionOwner(project);
  const first = owner.activeCompositionId;
  owner.createComposition({ name: 'Inactive sibling' });
  const sibling = owner.activeCompositionId;
  const compositions = owner.projectSnapshot().compositions;
  owner.dispatch({ ...owner.state, type: 'add-harmony-color', payload: { kind: 'neutral' } });
  const expected = owner.snapshot().colorDesign;
  assert.deepEqual(owner.projectSnapshot().compositions, compositions);
  for (const id of [first, sibling, first]) {
    owner.switchComposition(id);
    assert.deepEqual(owner.snapshot().colorDesign, expected);
    assert.deepEqual(owner.projectSnapshot().colorDesign, expected);
    assert.ok(
      owner.projectSnapshot().compositions.every((item) => !Object.hasOwn(item, 'colorDesign')),
    );
  }
  assertReopen(owner);
});

for (const kind of ['document', 'composition']) {
  test(`${kind}: selected Composition import retains receiving v2 controls, remaps collision IDs and omits unused recipe slots`, () => {
    const source = documentFixture();
    source.schemaVersion = 56;
    source.colorDesign = designFor({
      profile: 'vibrant',
      colorfulness: 1,
      lockedSlotIds: ['slot-a'],
    });
    source.colorSlots[0].value = { ...changed };
    const sourceBefore = serialize(source);
    for (const receivingVersion of [0, 1, 2]) {
      const target = documentFixture();
      if (receivingVersion) {
        target.schemaVersion = receivingVersion === 1 ? 55 : 56;
        target.colorDesign =
          receivingVersion === 1 ? v1() : designFor({ profile: 'fidelity', colorfulness: 0 });
      }
      const expected = structuredClone(target.colorDesign);
      const f = fixture(kind, freezeTree(target));
      const before = stateOf(f.session);
      const sourceProject = migrateLegacyDocumentToCompositionProject(source);
      const sourceProjectBefore = serializeCompositionProject(sourceProject);
      if (kind === 'document')
        f.send('import-composition', {
          sourceText: sourceBefore,
          compositionId: source.layers[0].id,
        });
      else
        f.send('import-project-compositions', {
          sourceProject: freezeTree(sourceProject),
          compositionIds: [sourceProject.compositions[0].id],
        });
      const result = kind === 'document' ? f.session.snapshot() : f.session.projectSnapshot();
      const layers = kind === 'document' ? result.layers : result.compositions.at(-1).layers;
      const imported = layers.at(-1).nodes.find(({ type }) => type === 'Color');
      const importedId = imported.params.value.colorSlotId;
      assert.notEqual(importedId, 'slot-a');
      assert.notEqual(importedId, 'slot-b');
      assert.deepEqual(result.colorSlots.find(({ id }) => id === importedId).value, changed);
      assert.deepEqual(result.colorSlots.slice(0, 2), target.colorSlots);
      assert.equal(result.colorSlots.length, 3, 'unused source recipe entry is not imported');
      assert.deepEqual(
        result.colorDesign,
        expected,
        'source controls never replace receiving controls',
      );
      assert.equal(serializeCompositionProject(sourceProject), sourceProjectBefore);
      assert.equal(serialize(source), sourceBefore);
      assert.equal(result.schemaVersion, 56);
      const after = stateOf(f.session);
      assert.equal(after.past.length, before.past.length + 1);
      assertReopen(f.session);
      f.send('undo');
      assertSaved(f.session, before);
      f.send('redo');
      assertSaved(f.session, after);
    }
  });

  test(`${kind}: selected bound-node copy/paste preserves one recipe and shared identity`, () => {
    const doc = documentFixture();
    doc.schemaVersion = 56;
    doc.colorDesign = designFor();
    const f = fixture(kind, doc);
    const layer = doc.layers[0];
    const source = layer.nodes.find(({ type }) => type === 'Color');
    f.send('copy-nodes', { layerId: layer.id, nodeIds: [source.id] });
    const before = stateOf(f.session);
    const result = f.send('paste-nodes', { layerId: layer.id });
    assert.equal(result.nodeIds.length, 1);
    const pasted = f.session.snapshot().layers[0].nodes.find(({ id }) => id === result.nodeIds[0]);
    assert.notEqual(pasted.id, source.id);
    assert.equal(pasted.params.value.colorSlotId, 'slot-a');
    assert.deepEqual(f.session.snapshot().colorDesign, designFor());
    assert.deepEqual(f.session.snapshot().colorSlots, doc.colorSlots);
    const after = stateOf(f.session);
    assertReopen(f.session);
    f.send('undo');
    assertSaved(f.session, before);
    f.send('redo');
    assertSaved(f.session, after);
  });
}

test('v2 migration reserves legacy Composition slot IDs without retargeting recipe references', () => {
  const doc = documentFixture();
  doc.schemaVersion = 56;
  doc.colorSlots[0].id = 'legacy-composition-1';
  doc.layers[0].nodes.find(({ type }) => type === 'Color').params.value.colorSlotId =
    'legacy-composition-1';
  doc.colorDesign = designFor();
  doc.colorDesign.palette[0].id = 'legacy-composition-1';
  doc.colorDesign.lockedSlotIds = ['legacy-composition-1'];
  const project = migrateLegacyDocumentToCompositionProject(freezeTree(doc));
  assert.notEqual(project.compositions[0].id, 'legacy-composition-1');
  assert.deepEqual(project.colorDesign, doc.colorDesign);
  assert.equal(
    project.compositions[0].layers[0].nodes.find(({ type }) => type === 'Color').params.value
      .colorSlotId,
    'legacy-composition-1',
  );
  assertReopen(createCompositionSessionOwner(project));
});

for (const kind of ['document', 'composition']) {
  test(`${kind}: harmony commands reject stale envelopes without changing state or history`, () => {
    const f = fixture(kind);
    const stale = structuredClone(f.session.state);
    f.send('create-harmony-palette');
    const before = stateOf(f.session);
    for (const type of ['create-harmony-palette', 'add-harmony-color']) {
      assert.throws(() => f.session.dispatch({ ...stale, type, payload: {} }), /STALE_REVISION/);
      assert.deepEqual(stateOf(f.session), before);
      assert.throws(
        () =>
          f.session.dispatch({ ...f.session.state, sessionId: 'wrong-session', type, payload: {} }),
        /STALE_SESSION/,
      );
      assert.deepEqual(stateOf(f.session), before);
    }
  });

  test(`${kind}: generated ID collisions fail atomically rather than retarget existing slot bindings or consume Redo`, (t) => {
    const doc = documentFixture();
    doc.colorSlots[0].id = 'color-collision';
    doc.layers[0].nodes.find(({ type }) => type === 'Color').params.value.colorSlotId =
      'color-collision';
    doc.schemaVersion = 56;
    doc.colorDesign = designFor();
    doc.colorDesign.palette[0].id = 'color-collision';
    const f = fixture(kind, doc);
    f.send('add-harmony-color');
    f.send('undo');
    const before = stateOf(f.session);
    t.mock.method(globalThis.crypto, 'randomUUID', () => 'collision');
    for (const type of ['create-harmony-palette', 'add-harmony-color']) {
      assert.throws(() => f.send(type), /duplicate|ID/i);
      assert.deepEqual(stateOf(f.session), before);
    }
    f.send('redo');
    assert.equal(f.session.snapshot().colorSlots.length, 3);
    assert.equal(
      f.session.snapshot().layers[0].nodes.find(({ type }) => type === 'Color').params.value
        .colorSlotId,
      'color-collision',
    );
  });

  test(`${kind}: foreign slot/entity identity collision remaps imported binding without retargeting receiving v2 recipe`, () => {
    const source = documentFixture();
    source.schemaVersion = 56;
    source.colorDesign = designFor();
    const target = documentFixture();
    target.schemaVersion = 56;
    target.colorSlots[0].id = 'target-a';
    const existingColor = target.layers[0].nodes.find(({ type }) => type === 'Color');
    existingColor.id = 'slot-a';
    existingColor.params.value.colorSlotId = 'target-a';
    target.colorDesign = designFor();
    target.colorDesign.palette[0].id = 'target-a';
    const f = fixture(kind, target);
    if (kind === 'document')
      f.send('import-composition', {
        sourceText: serialize(source),
        compositionId: source.layers[0].id,
      });
    else {
      const sourceProject = migrateLegacyDocumentToCompositionProject(source);
      f.send('import-project-compositions', {
        sourceProject,
        compositionIds: [sourceProject.compositions[0].id],
      });
    }
    const result = kind === 'document' ? f.session.snapshot() : f.session.projectSnapshot();
    const importedLayer =
      kind === 'document' ? result.layers.at(-1) : result.compositions.at(-1).layers[0];
    const importedColor = importedLayer.nodes.find(({ type }) => type === 'Color');
    assert.notEqual(importedColor.params.value.colorSlotId, 'slot-a');
    assert.notEqual(
      importedColor.params.value.colorSlotId,
      'target-a',
      'equal values never imply shared identity',
    );
    assert.deepEqual(result.colorDesign, target.colorDesign);
    const existingLayer = kind === 'document' ? result.layers[0] : result.compositions[0].layers[0];
    assert.deepEqual(existingLayer, target.layers[0]);
    assert.deepEqual(
      result.colorSlots.find(({ id }) => id === importedColor.params.value.colorSlotId).value,
      original,
    );
    assertReopen(f.session);
  });
}
