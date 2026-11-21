import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

const target = Object.freeze({
  documentGeneration: 1,
  compositionId: 'composition',
  sessionId: 'session',
  sessionRevision: 2,
  projectRevision: 3,
});
const request = (operation, args) => ({ version: 1, operation, arguments: args });
const edit = { target, layerId: 'layer', nodeId: 'node', value: 42 };

test('closed contract rejects malformed requests and unsupported commands without invoking the app', () => {
  const handle = createLiveEditorProtocol({
    readTarget: () => target,
    setBlendDistance: () => assert.fail('unexpected edit'),
  });
  assert.deepEqual(handle(null), { status: 'invalid-request' });
  assert.deepEqual(handle({ ...request('editor.inspect', {}), version: 2 }), {
    status: 'unsupported-version',
  });
  assert.deepEqual(handle(request('editor.inspect', { includeFilePath: true })), {
    status: 'invalid-arguments',
  });
  assert.deepEqual(handle(request('command.dispatch', edit)), { status: 'unsupported-operation' });
  for (const args of [
    { ...edit, path: 'not an API field' },
    { ...edit, target: { ...target, sessionRevision: '2' } },
    { ...edit, nodeId: '' },
    { ...edit, value: Infinity },
  ])
    assert.deepEqual(handle(request('blend.setDistance', args)), { status: 'invalid-arguments' });
});

test('inspect and successful edits return only the allowlisted editor target', () => {
  let revision = 2;
  const handle = createLiveEditorProtocol({
    readTarget: () => ({ ...target, sessionRevision: revision, extra: 'not public' }),
    setBlendDistance: (args) => {
      assert.deepEqual(args, edit);
      revision++;
      return { status: 'changed', receipt: { contents: 'not public' } };
    },
  });
  assert.deepEqual(handle(request('editor.inspect', {})), { status: 'ok', target });
  assert.deepEqual(handle(request('blend.setDistance', edit)), {
    status: 'changed',
    target: { ...target, sessionRevision: 3 },
  });
});

test('does not represent a stale edit or an ambiguous host failure as success', () => {
  let attempts = 0;
  const handle = createLiveEditorProtocol({
    readTarget: () => target,
    setBlendDistance: () => {
      if (++attempts === 1) return { status: 'stale-target' };
      throw Error('private host details');
    },
  });
  assert.deepEqual(handle(request('blend.setDistance', edit)), { status: 'stale-target' });
  assert.deepEqual(handle(request('blend.setDistance', edit)), { status: 'outcome-unknown' });
  assert.equal(attempts, 2);
});
