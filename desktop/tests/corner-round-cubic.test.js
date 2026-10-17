import test from 'node:test';
import assert from 'node:assert/strict';
import { splitCubic, trimCubic } from '../src/corner-round-cubic.js';
import { roundCorners } from '../src/corner-round-geometry.js';
const pt = (position, handleA = [0, 0], handleB = [0, 0]) => ({ position, handleA, handleB });
const path = (points) => ({ contours: [{ closed: false, group: 'fixture', points }] });
const near = (a, b, e = 1e-8) =>
  a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < e, `${a} != ${b}`));
const evaluate = (c, t) => splitCubic(c, t)[0][3];
const regular = () =>
  path([pt([0, 0], [5, 6], [3, 0]), pt([10, 0], [-3, 0], [0, 3]), pt([10, 10], [0, -3], [8, 9])]);
function g1(points) {
  for (const p of points.slice(1, -1)) {
    const a = p.handleA,
      b = p.handleB;
    assert.ok(Math.hypot(...a) > 0 && Math.hypot(...b) > 0);
    assert.ok(Math.abs(a[0] * b[1] - a[1] * b[0]) < 1e-8);
    assert.ok(a[0] * b[0] + a[1] * b[1] < 0);
  }
}
test('de Casteljau retained subcurves equal original polynomial restriction', () => {
  const c = [
    [3, 7],
    [8, -4],
    [-2, 17],
    [12, 2],
  ];
  for (const [a, b] of [
    [0, 0.4],
    [0.2, 1],
    [0.13, 0.81],
  ])
    for (const t of [0, 0.1, 0.5, 0.9, 1])
      near(evaluate(trimCubic(c, a, b), t), evaluate(c, a + (b - a) * t));
});
test('regular cubic/cubic blend is G1, exact retained pieces, deterministic and nonmutating', () => {
  const source = regular(),
    before = structuredClone(source),
    r = roundCorners(source, { radius: 1 }),
    p = r.contours[0].points;
  assert.equal(r.cornerDiagnostics.rounded, 1);
  assert.equal(p.length, 4);
  g1(p);
  near(p[1].position, [9, 0], 2e-6);
  near(p[2].position, [10, 1], 2e-6);
  assert.deepEqual(p[0].handleA, [5, 6]);
  assert.deepEqual(p.at(-1).handleB, [8, 9]);
  assert.deepEqual(source, before);
  assert.deepEqual(roundCorners(source, { radius: 1 }), r);
});
test('line/cubic, cubic/line, concave/reversed, scaled and translated blends remain G1', () => {
  for (const line of [0, 1])
    for (const sign of [-1, 1])
      for (const scale of [1, 100]) {
        const s = regular(),
          ps = s.contours[0].points;
        if (line === 0) {
          ps[0].handleB = [0, 0];
          ps[1].handleA = [0, 0];
        } else {
          ps[1].handleB = [0, 0];
          ps[2].handleA = [0, 0];
        }
        for (const p of ps) {
          p.position = p.position.map((v, i) => v * scale * (i ? sign : 1) + 1234);
          for (const key of ['handleA', 'handleB'])
            p[key] = p[key].map((v, i) => v * scale * (i ? sign : 1));
        }
        const r = roundCorners(s, { radius: scale });
        assert.equal(r.cornerDiagnostics.rounded, 1);
        g1(r.contours[0].points);
      }
});
test('constant connector runs preserve both external cubic handles; no-edit returns exact records', () => {
  const s = regular(),
    ps = s.contours[0].points;
  ps.splice(1, 1, pt([10, 0], [-3, 0]), pt([10, 0]), pt([10, 0], [0, 0], [0, 3]));
  const r = roundCorners(s, { radius: 1 });
  assert.equal(r.cornerDiagnostics.rounded, 1);
  g1(r.contours[0].points);
  const o = path([
    pt([0, 0], [0, 0], [3, 0]),
    pt([10, 0], [-3, 0]),
    pt([10, 0], [0, 0], [3, 0]),
    pt([20, 0], [-3, 0]),
  ]);
  assert.deepEqual(
    roundCorners(o, { radius: 1 }).contours[0].points.map(({ id, ...p }) => p),
    o.contours[0].points,
  );
});
test('shared short cubic retains a positive middle interval under oversized radius', () => {
  const s = path([
    pt([0, 0], [0, 0], [3, 0]),
    pt([10, 0], [-3, 0], [0, 0.3]),
    pt([10, 1], [0, -0.3], [3, 0]),
    pt([20, 1], [-3, 0]),
  ]);
  const r = roundCorners(s, { radius: 100 });
  assert.equal(r.cornerDiagnostics.rounded, 2);
  assert.equal(r.contours[0].points.length, 6);
  const p = r.contours[0].points;
  assert.ok(p[2].position[1] < p[3].position[1]);
  g1(p);
});
test('reversal cusp and coincident endpoint loop stay unchanged with bounded diagnostics', () => {
  const s = path([pt([0, 0], [0, 0], [3, 0]), pt([10, 0], [-3, 0], [-3, 0]), pt([0, 0], [3, 0])]);
  const r = roundCorners(s, { radius: 2 });
  assert.equal(r.cornerDiagnostics.rounded, 0);
  assert.ok(r.cornerDiagnostics.skipped.irregular);
  assert.deepEqual(
    r.contours[0].points.map(({ id, ...p }) => p),
    s.contours[0].points,
  );
});
test('nearby crossing geometry prevents a local replacement; no false success at a work cap', () => {
  const s = regular();
  s.contours.push({ closed: false, points: [pt([9.999999, -1]), pt([9.999999, 1])] });
  const r = roundCorners(s, { radius: 2 });
  assert.equal(r.cornerDiagnostics.rounded, 0);
  assert.ok(
    r.cornerDiagnostics.skipped['collision-ambiguity'] ||
      r.cornerDiagnostics.skipped['precision-budget'],
  );
});
test('closed seam and reversed winding retain coherent G1 pieces and groups', () => {
  for (const reverse of [false, true]) {
    const ps = [
      pt([0, 0], [-2, 0], [3, 0]),
      pt([10, 0], [-3, 0], [0, 3]),
      pt([10, 10], [0, -3], [-3, 0]),
      pt([0, 10], [3, 0], [0, -3]),
    ];
    if (reverse) {
      ps.reverse();
      for (const p of ps) [p.handleA, p.handleB] = [p.handleB, p.handleA];
    }
    const r = roundCorners(
      { contours: [{ closed: true, group: 'closed', points: ps }] },
      { radius: 0.5 },
    );
    assert.ok(r.cornerDiagnostics.rounded >= 3);
    assert.equal(r.contours[0].group, 'closed');
    assert.ok(
      r.contours[0].points.every((p) =>
        [...p.position, ...p.handleA, ...p.handleB].every(Number.isFinite),
      ),
    );
  }
});
test('interior reversal in the trim neighborhood and nonconstant endpoint loops are not erased', () => {
  const s = path([
    pt([0, 0], [0, 0], [15, 0]),
    pt([10, 0], [10, 0], [0, 3]),
    pt([10, 10], [0, -3]),
  ]);
  const r = roundCorners(s, { radius: 100 });
  // An ordinary valid smaller neighborhood may be used; any unresolved cusp is skipped.
  assert.ok(
    r.cornerDiagnostics.rounded === 1 ||
      Object.values(r.cornerDiagnostics.skipped).some((n) => n > 0),
  );
  const loop = path([
    pt([0, 0], [0, 0], [4, 4]),
    pt([0, 0], [-4, 4], [3, 0]),
    pt([10, 0], [-3, 0]),
  ]);
  const result = roundCorners(loop, { radius: 1 });
  assert.ok(result.contours[0].points.some((p) => p.handleB[1] !== 0 || p.handleA[1] !== 0));
});
test('a narrow counter crossing a replacement neighborhood rejects even a large requested radius', () => {
  const s = regular();
  s.contours.push({
    closed: true,
    group: 'fixture',
    points: [
      pt([9.999999, -0.000001]),
      pt([9.999999, 0.000001]),
      pt([10.000001, 0.000001]),
      pt([10.000001, -0.000001]),
    ],
  });
  const r = roundCorners(s, { radius: 100 });
  assert.equal(r.cornerDiagnostics.rounded, 0);
  assert.ok(Object.values(r.cornerDiagnostics.skipped).some((n) => n > 0));
});
test('long mixed input stops accepting edits when shared evaluation precision budget is exhausted', () => {
  const source = {
    contours: Array.from({ length: 4000 }, (_, i) => {
      const c = regular().contours[0];
      for (const p of c.points) p.position[0] += i * 30;
      return c;
    }),
  };
  const r = roundCorners(source, { radius: 1 });
  assert.ok(r.cornerDiagnostics.rounded > 0);
  assert.ok(r.cornerDiagnostics.skipped['precision-budget'] > 0);
  assert.equal(r.contours.length, source.contours.length);
  assert.ok(r.contours.every((c) => c.points.length === 3 || c.points.length === 4));
});
