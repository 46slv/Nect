import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, document, node, connect, serialize, evaluate } from '../src/core.js';
function fixture(multi = false) {
  const doc = document(),
    l = doc.layers[0],
    curve = node('Curve'),
    extra = node('Value'),
    fill = node('Fill'),
    out = node('Output');
  curve.params.closed.value = false;
  curve.points.forEach((p, i) => {
    p.handleA.value = [-i - 3, i + 1];
    p.handleB.value = [i + 7, -i - 2];
    p.handleType = 'corner';
  });
  let source = curve;
  if (multi) {
    source = node('Multi Path');
    source.points = curve.points;
    source.contours = [
      { id: 'open-contour', closed: false, points: curve.points.slice(0, 2).map((p) => p.id) },
      { id: 'other-contour', closed: true, points: curve.points.slice(2).map((p) => p.id) },
    ];
    source.units = [{ id: 'part-unit', contours: source.contours.map((c) => c.id) }];
    doc.schemaVersion = 32;
  }
  l.nodes = [extra, source, fill, out];
  l.output = out.id;
  connect(l, source.id, fill.id, 'path');
  connect(l, fill.id, out.id, 'path');
  const s = new CommandSession(doc),
    send = (type, payload = {}) => s.dispatch({ ...s.state, type, payload }),
    ids = multi ? source.contours[0].points : source.points.map((p) => p.id);
  const begin = (side = 'tail') =>
    send('begin-gesture', {
      target: {
        kind: 'pen',
        layerId: l.id,
        nodeId: source.id,
        endpointId: side === 'head' ? ids[0] : ids.at(-1),
        ...(multi ? { contourId: 'open-contour' } : {}),
      },
    });
  return {
    doc,
    l,
    source,
    s,
    send,
    begin,
    ids,
    live: () => s.snapshot().layers[0].nodes.find((n) => n.id === source.id),
  };
}
for (const multi of [false, true])
  for (const side of ['head', 'tail'])
    test(`${multi ? 'Multi Path' : 'Curve'} ${side} continuation preserves old records and commits once`, () => {
      const f = fixture(multi),
        before = serialize(f.s.snapshot()),
        old = structuredClone(f.source),
        token = f.begin(side);
      assert.equal(token.resuming, true);
      assert.equal(token.pointPressed, false);
      assert.equal(token.nodeId, f.source.id);
      assert.equal(serialize(f.s.snapshot()), before);
      let t = f.send('append-pen-point', { ...token, position: [310, 270] }),
        newId = t.pointId;
      t = f.send('update-pen-point', { ...t, delta: [20, -5] });
      t = f.send('accept-pen-point', t);
      f.send('commit-pen', { ...t, closed: false });
      const n = f.live();
      assert.equal(f.s.snapshot().layers.length, 1);
      assert.equal(f.s.historyView.past.length, 1);
      for (const p of old.points)
        assert.deepEqual(
          n.points.find((q) => q.id === p.id),
          p,
        );
      const ids = multi ? n.contours[0].points : n.points.map((p) => p.id);
      assert.equal(side === 'head' ? ids[0] : ids.at(-1), newId);
      const added = n.points.find((p) => p.id === newId);
      assert.deepEqual(added[side === 'head' ? 'handleA' : 'handleB'].value, [20, -5]);
      if (multi) {
        assert.deepEqual(n.contours[1], old.contours[1]);
        assert.deepEqual(n.units, old.units);
      }
      assert.equal(f.s.snapshot().layers[0].nodes[0].id, f.l.nodes[0].id);
      f.send('undo');
      assert.equal(serialize(f.s.snapshot()), before);
      f.send('redo');
      assert.equal(f.live().points.length, old.points.length + 1);
    });
