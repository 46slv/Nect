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
  exportSVG,
  activeParameters,
} from '../src/core.js';
const near = (a, b) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < 1e-6, `${a} != ${b}`));
function scene(raw = 'M0 0L100 0') {
  const doc = document(),
    l = doc.layers[0],
    s = node('Imported Path'),
    r = node('Resample Path'),
    f = node('Fill'),
    o = node('Output');
  s.rawPath = raw;
  l.nodes = [s, r, f, o];
  l.output = o.id;
  connect(l, s.id, r.id, 'path');
  connect(l, r.id, f.id, 'path');
  connect(l, f.id, o.id, 'path');
  r.params.count.value = 5;
  return { doc, l, s, r };
}
test('Resample open paths produces equal distance samples with exact endpoints and zero handles', () => {
  const { l, s, r } = scene(),
    original = JSON.stringify(s);
  const result = evaluate(l).run(r.id);
  assert.equal(result.contours[0].closed, false);
  result.contours[0].points.forEach((p, i) => {
    near(p.position, [i * 25, 0]);
    assert.deepEqual(p.handleA, [0, 0]);
    assert.deepEqual(p.handleB, [0, 0]);
  });
  assert.equal(JSON.stringify(s), original);
});
test('Resample cubic curves uses arc length and closed paths omit duplicated seam', () => {
  const f = scene('M0 0C0 100 100 100 100 0');
  f.r.params.count.value = 3;
  const p = evaluate(f.l).run(f.r.id).contours[0].points;
  near(p[0].position, [0, 0]);
  near(p[1].position, [50, 75]);
  near(p[2].position, [100, 0]);
  f.s.rawPath = 'M0 0L100 0L100 100L0 100Z';
  f.r.params.count.value = 4;
  const c = evaluate(f.l).run(f.r.id).contours[0];
  assert.equal(c.closed, true);
  [
    [0, 0],
    [100, 0],
    [100, 100],
    [0, 100],
  ].forEach((v, i) => near(c.points[i].position, v));
});
test('Resample editable roundtrip, unique IDs, export and bypass', () => {
  const { doc, l, s, r } = scene('M0 0L100 0 M0 20L100 20');
  const path = evaluate(l).run(r.id);
  assert.equal(new Set(path.contours.flatMap((c) => c.points.map((p) => p.id))).size, 10);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0]).run(r.id), path);
  assert.match(exportSVG(doc), /<path /);
  r.bypassed = true;
  assert.deepEqual(evaluate(l).run(r.id), evaluate(l).run(s.id));
});
test('Resample invalid counts and excessive output fail atomically, with undo and lock', () => {
  const f = scene('M0 0L100 0L100 100Z'),
    session = new CommandSession(f.doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: f.l.id, nodeId: f.r.id, ...payload },
    });
  const before = session.snapshot();
  for (const value of [0, 2, 2.5, 1025]) {
    assert.throws(() => send('set-parameter', { key: 'count', value }));
    assert.deepEqual(session.snapshot(), before);
  }
  send('set-parameter', { key: 'count', value: 8 });
  const changed = session.snapshot();
  send('undo');
  assert.deepEqual(session.snapshot(), before);
  send('redo');
  assert.deepEqual(session.snapshot(), changed);
  send('set-parameter-lock', { key: 'count', locked: true });
  assert.throws(() => send('set-parameter', { key: 'count', value: 16 }), /lock/i);
  f.s.rawPath = Array.from({ length: 100 }, (_, i) => `M0 ${i}L100 ${i}`).join(' ');
  f.r.params.count.value = 1024;
  assert.match(evaluate(f.l).error, /budget/);
});

