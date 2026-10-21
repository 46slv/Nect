import test from 'node:test';
import assert from 'node:assert/strict';
import { node, serialize } from '../src/core.js';
import { selectedPointBounds } from '../src/selected-point-bounds.js';
import { compoundFixture } from './fixtures/contour-navigation.js';

function fixture() {
  const f = compoundFixture([8, 5, 7, 4]);
  f.ids = [
    f.path.contours[0].points[1],
    f.path.contours[1].points[2],
    f.path.contours[3].points[0],
  ];
  [
    [-12, 4],
    [8, -6],
    [2, 18],
  ].forEach((value, i) => {
    const p = f.path.points.find((p) => p.id === f.ids[i]);
    p.position.value = value;
    p.handleA.value = [-10000, 50000];
    p.handleB.value = [10000, -50000];
    p.handleA.locked = true;
  });
  return f;
}

test('selected anchors use stable membership across holes/open contours and ignore handles/unselected geometry', () => {
  const f = fixture(),
    before = serialize(f.doc);
  const summary = selectedPointBounds(f.path, new Set(f.ids));
  assert.deepEqual(summary, {
    state: 'ready',
    count: 3,
    values: {
      minX: -12,
      maxX: 8,
      width: 20,
      minY: -6,
      maxY: 18,
      height: 24,
    },
  });
  f.path.points.reverse();
  assert.deepEqual(selectedPointBounds(f.path, [...f.ids].reverse()), summary);
  f.path.points.reverse();
  assert.equal(serialize(f.doc), before);
});

test('single/coincident anchors and very small values remain valid without misleading zero dimensions', () => {
  const f = fixture(),
    point = f.path.points.find((p) => p.id === f.ids[0]);
  point.position.value = [1e-20, -1e-20];
  const one = selectedPointBounds(f.path, [point.id]);
  assert.deepEqual(one.values, {
    minX: 1e-20,
    maxX: 1e-20,
    width: 0,
    minY: -1e-20,
    maxY: -1e-20,
    height: 0,
  });
  f.path.points.find((p) => p.id === f.ids[1]).position.value = [2e-20, -1e-20];
  assert.equal(selectedPointBounds(f.path, f.ids.slice(0, 2)).values.width, 1e-20);
});

test('empty and every ineligible selection produce a reason and no partial numeric values', async (t) => {
  assert.deepEqual(selectedPointBounds(null, []), {
    state: 'empty',
    count: 0,
    reason: 'No points selected',
  });
  for (const [name, change, reason] of [
    [
      'missing owner',
      (f) => {
        f.path = null;
      },
      /Owned/,
    ],
    [
      'generated owner',
      (f) => {
        f.path = node('Path Remap');
      },
      /Owned/,
    ],
    [
      'missing member',
      (f) => {
        f.ids.push('missing');
      },
      /changed/,
    ],
    [
      'duplicate member',
      (f) => {
        f.ids.push(f.ids[0]);
      },
      /changed/,
    ],
    [
      'locked node',
      (f) => {
        f.path.locked = true;
      },
      /Locked/,
    ],
    [
      'bypassed',
      (f) => {
        f.path.bypassed = true;
      },
      /Bypassed/,
    ],
    [
      'locked point',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.locked = true;
      },
      /Locked/,
    ],
    [
      'expression',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.expression = 'value; value';
      },
      /Driven/,
    ],
    [
      'connection',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.source = 'supplier';
      },
      /Driven/,
    ],
    [
      'NaN',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.value[0] = NaN;
      },
      /Invalid/,
    ],
    [
      'type',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[2]).position.type = 'Scalar';
      },
      /Invalid/,
    ],
    [
      'overflow',
      (f) => {
        f.path.points.find((p) => p.id === f.ids[0]).position.value = [-Number.MAX_VALUE, 0];
        f.path.points.find((p) => p.id === f.ids[1]).position.value = [Number.MAX_VALUE, 0];
      },
      /range/,
    ],
  ])
    await t.test(name, () => {
      const f = fixture();
      change(f);
      const before = structuredClone(f);
      const result = selectedPointBounds(f.path, f.ids);
      assert.equal(result.state, 'unavailable');
      assert.equal(result.count, f.ids.length);
      assert.match(result.reason, reason);
      assert.equal(Object.hasOwn(result, 'values'), false);
      assert.deepEqual(f, before);
    });
  const f = fixture();
  assert.match(selectedPointBounds(f.path, f.ids, true).reason, /Locked/);
});

test('large selections use the existing bounds helper and preserve exact extrema under cancellation', () => {
  const curve = node('Curve'),
    template = curve.points[0];
  curve.points = Array.from({ length: 10000 }, (_, i) => ({
    ...structuredClone(template),
    id: `p${i}`,
    position: { ...structuredClone(template.position), value: [i - 5000, i / 10] },
  }));
  const summary = selectedPointBounds(
    curve,
    curve.points.map((p) => p.id),
  );
  assert.equal(summary.count, 10000);
  assert.deepEqual(summary.values, {
    minX: -5000,
    maxX: 4999,
    width: 9999,
    minY: 0,
    maxY: 999.9,
    height: 999.9,
  });
  curve.points[0].position.value = [-1e16, 0];
  curve.points[1].position.value = [1, 0];
  assert.equal(selectedPointBounds(curve, ['p0', 'p1']).values.maxX, 1);
});
