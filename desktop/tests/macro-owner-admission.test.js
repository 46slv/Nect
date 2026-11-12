import test from 'node:test';
import assert from 'node:assert/strict';
import { History } from '../src/core.js';
import { CompositionSessionOwner } from '../src/composition-session-owner.js';

// This fake inner Session exercises Project ownership and History only. No
// product Macro node, evaluator, UI or persisted schema is connected here.
let sequence = 0;
let afterPreparedCommit = null;
const clone = (value) => structuredClone(value);
const uid = (prefix) => `${prefix}-test-${++sequence}`;
const sameDocument = (a, b) => JSON.stringify(a) === JSON.stringify(b);

function definition(defaultValue = 1) {
  return {
    format: 'nect.macro',
    formatVersion: 1,
    id: 'double',
    revision: 1,
    name: 'Double',
    input: { type: 'Path', nodeId: 'a', socket: 'path' },
    output: { type: 'Path', nodeId: 'b', port: null },
    nodes: [
      {
        id: 'a',
        type: 'Transform',
        params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
      },
      {
        id: 'b',
        type: 'Transform',
        params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
      },
    ],
    edges: [{ sourceNodeId: 'a', sourcePort: null, targetNodeId: 'b', targetSocket: 'path' }],
    parameters: [
      { id: 'scale', label: 'Scale', nodeId: 'a', key: 'scale', type: 'Scalar', defaultValue },
    ],
  };
}
function blankDocument() {
  return {
    schemaVersion: 39,
    name: 'Project',
    width: 640,
    height: 480,
    layers: [{ id: uid('layer'), name: 'Artwork', nodes: [] }],
  };
}
function project() {
  return {
    compositionSchemaVersion: 1,
    schemaVersion: 39,
    name: 'Project',
    compositions: [
      {
        id: 'comp-a',
        name: 'A',
        width: 640,
        height: 480,
        layers: [{ id: 'layer-a', name: 'A', nodes: [] }],
      },
      {
        id: 'comp-b',
        name: 'B',
        width: 320,
        height: 240,
        layers: [{ id: 'layer-b', name: 'B', nodes: [] }],
      },
    ],
  };
}

class TestSession {
  constructor(document, { beforeCandidateCommit } = {}) {
    this.doc = clone(document);
    this.beforeCandidateCommit = beforeCandidateCommit;
    this.id = uid('session');
    this.revision = 0;
    this.hasGesture = false;
    this.hasTextComposition = false;
    this.hasTextDrafts = false;
    this.prepared = null;
  }
  get state() {
    return { sessionId: this.id, expectedRevision: this.revision };
  }
  snapshot() {
    return clone(this.doc);
  }
  dispatch(command) {
    if (command.sessionId !== this.id || command.expectedRevision !== this.revision)
      throw Error('STALE_REVISION');
    if (command.type !== 'test-shared-patch') throw Error('Unknown test command');
    const next = { ...clone(this.doc), ...clone(command.payload) };
    if (next.deleteMacroDefinitions === true) {
      delete next.macroDefinitions;
      delete next.deleteMacroDefinitions;
    }
    this.beforeCandidateCommit(this.doc, next);
    this.doc = next;
    this.revision++;
    return { ...this.state, changed: true };
  }
  async prepareCommand(command) {
    const token = Object.freeze({});
    this.prepared = { token, candidate: { ...clone(this.doc), ...clone(command.payload) } };
    return token;
  }
  commitPrepared(token) {
    if (!this.prepared || this.prepared.token !== token) throw Error('STALE_REVISION');
    this.beforeCandidateCommit(this.doc, this.prepared.candidate);
    this.doc = this.prepared.candidate;
    this.prepared = null;
    this.revision++;
    afterPreparedCommit?.();
    return { ...this.state, changed: true };
  }
  cancelPrepared() {
    this.prepared = null;
  }
}
const adapters = {
  History,
  CommandSession: TestSession,
  blankDocument,
  cloneDocument: clone,
  sameDocument,
  uid,
  validateDocument: (document) => document,
  macroProjectAdmission: (schemaVersion) => schemaVersion === 40,
};
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

test('redo pin conflict is refused before the inner Session commits or History changes', () => {
  const owner = new CompositionSessionOwner(project(), adapters);
  send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
  const admitted = owner.projectSnapshot();
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    history = owner.historyState,
    state = owner.state;
  assert.equal(history.canRedo, true);
  assert.throws(
    () =>
      send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition(9)] }),
    /conflict/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
  assert.deepEqual(owner.state, state);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), admitted);
});

test('unchanged pinned definitions survive normal Project edits, Undo and Redo', () => {
  const owner = new CompositionSessionOwner(project(), adapters);
  send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
  const admitted = owner.projectSnapshot();
  owner.createComposition({ name: 'Third', width: 100, height: 200 });
  assert.deepEqual(owner.projectSnapshot().macroDefinitions, admitted.macroDefinitions);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), admitted);
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().compositions.length, 3);
  assert.deepEqual(owner.projectSnapshot().macroDefinitions, admitted.macroDefinitions);
});

