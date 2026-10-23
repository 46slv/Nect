import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  connect,
  deserialize,
  exportSVG,
  node,
  serialize,
  evaluate,
  enableRemapPoint,
} from '../src/core.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { anchorAlignments, pointAlignmentContext } from '../src/point-alignment.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

function harness(f = directMarqueeFixture(), ids = f.curve.points.slice(0, 3).map((p) => p.id)) {
  const owner = f.curve || f.path,
    layer = f.layer,
    session = new CommandSession(f.doc),
    send = (type, payload = {}, envelope = session.state) =>
      session.dispatch({
        ...envelope,
        type,
        payload: { layerId: layer.id, nodeId: owner.id, ...payload },
      }),
    context = () => pointAlignmentContext(session.snapshot(), layer.id, owner.id, ids),
    align = (action, extra = {}, envelope) =>
      send(
        'align-points',
        { pointIds: ids, action, sourceKey: context().sourceKey, ...extra },
        envelope,
      ),
    read = () => session.snapshot().layers[0].nodes.find((n) => n.id === owner.id);
  return { ...f, owner, ids, session, send, context, align, read };
}
const near = (a, b, error = 1e-9) => assert.ok(Math.abs(a - b) <= error, `${a} != ${b}`);
test('the two-anchor minimum is a single edit and leaves the other anchors unchanged', () => {
  const f = directMarqueeFixture(),
    h = harness(f, ['anchor-0', 'anchor-1']);
  h.align('center-x');
  assert.deepEqual(
    h
      .read()
      .points.slice(0, 2)
      .map((p) => p.position.value),
    [
      [150, 100],
      [150, 100],
    ],
  );
  assert.deepEqual(h.read().points.slice(2), f.curve.points.slice(2));
  assert.equal(h.session.historyView.past.length, 1);
});
for (const [action, , axis, edge] of anchorAlignments)
  test(`${action} uses range extrema/midpoint and preserves unselected anchors/relative handles/IDs`, () => {
    const f = directMarqueeFixture();
    [
      [0, 20],
      [10, 50],
      [30, 90],
      [300, 300],
    ].forEach((xy, i) => {
      f.curve.points[i].position.value = xy;
    });
    f.curve.points[0].handleA.locked = true;
    f.curve.points[1].handleB.expression = 'value; value';
    const h = harness(f),
      before = h.session.snapshot(),
      saved = serialize(before),
      values = axis ? [20, 55, 90] : [0, 15, 30];
    h.align(action);
    const live = h.read();
    live.points.forEach((p, i) => {
      const expected = structuredClone(f.curve.points[i]);
      if (i < 3) expected.position.value[axis] = values[edge];
      assert.deepEqual(p, expected);
    });
    assert.equal(h.session.historyView.past.length, 1);
    assert.deepEqual(deserialize(serialize(h.session.snapshot())), h.session.snapshot());
    const svg = exportSVG(h.session.snapshot(), {});
    assert.equal(exportSVG(deserialize(serialize(h.session.snapshot())), {}), svg);
    h.send('undo');
    assert.equal(serialize(h.session.snapshot()), saved);
    h.send('redo');
    assert.deepEqual(h.read(), live);
    const state = h.session.state,
      document = serialize(h.session.snapshot());
    h.align(action);
    assert.deepEqual(h.session.state, state);
    assert.equal(serialize(h.session.snapshot()), document);
  });

test('rotation and nonuniform scale align in artboard axes with one Undo and unchanged orthogonal coordinates', () => {
  for (const [action, , axis, edge] of anchorAlignments) {
    const h = harness(directMarqueeFixture({ transformed: true })),
      before = h.session.snapshot(),
      a = directSelectionSource(before, h.layer.id).path.contours[0].points.slice(0, 3),
      coordinates = a.map((p) => p.position[axis]),
      low = Math.min(...coordinates),
      high = Math.max(...coordinates),
      target = [low, low / 2 + high / 2, high][edge];
    h.align(action);
    const b = directSelectionSource(h.session.snapshot(), h.layer.id).path.contours[0].points.slice(
      0,
      3,
    );
    b.forEach((p, i) => {
      near(p.position[axis], target);
      near(p.position[1 - axis], a[i].position[1 - axis]);
    });
    h.read().points.forEach((p, i) => {
      assert.deepEqual(p.handleA, h.curve.points[i].handleA);
      assert.deepEqual(p.handleB, h.curve.points[i].handleB);
    });
    assert.deepEqual(h.read().points[3], h.curve.points[3]);
    const state = h.session.state;
    h.align(action);
    assert.deepEqual(h.session.state, state);
    h.send('undo');
    assert.deepEqual(h.session.snapshot(), before);
  }
});

