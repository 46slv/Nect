import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  evaluate,
  validate,
  CommandSession,
  serialize,
  deserialize,
} from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { prepareKeyColorRecipeInitialization } from '../src/key-color-recipe-document.js';
import { DEFAULT_HARMONY_WHEEL } from '../src/harmony-wheel.js';
import { editKeyColorRecipe } from '../src/key-color-recipe.js';

const initialize = 'initialize-key-color-recipe';
const keyValue = { hex: '#aBcD09', alpha: 0 };
// Deliberately explicit fixture coefficients, not an approved product preset.
function request() {
  return {
    version: 1,
    modelVersion: 2,
    solverVersion: 'experimental-affine-v1',
    colorSemanticsVersion: 'linear-srgb-straight-v1',
    paletteId: 'fixture-only-palette',
    intent: {
      keyColor: { id: 'fresh-key', role: 'Identity', value: { ...keyValue }, hueHint: 17 },
      preset: {
        id: 'explicit-fixture-only',
        version: 1,
        harmony: 'triadic',
        hueSteps: 12,
        slots: [
          {
            id: 'fresh-derived',
            role: 'Fixture background',
            position: 1,
            lightnessScale: 0,
            lightnessOffset: 0.83,
            chromaScale: 0,
            chromaOffset: 0,
            alpha: 1,
          },
        ],
      },
      slots: [{ id: 'fresh-derived', role: 'Fixture background', deltaL: 0.03, deltaRatioCmax: 0 }],
      settings: { areaAware: false },
    },
    // Allocation order is explicitly independent of intent/resolved row order.
    allocations: [
      { slotId: 'fresh-derived', name: 'Exact background name' },
      { slotId: 'fresh-key', name: 'Exact C1 name' },
    ],
  };
}
function legacyDocument() {
  const doc = blankDocument();
  doc.schemaVersion = 57;
  doc.colorSlots = [
    { id: 'legacy', name: 'Existing name', value: { hex: '#123456', alpha: 0.3 } },
    { id: 'neutral', name: 'Existing neutral', value: { hex: '#aAbBcC', alpha: 0 } },
  ];
  doc.colorDesign = {
    version: 2,
    profile: 'soft',
    colorfulness: 0.38,
    areaAware: false,
    lockedSlotIds: ['legacy'],
    wheel: { ...DEFAULT_HARMONY_WHEEL },
    palette: [
      { id: 'legacy', kind: 'chromatic', position: 0, ring: 3, role: 'Primary', overrideC: 0.12 },
      { id: 'neutral', kind: 'neutral', L: 0.91, role: 'Background' },
    ],
  };
  const color = node('Color');
  color.params.value.colorSlotId = 'legacy';
  doc.layers[0].nodes.push(color);
  return doc;
}
function fixture(kind, doc = blankDocument()) {
  const session =
    kind === 'document'
      ? new CommandSession(doc)
      : createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  return {
    session,
    send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
  };
}
function captured(s) {
  return {
    document: serialize(s.snapshot()),
    project: s.projectSnapshot ? serializeCompositionProject(s.projectSnapshot()) : null,
    state: { ...s.state },
    projectRevision: s.projectRevision,
    past: structuredClone(s.historyView.past),
    future: structuredClone(s.historyView.future),
  };
}
function withRedo(f) {
  f.send('set-document-settings', { key: 'width', value: 1001 });
  f.send('undo');
  assert.equal(f.session.historyView.future.length, 1);
  return captured(f.session);
}
function rejected(f, payload, pattern) {
  const before = captured(f.session);
  assert.throws(() => f.send(initialize, payload), pattern);
  assert.deepEqual(captured(f.session), before);
}
function retarget(payload, oldId, newId) {
  if (payload.intent.keyColor.id === oldId) payload.intent.keyColor.id = newId;
  for (const slot of [...payload.intent.slots, ...payload.intent.preset.slots])
    if (slot.id === oldId) slot.id = newId;
  for (const allocation of payload.allocations)
    if (allocation.slotId === oldId) allocation.slotId = newId;
}
function reopen(s) {
  if (s.projectSnapshot) {
    const text = serializeCompositionProject(s.projectSnapshot()),
      reopened = createCompositionSessionOwner(deserializeCompositionProject(text));
    assert.equal(serializeCompositionProject(reopened.projectSnapshot()), text);
  } else {
    const text = serialize(s.snapshot());
    assert.equal(serialize(new CommandSession(deserialize(text)).snapshot()), text);
  }
}

