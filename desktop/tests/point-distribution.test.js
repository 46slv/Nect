import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  node,
  point,
  serialize,
  deserialize,
  exportSVG,
  connect,
} from '../src/core.js';
import { pointAlignmentContext } from '../src/point-alignment.js';
import { directSelectionSource } from '../src/direct-selection.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';
import { alignmentFixture } from './fixtures/point-alignment.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

function harness(f, ids, owner = f.curve || f.owner) {
  const session = new CommandSession(f.doc),
    send = (type, payload = {}, envelope = session.state) =>
      session.dispatch({
        ...envelope,
        type,
        payload: { layerId: f.layer.id, nodeId: owner.id, ...payload },
      }),
    context = (selection = ids) =>
      pointAlignmentContext(session.snapshot(), f.layer.id, owner.id, selection),
    space = (action, patch = {}, envelope) =>
      send(
        'distribute-points',
        { pointIds: ids, action, sourceKey: context().sourceKey, ...patch },
        envelope,
      ),
    read = () => session.snapshot().layers[0].nodes.find((n) => n.id === owner.id);
  return { ...f, owner, ids, session, send, context, space, read };
}
function setup(values, axis = 0) {
  const f = directMarqueeFixture();
  f.curve.params.closed.value = false;
  f.curve.points = [...values, 350].map((v, i) => {
    const p = point(axis ? i * 17 : v, axis ? v : i * 17);
    p.id = `spacing-${i}`;
    p.handleA.value = [-2, 3];
    p.handleB.value = [4, -5];
    return p;
  });
  return harness(
    f,
    f.curve.points.slice(0, -1).map((p) => p.id),
  );
}
const near = (a, b, epsilon = 1e-9) => assert.ok(Math.abs(a - b) <= epsilon, `${a} != ${b}`);

for (const axis of [0, 1])
  test(`${axis ? 'vertical' : 'horizontal'}: three points preserve endpoints/other axis/handles/unselected data with one Undo`, () => {
    const h = setup([0, 10, 30], axis),
      before = h.session.snapshot();
    h.space(axis ? 'y' : 'x');
    const live = h.read();
    assert.deepEqual(
      live.points.slice(0, 3).map((p) => p.position.value[axis]),
      [0, 15, 30],
    );
    live.points.forEach((p, i) => {
      const expected = structuredClone(h.curve.points[i]);
      if (i === 1) expected.position.value[axis] = 15;
      assert.deepEqual(p, expected);
    });
    assert.equal(h.session.historyView.past.length, 1);
    const doc = h.session.snapshot(),
      svg = exportSVG(doc, {});
    assert.deepEqual(deserialize(serialize(doc)), doc);
    assert.equal(exportSVG(deserialize(serialize(doc)), {}), svg);
    h.send('undo');
    assert.deepEqual(h.session.snapshot(), before);
    h.send('redo');
    assert.deepEqual(h.read(), live);
    const saved = serialize(h.session.snapshot()),
      state = h.session.state;
    h.space(axis ? 'y' : 'x');
    assert.equal(serialize(h.session.snapshot()), saved);
    assert.deepEqual(h.session.state, state);
  });

test('stable source traversal breaks ties independently of selection/click order', () => {
  const a = setup([0, 0, 20, 20, 50]),
    b = harness(a, [...a.ids].reverse());
  a.space('x');
  b.space('x');
  assert.deepEqual(a.read(), b.read());
  assert.deepEqual(
    a
      .read()
      .points.slice(0, 5)
      .map((p) => p.position.value[0]),
    [0, 12.5, 25, 37.5, 50],
  );
  assert.deepEqual(a.read().points[0], a.curve.points[0]);
  assert.deepEqual(a.read().points[4], a.curve.points[4]);
  const state = b.session.state;
  b.space('x', { pointIds: a.ids });
  assert.deepEqual(b.session.state, state);
});

