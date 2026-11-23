import assert from 'node:assert/strict';
import test from 'node:test';
import { createLiveEditorProtocol } from '../src/live-editor-protocol.js';
import { readLiveLayerState } from '../src/live-editor-objects.js';

const target = {
  documentGeneration: 1,
  compositionId: 'comp',
  sessionId: 'session',
  sessionRevision: 2,
  projectRevision: 3,
};
const args = { target, layerId: 'layer-a' };
const request = (payload = args) => ({ version: 1, operation: 'layer.state', arguments: payload });
const expected = {
  status: 'ok',
  target,
  layerId: 'layer-a',
  visible: false,
  locked: true,
  guide: true,
  solo: true,
};

test('layer state reads stored flags only, without revealing metadata or writing', () => {
  const layer = {
    id: args.layerId,
    visible: false,
    locked: true,
    guide: true,
    solo: true,
    name: 'private',
    nodes: [],
  };
  const doc = { layers: [layer] };
  const before = structuredClone(doc);
  const readTarget = () => target;
  const handle = createLiveEditorProtocol({
    readTarget,
    readLayerState: (input) => readLiveLayerState({ readTarget, readDocument: () => doc }, input),
  });
  assert.deepEqual(handle(request()), expected);
  assert.deepEqual(doc, before);
  assert.equal(handle(request({ ...args, includeNames: true })).status, 'invalid-arguments');
  assert.equal(handle(request({ ...args, layerId: 'missing' })).status, 'unsupported-target');
  assert.equal(createLiveEditorProtocol({ readTarget })(request()).status, 'unsupported-operation');
  doc.layers.push(layer);
  assert.equal(handle(request()).status, 'unavailable');
  doc.layers = [{ id: args.layerId }];
  assert.deepEqual(handle(request()), {
    ...expected,
    visible: true,
    locked: false,
    guide: false,
    solo: false,
  });
});

test('layer state fails closed before and during a document change', () => {
  let active = { ...target, projectRevision: 4 };
  const readTarget = () => active;
  const withDoc = (readDocument) => readLiveLayerState({ readTarget, readDocument }, args);
  assert.deepEqual(
    withDoc(() => assert.fail('stale request must not read')),
    { status: 'stale-target' },
  );
  active = target;
  assert.deepEqual(
    withDoc(() => null),
    { status: 'unavailable' },
  );
  assert.deepEqual(
    withDoc(() => {
      active = { ...target, documentGeneration: 2 };
      return { layers: [{ id: args.layerId }] };
    }),
    { status: 'stale-target' },
  );
});