for (const kind of ['document', 'composition']) {
  test(`${kind}: explicit initialization appends exact IDs/names/alpha0 and receipt in one Undo`, () => {
    const f = fixture(kind),
      before = captured(f.session),
      payload = request(),
      result = f.send(initialize, payload),
      after = captured(f.session),
      doc = f.session.snapshot(),
      snapshot = doc.keyColorRecipe.committedSnapshot;
    assert.equal(result.changed, true);
    assert.deepEqual(result.slotIds, ['fresh-derived', 'fresh-key']);
    assert.equal(result.paletteRevision, 0);
    assert.equal(snapshot.revision, 0);
    assert.equal(doc.schemaVersion, 58);
    assert.equal(after.state.sessionId, before.state.sessionId);
    assert.equal(after.state.expectedRevision, before.state.expectedRevision + 1);
    if (kind === 'composition') assert.equal(after.projectRevision, before.projectRevision + 1);
    assert.equal(after.past.length, before.past.length + 1);
    assert.deepEqual(after.future, []);
    assert.deepEqual(doc.keyColorRecipe, {
      version: 1,
      mode: 'retained-only',
      committedSnapshot: snapshot,
    });
    assert.deepEqual(
      doc.colorSlots,
      payload.allocations.map(({ slotId, name }) => ({
        id: slotId,
        name,
        value: snapshot.resolved.find((row) => row.id === slotId).value,
      })),
    );
    assert.deepEqual(doc.colorSlots.find((slot) => slot.id === 'fresh-key').value, keyValue);
    assert.equal(doc.colorSlots.find((slot) => slot.id === 'fresh-derived').value.alpha, 1);
    assert.deepEqual(doc.layers, JSON.parse(before.document).layers);
    assert.equal(Object.hasOwn(doc, 'colorDesign'), false);
    reopen(f.session);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), JSON.parse(before.document));
    if (before.project)
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), before.project);
    f.send('redo');
    assert.deepEqual(f.session.snapshot(), JSON.parse(after.document));
    if (after.project)
      assert.equal(serializeCompositionProject(f.session.projectSnapshot()), after.project);
    payload.intent.keyColor.value.hex = '#000000';
    payload.allocations[0].name = 'Caller mutation';
    assert.deepEqual(f.session.snapshot(), JSON.parse(after.document));
  });

  test(`${kind}: legacy registry/order/locks/recipe/bindings survive and repeated initialization refuses`, () => {
    const f = fixture(kind, legacyDocument()),
      old = f.session.snapshot();
    f.send(initialize, request());
    const doc = f.session.snapshot();
    assert.equal(JSON.stringify(doc.colorSlots.slice(0, 2)), JSON.stringify(old.colorSlots));
    assert.equal(JSON.stringify(doc.colorDesign), JSON.stringify(old.colorDesign));
    assert.deepEqual(doc.layers, old.layers);
    f.send('set-color-slot', { slotId: 'neutral', value: { hex: '#334455', alpha: 0.7 } });
    f.send('undo');
    rejected(f, request(), /recipe exists/);
    // Historical receipt does not take ownership of manually edited values.
    const receipt = structuredClone(doc.keyColorRecipe);
    f.send('set-color-slot', { slotId: 'fresh-key', value: { hex: '#112233', alpha: 0.4 } });
    assert.deepEqual(f.session.snapshot().keyColorRecipe, receipt);
    reopen(f.session);
  });

  test(`${kind}: fresh-only slot and active entity collisions preserve exact envelope and pending Redo`, () => {
    const f = fixture(kind, legacyDocument());
    withRedo(f);
    for (const collision of [
      'legacy',
      f.session.snapshot().layers[0].id,
      f.session.snapshot().layers[0].nodes[0].id,
    ]) {
      const payload = request();
      retarget(payload, 'fresh-key', collision);
      rejected(f, payload, /fresh|Duplicate|duplicate/);
    }
    f.send('redo');
    assert.equal(f.session.snapshot().width, 1001);
  });

  test(`${kind}: bad allocations, unsupported versions, missing active maps and alpha reject atomically`, () => {
    const f = fixture(kind);
    withRedo(f);
    for (const change of [
      (p) => p.allocations.pop(),
      (p) => p.allocations.push({ slotId: 'extra', name: 'Not intended' }),
      (p) => p.allocations.push({ ...p.allocations[0] }),
      (p) => (p.allocations[0].name = ''),
      (p) => (p.allocations[0].name = ' '.repeat(3)),
      (p) => (p.allocations[0].name = 'a'.repeat(129)),
      (p) => (p.allocations[0].slotId = 'bad id'),
      (p) => (p.allocations[0].unexpected = true),
      (p) => (p.version = 2),
      (p) => (p.modelVersion = 1),
      (p) => (p.solverVersion = 'future-solver'),
      (p) => (p.colorSemanticsVersion = 'future-color'),
      (p) => (p.intent.preset.version = 2),
      (p) => delete p.intent.keyColor.hueHint,
      (p) => delete p.intent.settings,
      (p) => delete p.intent.preset.slots[0].lightnessScale,
      (p) => delete p.intent.preset.slots[0].alpha,
      (p) => (p.intent.preset.slots = []),
      (p) => (p.intent.preset.slots[0].role = 'Inferred new role'),
      (p) => (p.intent.preset.slots[0].alpha = 0),
      (p) => (p.intent.preset.slots[0].alpha = 0.5),
      (p) => (p.intent.keyColor.value.extra = true),
      (p) => (p.intent.slots[0].unexpected = true),
      (p) => (p.intent.settings.unknown = true),
      (p) => (p.unknown = true),
    ]) {
      const payload = request();
      change(payload);
      rejected(f, payload);
    }
  });

  test(`${kind}: raw lossless JSON guard runs before cloning or accessors`, () => {
    const f = fixture(kind);
    withRedo(f);
    for (const value of [
      undefined,
      NaN,
      Infinity,
      -Infinity,
      1n,
      Symbol('bad'),
      () => {},
      new Date(),
    ]) {
      const payload = request();
      payload.intent.slots[0].deltaL = value;
      rejected(f, payload, /JSON/);
    }
    for (const change of [
      (p) => (p.intent.settings.cycle = p),
      (p) => delete p.intent.slots[0],
      (p) => (p.allocations.extra = 'JSON would drop this'),
      (p) => (p.intent.settings[Symbol('key')] = true),
      (p) => Object.defineProperty(p.intent.settings, 'hidden', { value: true }),
      (p) =>
        Object.defineProperty(p.intent.keyColor.value, 'hex', {
          enumerable: true,
          get() {
            throw Error('ACCESSOR RAN');
          },
        }),
      (p) =>
        Object.defineProperty(p, 'intent', {
          enumerable: true,
          get() {
            throw Error('ACCESSOR RAN');
          },
        }),
    ]) {
      const payload = request();
      change(payload);
      rejected(f, payload, /JSON/);
    }
  });

  test(`${kind}: combined registry budget is checked before any authored mutation`, () => {
    const doc = blankDocument();
    doc.schemaVersion = 54;
    doc.colorSlots = Array.from({ length: 255 }, (_, i) => ({
      id: `old-${i}`,
      name: `Old ${i}`,
      value: { hex: '#123456', alpha: 1 },
    }));
    const f = fixture(kind, doc);
    withRedo(f);
    rejected(f, request(), /budget/);
    const payload = request();
    payload.intent.slots = [];
    payload.intent.preset.slots = [];
    payload.allocations = payload.allocations.slice(1);
    f.send(initialize, payload);
    assert.equal(f.session.snapshot().colorSlots.length, 256);
  });

  test(`${kind}: nonordinary arrays and proxy allocations reject before caller methods or commit`, () => {
    const f = fixture(kind);
    withRedo(f);
    let mapCalls = 0;
    class CallerArray extends Array {
      map() {
        mapCalls++;
        throw Error('CALLER MAP RAN');
      }
    }
    for (const field of ['allocations', 'slots', 'plans']) {
      const payload = request();
      if (field === 'allocations') payload.allocations = CallerArray.from(payload.allocations);
      else if (field === 'slots') payload.intent.slots = CallerArray.from(payload.intent.slots);
      else payload.intent.preset.slots = CallerArray.from(payload.intent.preset.slots);
      rejected(f, payload, /JSON array/);
    }
    const payload = request();
    payload.allocations = new Proxy(payload.allocations, {
      get(target, key, receiver) {
        if (key === 'map') {
          mapCalls++;
          throw Error('CALLER MAP RAN');
        }
        return Reflect.get(target, key, receiver);
      },
    });
    rejected(f, payload, /clone/i);
    rejected(f, new Proxy(request(), {}), /clone/i);
    assert.equal(mapCalls, 0);
  });

  test(`${kind}: detached preparation has no authored effect; captured envelope never rebases`, () => {
    const f = fixture(kind),
      owner = f.session,
      envelope = { ...owner.state },
      before = captured(owner),
      payload = request(),
      preview = prepareKeyColorRecipeInitialization(owner.snapshot(), payload, new Map());
    assert.deepEqual(captured(owner), before);
    assert.equal(preview.snapshot.revision, 0);
    assert.deepEqual(preview.slots.find((row) => row.id === 'fresh-key').value, keyValue);
    preview.snapshot.intent.keyColor.value.hex = '#000000';
    preview.slots[0].name = 'Detached preview mutation';
    assert.deepEqual(payload, request());
    assert.deepEqual(captured(owner), before);
    f.send('set-document-settings', { key: 'height', value: 1002 });
    const edited = captured(owner);
    assert.equal(owner, f.session);
    assert.throws(
      () => owner.dispatch({ ...envelope, type: initialize, payload }),
      /STALE_REVISION/,
    );
    assert.deepEqual(captured(owner), edited);
    f.send('undo');
    assert.throws(
      () => owner.dispatch({ ...envelope, type: initialize, payload }),
      /STALE_SESSION|STALE_REVISION/,
    );
    assert.equal(Object.hasOwn(owner.snapshot(), 'keyColorRecipe'), false);
  });

  test(`${kind}: ordinary New and native load keep the absence of recipe/defaults`, () => {
    const f = fixture(kind),
      before = captured(f.session);
    assert.equal(Object.hasOwn(f.session.snapshot(), 'keyColorRecipe'), false);
    assert.equal(Object.hasOwn(f.session.snapshot(), 'colorSlots'), false);
    reopen(f.session);
    assert.deepEqual(captured(f.session), before);
  });
}