test('one compound source preserves holes, units and authored storage order across contour selection', () => {
  const f = compoundFixture([4, 4, 4, 2]),
    ids = f.path.contours.slice(0, 3).map((c) => c.points[0]);
  ids.forEach((id, i) => {
    f.path.points.find((p) => p.id === id).position.value[0] = i < 2 ? -30 : 90;
  });
  const h = harness(f, [...ids].reverse(), f.path),
    before = structuredClone(f.path);
  h.space('x');
  const live = h.read();
  assert.deepEqual(live.contours, before.contours);
  assert.deepEqual(live.units, before.units);
  assert.deepEqual(
    live.points.map((p) => p.id),
    before.points.map((p) => p.id),
  );
  for (const p of live.points) {
    const expected = structuredClone(before.points.find((q) => q.id === p.id));
    if (p.id === ids[1]) expected.position.value[0] = 30;
    assert.deepEqual(p, expected);
  }
  assert.equal(h.session.historyView.past.length, 1);
  h.send('undo');
  assert.deepEqual(h.read(), before);
});

test('negative coordinates are sorted spatially without rewriting authored point/contour order', () => {
  const h = setup([0, 120, -90, -30]),
    ids = h.curve.points.map((p) => p.id);
  h.space('x');
  assert.deepEqual(
    h
      .read()
      .points.slice(0, 4)
      .map((p) => p.position.value[0]),
    [50, 120, -90, -20],
  );
  assert.deepEqual(
    h.read().points.map((p) => p.id),
    ids,
  );
});

test('many anchors use one candidate and leave the unselected tail unchanged', () => {
  const h = setup(Array.from({ length: 257 }, (_, i) => (i * i) / 4 - 100));
  h.space('x');
  h.read()
    .points.slice(0, 257)
    .forEach((p, i) => near(p.position.value[0], -100 + i * 64));
  assert.deepEqual(h.read().points.at(-1), h.curve.points.at(-1));
  assert.equal(h.session.historyView.past.length, 1);
});

test('all-equal and already-even coordinates are document/history/Redo neutral', () => {
  for (const values of [
    [5, 5, 5, 5],
    [-30, 0, 30, 60],
  ]) {
    const h = setup(values);
    h.send('rename', { name: 'Temporary' });
    h.send('undo');
    const before = serialize(h.session.snapshot()),
      state = h.session.state,
      future = h.session.historyView.future.length;
    h.space('x');
    assert.equal(serialize(h.session.snapshot()), before);
    assert.deepEqual(h.session.state, state);
    assert.equal(h.session.historyView.future.length, future);
  }
});

test('rotation/nonuniform scale use artboard sorting, fixed endpoints and unchanged orthogonal coordinates', () => {
  for (const axis of [0, 1]) {
    const f = directMarqueeFixture({ transformed: true }),
      h = harness(
        f,
        f.curve.points.slice(0, 3).map((p) => p.id),
      ),
      before = h.context(),
      sorted = [...before.points].sort((a, b) => a.position[axis] - b.position[axis]),
      low = sorted[0].position[axis],
      high = sorted.at(-1).position[axis];
    h.space(axis ? 'y' : 'x');
    const after = new Map(
      directSelectionSource(h.session.snapshot(), f.layer.id).path.contours[0].points.map((p) => [
        p.id,
        p,
      ]),
    );
    sorted.forEach((p, i) => {
      near(after.get(p.id).position[axis], low + ((high - low) * i) / 2);
      near(after.get(p.id).position[1 - axis], p.position[1 - axis]);
    });
    for (const endpoint of [sorted[0].id, sorted.at(-1).id])
      assert.deepEqual(
        h.read().points.find((p) => p.id === endpoint),
        f.curve.points.find((p) => p.id === endpoint),
      );
    assert.deepEqual(h.read().points[3], f.curve.points[3]);
    h.read().points.forEach((p, i) => {
      assert.deepEqual(p.handleA, f.curve.points[i].handleA);
      assert.deepEqual(p.handleB, f.curve.points[i].handleB);
    });
    const state = h.session.state;
    h.space(axis ? 'y' : 'x');
    assert.deepEqual(h.session.state, state);
  }
});

