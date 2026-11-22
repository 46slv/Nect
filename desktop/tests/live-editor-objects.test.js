import assert from 'node:assert/strict';
import test from 'node:test';
import { readFileSync } from 'node:fs';
import { deserialize } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  deserializeCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import {
  LIVE_OBJECT_PAGE_SIZE,
  readLiveEditorObjects,
  readLiveNodeInputWires,
} from '../src/live-editor-objects.js';

const target = {
  documentGeneration: 1,
  compositionId: 'composition',
  sessionId: 'session',
  sessionRevision: 2,
  projectRevision: 3,
};

test('object pages only expose addresses, type and layer state, with stable paging', () => {
  const nodes = Object.freeze(
    Array.from({ length: 34 }, (_, index) =>
      Object.freeze({ id: 'n' + index, type: 'Text', text: 'private text', path: '/private' }),
    ),
  );
  const document = Object.freeze({
    path: '/private-file',
    layers: Object.freeze([
      Object.freeze({ id: 'l1', name: 'private name', locked: true, visible: false, nodes }),
    ]),
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
    items: [{ layerIndex: 0, layerId: 'l1', locked: true, visible: false }],
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

test('wide Unicode and escaped addresses paginate within the native byte frame', () => {
  const nodes = Array.from({ length: 10 }, (_, index) => ({
    id: '\u63db'.repeat(248) + '\n' + index,
    type: 'Text',
  }));
  const host = { readTarget: () => target, readDocument: () => ({ layers: [{ id: 'l', nodes }] }) };
  const found = [];
  let offset = 0;
  do {
    const page = readLiveEditorObjects(host, { target, layerIndex: 0, offset });
    assert.equal(page.status, 'ok');
    assert.ok(Buffer.byteLength(JSON.stringify(page)) <= 3500);
    assert.ok(page.items.length < nodes.length);
    found.push(...page.items.map((item) => item.nodeId));
    if (page.nextOffset !== null) assert.ok(page.nextOffset > offset);
    offset = page.nextOffset;
  } while (offset !== null);
  assert.deepEqual(
    found,
    nodes.map((node) => node.id),
  );
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

test('node wire discovery projects only the explicitly addressed input names and sources', () => {
  const document = {
    layers: [
      { id: 'other', nodes: [{ id: 'selected', params: { private: { source: 'not-mine' } } }] },
      {
        id: 'layer',
        nodes: [
          { id: 'a' },
          { id: 'b' },
          {
            id: 'selected',
            params: {
              first: { source: 'a', value: 'not-returned' },
              second: { source: 'b', sourcePort: 'position' },
              expression: { expression: 'not-returned' },
              literal: { value: '/not-returned' },
            },
            reference: { nodeId: 'another-reference' },
          },
        ],
      },
    ],
  };
  const before = structuredClone(document);
  const host = { readTarget: () => target, readDocument: () => document };
  const args = { target, layerId: 'layer', nodeId: 'selected', offset: 0 };
  assert.deepEqual(readLiveNodeInputWires(host, args), {
    status: 'ok',
    target,
    layerId: 'layer',
    nodeId: 'selected',
    items: [
      { input: 'first', sourceId: 'a', sourcePort: null },
      { input: 'second', sourceId: 'b', sourcePort: 'position' },
    ],
    nextOffset: null,
  });
  assert.deepEqual(document, before);
  assert.deepEqual(readLiveNodeInputWires(host, { ...args, offset: 2 }).items, []);
  assert.deepEqual(readLiveNodeInputWires(host, { ...args, offset: 3 }), {
    status: 'invalid-arguments',
  });
  assert.deepEqual(readLiveNodeInputWires(host, { ...args, nodeId: 'absent' }), {
    status: 'unsupported-target',
  });
});

test('node wire discovery has bounded pages and rechecks the target before publishing', () => {
  const params = Object.fromEntries(
    Array.from({ length: 45 }, (_, index) => [
      'input' + index,
      { source: '\u63db'.repeat(245) + '\n' + index },
    ]),
  );
  const document = { layers: [{ id: 'layer', nodes: [{ id: 'node', params }] }] };
  let revision = target.projectRevision;
  const host = {
    readTarget: () => ({ ...target, projectRevision: revision }),
    readDocument: () => document,
  };
  const args = { target, layerId: 'layer', nodeId: 'node', offset: 0 };
  const found = [];
  do {
    const page = readLiveNodeInputWires(host, args);
    assert.equal(page.status, 'ok');
    assert.ok(page.items.length > 0 && Buffer.byteLength(JSON.stringify(page)) <= 3500);
    if (page.nextOffset !== null) assert.ok(page.nextOffset > args.offset);
    found.push(...page.items.map((item) => item.input));
    args.offset = page.nextOffset;
  } while (args.offset !== null);
  assert.deepEqual(found, Object.keys(params));
  assert.deepEqual(
    readLiveNodeInputWires(
      {
        ...host,
        readDocument: () => {
          revision++;
          return document;
        },
      },
      { ...args, offset: 0 },
    ),
    { status: 'stale-target' },
  );
});

test('authored wire discovery follows GUI history for a real named Text output', () => {
  const seed = readFileSync(
    new URL('../examples/text-position-reuse.json', import.meta.url),
    'utf8',
  );
  const live = createCompositionSessionOwner(deserializeCompositionProject(seed, deserialize));
  const readTarget = () => ({
    documentGeneration: 1,
    compositionId: live.activeCompositionId,
    sessionId: live.state.sessionId,
    sessionRevision: live.state.expectedRevision,
    projectRevision: live.projectRevision,
  });
  const host = { readTarget, readDocument: () => live.snapshot() };
  const handler = createLiveEditorProtocol({
    readTarget,
    readNodeInputWires: (args) => readLiveNodeInputWires(host, args),
  });
  const layer = live.snapshot().layers[0];
  const address = { layerId: layer.id, nodeId: 'text-xy', offset: 0 };
  const inspect = (target = readTarget()) =>
    handler({ version: 1, operation: 'graph.inputWires', arguments: { ...address, target } });
  const before = readTarget();
  const serialize = () => serializeCompositionProject(live.projectSnapshot());
  const original = serialize();
  const edges = [{ input: 'position', sourceId: 'text-master', sourcePort: 'position' }];
  assert.deepEqual(inspect().items, edges);
  assert.deepEqual(readTarget(), before);
  assert.equal(serialize(), original);
  const { offset, ...nodeAddress } = address;
  assert.equal(
    live.dispatch({
      ...live.state,
      type: 'disconnect',
      payload: { ...nodeAddress, key: 'position' },
    }).changed,
    true,
  );
  assert.deepEqual(inspect(before), { status: 'stale-target' });
  assert.deepEqual(inspect().items, []);
  assert.equal(live.dispatch({ ...live.state, type: 'undo' }).changed, true);
  assert.deepEqual(inspect().items, edges);
  assert.equal(serialize(), original);
});