test('composition: inactive Composition entity collisions reject before inner commit and preserve Session/Redo', () => {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument()),
    inactive = blankDocument(),
    curve = node('Curve');
  project.schemaVersion = 44;
  inactive.layers[0].nodes.push(curve);
  inactive.layers[0].nodeGroups = [{ id: 'inactive-group', name: 'Group', nodeIds: [curve.id] }];
  inactive.editor.guides.push({ id: 'inactive-guide', axis: 'x', position: 10, locked: false });
  project.compositions.push({
    id: 'inactive-composition',
    name: 'Inactive',
    width: inactive.width,
    height: inactive.height,
    layers: inactive.layers,
    editor: inactive.editor,
  });
  const session = createCompositionSessionOwner(project),
    f = {
      session,
      send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    };
  withRedo(f);
  for (const collision of [
    project.compositions[0].id,
    'inactive-composition',
    inactive.layers[0].id,
    inactive.layers[0].nodes[0].id,
    curve.points[0].id,
    'inactive-guide',
    'inactive-group',
  ]) {
    const payload = request();
    retarget(payload, 'fresh-derived', collision);
    rejected(f, payload, /Duplicate ID/);
  }
  f.send('redo');
  assert.equal(session.snapshot().width, 1001);
});

test('composition: captured preparation cannot commit after a Composition switch', () => {
  const f = fixture('composition'),
    owner = f.session,
    payload = request(),
    envelope = { ...owner.state },
    firstId = owner.activeCompositionId;
  prepareKeyColorRecipeInitialization(owner.snapshot(), payload, new Map());
  owner.createComposition({ name: 'Other' });
  owner.switchComposition(firstId);
  const before = captured(owner);
  assert.throws(() => owner.dispatch({ ...envelope, type: initialize, payload }), /STALE_SESSION/);
  assert.deepEqual(captured(owner), before);
});