test('documented inverse-map precision gives a repeat no-op even with large scale', () => {
  const f = directMarqueeFixture({ transformed: true });
  f.transform.params.scale.value = 10000;
  f.transform.params.scaleWidth.value = 30000;
  const h = harness(f);
  h.align('center-x');
  const before = serialize(h.session.snapshot()),
    state = h.session.state;
  h.align('center-x');
  assert.deepEqual(h.session.state, state);
  assert.equal(serialize(h.session.snapshot()), before);
});

test('compound membership, holes and open contours are retained by one atomic alignment', () => {
  const f = compoundFixture([8, 4, 6, 3]),
    ids = [
      f.path.contours[0].points[0],
      f.path.contours[1].points[2],
      f.path.contours[3].points[1],
    ],
    h = harness(f, ids);
  h.align('bottom');
  const live = h.read();
  assert.deepEqual(live.contours, f.path.contours);
  assert.deepEqual(live.units, f.path.units);
  for (const p of live.points)
    if (!ids.includes(p.id))
      assert.deepEqual(
        p,
        f.path.points.find((q) => p.id === q.id),
      );
});

function remapFixture() {
  const f = compoundFixture([4, 4, 4, 2]),
    remap = node('Path Remap');
  f.layer.nodes.splice(1, 0, remap);
  f.fill.params.path.source = null;
  connect(f.layer, f.path.id, remap.id, 'path');
  connect(f.layer, remap.id, f.fill.id, 'path');
  const source = f.path;
  return { ...f, curve: remap, source, remap };
}
test('Path Remap uses actual IDs and sparse edits without changing its source or zero-motion entries', () => {
  const f = remapFixture(),
    ids = f.source.contours[0].points.slice(0, 3),
    h = harness(f, ids),
    before = h.session.snapshot();
  h.align('left');
  const live = h.read();
  assert.equal(live.edits.length, 2);
  assert.deepEqual(
    h.session.snapshot().layers[0].nodes.find((n) => n.id === f.source.id),
    f.source,
  );
  const state = h.session.state;
  h.align('left');
  assert.deepEqual(h.session.state, state);
  h.send('undo');
  assert.deepEqual(h.session.snapshot(), before);
  h.send('redo');
  assert.deepEqual(h.read(), live);
});

test('retained Remap handle data and zero-motion legacy entries stay exact; suspended data is never rebound', () => {
  const f = remapFixture(),
    ids = f.source.contours[0].points.slice(0, 3),
    input = evaluate(f.layer).run(f.source.id);
  f.doc.schemaVersion = 49;
  f.remap.edits.push({
    pointId: ids[2],
    fingerprint: input.fingerprint,
    position: [0, 0],
    handleA: [2, -1],
    handleB: [-2, 1],
  });
  const held = structuredClone(f.remap.edits[0]),
    typed = enableRemapPoint(f.remap, input, ids[0]);
  typed.params.handleA.value = [3, -4];
  typed.params.handleA.locked = true;
  typed.params.handleB.expression = 'value; value';
  const handleA = structuredClone(typed.params.handleA),
    handleB = structuredClone(typed.params.handleB),
    h = harness(f, ids);
  h.align('left');
  assert.deepEqual(
    h.read().edits.find((e) => e.pointId === ids[2]),
    held,
  );
  assert.deepEqual(h.read().edits.find((e) => e.pointId === ids[0]).params.handleA, handleA);
  assert.deepEqual(h.read().edits.find((e) => e.pointId === ids[0]).params.handleB, handleB);
  const sourceKey = h.context().sourceKey,
    edits = h.read().edits;
  h.send('translate-points', {
    nodeId: f.source.id,
    pointIds: [f.source.points[0].id],
    delta: [1, 0],
  });
  const before = serialize(h.session.snapshot());
  assert.throws(
    () => h.send('align-points', { pointIds: ids, action: 'top', sourceKey }),
    /Rebind|suspended/i,
  );
  assert.equal(serialize(h.session.snapshot()), before);
  assert.deepEqual(h.read().edits, edits);
});

