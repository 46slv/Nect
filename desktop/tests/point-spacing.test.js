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
        'space-points',
        { pointIds: ids, action, sourceKey: context().sourceKey, spacing: 12.5, ...patch },
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
  test(`${axis ? 'Y' : 'X'} two-point fractional interval fixes minimum, moves maximum and retains handles with one Undo/Redo`, () => {
    const h = setup([-30, 90], axis),
      before = h.session.snapshot();
    h.space(axis ? 'y' : 'x');
    const live = h.read();
    assert.deepEqual(
      live.points.slice(0, 2).map((p) => p.position.value[axis]),
      [-30, -17.5],
    );
    live.points.forEach((p, i) => {
      const expected = structuredClone(h.curve.points[i]);
      if (i === 1) expected.position.value[axis] = -17.5;
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
    const state = h.session.state;
    h.space(axis ? 'y' : 'x');
    assert.deepEqual(h.session.state, state);
  });

test('many negative/unsorted/tied positions use stable source order independent of selection order', () => {
  const h = setup([20, -30, -30, 100, 40]),
    b = harness(h, [...h.ids].reverse());
  h.space('x', { spacing: 0.25 });
  b.space('x', { spacing: 0.25 });
  assert.deepEqual(h.read(), b.read());
  assert.deepEqual(
    h
      .read()
      .points.slice(0, 5)
      .map((p) => p.position.value[0]),
    [-29.5, -30, -29.75, -29, -29.25],
  );
  assert.deepEqual(
    h.read().points.map((p) => p.id),
    h.curve.points.map((p) => p.id),
  );
  const state = h.session.state;
  h.space('x', { spacing: 0.25 });
  assert.deepEqual(h.session.state, state);
  const many = setup(Array.from({ length: 257 }, (_, i) => 256 - i));
  many.space('x', { spacing: 0.5 });
  assert.deepEqual(
    many
      .read()
      .points.slice(0, 257)
      .map((p) => p.position.value[0]),
    Array.from({ length: 257 }, (_, i) => (256 - i) * 0.5),
  );
  assert.equal(many.session.historyView.past.length, 1);
});

test('zero collapses only the chosen axis, then no-op retains a Redo branch', () => {
  const h = setup([10, -20, 70]);
  h.space('x', { spacing: 0 });
  assert.deepEqual(
    h
      .read()
      .points.slice(0, 3)
      .map((p) => p.position.value[0]),
    [-20, -20, -20],
  );
  h.read().points.forEach((p, i) => near(p.position.value[1], h.curve.points[i].position.value[1]));
  h.send('rename', { name: 'Temporary' });
  h.send('undo');
  const doc = serialize(h.session.snapshot()),
    state = h.session.state,
    hist = h.session.historyView;
  h.space('x', { spacing: 0 });
  assert.equal(serialize(h.session.snapshot()), doc);
  assert.deepEqual(h.session.state, state);
  assert.deepEqual(h.session.historyView, hist);
});

test('compound source traversal breaks ties without changing holes, units, contour or storage order', () => {
  const f = compoundFixture([4, 4, 4, 2]),
    ids = f.path.contours.slice(0, 3).map((c) => c.points[0]);
  ids.forEach((id) => {
    f.path.points.find((p) => p.id === id).position.value[0] = -10;
  });
  const h = harness(f, [...ids].reverse(), f.path),
    before = structuredClone(f.path);
  h.space('x', { spacing: 2.5 });
  const live = h.read();
  assert.deepEqual(live.contours, before.contours);
  assert.deepEqual(live.units, before.units);
  live.points.forEach((p, i) => {
    const old = structuredClone(before.points[i]),
      index = ids.indexOf(p.id);
    if (index >= 0) old.position.value[0] = -10 + index * 2.5;
    assert.deepEqual(p, old);
  });
});

test('rotation/nonuniform transforms use artboard interval, retain the other axis and exact minimum', () => {
  for (const axis of [0, 1]) {
    const f = directMarqueeFixture({ transformed: true }),
      h = harness(
        f,
        f.curve.points.slice(0, 3).map((p) => p.id),
      );
    const before = h.context().points,
      sorted = before.toSorted((a, b) => a.position[axis] - b.position[axis]),
      low = sorted[0].position[axis];
    h.space(axis ? 'y' : 'x', { spacing: 8.125 });
    const after = new Map(h.context().points.map((p) => [p.id, p]));
    sorted.forEach((p, i) => {
      near(after.get(p.id).position[axis], low + i * 8.125);
      near(after.get(p.id).position[1 - axis], p.position[1 - axis]);
    });
    assert.deepEqual(
      h.read().points.find((p) => p.id === sorted[0].id),
      f.curve.points.find((p) => p.id === sorted[0].id),
    );
    h.read().points.forEach((p, i) => {
      assert.deepEqual(p.handleA, f.curve.points[i].handleA);
      assert.deepEqual(p.handleB, f.curve.points[i].handleB);
    });
    assert.deepEqual(h.read().points[3], f.curve.points[3]);
    const state = h.session.state;
    h.space(axis ? 'y' : 'x', { spacing: 8.125 });
    assert.deepEqual(h.session.state, state);
  }
});

test('Remap fractional spacing leaves source immutable and survives JSON/SVG roundtrip and Undo', () => {
  const f = alignmentFixture(true),
    h = harness(f, f.ids, f.owner),
    before = h.session.snapshot();
  h.space('y', { spacing: 4.75 });
  const doc = h.session.snapshot(),
    live = h.read();
  assert.deepEqual(
    doc.layers[0].nodes.find((n) => n.id === f.source.id),
    f.source,
  );
  assert.equal(live.edits.length, 2);
  assert.deepEqual(deserialize(serialize(doc)), doc);
  assert.equal(exportSVG(deserialize(serialize(doc)), {}), exportSVG(doc, {}));
  h.send('undo');
  assert.deepEqual(h.session.snapshot(), before);
  h.send('redo');
  assert.deepEqual(h.read(), live);
});

test('invalid interval/count/axis/IDs and stale revision/Session/source reject with no mutation', () => {
  for (const patch of [
    ...[-1, NaN, Infinity, -Infinity, Number.MAX_VALUE, '12', '', null, undefined, {}, []].map(
      (spacing) => ({ spacing }),
    ),
    { pointIds: [] },
    { pointIds: ['spacing-0'] },
    { pointIds: ['spacing-0', 'spacing-0'] },
    { pointIds: ['spacing-0', 'missing'] },
    { action: 'z' },
    { sourceKey: 'stale' },
  ]) {
    const h = setup([0, 30, 100]),
      before = serialize(h.session.snapshot()),
      state = h.session.state;
    assert.throws(() => h.space('x', patch));
    assert.equal(serialize(h.session.snapshot()), before);
    assert.deepEqual(h.session.state, state);
    assert.equal(h.session.historyView.past.length, 0);
  }
  const h = setup([0, 30]),
    old = h.session.state;
  h.send('rename', { name: 'New' });
  const before = serialize(h.session.snapshot());
  assert.throws(() => h.space('x', {}, old));
  assert.equal(serialize(h.session.snapshot()), before);
  assert.throws(() => h.space('x', {}, new CommandSession(h.session.snapshot()).state));
  assert.equal(serialize(h.session.snapshot()), before);
  const key = h.context().sourceKey;
  h.send('translate-points', { pointIds: [h.ids[0]], delta: [1, 0] });
  // Revision guards protect coordinate changes even where stable route identity does not change.
  assert.throws(() => h.space('x', { sourceKey: key }, old));
});

test('locked/driven stationary minimum and other members, singular/unsupported/locked routes reject whole selection', () => {
  for (const mutate of [
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
      f.transform.bypassed = true;
    },
  ]) {
    const f = directMarqueeFixture({ transformed: true }),
      h = harness(
        f,
        f.curve.points.slice(0, 3).map((p) => p.id),
      ),
      key = h.context().sourceKey;
    mutate(f);
    const session = new CommandSession(f.doc),
      before = serialize(session.snapshot()),
      state = session.state;
    assert.throws(() =>
      session.dispatch({
        ...state,
        type: 'space-points',
        payload: {
          layerId: f.layer.id,
          nodeId: f.curve.id,
          pointIds: h.ids,
          action: 'x',
          sourceKey: key,
          spacing: 5,
        },
      }),
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.past.length, 0);
  }
});

test('fresh revision cannot reuse a held Remap fingerprint or changed transform source key', () => {
  const f = alignmentFixture(true),
    h = harness(f, f.ids, f.owner),
    key = h.context().sourceKey;
  h.send('translate-points', {
    nodeId: f.source.id,
    pointIds: [f.source.points[0].id],
    delta: [1, 0],
  });
  const before = serialize(h.session.snapshot()),
    state = h.session.state;
  assert.throws(() => h.space('x', { sourceKey: key }));
  assert.equal(serialize(h.session.snapshot()), before);
  assert.deepEqual(h.session.state, state);
});
