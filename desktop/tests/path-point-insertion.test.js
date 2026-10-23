import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandSession,
  document,
  node,
  point,
  connect,
  serialize,
  deserialize,
  evaluate,
} from '../src/core.js';
import { splitPathSegment, segmentControls, hitPathSegment } from '../src/path-point-insertion.js';
const bezier = (ps, t) =>
  [0, 1].map(
    (k) =>
      ps[0][k] * (1 - t) ** 3 +
      3 * ps[1][k] * t * (1 - t) ** 2 +
      3 * ps[2][k] * t * t * (1 - t) +
      ps[3][k] * t ** 3,
  );
const plain = (p) =>
  Object.fromEntries(['position', 'handleA', 'handleB'].map((k) => [k, p[k].value]));
function fixture(multi = false, edit) {
  const doc = document(),
    l = doc.layers[0],
    n = node(multi ? 'Multi Path' : 'Curve');
  n.points = [point(100, 100), point(400, 200), point(500, 400)];
  n.points[0].handleB.value = [130, 150];
  n.points[0].handleA.value = [-130, -150];
  n.points[0].handleType = 'symmetric';
  n.points[1].handleA.value = [-100, -180];
  n.points[1].handleB.value = [100, 180];
  n.points[1].handleType = 'symmetric';
  if (multi) {
    doc.schemaVersion = Math.max(doc.schemaVersion, 22);
    const hole = [point(200, 170), point(240, 170), point(220, 200)];
    n.contours = [
      { id: 'outer', group: 'glyph', closed: true, points: n.points.map((p) => p.id) },
      { id: 'hole', group: 'glyph', closed: true, points: hole.map((p) => p.id) },
    ];
    n.units = [{ id: 'unit', contours: ['outer', 'hole'] }];
    n.points.push(...hole);
  } else n.params.closed.value = false;
  l.nodes.push(n);
  connect(l, n.id, l.output, 'path');
  edit?.(doc, l, n);
  const session = new CommandSession(doc);
  const payload = {
    layerId: l.id,
    nodeId: n.id,
    contourId: multi ? 'outer' : n.id,
    startPointId: n.points[0].id,
    endPointId: n.points[1].id,
    t: 0.3,
  };
  const send = (type, p = {}) =>
    session.dispatch({ ...session.state, type, payload: { ...payload, ...p } });
  const live = () => session.snapshot().layers[0].nodes.find((x) => x.id === n.id);
  return { doc, l, n, session, payload, send, live };
}
for (const [name, a, b] of [
  [
    'S curve',
    { position: [0, 0], handleB: [100, 150] },
    { position: [300, 0], handleA: [-100, -150] },
  ],
  ['straight', { position: [0, 0], handleB: [0, 0] }, { position: [300, 0], handleA: [0, 0] }],
  [
    'one handle',
    { position: [0, 0], handleB: [100, 150] },
    { position: [300, 0], handleA: [0, 0] },
  ],
  [
    'coincident anchors loop',
    { position: [0, 0], handleB: [100, 150] },
    { position: [0, 0], handleA: [-100, 150] },
  ],
])
  test(`de Casteljau preserves ${name} at .2/.5/.8`, () => {
    for (const t of [0.2, 0.5, 0.8]) {
      const s = splitPathSegment(a, b, t),
        old = segmentControls(a, b),
        left = segmentControls({ ...a, handleB: s.startHandle }, s),
        right = segmentControls(s, { ...b, handleA: s.endHandle });
      for (let i = 0; i <= 1000; i++) {
        const u = i / 1000,
          expected = bezier(old, u),
          actual = u <= t ? bezier(left, u / t) : bezier(right, (u - t) / (1 - t));
        assert.ok(Math.hypot(...actual.map((v, k) => v - expected[k])) < 1e-8);
      }
    }
  });