test('an opaque same-name field is preserved and admission is refused', () => {
  const original = project();
  original.macroDefinitions = { note: 'user data' };
  const owner = new CompositionSessionOwner(original, adapters),
    before = owner.projectSnapshot();
  assert.throws(
    () => send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] }),
    /opaque/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.equal(owner.historyState.canUndo, false);
});

test('prepared conflicting edits reject without changing Project or redo', async () => {
  const owner = new CompositionSessionOwner(project(), adapters);
  send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  const token = await owner.prepareCommand(
    {
      ...owner.state,
      type: 'test-shared-patch',
      payload: { schemaVersion: 40, macroDefinitions: [definition(8)] },
    },
    {},
  );
  assert.throws(() => owner.commitPrepared(token), /conflict/i);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().macroDefinitions[0].parameters[0].defaultValue, 1);
});

test('without an admission adapter the existing opaque Project behavior is unchanged', () => {
  const initial = project();
  initial.macroDefinitions = { arbitrary: 'preserved' };
  const { macroProjectAdmission: ignored, ...ordinaryAdapters } = adapters;
  const owner = new CompositionSessionOwner(initial, ordinaryAdapters);
  owner.createComposition({ name: 'Third', width: 100, height: 200 });
  assert.deepEqual(owner.projectSnapshot().macroDefinitions, initial.macroDefinitions);
});

test('unadmitted History keeps its opaque field without interpreting it as definitions', () => {
  const initial = project();
  initial.macroDefinitions = { note: 'user data' };
  const opaque = clone(initial.macroDefinitions);
  const owner = new CompositionSessionOwner(initial, adapters);
  send(owner, 'test-shared-patch', { deleteMacroDefinitions: true });
  send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
  send(owner, 'undo');
  assert.equal(Object.hasOwn(owner.projectSnapshot(), 'macroDefinitions'), false);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot().macroDefinitions, opaque);
  send(owner, 'redo');
  send(owner, 'redo');
  assert.equal(owner.projectSnapshot().macroDefinitions[0].id, 'double');
});

test('invalid pins in an inactive admitted Composition are refused on opening', () => {
  const initial = project();
  initial.schemaVersion = 40;
  initial.macroDefinitions = [definition()];
  initial.compositions[1].layers[0].nodes.push({
    id: 'macro-inactive',
    type: 'Macro',
    macro: { definitionId: 'missing', definitionRevision: 1, overrides: {} },
  });
  assert.throws(() => new CompositionSessionOwner(initial, adapters), /missing/i);
});

test('reentrant Project change wins and the stale outer Macro edit is refused', () => {
  let owner,
    trigger = false;
  const classify = (schemaVersion) => {
    if (trigger) {
      trigger = false;
      owner.createComposition({ name: 'Concurrent', width: 100, height: 200 });
    }
    return schemaVersion === 40;
  };
  owner = new CompositionSessionOwner(project(), { ...adapters, macroProjectAdmission: classify });
  send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
  trigger = true;
  assert.throws(() => send(owner, 'test-shared-patch', { name: 'Stale outer edit' }), /STALE/i);
  assert.equal(owner.projectSnapshot().name, 'Project');
  assert.equal(owner.projectSnapshot().compositions.at(-1).name, 'Concurrent');
  assert.equal(owner.projectSnapshot().macroDefinitions[0].id, 'double');
});

test('a prepared edit rejects after a Composition switch or intervening Project edit', async () => {
  for (const change of [
    (owner) => owner.switchComposition('comp-b'),
    (owner) => owner.createComposition({ name: 'Concurrent', width: 100, height: 200 }),
  ]) {
    const owner = new CompositionSessionOwner(project(), adapters);
    send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
    const token = await owner.prepareCommand(
      { ...owner.state, type: 'test-shared-patch', payload: { name: 'Stale' } },
      {},
    );
    change(owner);
    const afterChange = owner.projectSnapshot();
    assert.throws(() => owner.commitPrepared(token), /STALE/i);
    assert.deepEqual(owner.projectSnapshot(), afterChange);
  }
});

test('a reentrant Project edit after prepared inner commit keeps the new Session', async () => {
  const owner = new CompositionSessionOwner(project(), adapters);
  send(owner, 'test-shared-patch', { schemaVersion: 40, macroDefinitions: [definition()] });
  const token = await owner.prepareCommand(
    { ...owner.state, type: 'test-shared-patch', payload: { name: 'Stale prepared edit' } },
    {},
  );
  let currentState;
  afterPreparedCommit = () => {
    afterPreparedCommit = null;
    owner.createComposition({ name: 'Concurrent', width: 100, height: 200 });
    currentState = owner.state;
  };
  try {
    assert.throws(() => owner.commitPrepared(token), /STALE/i);
    assert.deepEqual(owner.state, currentState);
    assert.equal(owner.projectSnapshot().name, 'Project');
    assert.equal(owner.projectSnapshot().compositions.at(-1).name, 'Concurrent');
  } finally {
    afterPreparedCommit = null;
  }
});
