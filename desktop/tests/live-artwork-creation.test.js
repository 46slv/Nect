import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveArtworkCreation } from '../src/live-artwork-creation.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

test('target-bound shape and text creation go through the existing editor command and history', () => {
  const raw = readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8');
  const owner = createCompositionSessionOwner(deserializeCompositionProject(raw, deserialize));
  const target = () => ({
    documentGeneration: 1,
    compositionId: owner.activeCompositionId,
    sessionId: owner.state.sessionId,
    sessionRevision: owner.state.expectedRevision,
    projectRevision: owner.projectRevision,
  });
  const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
  let blocked = false;
  const create = createLiveArtworkCreation({
    readTarget: target,
    isEditBlocked: () => blocked,
    commit: (command) => owner.dispatch(command),
  });
  const before = snapshot();
  const rectangle = { target: target(), artworkType: 'Rectangle', at: [250, 300] };
  blocked = true;
  assert.equal(create(rectangle).status, 'edit-in-progress');
  blocked = false;
  assert.equal(create({ ...rectangle, at: [250, NaN] }).status, 'invalid-arguments');
  assert.equal(create({ ...rectangle, at: Array(2) }).status, 'invalid-arguments');
  assert.equal(create({ ...rectangle, artworkType: 'Output' }).status, 'invalid-arguments');
  assert.equal(snapshot(), before);
  const first = create(rectangle);
  assert.equal(first.status, 'changed');
  assert.equal(create(rectangle).status, 'stale-target');
  const selected = owner.snapshot().layers.find((layer) => layer.id === first.layerId);
  const shape = selected.nodes.find((node) => node.id === first.nodeId);
  assert.equal(shape.type, 'Rectangle');
  assert.deepEqual(shape.params.center.value, [250, 300]);
  const partial = snapshot();
  const handle = createLiveEditorProtocol({ readTarget: target, createArtwork: create });
  const intent = { target: target(), artworkType: 'Text', at: [200, 210] };
  const request = { version: 1, operation: 'artwork.create', arguments: intent };
  assert.deepEqual(createLiveEditorProtocol({ readTarget: target })(request), {
    status: 'unsupported-operation',
  });
  assert.deepEqual(handle({ ...request, arguments: { ...intent, path: 'unknown' } }), {
    status: 'invalid-arguments',
  });
  const second = handle(request);
  assert.deepEqual(second.target, target());
  assert.equal(second.status, 'changed');
  const textLayer = owner.snapshot().layers.find((layer) => layer.id === second.layerId);
  const text = textLayer.nodes.find((node) => node.id === second.nodeId);
  assert.equal(text.type, 'Text');
  assert.deepEqual(text.params.position.value, [200, 210]);
  const after = snapshot();
  owner.dispatch({ ...owner.state, type: 'undo', payload: {} });
  assert.equal(snapshot(), partial);
  owner.dispatch({ ...owner.state, type: 'redo', payload: {} });
  assert.equal(snapshot(), after);
});

for (const artworkType of ['Circle', 'Ellipse', 'Polygon', 'Star']) {
  test(`scoped ${artworkType} creation preserves other layers and round-trips through History`, () => {
    const raw = readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8');
    const owner = createCompositionSessionOwner(deserializeCompositionProject(raw, deserialize));
    const target = () => ({
      documentGeneration: 1,
      compositionId: owner.activeCompositionId,
      sessionId: owner.state.sessionId,
      sessionRevision: owner.state.expectedRevision,
      projectRevision: owner.projectRevision,
    });
    const snapshot = () => serializeCompositionProject(owner.projectSnapshot());
    const handle = createLiveEditorProtocol({
      readTarget: target,
      createArtwork: createLiveArtworkCreation({
        readTarget: target,
        isEditBlocked: () => false,
        commit: (command) => owner.dispatch(command),
      }),
    });
    const before = snapshot();
    const layers = structuredClone(owner.snapshot().layers);
    const at = [-25.5, 300.25];
    const request = {
      version: 1,
      operation: 'artwork.create',
      arguments: { target: target(), artworkType, at },
    };
    const result = handle(request);
    assert.equal(result.status, 'changed');
    assert.deepEqual(result.target, target());
    assert.deepEqual(owner.snapshot().layers.slice(0, -1), layers);
    const created = owner.snapshot().layers.at(-1);
    assert.equal(created.id, result.layerId);
    const node = created.nodes.find((entry) => entry.id === result.nodeId);
    assert.equal(node.type, artworkType);
    assert.deepEqual(node.params.center.value, at);
    const after = snapshot();
    assert.deepEqual(handle(request), { status: 'stale-target' });
    assert.equal(snapshot(), after);

    owner.dispatch({ ...owner.state, type: 'undo', payload: {} });
    assert.equal(snapshot(), before);
    owner.dispatch({ ...owner.state, type: 'redo', payload: {} });
    assert.equal(snapshot(), after);

    const reopened = createCompositionSessionOwner(
      deserializeCompositionProject(after, deserialize),
    );
    assert.equal(serializeCompositionProject(reopened.projectSnapshot()), after);
  });
}
