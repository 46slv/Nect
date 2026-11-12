import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, deserialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { encodeRgbaPng } from '../src/image-keyer.js';
import { encodeImage } from '../src/images.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

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
const makeProject = () => migrateLegacyDocumentToCompositionProject(blankDocument());
const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });

test('an existing Project keeps an opaque same-name field until explicit admission', () => {
  const project = makeProject();
  project.macroDefinitions = { opaque: 'user data' };
  const before = structuredClone(project),
    text = serializeCompositionProject(project),
    reopened = deserializeCompositionProject(text, deserialize);
  assert.deepEqual(reopened, before);
  const owner = createCompositionSessionOwner(project);
  assert.throws(
    () => send(owner, 'register-macro-definition', { definition: definition() }),
    /opaque/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.equal(owner.historyState.canUndo, false);
});

test('definition registration uses one Project History entry and survives save/reopen', () => {
  const owner = createCompositionSessionOwner(makeProject()),
    before = owner.projectSnapshot();
  const result = send(owner, 'register-macro-definition', { definition: definition() });
  assert.equal(result.changed, true);
  const admitted = owner.projectSnapshot();
  assert.equal(admitted.schemaVersion, 66);
  assert.equal(admitted.macroDefinitions[0].id, 'double');
  const text = serializeCompositionProject(admitted),
    reopened = deserializeCompositionProject(text, deserialize),
    reowner = createCompositionSessionOwner(reopened);
  assert.deepEqual(reowner.projectSnapshot(), admitted);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), admitted);
});

test('Project ownership is detached from the caller before and after Macro admission', () => {
  const input = makeProject();
  input.imageAssets = {
    'image-pixel': {
      name: 'pixel.png',
      mime: 'image/png',
      width: 1,
      height: 1,
      data: encodeImage(encodeRgbaPng(1, 1, new Uint8Array([255, 0, 0, 128]))),
    },
  };
  const original = structuredClone(input),
    owner = createCompositionSessionOwner(input);
  assert.equal(Object.isFrozen(input.imageAssets['image-pixel']), false);
  send(owner, 'register-macro-definition', { definition: definition() });
  const admitted = owner.projectSnapshot(),
    revision = owner.projectRevision;
  assert.deepEqual(input, original);
  assert.equal(Object.isFrozen(input.imageAssets['image-pixel']), false);
  input.imageAssets['image-pixel'].name = 'Changed externally';
  input.macroDefinitions = [definition(9)];
  assert.deepEqual(owner.projectSnapshot(), admitted);
  assert.equal(owner.projectRevision, revision);

  const typedInput = structuredClone(admitted),
    typedOwner = createCompositionSessionOwner(typedInput),
    typedBefore = typedOwner.projectSnapshot();
  typedInput.macroDefinitions[0].parameters[0].defaultValue = 8;
  assert.deepEqual(typedOwner.projectSnapshot(), typedBefore);
  assert.equal(typedOwner.projectRevision, 0);
});

test('registration preserves all other Compositions and shared authored data', () => {
  const first = makeProject(),
    second = makeProject().compositions[0];
  second.id = 'second-composition';
  second.name = 'Second';
  first.compositions.push(second);
  first.metadata = { id: 'opaque', source: 'original' };
  const owner = createCompositionSessionOwner(first),
    before = owner.projectSnapshot();
  send(owner, 'register-macro-definition', { definition: definition() });
  const after = owner.projectSnapshot();
  assert.deepEqual(after.compositions, before.compositions);
  assert.deepEqual(after.metadata, before.metadata);
  send(owner, 'undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), after);
});

test('equal registration is a no-op; conflict rejects without losing Project or redo', () => {
  const owner = createCompositionSessionOwner(makeProject());
  send(owner, 'register-macro-definition', { definition: definition() });
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  const same = send(owner, 'register-macro-definition', { definition: definition() });
  assert.equal(same.changed, false);
  assert.deepEqual(owner.historyState, history);
  assert.throws(
    () => send(owner, 'register-macro-definition', { definition: definition(9) }),
    /conflict/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'undo');
  const undone = owner.projectSnapshot(),
    undoHistory = owner.historyState;
  assert.equal(undoHistory.canRedo, true);
  assert.throws(
    () => send(owner, 'register-macro-definition', { definition: definition(9) }),
    /conflict/i,
  );
  assert.deepEqual(owner.projectSnapshot(), undone);
  assert.deepEqual(owner.historyState, undoHistory);
  send(owner, 'redo');
  assert.deepEqual(owner.projectSnapshot(), before);
});

test('typed Project admission rejects invalid definitions without mutating existing data', () => {
  const project = makeProject();
  project.schemaVersion = 66;
  project.compositionSchemaVersion = 2;
  project.macroDefinitions = [definition(), definition(9)];
  const before = structuredClone(project);
  assert.throws(() => serializeCompositionProject(project), /conflict/i);
  assert.deepEqual(project, before);
});

test('typed Project serialization rejects hidden definitions without calling their getters', () => {
  const project = makeProject();
  project.schemaVersion = 66;
  project.compositionSchemaVersion = 2;
  let called = 0;
  const definitions = [];
  Object.defineProperty(definitions, '0', {
    enumerable: true,
    get() {
      called++;
      return definition();
    },
  });
  Object.defineProperty(project, 'macroDefinitions', {
    enumerable: false,
    value: definitions,
  });
  assert.throws(() => serializeCompositionProject(project));
  assert.equal(called, 0);
});

test('registration rejects a definition getter without executing it or changing History', () => {
  const owner = createCompositionSessionOwner(makeProject()),
    before = owner.projectSnapshot(),
    history = owner.historyState,
    payload = {};
  let called = 0;
  Object.defineProperty(payload, 'definition', {
    enumerable: true,
    get() {
      called++;
      return definition();
    },
  });
  assert.throws(() => send(owner, 'register-macro-definition', payload), /definition/i);
  assert.equal(called, 0);
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
});

test('Project owner rejects a schema getter before it can execute', () => {
  const project = makeProject();
  let called = 0;
  Object.defineProperty(project, 'schemaVersion', {
    enumerable: true,
    get() {
      called++;
      return 66;
    },
  });
  assert.throws(() => createCompositionSessionOwner(project), /schema version/i);
  assert.equal(called, 0);
});