test('Remap spacing preserves source, sparse endpoints, topology, and JSON/SVG identity after reload', () => {
  const f = alignmentFixture(true),
    h = harness(f, f.ids, f.owner),
    before = h.session.snapshot();
  h.space('y');
  const live = h.read(),
    doc = h.session.snapshot();
  assert.equal(live.edits.length, 1);
  assert.deepEqual(
    doc.layers[0].nodes.find((n) => n.id === f.source.id),
    f.source,
  );
  assert.deepEqual(deserialize(serialize(doc)), doc);
  assert.equal(exportSVG(deserialize(serialize(doc)), {}), exportSVG(doc, {}));
  h.send('undo');
  assert.deepEqual(h.session.snapshot(), before);
  h.send('redo');
  assert.deepEqual(h.read(), live);
});

test('locked/driven endpoints or internal points, locked/singular/unsupported routes reject the entire operation', () => {
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
      f.transform.params.rotation.expression = 'value';
    },
    (f) => {
      f.fill.bypassed = true;
    },
  ]) {
    const f = directMarqueeFixture({ transformed: true }),
      ids = f.curve.points.slice(0, 3).map((p) => p.id),
      key = pointAlignmentContext(f.doc, f.layer.id, f.curve.id, ids).sourceKey;
    change(f);
    const h = harness(f, ids),
      before = serialize(h.session.snapshot()),
      state = h.session.state;
    assert.throws(() =>
      h.send('distribute-points', { pointIds: ids, action: 'x', sourceKey: key }),
    );
    assert.equal(serialize(h.session.snapshot()), before);
    assert.deepEqual(h.session.state, state);
  }
});

test('invalid count/action/IDs/source key and stale revision/Session reject without history', () => {
  const h = setup([0, 10, 30]),
    key = h.context().sourceKey;
  for (const patch of [
    { pointIds: h.ids.slice(0, 2) },
    { pointIds: [] },
    { pointIds: [h.ids[0], h.ids[0], h.ids[2]] },
    { pointIds: [h.ids[0], h.ids[1], 'missing'] },
    { action: 'curve-length' },
    { sourceKey: 'stale' },
    { sourceKey: null },
  ]) {
    const before = serialize(h.session.snapshot()),
      state = h.session.state;
    assert.throws(() =>
      h.send('distribute-points', { pointIds: h.ids, action: 'x', sourceKey: key, ...patch }),
    );
    assert.equal(serialize(h.session.snapshot()), before);
    assert.deepEqual(h.session.state, state);
  }
  const old = h.session.state;
  h.send('rename', { name: 'New revision' });
  const before = serialize(h.session.snapshot());
  assert.throws(() => h.space('x', {}, old), /STALE/);
  assert.throws(() => h.space('x', {}, new CommandSession(h.session.snapshot()).state), /STALE/);
  assert.equal(serialize(h.session.snapshot()), before);
});

test('source fingerprint and route changes invalidate an old spacing intent at a fresh revision', () => {
  const f = alignmentFixture(true),
    h = harness(f, f.ids, f.owner),
    key = h.context().sourceKey;
  h.send('translate-points', {
    nodeId: f.source.id,
    pointIds: [f.source.points[0].id],
    delta: [1, 0],
  });
  const before = serialize(h.session.snapshot());
  assert.throws(
    () => h.send('distribute-points', { pointIds: h.ids, action: 'y', sourceKey: key }),
    /source changed/,
  );
  assert.equal(serialize(h.session.snapshot()), before);
  assert.deepEqual(h.read().edits, []);
  const old = h.context().sourceKey;
  h.send('set-transform-control', { nodeId: f.transform.id, key: 'rotation', value: -25 });
  const changed = serialize(h.session.snapshot());
  assert.throws(
    () => h.send('distribute-points', { pointIds: h.ids, action: 'y', sourceKey: old }),
    /source changed/,
  );
  assert.equal(serialize(h.session.snapshot()), changed);
});