test('composition: caller allocation methods cannot inject an inactive entity after ID preflight', () => {
  const project = migrateLegacyDocumentToCompositionProject(blankDocument()),
    inactive = blankDocument();
  project.compositions.push({
    id: 'inactive-composition',
    name: 'Inactive',
    width: inactive.width,
    height: inactive.height,
    layers: inactive.layers,
  });
  const session = createCompositionSessionOwner(project),
    f = {
      session,
      send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    };
  withRedo(f);
  let mapCalls = 0;
  const inject = (target, callback) => {
    mapCalls++;
    return [
      ...Array.prototype.map.call(target, callback),
      { id: 'inactive-composition', name: 'Injected', value: { hex: '#000000', alpha: 1 } },
    ];
  };
  class InjectedAllocations extends Array {
    map(callback) {
      return inject(this, callback);
    }
  }
  const subclass = request();
  subclass.allocations = InjectedAllocations.from(subclass.allocations);
  rejected(f, subclass, /JSON array/);
  const proxy = request();
  proxy.allocations = new Proxy(proxy.allocations, {
    get(target, key, receiver) {
      return key === 'map'
        ? (callback) => inject(target, callback)
        : Reflect.get(target, key, receiver);
    },
  });
  rejected(f, proxy, /clone/i);
  assert.equal(mapCalls, 0);
  f.send('redo');
  assert.equal(session.snapshot().width, 1001);
});

