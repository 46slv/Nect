import test from 'node:test';
import assert from 'node:assert/strict';
import { CommandSession, connect, deserialize, node, serialize } from '../src/core.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

function fixture() {
  const f = compoundFixture([8, 5, 7, 4]);
  f.doc.schemaVersion = 42;
  f.path.points.forEach((p, i) => {
    p.handleA.value = [-2 - i / 10, 3];
    p.handleB.value = [4, -5 - i / 10];
  });
  f.ids = [
    f.path.contours[0].points[2],
    f.path.contours[1].points[1],
    f.path.contours[2].points[3],
    f.path.contours[3].points[0],
  ];
  return f;
}
function harness(f) {
  const session = new CommandSession(f.doc);
  const send = (type, payload = {}, envelope = session.state) =>
    session.dispatch({
      ...envelope,
      type,
      payload: { layerId: f.layer.id, ...payload },
    });
  const move = (payload = {}, envelope) =>
    send(
      'translate-points',
      {
        nodeId: f.path.id,
        pointIds: f.ids,
        delta: [12.5, -7.25],
        ...payload,
      },
      envelope,
    );
  return { session, send, move };
}

test('common point translation preserves handles, identities and contour relationships with one Undo', () => {
  const f = fixture();
  // Relative handles are not written, even when independently locked/driven.
  f.path.points.find((p) => p.id === f.ids[0]).handleA.locked = true;
  f.path.points.find((p) => p.id === f.ids[1]).handleB.expression = 'value; value';
  const { session, send, move } = harness(f),
    before = session.snapshot();
  move();
  const after = session.snapshot(),
    live = after.layers[0].nodes.find((n) => n.id === f.path.id);
  assert.deepEqual(live.contours, f.path.contours);
  assert.deepEqual(live.units, f.path.units);
  for (const p of f.path.points) {
    const expected = structuredClone(p);
    if (f.ids.includes(p.id))
      expected.position.value = p.position.value.map((v, i) => v + [12.5, -7.25][i]);
    assert.deepEqual(
      live.points.find((q) => q.id === p.id),
      expected,
    );
  }
  assert.deepEqual(
    after.layers[0].nodes.filter((n) => n.id !== f.path.id),
    before.layers[0].nodes.filter((n) => n.id !== f.path.id),
  );
  assert.equal(session.historyView.past.length, 1);
  assert.deepEqual(deserialize(serialize(after)), after);
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), after);
  assert.equal(session.historyView.past.length, 1);
  send('undo');
  const state = session.state;
  move({ delta: [0, 0] });
  assert.deepEqual(session.state, state);
  send('redo');
  assert.deepEqual(session.snapshot(), after);
});

test('one non-editable member rejects every selected point without history mutation', async (t) => {
  for (const [name, modify] of [
    [
      'locked point',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.locked = true;
      },
    ],
    [
      'driven point',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.expression = 'value; value';
      },
    ],
    [
      'locked node',
      (f) => {
        f.path.locked = true;
      },
    ],
    [
      'connected point',
      (f) => {
        const supplier = node('Vector');
        f.layer.nodes.push(supplier);
        connect(f.layer, supplier.id, f.path.id, `${f.ids[2]}.position`);
      },
    ],
    [
      'locked composition',
      (f) => {
        f.layer.locked = true;
      },
    ],
  ])
    await t.test(name, () => {
      const f = fixture();
      modify(f);
      const { session, move } = harness(f),
        before = serialize(session.snapshot()),
        state = session.state;
      assert.throws(() => move(), /locked|driven|editable/i);
      assert.equal(serialize(session.snapshot()), before);
      assert.deepEqual(session.state, state);
      assert.equal(session.historyView.past.length, 0);
    });
});

test('invalid, missing and stale move requests reject atomically', async (t) => {
  for (const payload of [
    { pointIds: [] },
    { pointIds: ['missing'] },
    { pointIds: ['missing', 'also-missing'] },
    { delta: [NaN, 0] },
    { delta: [0, Infinity] },
    { delta: [2] },
    { nodeId: 'missing' },
  ])
    await t.test(JSON.stringify(payload), () => {
      const f = fixture(),
        { session, move } = harness(f),
        before = serialize(session.snapshot());
      assert.throws(() => move(payload));
      assert.equal(serialize(session.snapshot()), before);
      assert.equal(session.historyView.past.length, 0);
    });
  const f = fixture(),
    { session, move } = harness(f),
    captured = session.state;
  move({ delta: [1, 0] });
  const before = serialize(session.snapshot()),
    state = session.state;
  assert.throws(() => move({}, captured), /stale|revision/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.deepEqual(session.state, state);
  assert.equal(session.historyView.past.length, 1);
});