test('fully degenerate segments and endpoint/nonfinite parameters reject', () => {
  const a = { position: [2, 3], handleB: [0, 0] },
    b = { position: [2, 3], handleA: [0, 0] };
  assert.throws(() => splitPathSegment(a, b, 0.5), /degenerate/);
  for (const t of [0, 1, -0.1, Infinity, NaN])
    assert.throws(() => splitPathSegment(a, b, t), /0 < t/);
});
for (const multi of [false, true])
  test(`${multi ? 'Multi Path' : 'Curve'} typed insertion preserves neighbors, topology, exact undo/redo and codec`, () => {
    const f = fixture(multi),
      before = f.session.snapshot(),
      old = f.live(),
      result = f.send('insert-path-point'),
      after = f.live(),
      created = after.points.find((p) => p.id === result.pointId);
    assert.equal(result.changed, true);
    assert.equal(after.points.length, old.points.length + 1);
    assert.deepEqual(
      after.points.find((p) => p.id === old.points[0].id).handleA,
      old.points[0].handleA,
    );
    assert.deepEqual(
      after.points.find((p) => p.id === old.points[1].id).handleB,
      old.points[1].handleB,
    );
    for (const p of old.points.slice(2))
      assert.deepEqual(
        after.points.find((q) => q.id === p.id),
        p,
      );
    assert.equal(created.position.type, 'Vector2');
    assert.equal(created.handleType, 'smooth');
    assert.equal(after.points.find((p) => p.id === old.points[0].id).handleType, 'smooth');
    if (multi) {
      assert.deepEqual(after.units, old.units);
      assert.deepEqual(after.contours[1], old.contours[1]);
      assert.equal(after.contours[0].group, 'glyph');
      assert.equal(after.contours[0].points[1], created.id);
    }
    assert.equal(f.session.historyView.past.length, 1);
    const saved = serialize(f.session.snapshot());
    assert.equal(serialize(deserialize(saved)), saved);
    f.send('undo');
    assert.deepEqual(f.session.snapshot(), before);
    f.send('redo');
    assert.equal(serialize(f.session.snapshot()), saved);
  });