for (const kind of ['document', 'composition']) {
  test(`${kind}: live trial preserves stable bindings, atomic history, reopen and divergent revision caches`, () => {
    const doc = legacyDocument(),
      layer = doc.layers[0],
      shape = node('Rectangle'),
      fill = node('Fill');
    const output = layer.nodes[0];
    layer.nodes.push(shape, fill);
    connect(layer, shape.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');
    const f = fixture(kind, doc),
      payload = request();
    Object.assign(payload.intent.preset.slots[0], {
      lightnessScale: 0.6,
      lightnessOffset: 0.1,
      chromaScale: 0.8,
    });
    const before = f.session.snapshot();
    f.send('initialize-live-key-color-recipe', payload);
    const created = f.session.snapshot();
    assert.equal(created.schemaVersion, 59);
    assert.equal(created.keyColorRecipe.mode, 'live');
    assert.deepEqual(created.colorSlots.slice(0, 2), before.colorSlots);
    assert.deepEqual(created.colorSlots.find((s) => s.id === 'fresh-key').value, keyValue);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), before);
    f.send('redo');
    assert.deepEqual(f.session.snapshot(), created);
    f.send('bind-color-slot', {
      layerId: layer.id,
      nodeId: fill.id,
      key: 'color',
      slotId: 'fresh-derived',
    });
    const bound = f.session.snapshot();
    const graph = bound.layers[0];
    const render = (d) => evaluate(graph, {}, d.imageAssets, d).run(fill.id).fill;
    const first = render(bound);
    const previous = bound.keyColorRecipe.committedSnapshot;
    const next = editKeyColorRecipe(previous, {
      type: 'key-color',
      expectedRevision: previous.revision,
      value: { hex: '#d03270', alpha: 0.25 },
    });
    f.send('apply-live-key-color-recipe-snapshot', {
      snapshot: next,
      expectedPaletteRevision: previous.revision,
    });
    const changed = f.session.snapshot(),
      changedValue = changed.colorSlots.find((s) => s.id === 'fresh-derived').value;
    assert.notDeepEqual(changedValue, first);
    assert.deepEqual(render(changed), changedValue);
    assert.equal(
      changed.layers[0].nodes.find((n) => n.id === fill.id).params.color.colorSlotId,
      'fresh-derived',
    );
    assert.deepEqual(changed.colorSlots.find((s) => s.id === 'fresh-key').value, {
      hex: '#d03270',
      alpha: 0.25,
    });
    reopen(f.session);
    f.send('undo');
    assert.deepEqual(render(f.session.snapshot()), first);
    f.send('redo');
    assert.deepEqual(render(f.session.snapshot()), changedValue);
    f.send('undo');
    const alternative = editKeyColorRecipe(previous, {
      type: 'key-color',
      expectedRevision: previous.revision,
      value: { hex: '#2478ee', alpha: 1 },
    });
    f.send('apply-live-key-color-recipe-snapshot', {
      snapshot: alternative,
      expectedPaletteRevision: previous.revision,
    });
    const branched = f.session.snapshot();
    assert.equal(
      branched.keyColorRecipe.committedSnapshot.revision,
      changed.keyColorRecipe.committedSnapshot.revision,
    );
    assert.notDeepEqual(render(branched), changedValue);
    assert.deepEqual(
      render(branched),
      branched.colorSlots.find((s) => s.id === 'fresh-derived').value,
    );
    f.send('unbind-color-slot', { layerId: layer.id, nodeId: fill.id, key: 'color' });
    const frozen = f.session.snapshot().layers[0].nodes.find((n) => n.id === fill.id).params.color;
    assert.equal(frozen.colorSlotId, undefined);
    assert.deepEqual(frozen.value, branched.colorSlots.find((s) => s.id === 'fresh-derived').value);
  });

  test(`${kind}: live ownership rejects stale, forged and conflicting edits without touching history or redo`, () => {
    const f = fixture(kind, legacyDocument());
    f.send('initialize-live-key-color-recipe', request());
    f.send('set-document-settings', { key: 'width', value: 1001 });
    f.send('undo');
    const original = f.session.snapshot().keyColorRecipe.committedSnapshot;
    const forged = structuredClone(original);
    forged.intent.preset.slots[0].lightnessOffset = 0.2;
    const competingDesign = structuredClone(f.session.snapshot().colorDesign);
    competingDesign.lockedSlotIds.push('fresh-key');
    const operations = [
      ['apply-live-key-color-recipe-snapshot', { snapshot: original, expectedPaletteRevision: 1 }],
      ['apply-live-key-color-recipe-snapshot', { snapshot: forged, expectedPaletteRevision: 0 }],
      ['set-color-slot', { slotId: 'fresh-key', value: { hex: '#112233', alpha: 1 } }],
      [
        'set-color-slots',
        {
          updates: [
            { slotId: 'legacy', value: { hex: '#112233', alpha: 1 } },
            { slotId: 'fresh-derived', value: { hex: '#334455', alpha: 1 } },
          ],
        },
      ],
      ['begin-gesture', { target: { kind: 'color-slots', slotIds: ['legacy', 'fresh-derived'] } }],
      ['set-color-slots', { updates: [], design: competingDesign }],
      ['clear-key-color-recipe-snapshot', {}],
      ['replace-key-color-recipe-snapshot', { snapshot: original }],
      ['apply-key-color-recipe-snapshot', { snapshot: original }],
    ];
    for (const [type, payload] of operations) {
      const before = captured(f.session);
      assert.throws(() => f.send(type, payload));
      assert.deepEqual(captured(f.session), before, type);
    }
    const beforeNoop = captured(f.session);
    f.send('apply-live-key-color-recipe-snapshot', {
      snapshot: original,
      expectedPaletteRevision: 0,
    });
    assert.deepEqual(captured(f.session), beforeNoop);
    f.send('redo');
    assert.equal(f.session.snapshot().width, 1001);
    f.send('rename-color-slot', { slotId: 'fresh-derived', name: 'Still the same ID' });
    f.send('set-color-slot', { slotId: 'legacy', value: { hex: '#112233', alpha: 1 } });
    assert.deepEqual(f.session.snapshot().colorSlots.find((s) => s.id === 'legacy').value, {
      hex: '#112233',
      alpha: 1,
    });
  });

  test(`${kind}: live codec rejects mismatched projection and unknown versions without rewriting saved snapshots`, () => {
    const f = fixture(kind);
    f.send('initialize-live-key-color-recipe', request());
    const saved = f.session.projectSnapshot ? f.session.projectSnapshot() : f.session.snapshot();
    const decode = f.session.projectSnapshot ? deserializeCompositionProject : deserialize;
    const text = JSON.stringify(saved);
    assert.deepEqual(decode(text).keyColorRecipe, saved.keyColorRecipe);
    for (const mutate of [
      (d) => {
        d.colorSlots.find((s) => s.id === 'fresh-key').value.hex = '#abcdef';
      },
      (d) => {
        d.keyColorRecipe.version = 3;
      },
      (d) => {
        d.keyColorRecipe.mode = 'unknown';
      },
      (d) => {
        d.schemaVersion = 58;
      },
      (d) => {
        d.keyColorRecipe.committedSnapshot.solverVersion = 'unknown';
      },
      (d) => {
        d.keyColorRecipe.committedSnapshot.colorSemanticsVersion = 'unknown';
      },
      (d) => {
        const snapshot = d.keyColorRecipe.committedSnapshot;
        snapshot.version = 1;
        for (const row of snapshot.resolved) {
          delete row.rTarget;
          delete row.rRealized;
        }
      },
    ]) {
      const invalid = structuredClone(saved);
      mutate(invalid);
      assert.throws(() => decode(JSON.stringify(invalid)));
    }
    // Loading validates a durable last-solved snapshot; it does not claim to
    // prove every mathematical intent/result relationship by recomputing it.
    assert.equal(JSON.stringify(saved), text);
  });

  test(`${kind}: live no-op after reloading differently ordered Color keys preserves pending Redo`, () => {
    const source = fixture(kind);
    source.send('initialize-live-key-color-recipe', request());
    const saved = source.session.projectSnapshot
      ? source.session.projectSnapshot()
      : source.session.snapshot();
    for (const slot of saved.colorSlots)
      slot.value = { alpha: slot.value.alpha, hex: slot.value.hex };
    const text = JSON.stringify(saved);
    const session =
      kind === 'document'
        ? new CommandSession(deserialize(text))
        : createCompositionSessionOwner(deserializeCompositionProject(text));
    const f = {
      session,
      send: (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    };
    f.send('set-document-settings', { key: 'width', value: 1001 });
    f.send('undo');
    const before = captured(session);
    assert.equal(session.historyView.future.length, 1);
    assert.deepEqual(Object.keys(session.snapshot().colorSlots[0].value), ['alpha', 'hex']);
    const snapshot = session.snapshot().keyColorRecipe.committedSnapshot;
    const result = f.send('apply-live-key-color-recipe-snapshot', {
      snapshot,
      expectedPaletteRevision: snapshot.revision,
    });
    assert.equal(result.changed, false);
    assert.deepEqual(captured(session), before);
    f.send('redo');
    assert.equal(session.snapshot().width, 1001);
  });

  test(`${kind}: live validation rejects a snapshot accessor without invoking it`, () => {
    const f = fixture(kind);
    f.send('initialize-live-key-color-recipe', request());
    const value = f.session.projectSnapshot ? f.session.projectSnapshot() : f.session.snapshot();
    let reads = 0;
    Object.defineProperty(value.keyColorRecipe.committedSnapshot, 'version', {
      enumerable: true,
      configurable: true,
      get() {
        reads++;
        return 2;
      },
    });
    assert.throws(() =>
      kind === 'document' ? validate(value) : validateCompositionProject(value),
    );
    assert.equal(reads, 0);
    if (kind === 'composition') {
      const arrayCase = f.session.projectSnapshot();
      const composition = arrayCase.compositions[0];
      Object.defineProperty(arrayCase.compositions, '0', {
        enumerable: true,
        configurable: true,
        get() {
          reads++;
          return composition;
        },
      });
      assert.throws(() => validateCompositionProject(arrayCase));
      const requiredField = f.session.projectSnapshot();
      Object.defineProperty(requiredField, 'schemaVersion', {
        enumerable: false,
        configurable: true,
        get() {
          reads++;
          return 59;
        },
      });
      assert.throws(() => validateCompositionProject(requiredField));
      assert.equal(reads, 0);
    }
  });

  test(`${kind}: import refuses linked references atomically; explicit unbind and unrelated imports still work`, () => {
    const source = fixture('document', legacyDocument());
    source.send('initialize-live-key-color-recipe', request());
    const sourceDoc = source.session.snapshot();
    const color = sourceDoc.layers[0].nodes.find((n) => n.type === 'Color');
    const layerId = sourceDoc.layers[0].id;
    source.send('bind-color-slot', {
      layerId,
      nodeId: color.id,
      key: 'value',
      slotId: 'fresh-derived',
    });
    const target = fixture(kind);
    withRedo(target);
    const sourceBefore = serialize(source.session.snapshot()),
      targetBefore = captured(target.session);
    const project = migrateLegacyDocumentToCompositionProject(source.session.snapshot());
    const importSource = () =>
      kind === 'document'
        ? target.send('import-composition', {
            sourceText: serialize(source.session.snapshot()),
            compositionId: layerId,
          })
        : target.send('import-project-compositions', {
            sourceProject: project,
            compositionIds: [project.compositions[0].id],
          });
    assert.throws(
      importSource,
      (error) => error.code === 'LIVE_COLOR_IMPORT_UNSUPPORTED' && /Unbind/.test(error.message),
    );
    assert.equal(serialize(source.session.snapshot()), sourceBefore);
    assert.deepEqual(captured(target.session), targetBefore);
    target.send('redo');
    assert.equal(target.session.snapshot().width, 1001);
    source.send('unbind-color-slot', { layerId, nodeId: color.id, key: 'value' });
    const detached = source.session.snapshot();
    if (kind === 'document')
      target.send('import-composition', {
        sourceText: serialize(detached),
        compositionId: layerId,
      });
    else {
      const detachedProject = migrateLegacyDocumentToCompositionProject(detached);
      target.send('import-project-compositions', {
        sourceProject: detachedProject,
        compositionIds: [detachedProject.compositions[0].id],
      });
    }
    assert.equal(source.session.snapshot().keyColorRecipe.mode, 'live');
    assert.equal(serialize(source.session.snapshot()), serialize(detached));
  });

  test(`${kind}: importing an ordinary bound color preserves the destination live palette`, () => {
    const source = legacyDocument(),
      target = fixture(kind, legacyDocument());
    target.send('initialize-live-key-color-recipe', request());
    const before = target.session.snapshot();
    if (kind === 'document')
      target.send('import-composition', {
        sourceText: serialize(source),
        compositionId: source.layers[0].id,
      });
    else {
      const project = migrateLegacyDocumentToCompositionProject(source);
      target.send('import-project-compositions', {
        sourceProject: project,
        compositionIds: [project.compositions[0].id],
      });
    }
    const after = target.session.snapshot();
    assert.deepEqual(after.keyColorRecipe, before.keyColorRecipe);
    for (const id of ['fresh-key', 'fresh-derived'])
      assert.deepEqual(
        after.colorSlots.find((slot) => slot.id === id),
        before.colorSlots.find((slot) => slot.id === id),
      );
  });
}
