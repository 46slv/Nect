import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveDocumentReopen } from '../src/live-document-reopen.js';

const original = {
  documentGeneration: 1,
  compositionId: 'a',
  sessionId: 's',
  sessionRevision: 2,
  projectRevision: 3,
};

test('reopen only loads the exact saved snapshot with the current target and no intervening draft', async () => {
  let observed = original,
    blocked = false,
    saved = null,
    onRead = () => {},
    reads = 0;
  const loaded = [];
  const reopen = createLiveDocumentReopen({
    readTarget: () => observed,
    readDocument: () => 'current project',
    isBlocked: () => blocked,
    readCopy: async () => {
      reads++;
      onRead();
      return saved;
    },
    loadCopy: (contents) => loaded.push(contents),
  });
  assert.equal((await reopen({ ...original, projectRevision: 2 })).status, 'stale-target');
  assert.equal(reads, 0);
  assert.equal((await reopen(original)).status, 'copy-not-found');
  saved = 'another project';
  assert.equal((await reopen(original)).status, 'copy-does-not-match');
  saved = 'current project';
  onRead = () => {
    observed = { ...original, projectRevision: 4 };
  };
  assert.equal((await reopen(original)).status, 'stale-target');
  observed = original;
  onRead = () => {
    blocked = true;
  };
  assert.equal((await reopen(original)).status, 'edit-in-progress');
  assert.deepEqual(loaded, []);
  blocked = false;
  onRead = () => {};
  assert.equal((await reopen(original)).status, 'reopened');
  assert.deepEqual(loaded, ['current project']);
});
