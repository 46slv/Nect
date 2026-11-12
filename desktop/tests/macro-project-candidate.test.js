import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  serializeCompositionProject,
  validateCompositionProject,
  validateMacroProjectCandidate,
  validateMacroProjectTransition,
} from '../src/composition-project-model.js';
import { mergeMacroDefinitions } from '../src/macro-project.js';

// These fixtures describe a candidate codec contract. Macro is not yet a
// supported Session node, and normal Open/Save does not invoke this check.
function definition(revision = 1) {
  return {
    format: 'nect.macro',
    formatVersion: 1,
    id: 'double-transform',
    revision,
    name: 'Double Transform',
    input: { type: 'Path', nodeId: 'local-a', socket: 'path' },
    output: { type: 'Path', nodeId: 'local-b', port: null },
    nodes: [
      {
        id: 'local-a',
        type: 'Transform',
        params: { anchor: [2, 0], offset: [0, 0], scale: 1, rotation: 0 },
      },
      {
        id: 'local-b',
        type: 'Transform',
        params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
      },
    ],
    edges: [
      { sourceNodeId: 'local-a', sourcePort: null, targetNodeId: 'local-b', targetSocket: 'path' },
    ],
    parameters: [
      {
        id: 'scale',
        label: 'Scale',
        nodeId: 'local-a',
        key: 'scale',
        type: 'Scalar',
        defaultValue: 1,
      },
    ],
  };
}
function instance(id, revision = 1, overrides = {}) {
  return {
    id,
    type: 'Macro',
    macro: { definitionId: 'double-transform', definitionRevision: revision, overrides },
  };
}
const noRetainedDefinitions = () => ({ retainedDefinitionSets: [] });
function project() {
  const second = definition(2);
  second.parameters[0].defaultValue = 3;
  return {
    compositionSchemaVersion: 1,
    schemaVersion: 39,
    name: 'Project',
    macroDefinitions: [definition(), second],
    imageAssets: { image: { name: 'image.png', data: 'preserved' } },
    metadata: { owner: 'original', id: 'opaque-id' },
    compositions: [
      {
        id: 'comp-a',
        name: 'A',
        width: 640,
        height: 480,
        layers: [
          {
            id: 'layer-a',
            name: 'A',
            nodes: [instance('macro-a', 1, { scale: 0 })],
            output: 'macro-a',
          },
        ],
      },
      {
        id: 'comp-b',
        name: 'B',
        width: 320,
        height: 240,
        layers: [{ id: 'layer-b', name: 'B', nodes: [instance('macro-b', 2)], output: 'macro-b' }],
      },
    ],
  };
}

test('candidate checks every Composition without changing Project data or pinned revisions', () => {
  const candidate = project(),
    before = structuredClone(candidate);
  assert.strictEqual(validateMacroProjectCandidate(candidate), candidate);
  assert.deepEqual(candidate, before);
  const reopened = deserializeCompositionProject(serializeCompositionProject(candidate));
  assert.deepEqual(reopened, before);
  assert.strictEqual(validateMacroProjectCandidate(reopened), reopened);
  assert.deepEqual(
    activeCompositionDocument(reopened, 'comp-a').macroDefinitions,
    before.macroDefinitions,
  );
  assert.equal(reopened.compositions[0].layers[0].nodes[0].macro.overrides.scale, 0);
  assert.equal(reopened.compositions[1].layers[0].nodes[0].macro.definitionRevision, 2);
});

test('a missing or changed pin in an inactive Composition rejects the whole candidate', () => {
  for (const edit of [
    (p) => {
      p.compositions[1].layers[0].nodes[0].macro.definitionRevision = 3;
    },
    (p) => {
      p.compositions[1].layers[0].nodes[0].macro.definitionId = 'missing';
    },
    (p) => {
      p.macroDefinitions.pop();
    },
  ]) {
    const candidate = project();
    edit(candidate);
    const before = structuredClone(candidate);
    assert.throws(() => validateMacroProjectCandidate(candidate), /Macro.*definition|pin/i);
    assert.deepEqual(candidate, before);
  }
});

test('a definition ID and revision cannot have duplicate or conflicting contents', () => {
  for (const conflict of [false, true]) {
    const candidate = project(),
      duplicate = definition();
    if (conflict) duplicate.parameters[0].defaultValue = 4;
    candidate.macroDefinitions.push(duplicate);
    const before = structuredClone(candidate);
    assert.throws(() => validateMacroProjectCandidate(candidate), /duplicate|conflict/i);
    assert.deepEqual(candidate, before);
  }
});

test('a candidate cannot silently change a previously pinned definition revision', () => {
  const before = project(),
    changed = structuredClone(before);
  changed.macroDefinitions[0].parameters[0].defaultValue = 4;
  assert.strictEqual(validateMacroProjectCandidate(changed), changed);
  const beforeCopy = structuredClone(before),
    changedCopy = structuredClone(changed);
  assert.throws(
    () => validateMacroProjectTransition(before, changed, noRetainedDefinitions()),
    /conflict/i,
  );
  assert.deepEqual(before, beforeCopy);
  assert.deepEqual(changed, changedCopy);

  const addedRevision = structuredClone(before);
  addedRevision.macroDefinitions.push(definition(3));
  assert.strictEqual(
    validateMacroProjectTransition(before, addedRevision, noRetainedDefinitions()),
    addedRevision,
  );

  const reordered = structuredClone(before),
    original = reordered.macroDefinitions[0];
  reordered.macroDefinitions[0] = Object.fromEntries(Object.entries(original).reverse());
  assert.strictEqual(
    validateMacroProjectTransition(before, reordered, noRetainedDefinitions()),
    reordered,
  );
});

