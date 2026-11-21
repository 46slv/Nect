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
  const second = create({ target: target(), artworkType: 'Text', at: [200, 210] });
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
