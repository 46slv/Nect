import assert from 'node:assert/strict';
import test from 'node:test';
import { readLiveLayerFolders } from '../src/live-editor-objects.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';

const target = {
  documentGeneration: 1,
  compositionId: 'composition',
  sessionId: 'session',
  sessionRevision: 2,
  projectRevision: 3,
};
const make = (layers, folders) => ({
  layers: layers.map((id) => ({ id })),
  editor: { layerFolders: folders },
});
const inspect = (doc, offset = 0) =>
  readLiveLayerFolders({ readTarget: () => target, readDocument: () => doc }, { target, offset });

test('folder topology reads direct ownership and ancestry without names or changes', () => {
  const doc = make(
    ['back', 'middle', 'front'],
    [
      { id: 'parent', name: 'private name', members: [{ layerId: 'back' }] },
      {
        id: 'child',
        name: 'another private name',
        parentId: 'parent',
        members: [{ layerId: 'middle' }],
      },
      { id: 'empty', name: 'empty name', members: [] },
    ],
  );
  const before = structuredClone(doc);
  const request = { version: 1, operation: 'layerFolders.list', arguments: { target, offset: 0 } };
  assert.equal(
    createLiveEditorProtocol({ readTarget: () => target })(request).status,
    'unsupported-operation',
  );
  const handle = createLiveEditorProtocol({
    readTarget: () => target,
    listLayerFolders: (args) =>
      readLiveLayerFolders({ readTarget: () => target, readDocument: () => doc }, args),
  });
  assert.deepEqual(handle(request), {
    status: 'ok',
    target,
    items: [
      { kind: 'folder', folderId: 'parent', parentFolderId: null },
      { kind: 'membership', folderId: 'parent', layerId: 'back' },
      { kind: 'folder', folderId: 'child', parentFolderId: 'parent' },
      { kind: 'membership', folderId: 'child', layerId: 'middle' },
      { kind: 'folder', folderId: 'empty', parentFolderId: null },
    ],
    nextOffset: null,
  });
  assert.deepEqual(inspect(doc, 5), { status: 'ok', target, items: [], nextOffset: null });
  assert.equal(inspect(doc, 6).status, 'invalid-arguments');
  assert.equal(
    handle({ ...request, arguments: { ...request.arguments, names: true } }).status,
    'invalid-arguments',
  );
  assert.deepEqual(doc, before);
});

test('folder pages respect escaped-byte size and return each member exactly once', () => {
  const ids = Array.from({ length: 35 }, (_, n) => '層'.repeat(245) + '"\\' + n);
  const folderId = '箱'.repeat(250);
  const doc = make(ids, [
    { id: folderId, name: 'safe', members: ids.map((layerId) => ({ layerId })) },
  ]);
  const rows = [];
  let offset = 0;
  while (offset !== null) {
    const page = inspect(doc, offset);
    assert.equal(page.status, 'ok');
    assert.ok(page.items.length && page.items.length < 32);
    assert.ok(Buffer.byteLength(JSON.stringify(page), 'utf8') <= 3500);
    rows.push(...page.items);
    assert.ok(page.nextOffset === null || page.nextOffset > offset);
    offset = page.nextOffset;
  }
  assert.deepEqual(rows.map((row) => row.layerId).filter(Boolean), ids);
  assert.equal(rows.filter((row) => row.kind === 'folder').length, 1);
});

test('folder topology rejects stale revisions and corrupt ancestry', () => {
  const doc = make(['a'], [{ id: 'folder', name: 'sample', members: [{ layerId: 'a' }] }]);
  const stale = { ...target, projectRevision: target.projectRevision - 1 };
  assert.equal(
    readLiveLayerFolders(
      {
        readTarget: () => target,
        readDocument: () => assert.fail('must not read a stale document'),
      },
      { target: stale, offset: 0 },
    ).status,
    'stale-target',
  );
  let observed = target;
  assert.equal(
    readLiveLayerFolders(
      {
        readTarget: () => observed,
        readDocument() {
          observed = { ...target, documentGeneration: 2 };
          return doc;
        },
      },
      { target, offset: 0 },
    ).status,
    'stale-target',
  );
  doc.editor.layerFolders[0].parentId = 'folder';
  assert.equal(inspect(doc).status, 'unavailable');
});
