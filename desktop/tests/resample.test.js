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
test('Resample zero-length and empty paths remain finite', () => {
  const f = scene('M10 20L10 20');
  const result = evaluate(f.l).run(f.r.id);
  result.contours[0].points.forEach((p) => near(p.position, [10, 20]));
  f.s.rawPath = '';
  assert.deepEqual(evaluate(f.l).run(f.r.id).contours, []);
});
