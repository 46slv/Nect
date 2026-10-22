import test from 'node:test';
import assert from 'node:assert/strict';
import {
  angleDialIndicator,
  angleDialPointer,
  advanceAngleDial,
} from '../src/repeater-angle-dial.js';
import { CommandSession, document, node, evaluate } from '../src/core.js';

function fixture(value = 725, driven = false) {
  const doc = document(),
    layer = doc.layers[0],
    repeat = node('Geometry Repeater'),
    source = node('Value');
  repeat.operation = 'ae-2d';
  doc.schemaVersion = 37;
  repeat.params.rotation.value = value;
  source.params.value.value = 40;
  if (driven) {
    repeat.params.rotation.expression = 'value * 2';
    if (driven === 'connected') repeat.params.rotation.source = source.id;
  }
  layer.nodes = [repeat, source, node('Output')];
  layer.output = layer.nodes.at(-1).id;
  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const target = {
    kind: 'parameter',
    layerId: layer.id,
    nodeId: repeat.id,
    key: 'rotation',
    authoredBase: true,
  };
  const q = () => session.snapshot().layers[0].nodes[0].params.rotation;
  return { session, send, target, q, layer, repeat };
}
function travel(angles) {
  return angles
    .slice(1)
    .reduce((state, angle) => advanceAngleDial(state, angle), { angle: angles[0], delta: 0 }).delta;
}

test('indicator only wraps; authored signed/unwrapped and tiny values are not rounded', () => {
  assert.equal(angleDialIndicator(725), 5);
  assert.equal(angleDialIndicator(735), 15);
  assert.equal(angleDialIndicator(-725), 355);
  assert.equal(angleDialIndicator(1e-16), 1e-16);
  assert.equal(angleDialIndicator(360 * 10000000 + 5), 5);
});
test('signed angular deltas cross seams and accumulate positive/negative full turns', () => {
  assert.equal(travel([170, -180, -170]), 20);
  assert.equal(travel([-170, 180, 170]), -20);
  assert.equal(travel([0, 90, 180, -90, 0, 90, 180, -90, 0]), 720);
  assert.equal(travel([0, -90, -180, 90, 0]), -360);
  assert.equal(travel([0, 90, 0, -90, 0]), 0);
});
test('center dead zone breaks angular continuity, never jumping on exit', () => {
  const rect = { left: 100, top: 200, width: 36, height: 36 };
  assert.equal(angleDialPointer(rect, [118, 218]), null);
  assert.equal(angleDialPointer(rect, [121, 218]), null);
  assert.equal(angleDialPointer(rect, [130, 218]), 0);
  assert.equal(angleDialPointer(rect, [118, 230]), 90);
  assert.equal(travel([0, null, 180, -170]), 10);
});
test('725→735 previews one Scalar, commits one Undo/Redo and preserves exact starting fractions', () => {
  const { session, send, target, q } = fixture();
  let token = send('begin-gesture', { target });
  token = send('update-gesture', { ...token, update: { value: 725 + travel([170, 180]) } });
  assert.equal(q().value, 725);
  assert.equal(
    session.previewSnapshot(token.gestureId).document.layers[0].nodes[0].params.rotation.value,
    735,
  );
  send('commit-gesture', token);
  assert.equal(q().value, 735);
  assert.equal(session.historyView.past.length, 1);
  assert.equal(angleDialIndicator(q().value), 15);
  send('undo');
  assert.equal(q().value, 725);
  send('redo');
  assert.equal(q().value, 735);
  const tiny = fixture(0.000000000123456789);
  token = tiny.send('begin-gesture', { target: tiny.target });
  token = tiny.send('update-gesture', {
    ...token,
    update: { value: tiny.q().value + travel([0, 0]) },
  });
  tiny.send('commit-gesture', token);
  assert.equal(tiny.q().value, 0.000000000123456789);
  assert.equal(tiny.session.historyView.past.length, 0);
});
test('cancel/no-op retain revision and existing redo; stale and invalid updates never commit', () => {
  const { session, send, target, q } = fixture();
  send('set-parameter-base', { ...target, value: 800 });
  send('undo');
  const state = session.state,
    history = session.historyView;
  const start = send('begin-gesture', { target });
  let token = send('update-gesture', { ...start, update: { value: 1095 } });
  assert.throws(
    () => send('update-gesture', { ...start, update: { value: 900 } }),
    /STALE_GESTURE/,
  );
  assert.throws(() => send('update-gesture', { ...token, update: { value: NaN } }), /finite/);
  assert.throws(() => send('update-gesture', { ...token, update: { value: Infinity } }), /finite/);
  send('cancel-gesture', token);
  assert.equal(q().value, 725);
  assert.deepEqual(session.state, state);
  assert.deepEqual(session.historyView, history);
  send('redo');
  assert.equal(q().value, 800);
  assert.throws(
    () => send('begin-gesture', { target: { ...target, nodeId: 'deleted' } }),
    /Missing/,
  );
  assert.equal(session.hasGesture, false);
});
for (const driven of ['expression', 'connected'])
  test(`${driven} Base gesture preserves source, expression and effective priority`, () => {
    const { session, send, target, q, layer, repeat } = fixture(725, driven),
      original = q();
    let token = send('begin-gesture', { target });
    token = send('update-gesture', { ...token, update: { value: 735 } });
    send('commit-gesture', token);
    assert.deepEqual(q(), { ...original, value: 735 });
    const doc = session.snapshot();
    assert.equal(
      evaluate(doc.layers[0], {}, {}, doc).parameterValue(repeat.id, 'rotation'),
      driven === 'connected' ? 40 : 1470,
    );
    send('undo');
    assert.deepEqual(q(), original);
    assert.throws(
      () => send('begin-gesture', { target: { ...target, authoredBase: false } }),
      /driven/,
    );
    send('set-parameter-lock', {
      layerId: layer.id,
      nodeId: repeat.id,
      key: 'rotation',
      locked: true,
    });
    assert.throws(() => send('begin-gesture', { target }), /locked/);
  });
