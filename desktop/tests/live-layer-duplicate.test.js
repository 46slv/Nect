import assert from 'node:assert/strict';
import test from 'node:test';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { deserialize, serialize } from '../src/core.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveLayerDuplicate } from '../src/live-layer-duplicate.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

test('opt-in duplicate targets an authored layer and returns its fresh identity once', () => {
  const sample = orderingDocument();
  sample.layers[1].locked = true;
  const seed = serialize(sample);
  const makeOwner = () =>
    createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
  const owner = makeOwner();
  const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let blocked = false;
  let writes = 0;
  const duplicateLayer = createLiveLayerDuplicate({
    readTarget,
    getOwner: () => owner,
    isEditBlocked: () => blocked,
    commit(active, command) {
      assert.equal(active, owner);
      assert.equal(command.type, 'duplicate-layer');
      writes++;
      return active.dispatch(command);
    },
  });
  const handle = createLiveEditorProtocol({ readTarget, duplicateLayer });
  const [source, locked, other] = owner.snapshot().layers;
  const before = snapshot();
  const initial = readTarget();
  const request = {
    version: 1,
    operation: 'layer.duplicate',
    arguments: { target: initial, layerId: source.id },
  };
  const edit = (args) => handle({ ...request, arguments: { ...request.arguments, ...args } });
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  assert.equal(edit({ token: 'clipboard-is-a-different-operation' }).status, 'invalid-arguments');
  assert.equal(edit({ layerId: 'missing' }).status, 'unsupported-target');
  assert.equal(edit({ layerId: locked.id }).status, 'rejected');
  blocked = true;
  assert.equal(handle(request).status, 'edit-in-progress');
  blocked = false;
  assert.equal(writes, 0);
  assert.equal(snapshot(), before);

  const changed = handle(request);
  const copyId = changed.layerId;
  assert.deepEqual(changed, {
    status: 'changed',
    sourceLayerId: source.id,
    layerId: copyId,
    target: readTarget(),
  });
  assert.equal(changed.target.projectRevision, initial.projectRevision + 1);
  const after = snapshot();
  const layers = owner.snapshot().layers;
  const copy = layers.find((l) => l.id === copyId);
  assert.ok(copy && copy.id !== source.id);
  assert.deepEqual(
    layers.filter((l) => l.id !== copyId),
    [source, locked, other],
  );
  assert.notEqual(copy.output, source.output);
  assert.deepEqual(
    copy.nodes.map((n) => n.type),
    source.nodes.map((n) => n.type),
  );
  const sourceNodeIds = new Set(source.nodes.map((n) => n.id));
  assert.ok(copy.nodes.every((n) => !sourceNodeIds.has(n.id)));
  assert.ok(copy.nodes.some((n) => n.id === copy.output));
  assert.equal(handle(request).status, 'stale-target');
  assert.equal(writes, 1);

  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.equal(snapshot(), before);
  owner.dispatch({ ...owner.state, type: 'redo' });
  assert.equal(snapshot(), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after, deserialize));
  assert.deepEqual(
    reopened.snapshot().layers.find((l) => l.id === copyId),
    copy,
  );
});

test('lost duplicate acknowledgement cannot create a second copy on exact replay', () => {
  const owner = createCompositionSessionOwner(
    deserializeCompositionProject(serialize(orderingDocument()), deserialize),
  );
  let generation = 1;
  const readTarget = () => ({
    documentGeneration: generation,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  let writes = 0;
  const duplicateLayer = createLiveLayerDuplicate({
    readTarget,
    getOwner: () => owner,
    isEditBlocked: () => false,
    commit(active, command) {
      writes++;
      active.dispatch(command);
      throw Error('simulated acknowledgement loss after committing');
    },
  });
  const handle = createLiveEditorProtocol({ readTarget, duplicateLayer });
  const initial = owner.snapshot().layers;
  const source = initial[0].id;
  const originalTarget = readTarget();
  const inspectRequest = { version: 1, operation: 'editor.inspect', arguments: {} };
  const request = {
    version: 1,
    operation: 'layer.duplicate',
    arguments: { target: originalTarget, layerId: source },
  };

  generation++;
  assert.deepEqual(handle(request), { status: 'stale-target' });
  assert.equal(writes, 0);
  const exactRequest = {
    ...request,
    arguments: { target: handle(inspectRequest).target, layerId: source },
  };
  assert.deepEqual(handle(exactRequest), { status: 'outcome-unknown' });
  assert.deepEqual(handle(exactRequest), { status: 'stale-target' });
  assert.equal(writes, 1);

  const fresh = handle(inspectRequest).target;
  assert.equal(fresh.projectRevision, originalTarget.projectRevision + 1);
  const beforeIds = new Set(initial.map((l) => l.id));
  const layers = owner.snapshot().layers;
  const [copy] = layers.filter((l) => !beforeIds.has(l.id));
  assert.ok(copy);
  assert.equal(layers.length, initial.length + 1);
  assert.deepEqual(
    layers.filter((l) => beforeIds.has(l.id)),
    initial,
  );
  assert.notEqual(copy.id, source);
  owner.dispatch({ ...owner.state, type: 'undo' });
  assert.deepEqual(owner.snapshot().layers, initial);
});
