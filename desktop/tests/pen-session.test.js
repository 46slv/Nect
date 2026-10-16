import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document } from '../src/core.js';

function fixture() {
  const s = new CommandSession(document());
  const send = (type, payload) => s.dispatch({ ...s.state, type, payload });
  const begin = () => send('begin-gesture', { target: { kind: 'pen' }, position: [100, 100] });
  return { s, send, begin };
}

test('Pen keeps stable identities private, point rollback preserves released points, finish commits once', () => {
  const { s, send, begin } = fixture(),
    before = s.snapshot();
  let token = begin();
  const first = token.pointId,
    layerId = token.layerId;
  token = send('accept-pen-point', token);
  token = send('append-pen-point', { ...token, position: [200, 150] });
  const second = token.pointId;
  token = send('update-pen-point', { ...token, delta: [30, 20] });
  const preview = s.previewSnapshot(token.gestureId).document;
  const curve = preview.layers.at(-1).nodes[0];
  assert.deepEqual(curve.points[1].handleA.value, [-30, -20]);
  curve.points[0].id = 'foreign';
  token = send('accept-pen-point', token);
  token = send('append-pen-point', { ...token, position: [300, 180] });
  token = send('update-pen-point', { ...token, delta: [10, 5] });
  token = send('cancel-pen-point', token);
  assert.deepEqual(s.snapshot(), before);
  assert.deepEqual(
    s
      .previewSnapshot(token.gestureId)
      .document.layers.at(-1)
      .nodes[0].points.map((p) => p.id),
    [first, second],
  );
  assert.throws(() => send('commit-pen', { ...token, closed: 'yes' }), /open or closed/);
  assert.equal(s.gestureState.gestureId, token.gestureId);
  const done = send('commit-pen', { ...token, closed: false });
  assert.equal(done.layerId, layerId);
  assert.equal(s.legacyHistory.past.length, 1);
  assert.equal(s.snapshot().layers.at(-1).nodes[0].params.closed.value, false);
  send('undo');
  assert.deepEqual(s.snapshot(), before);
  send('redo');
  assert.deepEqual(
    s
      .snapshot()
      .layers.at(-1)
      .nodes[0].points.map((p) => p.id),
    [first, second],
  );
});

test('first pressed point and whole path cancellation preserve redo; foreign/stale commands cannot install drafts', () => {
  const { s, send, begin } = fixture();
  send('rename', { layerId: s.snapshot().layers[0].id, name: 'Next' });
  send('undo');
  const original = s.legacyHistory.doc;
  let token = begin();
  const initial = token;
  token = send('update-pen-point', { ...token, delta: [5, 6] });
  assert.throws(() => send('accept-pen-point', initial), /STALE_GESTURE/);
  assert.throws(() => send('undo'), /GESTURE_ACTIVE/);
  assert.equal(send('cancel-pen-point', token).discarded, true);
  assert.equal(s.hasGesture, false);
  assert.equal(s.legacyHistory.doc, original);
  token = begin();
  token = send('accept-pen-point', token);
  assert.equal(send('commit-pen', { ...token, closed: false }).discarded, true);
  token = begin();
  token = send('accept-pen-point', token);
  token = send('append-pen-point', { ...token, position: [200, 200] });
  s.legacyHistory.doc.name = 'Newer legacy work';
  assert.throws(() => send('accept-pen-point', token), /STALE_GESTURE/);
  send('cancel-gesture', token);
  assert.equal(s.snapshot().name, 'Newer legacy work');
  assert.equal(s.legacyHistory.future.length, 1);
  assert.equal(s.snapshot().layers.length, 1);
});