test('candidate transitions refuse definition removal until reuse and History are defined', () => {
  const before = project(),
    changed = structuredClone(before);
  changed.macroDefinitions.pop();
  changed.compositions[1].layers[0].nodes = [];
  assert.strictEqual(validateMacroProjectCandidate(changed), changed);
  assert.throws(
    () => validateMacroProjectTransition(before, changed, noRetainedDefinitions()),
    /remov/i,
  );
});

test('retained History pins reject reuse with different contents after an Undo', () => {
  const retained = project(),
    before = structuredClone(retained);
  delete before.macroDefinitions;
  for (const composition of before.compositions) composition.layers[0].nodes = [];
  const candidate = structuredClone(before),
    conflicting = definition();
  conflicting.parameters[0].defaultValue = 9;
  candidate.macroDefinitions = [conflicting];
  const oldFuture = structuredClone(retained),
    oldBefore = structuredClone(before),
    oldCandidate = structuredClone(candidate);
  assert.throws(() => validateMacroProjectTransition(before, candidate), /retained/i);
  assert.throws(
    () =>
      validateMacroProjectTransition(before, candidate, {
        retainedDefinitionSets: [retained.macroDefinitions],
      }),
    /conflict/i,
  );
  assert.deepEqual(retained, oldFuture);
  assert.deepEqual(before, oldBefore);
  assert.deepEqual(candidate, oldCandidate);
  candidate.macroDefinitions = [definition()];
  assert.strictEqual(
    validateMacroProjectTransition(before, candidate, {
      retainedDefinitionSets: [retained.macroDefinitions],
    }),
    candidate,
  );
});

test('a transition requires an explicit data-only retained definition set', () => {
  const candidate = project();
  let called = 0;
  const options = {};
  Object.defineProperty(options, 'retainedDefinitionSets', {
    get() {
      called++;
      return [];
    },
  });
  assert.throws(() => validateMacroProjectTransition(candidate, candidate, options), /retained/i);
  assert.equal(called, 0);
  assert.throws(
    () => validateMacroProjectTransition(candidate, candidate, { retainedDefinitionSets: null }),
    /retained/i,
  );
});

test('definition merge deduplicates equal pins and refuses conflicts without mutating inputs', () => {
  const first = definition(),
    same = Object.fromEntries(Object.entries(structuredClone(first)).reverse()),
    second = definition(2),
    existing = [first],
    incoming = [same, second],
    before = structuredClone([existing, incoming]),
    merged = mergeMacroDefinitions(existing, incoming);
  assert.deepEqual(
    merged.map(({ revision }) => revision),
    [1, 2],
  );
  assert.ok(Object.isFrozen(merged));
  assert.ok(Object.isFrozen(merged[0].nodes[0].params));
  assert.deepEqual([existing, incoming], before);
  const conflicting = definition();
  conflicting.name = 'Different content';
  assert.throws(() => mergeMacroDefinitions(existing, [conflicting]), /conflict/i);
  assert.deepEqual([existing, incoming], before);
});

test('definition merge rejects accessor array entries without executing them', () => {
  const incoming = [definition()];
  let called = 0;
  Object.defineProperty(incoming, '0', {
    enumerable: true,
    get() {
      called++;
      return definition();
    },
  });
  assert.throws(() => mergeMacroDefinitions([], incoming), /definition/i);
  assert.equal(called, 0);
});

test('override IDs and effective Transform values are validated for every instance', () => {
  for (const overrides of [{ missing: 1 }, { scale: '2' }, { scale: Number.MAX_VALUE }]) {
    const candidate = project();
    candidate.compositions[1].layers[0].nodes[0].macro.overrides = overrides;
    const before = structuredClone(candidate);
    assert.throws(() => validateMacroProjectCandidate(candidate));
    assert.deepEqual(candidate, before);
  }
});

test('Macro presets are refused until their definition closure is supported', () => {
  const candidate = project();
  candidate.nodePresets = [{ id: 'preset-1', name: 'Preset', nodes: [instance('preset-macro')] }];
  assert.throws(() => validateMacroProjectCandidate(candidate), /preset.*Macro|Macro.*preset/i);
});

test('ordinary codec preserves an opaque field and does not opt it into the candidate contract', () => {
  const ordinary = project();
  delete ordinary.macroDefinitions;
  ordinary.compositions.forEach((c) => {
    c.layers[0].nodes = [];
  });
  ordinary.macroDefinitions = { note: 'user data, not a Macro registry' };
  const before = structuredClone(ordinary);
  assert.strictEqual(validateCompositionProject(ordinary), ordinary);
  assert.deepEqual(deserializeCompositionProject(serializeCompositionProject(ordinary)), before);
  assert.throws(() => validateMacroProjectCandidate(ordinary), /Macro.*definitions/i);
});

test('candidate validates the existing Project and rejects unsafe JSON without invoking getters', () => {
  const duplicateId = project();
  duplicateId.compositions[1].layers[0].nodes[0].id = 'macro-a';
  assert.throws(() => validateMacroProjectCandidate(duplicateId), /Duplicate ID/i);
  const candidate = project();
  let called = 0;
  Object.defineProperty(candidate.compositions[0].layers[0].nodes[0].macro, 'overrides', {
    enumerable: true,
    get() {
      called++;
      return {};
    },
  });
  assert.throws(() => validateMacroProjectCandidate(candidate), /accessor/i);
  assert.equal(called, 0);
});