test('endpoint selection, cancelled pressed point and unchanged finish preserve source and redo', () => {
  const f = fixture();
  let t = f.begin();
  const before = serialize(f.s.snapshot());
  const ended = f.send('commit-pen', { ...t, closed: false });
  assert.equal(ended.changed, false);
  assert.equal(f.s.historyView.past.length, 0);
  assert.equal(serialize(f.s.snapshot()), before);
  t = f.begin();
  t = f.send('append-pen-point', { ...t, position: [300, 250] });
  t = f.send('cancel-pen-point', t);
  f.send('commit-pen', { ...t, closed: false });
  assert.equal(serialize(f.s.snapshot()), before);
  assert.equal(f.s.historyView.past.length, 0);
  t = f.begin('head');
  t = f.send('append-pen-point', { ...t, position: [20, 20] });
  f.send('cancel-gesture', { gestureId: t.gestureId });
  assert.equal(serialize(f.s.snapshot()), before);
});
test('closed, interior, generated, locked/driven and retained-Remap topology targets reject without a gesture', () => {
  const f = fixture(),
    before = serialize(f.s.snapshot()),
    target = { kind: 'pen', layerId: f.l.id, nodeId: f.source.id, endpointId: f.ids[1] };
  assert.throws(() => f.send('begin-gesture', { target }), /endpoint/);
  assert.equal(f.s.hasGesture, false);
  assert.equal(serialize(f.s.snapshot()), before);
  for (const edit of [
    (n) => (n.params.closed.value = true),
    (n) => (n.params.closed.locked = true),
    (n) => (n.points.at(-1).position.locked = true),
    (n) => (n.points.at(-1).position.expression = 'value; value'),
  ]) {
    const g = fixture();
    g.s.historyView.doc.layers[0].nodes.find((n) => n.id === g.source.id);
    const d = g.s.snapshot();
    edit(d.layers[0].nodes.find((n) => n.id === g.source.id));
    d.schemaVersion = 32;
    const s = new CommandSession(d);
    assert.throws(() =>
      s.dispatch({
        ...s.state,
        type: 'begin-gesture',
        payload: {
          target: { kind: 'pen', layerId: g.l.id, nodeId: g.source.id, endpointId: g.ids.at(-1) },
        },
      }),
    );
    assert.equal(s.hasGesture, false);
  }
  const doc = document(),
    l = doc.layers[0],
    s = new CommandSession(doc);
  assert.throws(
    () =>
      s.dispatch({
        ...s.state,
        type: 'begin-gesture',
        payload: {
          target: { kind: 'pen', layerId: l.id, nodeId: l.nodes[0].id, endpointId: 'generated' },
        },
      }),
    /owned/,
  );
  const g = fixture(),
    remap = node('Path Remap');
  g.doc.layers[0].nodes.push(remap);
  connect(g.doc.layers[0], g.source.id, remap.id, 'path');
  const input = evaluate(g.doc.layers[0]).run(g.source.id);
  remap.edits = [{ pointId: input.contours[0].points[0].id, position: [1, 0] }];
  const locked = new CommandSession(g.doc);
  assert.throws(
    () =>
      locked.dispatch({
        ...locked.state,
        type: 'begin-gesture',
        payload: {
          target: { kind: 'pen', layerId: g.l.id, nodeId: g.source.id, endpointId: g.ids.at(-1) },
        },
      }),
    /retained/,
  );
  assert.equal(locked.hasGesture, false);
});

for (const multi of [false, true])
  test(`closing a continued ${multi ? 'contour' : 'Curve'} is one Undo and leaves the original handles intact`, () => {
    const f = fixture(multi),
      before = serialize(f.s.snapshot());
    let t = f.begin('head');
    t = f.send('append-pen-point', { ...t, position: [20, 30] });
    t = f.send('accept-pen-point', t);
    f.send('commit-pen', { ...t, closed: true });
    const n = f.live();
    assert.equal(multi ? n.contours[0].closed : n.params.closed.value, true);
    for (const p of f.source.points)
      assert.deepEqual(
        n.points.find((q) => q.id === p.id),
        p,
      );
    if (multi) assert.deepEqual(n.contours[1], f.source.contours[1]);
    assert.equal(f.s.historyView.past.length, 1);
    f.send('undo');
    assert.equal(serialize(f.s.snapshot()), before);
  });
