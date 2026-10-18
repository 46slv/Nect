import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { CommandSession, document, node } from '../src/core.js';
import { parseFont } from '../src/fonts.js';

function fixture({ omitWidth = false } = {}) {
  const doc = document(),
    layer = doc.layers[0],
    a = layer.nodes[0],
    b = layer.nodes[1];
  if (omitWidth) delete a.width;
  const session = new CommandSession(doc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const begin = (target = { kind: 'graph-translate', layerId: layer.id, nodeIds: [a.id, b.id] }) =>
    send('begin-gesture', { target });
  return { session, send, begin, layer, a, b };
}

test('private gesture preview is detached; absolute baseline updates commit once with stable targets', () => {
  const { session: s, send, begin, a, b } = fixture(),
    initial = s.snapshot();
  let token = begin();
  token = send('update-gesture', { ...token, update: { delta: [50, 20] } });
  assert.deepEqual(s.snapshot(), initial);
  const preview = s.previewSnapshot(token.gestureId);
  assert.equal(preview.document.layers[0].nodes[0].position[0], a.position[0] + 50);
  preview.document.layers[0].nodes[0].position[0] = 9000;
  assert.notEqual(s.previewSnapshot(token.gestureId).document.layers[0].nodes[0].position[0], 9000);
  token = send('update-gesture', { ...token, update: { delta: [60, 30], nodeIds: ['foreign'] } });
  const final = s.previewSnapshot(token.gestureId).document;
  send('commit-gesture', token);
  assert.deepEqual(s.snapshot(), final);
  assert.equal(s.snapshot().layers[0].nodes[1].position[0], b.position[0] + 60);
  assert.equal(s.historyView.past.length, 1);
  assert.equal(s.hasGesture, false);
  send('undo');
  assert.deepEqual(s.snapshot(), initial);
  send('redo');
  assert.deepEqual(s.snapshot(), final);
  assert.throws(() => send('commit-gesture', token), /STALE_GESTURE/);
});

test('invalid/stale previews retain last candidate; cancellation never restores over foreign committed changes', () => {
  const { session: s, send, begin } = fixture();
  const first = begin(),
    token = send('update-gesture', { ...first, update: { delta: [5, 8] } });
  const preview = s.previewSnapshot(token.gestureId);
  assert.throws(
    () => send('update-gesture', { ...first, update: { delta: [7, 9] } }),
    /STALE_GESTURE/,
  );
  assert.throws(() => send('update-gesture', { ...token, update: { delta: [Infinity, 9] } }));
  assert.deepEqual(s.previewSnapshot(token.gestureId), preview);
  assert.throws(() => {
    s.historyView.doc.name = 'Foreign write';
  }, TypeError);
  const detached = s.snapshot();
  detached.name = 'Detached write';
  assert.notEqual(s.snapshot().name, detached.name);
  send('cancel-gesture', token);
  const next = begin();
  s.dispatch({ ...s.state, expectedRevision: -1, type: 'cancel-gesture', payload: next });
  assert.equal(s.hasGesture, false);
});

test('gesture excludes authored commands and text staging; returned-to-origin and cancel preserve redo', () => {
  const { session: s, send, begin, layer, a } = fixture();
  const text = send('create-node', { layerId: layer.id, nodeType: 'Text' });
  send('rename', { layerId: layer.id, name: 'Next' });
  send('undo');
  const revision = s.state.expectedRevision,
    before = s.snapshot();
  const draft = send('begin-text-draft', { layerId: layer.id, nodeId: text.nodeId });
  let token = begin();
  for (const [type, payload] of [
    ['undo', {}],
    ['begin-gesture', { target: { kind: 'graph-width', layerId: layer.id, nodeId: a.id } }],
    ['begin-text-draft', { layerId: layer.id, nodeId: text.nodeId }],
    ['stage-text-draft', { draftId: draft.draftId }],
  ])
    assert.throws(() => send(type, payload), /GESTURE_ACTIVE/);
  token = send('update-gesture', { ...token, update: { delta: [12, 15] } });
  token = send('update-gesture', { ...token, update: { delta: [0, 0] } });
  send('commit-gesture', token);
  assert.equal(s.state.expectedRevision, revision);
  send('redo');
  assert.equal(s.snapshot().layers[0].name, 'Next');
  send('undo');
  const cancel = begin();
  send('cancel-gesture', cancel);
  assert.equal(s.historyView.future.length, 1);
  assert.deepEqual(s.snapshot(), before);
});

test('graph width validates locked/missing targets, preserves absent defaults and rejects foreign tokens', () => {
  const { session: s, send, begin, layer, a } = fixture({ omitWidth: true });
  let token = begin({ kind: 'graph-width', layerId: layer.id, nodeId: a.id });
  const originalDocument = s.historyView.doc;
  token = send('update-gesture', { ...token, update: { width: 400 } });
  assert.throws(() => send('update-gesture', { ...token, update: { width: 600 } }));
  token = send('update-gesture', { ...token, update: { width: 264 } });
  assert.throws(
    () => s.dispatch({ ...s.state, sessionId: 'foreign', type: 'cancel-gesture', payload: token }),
    /STALE_SESSION/,
  );
  send('commit-gesture', token);
  assert.equal(s.state.expectedRevision, 0);
  assert.equal(
    s.historyView.doc,
    originalDocument,
    'no-op preserves UI selection-history identity',
  );
  assert.equal(Object.hasOwn(s.snapshot().layers[0].nodes[0], 'width'), false);
  send('set-layer-state', { layerId: layer.id, key: 'locked', value: true });
  assert.throws(() => begin(), /locked/);
  assert.equal(s.hasGesture, false);
});

test('parameter gestures reuse discrete type/domain validation and retain fallback plus redo on failure', () => {
  const { session: s, send, begin, layer, a } = fixture();
  const before = s.snapshot();
  let token = begin({ kind: 'parameter', layerId: layer.id, nodeId: a.id, key: 'radius' });
  token = send('update-gesture', { ...token, update: { value: 180 } });
  assert.equal(s.snapshot().layers[0].nodes[0].params.radius.value, 120);
  assert.equal(
    s.previewSnapshot(token.gestureId).document.layers[0].nodes[0].params.radius.value,
    180,
  );
  const accepted = s.previewSnapshot(token.gestureId);
  assert.throws(() => send('update-gesture', { ...token, update: { value: Infinity } }));
  assert.deepEqual(s.previewSnapshot(token.gestureId), accepted);
  send('commit-gesture', token);
  send('undo');
  assert.deepEqual(s.snapshot(), before);
  token = begin({ kind: 'parameter', layerId: layer.id, nodeId: a.id, key: 'radius' });
  send('commit-gesture', token);
  send('redo');
  assert.equal(s.snapshot().layers[0].nodes[0].params.radius.value, 180);
});

test('Text previews own maps and restore sparse inheritance at origin without consuming redo', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    font = parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    doc = document(),
    text = node('Text'),
    layer = doc.layers[0];
  text.text = 'ABA';
  layer.nodes.push(text);
  const s = new CommandSession(doc, { fonts: () => ({ default: font }) }),
    send = (type, payload) => s.dispatch({ ...s.state, type, payload }),
    initial = s.snapshot();
  let token = send('begin-gesture', {
    target: {
      kind: 'text-values',
      layerId: layer.id,
      nodeId: text.id,
      textKind: 'offset',
      ids: ['c1', 'c2'],
      axis: 0,
    },
  });
  token = send('update-gesture', { ...token, update: { values: [12, 22] } });
  const preview = s.previewSnapshot(token.gestureId).document;
  assert.deepEqual(s.snapshot(), initial);
  assert.deepEqual(preview.layers[0].nodes.at(-1).characterPositions.offsets, {
    c1: [12, 0],
    c2: [22, 0],
  });
  assert.throws(() => send('update-gesture', { ...token, update: { values: [12, Infinity] } }));
  assert.deepEqual(s.previewSnapshot(token.gestureId).document, preview);
  send('commit-gesture', token);
  send('undo');
  const state = s.state;
  token = send('begin-gesture', {
    target: {
      kind: 'text-values',
      layerId: layer.id,
      nodeId: text.id,
      textKind: 'style',
      ids: ['c1', 'c2'],
      key: 'size',
    },
  });
  token = send('update-gesture', { ...token, update: { values: [110, 110] } });
  token = send('update-gesture', { ...token, update: { values: [100, 100] } });
  send('commit-gesture', token);
  assert.deepEqual(s.state, state);
  assert.deepEqual(s.snapshot(), initial);
  send('redo');
  assert.deepEqual(s.snapshot(), preview);
});
