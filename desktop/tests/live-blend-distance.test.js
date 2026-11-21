import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { deserialize, evaluate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveBlendDistanceOperation } from '../src/live-blend-distance.js';
import { captureEditorTarget } from '../src/live-editor-target.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

test('typed Blend distance preserves scope, history and the live owner', () => {
  const raw = readFileSync(new URL('../examples/blend-distance.json', import.meta.url), 'utf8');
  const owner = createCompositionSessionOwner(deserializeCompositionProject(raw, deserialize));
  const layer = owner.snapshot().layers[0];
  const node = layer.nodes.find((candidate) => candidate.type === 'Blend');
  const target = () =>
    captureEditorTarget({
      documentGeneration: 1,
      compositionId: owner.activeCompositionId,
      sessionId: owner.state.sessionId,
      sessionRevision: owner.state.expectedRevision,
      projectRevision: owner.projectRevision,
    });
  let blocked = false;
  const set = createLiveBlendDistanceOperation({
    readTarget: target,
    getOwner: () => owner,
    isEditBlocked: () => blocked,
    commit: (liveOwner, command) => liveOwner.dispatch(command),
  });
  const count = () => {
    const doc = owner.snapshot();
    return evaluate(doc.layers[0], {}, doc.imageAssets, doc).path.children.length;
  };
  const before = serializeCompositionProject(owner.projectSnapshot());
  const request = { target: target(), layerId: layer.id, nodeId: node.id, value: 60 };
  blocked = true;
  assert.equal(set(request).status, 'edit-in-progress');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  blocked = false;
  assert.equal(set({ ...request, value: Infinity }).status, 'invalid-arguments');
  assert.equal(set({ ...request, nodeId: 'missing' }).status, 'unsupported-target');
  assert.equal(count(), 16);
  assert.equal(set(request).status, 'changed');
  assert.equal(count(), 11);
  const handle = createLiveEditorProtocol({ readTarget: target, setBlendDistance: set });
  const roundTrip = (operation, args) =>
    handle(JSON.parse(JSON.stringify({ version: 1, operation, arguments: args })));
  const current = roundTrip('editor.inspect', {});
  assert.deepEqual(current, { status: 'ok', target: target() });
  assert.deepEqual(roundTrip('blend.setDistance', request), { status: 'stale-target' });
  assert.deepEqual(roundTrip('blend.setDistance', { ...request, target: current.target }), {
    status: 'unchanged',
    target: target(),
  });
  const committed = serializeCompositionProject(owner.projectSnapshot());
  assert.equal(set({ ...request, value: 70 }).status, 'stale-target');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), committed);
  owner.dispatch({ ...owner.state, type: 'undo', payload: {} });
  assert.equal(count(), 16);
  owner.dispatch({ ...owner.state, type: 'redo', payload: {} });
  assert.equal(count(), 11);
});
