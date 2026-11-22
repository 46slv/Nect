import assert from 'node:assert/strict';
import test from 'node:test';
import { deserialize, exportSVG, serialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { createLiveArtworkTranslation } from '../src/live-artwork-translation.js';
import { orderingDocument } from './fixtures/artwork-ordering.js';

const doc = orderingDocument();
doc.layers[1].locked = true;
const seed = serialize(doc);
const makeOwner = () =>
  createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
const snapshot = (owner) => serializeCompositionProject(owner.projectSnapshot());

test('explicit artwork translation is one GUI-equivalent edit and an old request cannot repeat it', () => {
  const live = makeOwner();
  const gui = makeOwner();
  const [first, locked, third] = live.snapshot().layers;
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  let blocked = false;
  let attempts = 0;
  const translateArtwork = createLiveArtworkTranslation({
    readTarget,
    getOwner: () => live,
    isEditBlocked: () => blocked,
    commit(owner, command) {
      attempts++;
      return owner.dispatch(command);
    },
  });
  const handle = createLiveEditorProtocol({ readTarget, translateArtwork });
  const target = readTarget();
  const before = snapshot(live);
  const beforeSvg = exportSVG(live.snapshot());
  const payload = { layerIds: [first.id, third.id], delta: [12.5, -8] };
  const request = { version: 1, operation: 'artwork.translate', arguments: { target, ...payload } };
  const edit = (override) =>
    handle({ ...request, arguments: { ...request.arguments, ...override } });
  assert.equal(createLiveEditorProtocol({ readTarget })(request).status, 'unsupported-operation');
  for (const invalid of [
    { layerIds: [] },
    { layerIds: [first.id, first.id] },
    { layerIds: new Array(1) },
    { layerIds: Array.from({ length: 101 }, (_, i) => `layer-${i}`) },
    { delta: [0] },
    { delta: ['1', 2] },
    { delta: [Infinity, 1] },
    { nodeId: 'not-supported' },
  ])
    assert.equal(edit(invalid).status, 'invalid-arguments');
  assert.equal(edit({ layerIds: [first.id, 'missing'] }).status, 'unsupported-target');
  assert.equal(edit({ layerIds: [first.id, locked.id] }).status, 'rejected');
  blocked = true;
  assert.equal(handle(request).status, 'edit-in-progress');
  blocked = false;
  assert.equal(attempts, 0);
  assert.equal(snapshot(live), before);
  assert.equal(edit({ delta: [0, 0] }).status, 'unchanged');
  assert.deepEqual(readTarget(), target);
  assert.equal(snapshot(live), before);

  gui.dispatch({ ...gui.state, type: 'translate-artworks', payload });
  handle(request); // Deliberately discard the acknowledgement.
  const after = snapshot(live);
  assert.equal(after, snapshot(gui));
  assert.equal(readTarget().projectRevision, target.projectRevision + 1);
  assert.notEqual(exportSVG(live.snapshot()), beforeSvg);
  assert.equal(exportSVG(live.snapshot()), exportSVG(gui.snapshot()));
  assert.deepEqual(live.snapshot().layers[1], locked);
  assert.equal(handle(request).status, 'stale-target');
  assert.equal(attempts, 2);
  assert.equal(snapshot(live), after);

  live.dispatch({ ...live.state, type: 'undo' });
  assert.equal(snapshot(live), before);
  live.dispatch({ ...live.state, type: 'redo' });
  assert.equal(snapshot(live), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after, deserialize));
  assert.equal(snapshot(reopened), after);
});
