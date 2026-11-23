import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { readLiveLayerFolderPlan } from '../src/live-layer-folder-plan.js';

const initial = {
  documentGeneration: 1,
  compositionId: 'comp',
  sessionId: 'session',
  sessionRevision: 2,
  projectRevision: 3,
};
const fixture = (ids) => ({
  layers: ids.map((id) => ({ id })),
  editor: {
    layerFolders: [
      { id: 'group', name: 'not a response field', members: ids.map((layerId) => ({ layerId })) },
    ],
  },
});

test('folder plan previews exact paint and membership without modifying the document', () => {
  const doc = fixture(['back', 'middle', 'front']);
  const before = structuredClone(doc);
  let target = initial;
  const readTarget = () => target;
  const args = {
    target: initial,
    request: { mode: 'remove', folderId: 'group', layerIds: ['middle'] },
  };
  const req = { version: 1, operation: 'layerFolders.plan', arguments: args };
  const make = (readDocument) =>
    createLiveEditorProtocol({
      readTarget,
      readLayerFolderPlan: (args) => readLiveLayerFolderPlan({ readTarget, readDocument }, args),
    });
  assert.equal(createLiveEditorProtocol({ readTarget })(req).status, 'unsupported-operation');
  const handler = make(() => doc);
  assert.deepEqual(handler(req), {
    status: 'ok',
    target: initial,
    before: ['back', 'middle', 'front'],
    after: ['back', 'front', 'middle'],
    orderChanged: true,
    membership: [{ folderId: 'group', layerIds: ['back', 'front'] }],
  });
  assert.deepEqual(doc, before);
  assert.equal(
    handler({ ...req, arguments: { ...args, commit: true } }).status,
    'invalid-arguments',
  );
  assert.equal(
    handler({ ...req, arguments: { ...args, request: { ...args.request, mode: 'delete' } } })
      .status,
    'invalid-arguments',
  );
  assert.equal(
    handler({ ...req, arguments: { ...args, request: { ...args.request, layerIds: ['missing'] } } })
      .status,
    'rejected',
  );
  assert.equal(
    make(() => {
      target = { ...initial, projectRevision: 4 };
      return doc;
    })(req).status,
    'stale-target',
  );
  assert.equal(make(() => assert.fail('stale plan must not read'))(req).status, 'stale-target');
});

test('an oversized folder plan rejects instead of truncating a paint permutation', () => {
  const ids = Array.from({ length: 18 }, (_, i) => i + '-'.repeat(250));
  const doc = fixture(ids);
  const plan = readLiveLayerFolderPlan(
    { readTarget: () => initial, readDocument: () => doc },
    { target: initial, request: { mode: 'remove', folderId: 'group', layerIds: [ids[2]] } },
  );
  assert.deepEqual(plan, { status: 'plan-too-large' });
});
