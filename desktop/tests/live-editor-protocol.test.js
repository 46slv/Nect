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

test('document copy is opt-in and accepts only the exact target', async () => {
  const config = { readTarget: () => target };
  const packet = request('document.saveCopy', { target });
  assert.deepEqual(createLiveEditorProtocol(config)(packet), { status: 'unsupported-operation' });
  let writes = 0;
  const saveCopy = (selected) => {
    assert.deepEqual(selected, target);
    writes++;
    return { status: 'accepted-by-writer', internal: 'never forwarded' };
  };
  const handle = createLiveEditorProtocol({ ...config, saveCopy });
  assert.deepEqual(handle(request('document.saveCopy', { target, path: 'another.json' })),
    { status: 'invalid-arguments' });
  assert.deepEqual(await handle(packet), { status: 'accepted-by-writer' });
  assert.equal(writes, 1);
});

test('PNG routing does not accept caller-supplied paths or raster options', async () => {
  let writes = 0;
  const handle = createLiveEditorProtocol({
    readTarget: () => target,
    writeSvg: () => assert.fail('wrong format'),
    writePng: (selected) => {
      assert.deepEqual(selected, target);
      writes++;
      return { status: 'accepted-by-writer' };
    },
  });
  const exportPng = (args) => handle(request('artwork.exportPng', args));
  assert.deepEqual(exportPng({ target, scale: 99 }), { status: 'invalid-arguments' });
  assert.deepEqual(exportPng({ target, path: 'arbitrary.png' }), { status: 'invalid-arguments' });
  assert.deepEqual(await exportPng({ target }), { status: 'accepted-by-writer' });
  assert.equal(writes, 1);
});