test('Resample spacing mode honors lock, undo, redo, and mode-toggle rollback', () => {
  const f = scene(),
    session = new CommandSession(f.doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: f.l.id, nodeId: f.r.id, ...payload },
      });
  send('set-operation', { operation: 'spacing' });
  send('set-parameter-lock', { key: 'spacing', locked: true });
  const locked = session.snapshot();
  assert.throws(() => send('set-parameter', { key: 'spacing', value: 10 }), /locked/i);
  assert.deepEqual(session.snapshot(), locked);
  send('set-parameter-lock', { key: 'spacing', locked: false });
  send('set-parameter', { key: 'spacing', value: 10 });
  const changed = session.snapshot();
  send('undo');
  assert.equal(
    session.snapshot().layers[0].nodes.find((n) => n.id === f.r.id).params.spacing.value,
    24,
  );
  send('redo');
  assert.deepEqual(session.snapshot(), changed);

  const invalidMode = scene();
  invalidMode.r.params.spacing.expression = 'invalid()';
  const failedSession = new CommandSession(invalidMode.doc),
    beforeToggle = failedSession.snapshot();
  assert.throws(() =>
    failedSession.dispatch({
      ...failedSession.state,
      type: 'set-operation',
      payload: {
        layerId: invalidMode.l.id,
        nodeId: invalidMode.r.id,
        operation: 'spacing',
      },
    }),
  );
  assert.deepEqual(failedSession.snapshot(), beforeToggle);
});
test('Resample zero-length and empty paths remain finite', () => {
  const f = scene('M10 20L10 20');
  const result = evaluate(f.l).run(f.r.id);
  result.contours[0].points.forEach((p) => near(p.position, [10, 20]));
  f.s.rawPath = '';
  assert.deepEqual(evaluate(f.l).run(f.r.id).contours, []);
});

test('Resample defaults to Count and spacing mode distributes maximum gaps per contour', () => {
  const { l, r } = scene('M0 0L100 0 M0 30L200 30');
  assert.equal(r.operation, 'count');
  const input = layersDoc(l),
    session = new CommandSession(input);
  session.dispatch({
    ...session.state,
    type: 'set-operation',
    payload: { layerId: l.id, nodeId: r.id, operation: 'spacing' },
  });
  const doc = session.snapshot(),
    nodeAfter = doc.layers[0].nodes.find((n) => n.id === r.id),
    path = evaluate(doc.layers[0]).run(r.id);
  assert.equal(doc.schemaVersion, Math.max(input.schemaVersion, 12));
  assert.deepEqual(
    activeParameters(nodeAfter).map(([key]) => key),
    ['path', 'spacing'],
  );
  assert.deepEqual(
    path.contours.map((c) => c.points.length),
    [6, 10],
  );
  for (const contour of path.contours) {
    const gaps = contour.points
      .slice(1)
      .map((p, i) =>
        Math.hypot(
          p.position[0] - contour.points[i].position[0],
          p.position[1] - contour.points[i].position[1],
        ),
      );
    assert.ok(gaps.every((gap) => gap <= 24 + 1e-6));
    assert.deepEqual(contour.points[0].position, [0, contour.points.length === 6 ? 0 : 30]);
    assert.deepEqual(contour.points.at(-1).position, [
      contour.points.length === 6 ? 100 : 200,
      contour.points.length === 6 ? 0 : 30,
    ]);
  }
  assert.deepEqual(deserialize(serialize(doc)), doc);
});

test('Resample spacing keeps endpoints, omits closed seam, and applies minimums to degenerate paths', () => {
  const f = scene('M0 0L100 0L100 100L0 100Z');
  f.r.operation = 'spacing';
  f.r.params.spacing.value = 150;
  let contour = evaluate(f.l).run(f.r.id).contours[0];
  assert.equal(contour.points.length, 3);
  assert.notDeepEqual(contour.points[0].position, contour.points.at(-1).position);

  f.s.rawPath = 'M10 20L10 20';
  contour = evaluate(f.l).run(f.r.id).contours[0];
  assert.equal(contour.points.length, 2);
  assert.deepEqual(
    contour.points.map((p) => p.position),
    [
      [10, 20],
      [10, 20],
    ],
  );

  f.s.rawPath = 'M10 20L10 20Z';
  contour = evaluate(f.l).run(f.r.id).contours[0];
  assert.equal(contour.closed, true);
  assert.equal(contour.points.length, 3);
  assert.deepEqual(
    contour.points.map((p) => p.position),
    [
      [10, 20],
      [10, 20],
      [10, 20],
    ],
  );

  f.s.rawPath = '';
  assert.deepEqual(evaluate(f.l).run(f.r.id).contours, []);
  const emptySource = node('Curve');
  emptySource.points = [];
  f.l.nodes.unshift(emptySource);
  connect(f.l, emptySource.id, f.r.id, 'path');
  contour = evaluate(f.l).run(f.r.id).contours[0];
  assert.deepEqual(contour.points, []);
});

