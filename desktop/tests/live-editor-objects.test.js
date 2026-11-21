import assert from 'node:assert/strict';
import test from 'node:test';
import { LIVE_OBJECT_PAGE_SIZE, readLiveEditorObjects } from '../src/live-editor-objects.js';

const target = {
  documentGeneration: 1,
  compositionId: 'composition',
  sessionId: 'session',
  sessionRevision: 2,
  projectRevision: 3,
};

test('object pages only expose addresses, type and locked state, with stable paging', () => {
  const nodes = Object.freeze(
    Array.from({ length: 34 }, (_, index) =>
      Object.freeze({ id: 'n' + index, type: 'Text', text: 'private text', path: '/private' }),
    ),
  );
  const document = Object.freeze({
    path: '/private-file',
    layers: Object.freeze([Object.freeze({ id: 'l1', name: 'private name', locked: true, nodes })]),
  });
  const host = {
    readTarget: () => ({ ...target, path: '/private' }),
    readDocument: () => document,
  };
  const get = (layerIndex, offset) => readLiveEditorObjects(host, { target, layerIndex, offset });
  assert.deepEqual(get(null, 0), {
    status: 'ok',
    target,
    layerId: null,
    items: [{ layerIndex: 0, layerId: 'l1', locked: true }],
    nextOffset: null,
  });
  const first = get(0, 0);
  assert.equal(first.items.length, LIVE_OBJECT_PAGE_SIZE);
  assert.equal(first.nextOffset, 32);
  assert.deepEqual(first.items[0], { nodeId: 'n0', type: 'Text' });
  assert.deepEqual(get(0, 32), {
    status: 'ok',
    target,
    layerId: 'l1',
    items: [
      { nodeId: 'n32', type: 'Text' },
      { nodeId: 'n33', type: 'Text' },
    ],
    nextOffset: null,
  });
  assert.deepEqual(get(0, 34).items, []);
  assert.deepEqual(get(0, 35), { status: 'invalid-arguments' });
  assert.deepEqual(get(1, 0), { status: 'unsupported-target' });
});

test('stale target never reads a newer document or returns a stale page', () => {
  const args = { target, layerIndex: null, offset: 0 };
  assert.deepEqual(
    readLiveEditorObjects(
      {
        readTarget: () => ({ ...target, documentGeneration: 2 }),
        readDocument: () => assert.fail('must not read'),
      },
      args,
    ),
    { status: 'stale-target' },
  );
  let revision = 3;
  assert.deepEqual(
    readLiveEditorObjects(
      {
        readTarget: () => ({ ...target, projectRevision: revision }),
        readDocument: () => {
          revision++;
          return { layers: [{ id: 'not-returned' }] };
        },
      },
      args,
    ),
    { status: 'stale-target' },
  );
});
