import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveDocumentCopy } from '../src/live-document-copy.js';

test('a copy captures one current snapshot; stale, blocked and concurrent calls do not write', async () => {
  let target = {
    documentGeneration: 1,
    compositionId: 'composition',
    sessionId: 'session',
    sessionRevision: 2,
    projectRevision: 3,
  };
  let blocked = true;
  let snapshot = 'first';
  const writes = [];
  let complete;
  const copy = createLiveDocumentCopy({
    readTarget: () => target,
    readDocument: () => snapshot,
    isBlocked: () => blocked,
    writeCopy: (serialized) => {
      writes.push(serialized);
      return new Promise((resolve) => {
        complete = resolve;
      });
    },
  });
  assert.deepEqual(await copy({ ...target, documentGeneration: 0 }), { status: 'stale-target' });
  assert.deepEqual(await copy(target), { status: 'edit-in-progress' });
  blocked = false;
  const first = copy(target);
  snapshot = 'later';
  target = { ...target, projectRevision: 4 };
  assert.deepEqual(await copy(target), { status: 'busy' });
  complete(true);
  assert.deepEqual(await first, { status: 'accepted-by-writer' });
  assert.deepEqual(writes, ['first']);
});