test('Resample preflights contour and aggregate budgets before allocating output', () => {
  const f = scene('M0 0L2000 0');
  f.r.operation = 'spacing';
  f.r.params.spacing.value = 1;
  assert.match(evaluate(f.l).error, /1024 samples/);

  f.s.rawPath = Array.from({ length: 98 }, (_, i) => `M0 ${i}L1023 ${i}`).join(' ');
  assert.match(evaluate(f.l).error, /geometry budget/);

  const points = Array.from({ length: 3000 }, (_, i) => `L${i} 0`).join(' ');
  f.s.rawPath = `M0 0${points}`;
  f.r.params.spacing.value = 4;
  assert.match(evaluate(f.l).error, /geometry budget/);
});

test('Resample migrates legacy Boolean operations and spacing defaults, while enforcing schema 12', () => {
  const old = scene(),
    oldDoc = old.doc;
  oldDoc.schemaVersion = 11;
  old.r.operation = 'subtract';
  old.r.params.count.value = 19;
  delete old.r.params.spacing;
  const restored = deserialize(serialize(oldDoc)),
    migrated = restored.layers[0].nodes.find((n) => n.id === old.r.id);
  assert.equal(migrated.operation, 'count');
  assert.equal(migrated.params.count.value, 19);
  assert.equal(migrated.params.spacing.value, 24);

  for (const operation of ['union', 'subtract', 'intersect', 'difference']) {
    const legacy = structuredClone(oldDoc);
    legacy.layers[0].nodes.find((n) => n.id === old.r.id).operation = operation;
    assert.equal(
      deserialize(serialize(legacy)).layers[0].nodes.find((n) => n.id === old.r.id).operation,
      'count',
    );
  }
  const invalid = structuredClone(oldDoc);
  invalid.layers[0].nodes.find((n) => n.id === old.r.id).operation = 'surprise';
  assert.throws(() => deserialize(serialize(invalid)), /Invalid operation/);
  invalid.layers[0].nodes.find((n) => n.id === old.r.id).operation = 'spacing';
  assert.throws(() => deserialize(serialize(invalid)), /schemaVersion 12/);

  const current = scene();
  current.doc.schemaVersion = 12;
  current.r.operation = 'spacing';
  delete current.r.params.spacing;
  assert.throws(() => deserialize(serialize(current.doc)), /Unexpected parameter/);
});

test('Resample preserves dormant fields and ignores dormant expressions until activation', () => {
  const f = scene(),
    value = node('Value');
  value.params.value.value = 40;
  f.l.nodes.push(value);
  f.r.params.spacing.source = value.id;
  f.r.params.spacing.expression = 'invalid()';
  f.r.params.count.expression = '12';
  const before = structuredClone(f.r.params);
  assert.deepEqual(
    activeParameters(f.r).map(([key]) => key),
    ['path', 'count'],
  );
  assert.equal(evaluate(f.l).error, null);

  const session = new CommandSession(layersDoc(f.l)),
    send = (operation) =>
      session.dispatch({
        ...session.state,
        type: 'set-operation',
        payload: { layerId: f.l.id, nodeId: f.r.id, operation },
      });
  send('spacing');
  assert.deepEqual(session.snapshot().layers[0].nodes.find((n) => n.id === f.r.id).params, before);
  send('count');
  const restored = session.snapshot().layers[0].nodes.find((n) => n.id === f.r.id);
  assert.deepEqual(restored.params, before);
  assert.equal(restored.operation, 'count');
  assert.equal(session.snapshot().schemaVersion, Math.max(f.doc.schemaVersion, 12));
});

function layersDoc(layer) {
  const doc = document();
  doc.layers[0] = layer;
  return doc;
}
