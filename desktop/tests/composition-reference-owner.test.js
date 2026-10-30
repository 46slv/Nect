import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, layer, node, validate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';

function fixture({ bound = false } = {}) {
  const document = blankDocument(),
    target = document.layers[0],
    source = layer('Reference source');
  document.layers.push(source);
  let reference;
  if (bound) {
    reference = node('Composition Reference');
    reference.compositionReference = { compositionId: source.id };
    target.nodes.push(reference);
    document.schemaVersion = 31;
  }
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(document));
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const currentReference = (id = reference?.id) =>
    owner.snapshot().layers[0].nodes.find((entry) => entry.id === id);
  return { owner, source, target, reference, send, currentReference };
}

function roundTrip(owner) {
  const project = owner.projectSnapshot(),
    text = serializeCompositionProject(project),
    reopened = createCompositionSessionOwner(deserializeCompositionProject(text));
  assert.deepEqual(reopened.projectSnapshot(), project);
  assert.deepEqual(reopened.snapshot(), owner.snapshot());
  validate(reopened.snapshot());
  return reopened;
}

test('production owner creates, binds and clears a Reference with chronological Undo/Redo', () => {
  const { owner, source, target, send, currentReference } = fixture(),
    before = owner.projectSnapshot(),
    created = send('create-node', {
      layerId: target.id,
      nodeType: 'Composition Reference',
      position: [40, 40],
    }),
    nodeId = created.nodeId;
  assert.equal(currentReference(nodeId).compositionReference, null);
  assert.equal(owner.historyView.past.length, 1);
  roundTrip(owner);
  const unbound = owner.projectSnapshot();
  send('set-composition-reference', {
    layerId: target.id,
    nodeId,
    target: { compositionId: source.id },
  });
  assert.deepEqual(currentReference(nodeId).compositionReference, { compositionId: source.id });
  assert.equal(owner.historyView.past.length, 2);
  roundTrip(owner);
  const bound = owner.projectSnapshot();
  send('set-composition-reference', { layerId: target.id, nodeId, target: null });
  assert.equal(currentReference(nodeId).compositionReference, null);
  assert.equal(owner.historyView.past.length, 3);
  const cleared = owner.projectSnapshot();
  roundTrip(owner);

  for (const expected of [bound, unbound, before]) {
    send('undo');
    assert.deepEqual(owner.projectSnapshot(), expected);
  }
  for (const expected of [unbound, bound, cleared]) {
    send('redo');
    assert.deepEqual(owner.projectSnapshot(), expected);
  }
  assert.equal(currentReference(nodeId).compositionReference, null);
  roundTrip(owner);
});

test('production owner unlinks bound References and reopens unbound projects', () => {
  const { owner, target, reference, send, currentReference } = fixture({ bound: true }),
    before = owner.projectSnapshot();
  send('set-composition-reference', {
    layerId: target.id,
    nodeId: reference.id,
    target: null,
  });
  assert.equal(currentReference().compositionReference, null);
  assert.equal(owner.historyView.past.length, 1);
  roundTrip(owner);
  send('undo');
  assert.deepEqual(owner.projectSnapshot(), before);
  send('redo');
  assert.equal(currentReference().compositionReference, null);
});

test('production owner rejects malformed, missing and cross-outer targets atomically', () => {
  const { owner, target, source, reference, send } = fixture({ bound: true });
  for (const invalid of [
    undefined,
    false,
    0,
    '',
    [],
    {},
    { compositionId: null },
    { compositionId: '' },
    { compositionId: 'missing-layer' },
    { compositionId: source.id, extra: true },
    { compositionId: target.id },
  ]) {
    const before = owner.projectSnapshot(),
      history = owner.historyState;
    assert.throws(() =>
      send('set-composition-reference', {
        layerId: target.id,
        nodeId: reference.id,
        target: invalid,
      }),
    );
    assert.deepEqual(owner.projectSnapshot(), before);
    assert.deepEqual(owner.historyState, history);
  }
  const foreign = owner.createComposition({ name: 'Other outer Composition' });
  const project = owner.projectSnapshot(),
    foreignLayerId = project.compositions.find((entry) => entry.id === foreign).layers[0].id;
  owner.switchComposition(project.compositions[0].id);
  const before = owner.projectSnapshot(),
    history = owner.historyState;
  assert.throws(() =>
    send('set-composition-reference', {
      layerId: target.id,
      nodeId: reference.id,
      target: { compositionId: foreignLayerId },
    }),
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.historyState, history);
});