test('closed seam insertion appends an ID without duplicating first anchor or altering holes', () => {
  const f = fixture(true),
    old = f.live(),
    c = old.contours[1];
  const r = f.send('insert-path-point', {
    contourId: c.id,
    startPointId: c.points.at(-1),
    endPointId: c.points[0],
  });
  assert.deepEqual(f.live().contours[1].points, [...c.points, r.pointId]);
  assert.deepEqual(f.live().contours[0], old.contours[0]);
});
test('invalid adjacency, ownership, t, stale revision and duplicate release are atomic', () => {
  const f = fixture(),
    before = serialize(f.session.snapshot()),
    state = f.session.state;
  for (const p of [
    { contourId: 'missing' },
    { endPointId: f.n.points[2].id },
    { startPointId: f.n.points.at(-1).id, endPointId: f.n.points[0].id },
    { t: 0 },
    { t: 1 },
    { t: NaN },
  ]) {
    assert.throws(() => f.send('insert-path-point', p));
    assert.equal(serialize(f.session.snapshot()), before);
    assert.deepEqual(f.session.state, state);
    assert.equal(f.session.historyView.past.length, 0);
  }
  f.send('insert-path-point');
  const after = serialize(f.session.snapshot());
  assert.throws(
    () => f.session.dispatch({ ...state, type: 'insert-path-point', payload: f.payload }),
    /STALE_REVISION/,
  );
  assert.equal(serialize(f.session.snapshot()), after);
});
test('lock, driven, hidden and generated target refusal never partially commits', () => {
  const mutations = [
    (d, l, n) => {
      l.locked = true;
    },
    (d, l, n) => {
      l.visible = false;
    },
    (d, l, n) => {
      n.locked = true;
    },
    ...['position', 'handleB'].map((key) => (d, l, n) => {
      n.points[0][key].locked = true;
    }),
    (d, l, n) => {
      n.points[1].handleA.expression = '1;2';
    },
    (d, l, n) => {
      n.params.closed.locked = true;
    },
    (d, l, n) => {
      const v = node('Vector');
      l.nodes.push(v);
      n.points[0].position.source = v.id;
    },
  ];
  for (const edit of mutations) {
    const f = fixture(false, edit),
      before = serialize(f.session.snapshot());
    assert.throws(() => f.send('insert-path-point'));
    assert.equal(serialize(f.session.snapshot()), before);
    assert.equal(f.session.historyView.past.length, 0);
  }
  const f = fixture(false, (d, l, n) => {
    const r = node('Path Remap');
    l.nodes.push(r);
    connect(l, n.id, r.id, 'path');
  });
  assert.throws(
    () => f.send('insert-path-point', { nodeId: f.l.nodes.at(-1).id }),
    /Generated path/,
  );
});
test('unrelated driven points do not prevent insertion; reverse handle is never rewritten', () => {
  const f = fixture(false, (d, l, n) => {
    n.points[2].position.expression = '500;400';
    n.points[0].handleA.locked = true;
  });
  f.send('insert-path-point');
  assert.deepEqual(f.live().points[0].handleA, f.n.points[0].handleA);
});
test('retained downstream Remap edits refuse topology changes, preserving fingerprints and history', () => {
  for (const multi of [false, true]) {
    const f = fixture(multi, (d, l, n) => {
        const r = node('Path Remap');
        l.nodes.push(r);
        connect(l, n.id, r.id, 'path');
        const input = evaluate({ ...l, output: n.id }).path;
        r.edits = [
          {
            pointId: n.points[0].id,
            position: [2, 3],
            ...(input.fingerprint ? { fingerprint: input.fingerprint } : {}),
          },
        ];
        connect(l, r.id, l.output, 'path');
      }),
      before = serialize(f.session.snapshot());
    assert.throws(() => f.send('insert-path-point'), /retained Path Remap edits/);
    assert.equal(serialize(f.session.snapshot()), before);
    assert.equal(f.session.historyView.past.length, 0);
  }
});
test('node-wide 9999 -> 10000 succeeds; 10000 -> 10001 refuses across multiple contours', () => {
  const f = fixture(true, (d, l, n) => {
    for (let i = n.points.length; i < 9999; i++) {
      const p = point(i, 10);
      n.points.push(p);
      n.contours[1].points.push(p.id);
    }
  });
  f.send('insert-path-point');
  assert.equal(f.live().points.length, 10000);
  const before = serialize(f.session.snapshot());
  assert.throws(() => f.send('insert-path-point'), /Too many vertices/);
  assert.equal(serialize(f.session.snapshot()), before);
});
test('screen-space segment hit is zoom invariant; crossings and overlapping segments are ambiguous', () => {
  const path = {
    contours: [
      {
        id: 'a',
        closed: false,
        points: [
          { id: 'p0', position: [0, 0], handleA: [0, 0], handleB: [0, 0] },
          { id: 'p1', position: [100, 0], handleA: [0, 0], handleB: [0, 0] },
        ],
      },
    ],
  };
  for (const scale of [0.25, 1, 4, 8]) {
    assert.ok(hitPathSegment(path, [50, 5 / scale], scale));
    assert.equal(hitPathSegment(path, [50, 7 / scale], scale), null);
    assert.ok(Math.abs(hitPathSegment(path, [50, 0], scale).t - 0.5) < 1e-7);
  }
  const crossing = structuredClone(path);
  crossing.contours.push({
    id: 'b',
    closed: false,
    points: [
      { id: 'p2', position: [50, -50], handleA: [0, 0], handleB: [0, 0] },
      { id: 'p3', position: [50, 50], handleA: [0, 0], handleB: [0, 0] },
    ],
  });
  assert.equal(hitPathSegment(crossing, [50, 0], 1).ambiguous, true);
  path.contours.push({ ...path.contours[0], id: 'c' });
  assert.equal(hitPathSegment(path, [50, 0], 1).ambiguous, true);
});
test('unrelated retained Remap edits permit insertion; indirect dependencies remain protected', () => {
  const unrelated = fixture(false, (d, l) => {
    const c = node('Curve'),
      r = node('Path Remap');
    l.nodes.push(c, r);
    connect(l, c.id, r.id, 'path');
    r.edits = [{ pointId: c.points[0].id, position: [2, 3] }];
  });
  assert.equal(unrelated.send('insert-path-point').changed, true);
  const indirect = fixture(false, (d, l, n) => {
    const t = node('Transform'),
      r = node('Path Remap');
    l.nodes.push(t, r);
    connect(l, n.id, t.id, 'path');
    connect(l, t.id, r.id, 'path');
    r.edits = [{ pointId: n.points[0].id, position: [2, 3] }];
  });
  assert.throws(() => indirect.send('insert-path-point'), /retained Path Remap edits/);
});
test('one cubic self-crossing exposes both parameter roots instead of silently choosing one', () => {
  // x = 200 + 400(t-.25)(t-.75); y = 200 + 400(t-.25)(t-.75)(t-.5).
  const a = { id: 'a', position: [275, 162.5], handleA: [0, 0], handleB: [-400 / 3, 275 / 3] },
    b = { id: 'b', position: [275, 237.5], handleA: [-400 / 3, -275 / 3], handleB: [0, 0] },
    hit = hitPathSegment(
      { contours: [{ id: 'loop', closed: false, points: [a, b] }] },
      [200, 200],
      1,
    );
  assert.equal(hit.ambiguous, true);
  assert.equal(hit.candidates.length, 2);
  assert.ok(Math.abs(hit.candidates[0].t - 0.25) < 1e-7);
  assert.ok(Math.abs(hit.candidates[1].t - 0.75) < 1e-7);
});
test('an invalid evaluated source rejects before any authored change', () => {
  const f = fixture(false, (d, l, n) => {
      n.points[2].position.expression = 'missing(1);0';
    }),
    before = serialize(f.session.snapshot());
  assert.throws(() => f.send('insert-path-point'));
  assert.equal(serialize(f.session.snapshot()), before);
  assert.equal(f.session.historyView.past.length, 0);
});