test('locked/driven/connected member, locked route, singular or unsupported route rejects atomically', () => {
  for (const change of [
    (f) => {
      f.curve.points[0].position.locked = true;
    },
    (f) => {
      f.curve.points[1].position.expression = 'value; value';
    },
    (f) => {
      const v = node('Vector');
      f.layer.nodes.push(v);
      connect(f.layer, v.id, f.curve.id, `${f.curve.points[1].id}.position`);
    },
    (f) => {
      f.layer.locked = true;
    },
    (f) => {
      f.curve.locked = true;
    },
    (f) => {
      f.transform.params.scale.value = 0;
    },
    (f) => {
      f.transform.params.scale.value = 1e-13;
    },
    (f) => {
      f.transform.params.rotation.expression = 'value';
    },
    (f) => {
      f.fill.bypassed = true;
    },
  ]) {
    const f = directMarqueeFixture({ transformed: true }),
      ids = f.curve.points.slice(0, 3).map((p) => p.id),
      oldKey = pointAlignmentContext(f.doc, f.layer.id, f.curve.id, ids).sourceKey;
    change(f);
    const h = harness(f, ids),
      before = serialize(h.session.snapshot()),
      state = h.session.state;
    assert.throws(
      () => h.send('align-points', { pointIds: ids, action: 'left', sourceKey: oldKey }),
      /locked|driven|Transform|route|restore|Restore/i,
    );
    assert.equal(serialize(h.session.snapshot()), before);
    assert.deepEqual(h.session.state, state);
  }
});

test('invalid selections/actions/source keys, stale revision and foreign Session reject without mutation', () => {
  const h = harness(),
    key = h.context().sourceKey;
  for (const payload of [
    { pointIds: [] },
    { pointIds: [h.ids[0]] },
    { pointIds: [h.ids[0], h.ids[0]] },
    { pointIds: [h.ids[0], 'missing'] },
    { action: 'mean-x' },
    { sourceKey: key + 'stale' },
    { sourceKey: null },
  ]) {
    const before = serialize(h.session.snapshot()),
      state = h.session.state;
    assert.throws(() =>
      h.send('align-points', { pointIds: h.ids, action: 'left', sourceKey: key, ...payload }),
    );
    assert.equal(serialize(h.session.snapshot()), before);
    assert.deepEqual(h.session.state, state);
  }
  const old = h.session.state;
  h.send('rename', { name: 'Renamed source' });
  const before = serialize(h.session.snapshot());
  assert.throws(() => h.align('left', {}, old), /STALE/);
  assert.throws(() => h.align('left', {}, new CommandSession(h.session.snapshot()).state), /STALE/);
  assert.equal(serialize(h.session.snapshot()), before);
});

test('changed Remap fingerprint with unchanged IDs and changed Transform source keys reject fresh-revision stale intents', () => {
  const f = remapFixture(),
    ids = f.source.contours[0].points.slice(0, 3),
    h = harness(f, ids),
    old = h.context();
  h.send('translate-points', {
    nodeId: f.source.id,
    pointIds: [f.source.points[0].id],
    delta: [1, 0],
  });
  assert.notEqual(h.context().sourceKey, old.sourceKey);
  const before = serialize(h.session.snapshot());
  assert.throws(
    () => h.send('align-points', { pointIds: ids, action: 'left', sourceKey: old.sourceKey }),
    /source changed/,
  );
  assert.equal(serialize(h.session.snapshot()), before);
  assert.deepEqual(h.read().edits, []);
  const t = harness(directMarqueeFixture({ transformed: true })),
    key = t.context().sourceKey;
  t.send('set-transform-control', { nodeId: t.transform.id, key: 'rotation', value: 43 });
  const saved = serialize(t.session.snapshot());
  assert.throws(
    () => t.send('align-points', { pointIds: t.ids, action: 'left', sourceKey: key }),
    /source changed/,
  );
  assert.equal(serialize(t.session.snapshot()), saved);
});

test('ID-keyed movement rejects partial, duplicate, invalid and mixed uniform/per-point payloads', () => {
  for (const patch of [
    { pointMoves: [] },
    { pointMoves: [{ pointId: 'anchor-0', delta: [1, 0] }] },
    { pointMoves: [{ pointId: 'anchor-0', delta: [NaN, 0] }] },
    {
      pointMoves: [
        { pointId: 'anchor-0', delta: [1, 0] },
        { pointId: 'anchor-0', delta: [0, 1] },
      ],
    },
    { pointMoves: [{ pointId: 'anchor-0', delta: [1, 0] }], delta: [0, 0] },
  ]) {
    const h = harness(),
      before = serialize(h.session.snapshot());
    assert.throws(() => h.send('translate-points', { pointIds: h.ids, ...patch }));
    assert.equal(serialize(h.session.snapshot()), before);
    assert.equal(h.session.historyView.past.length, 0);
  }
});
