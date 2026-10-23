import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
  pathBounds,
} from '../src/core.js';
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-7, `${a} != ${b}`));
function fixture({ absolute = false, scale = -2, width = 3, skew = 31 } = {}) {
  const doc = document(),
    l = doc.layers[0],
    r = node('Rectangle'),
    t = node('Transform'),
    o = node('Output');
  l.nodes = [r, t, o];
  l.output = o.id;
  connect(l, r.id, t.id, 'path');
  connect(l, t.id, o.id, 'path');
  t.params.anchor.value = [13, 27];
  t.params.offset.value = [45, -16];
  t.params.rotation.value = 73;
  t.params.scale.value = scale;
  const param = (type, value) => ({ type, value, locked: false, expression: '' });
  t.params.uniformScale = param('Boolean', false);
  t.params.scaleWidth = param('Scalar', width);
  t.params.skew = param('Scalar', skew);
  t.params.skewAxis = param('Scalar', 21);
  if (absolute) t.params.position = param('Vector2', [170, -90]);
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: l.id, nodeId: t.id, ...payload },
    });
  const state = (d = session.snapshot()) => evaluate(d.layers[0]).transformState(t.id);
  return { doc, l, r, t, session, send, state };
}
for (const absolute of [false, true])
  for (const scale of [-2, 0, 1e-10, 2])
    test(`nine presets preserve affine, input and Offset (absolute=${absolute}, scale=${scale})`, () => {
      const f = fixture({ absolute, scale }),
        before = f.session.snapshot(),
        original = f.state();
      for (let preset = 0; preset < 9; preset++) {
        f.send('reposition-transform-anchor', { preset });
        const next = f.state(),
          b = pathBounds(original.input);
        near(next.matrix, original.matrix);
        near(next.anchor, [b[0][preset % 3], b[1][Math.floor(preset / 3)]]);
        assert.deepEqual(next.offset, original.offset);
        assert.deepEqual(f.session.snapshot().layers[0].nodes[0], before.layers[0].nodes[0]);
        near(f.state(deserialize(serialize(f.session.snapshot()))).matrix, original.matrix);
      }
      const pivot = f.state().pivot,
        anchor = f.state().anchor;
      f.send('set-transform-control', { key: 'rotation', value: 19 });
      near(f.state().pivot, pivot);
      near(f.state().anchor, anchor);
    });
test('output-space drag preserves geometry, one undo, cancel and exact out-and-back', () => {
  const f = fixture({ absolute: true }),
    before = f.session.snapshot(),
    original = f.state();
  const begin = () =>
    f.send('begin-gesture', {
      target: { kind: 'transform-anchor', layerId: f.l.id, nodeId: f.t.id },
    });
  let g = begin();
  for (const delta of [
    [10, 4],
    [30, -40],
  ])
    g = f.send('update-gesture', { ...g, update: { delta } });
  const preview = f.session.previewSnapshot(g.gestureId).document;
  near(f.state(preview).matrix, original.matrix);
  near(
    f.state(preview).pivot,
    original.pivot.map((v, i) => v + [30, -40][i]),
  );
  assert.deepEqual(f.session.snapshot(), before);
  f.send('commit-gesture', g);
  const after = f.session.snapshot();
  f.send('undo');
  assert.deepEqual(f.session.snapshot(), before);
  f.send('redo');
  assert.deepEqual(f.session.snapshot(), after);
  g = begin();
  g = f.send('update-gesture', { ...g, update: { delta: [22, 12] } });
  f.send('cancel-gesture', g);
  assert.deepEqual(f.session.snapshot(), after);
  const rev = f.session.state.expectedRevision;
  g = begin();
  g = f.send('update-gesture', { ...g, update: { delta: [22, 12] } });
  g = f.send('update-gesture', { ...g, update: { delta: [0, 0] } });
  const result = f.send('commit-gesture', g);
  assert.equal(result.changed, false);
  assert.deepEqual(f.session.snapshot(), after);
  assert.equal(f.session.state.expectedRevision, rev);
});
test('atomic refusals for locks, drivers, bypass, missing input, singular drag and invalid values', () => {
  for (const key of ['anchor', 'position'])
    for (const kind of ['locked', 'expression', 'source']) {
      const f = fixture({ absolute: true });
      if (kind === 'source') {
        const v = node('Vector');
        f.l.nodes.push(v);
        connect(f.l, v.id, f.t.id, key);
      } else f.t.params[key][kind] = kind === 'locked' ? true : '1;2';
      const session = new CommandSession(f.doc),
        before = serialize(session.snapshot());
      assert.throws(
        () =>
          session.dispatch({
            ...session.state,
            type: 'reposition-transform-anchor',
            payload: { layerId: f.l.id, nodeId: f.t.id, preset: 4 },
          }),
        /unlocked and undriven/,
      );
      assert.equal(serialize(session.snapshot()), before);
    }
  for (const mutate of [
    (f) => (f.l.locked = true),
    (f) => (f.t.bypassed = true),
    (f) => delete f.t.params.path.source,
  ]) {
    const f = fixture();
    mutate(f);
    const s = new CommandSession(f.doc),
      before = serialize(s.snapshot());
    assert.throws(() =>
      s.dispatch({
        ...s.state,
        type: 'reposition-transform-anchor',
        payload: { layerId: f.l.id, nodeId: f.t.id, preset: 4 },
      }),
    );
    assert.equal(serialize(s.snapshot()), before);
  }
  const f = fixture({ scale: 0 });
  assert.throws(() => f.send('reposition-transform-anchor', { delta: [1, 2] }), /Collapsed/);
  assert.throws(() => f.send('reposition-transform-anchor', { anchor: [NaN, 1] }), /finite/);
  assert.throws(() => f.send('reposition-transform-anchor', { preset: 9 }), /Unknown/);
  f.send('reposition-transform-anchor', { preset: 4 });
});
