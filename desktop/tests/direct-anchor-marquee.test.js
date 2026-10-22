import test from 'node:test';
import assert from 'node:assert/strict';
import { anchorMarqueeIds } from '../src/direct-anchor-marquee.js';
import { pointControlTargets } from '../src/point-controls.js';
import { directSelectionSource, transformDirectPath } from '../src/direct-selection.js';
import { serialize } from '../src/core.js';
import { directMarqueeFixture } from './fixtures/direct-anchor-marquee.js';

test('anchor centers are inclusive, direction independent, handle/segment/bounds independent', () => {
  const targets = [
    { pointId: 'b', key: 'position', position: [20, 20] },
    { pointId: 'a', key: 'position', position: [10, 10] },
    { pointId: 'c', key: 'position', position: [30, 30] },
    { pointId: 'c', key: 'handleA', position: [15, 15] },
  ];
  assert.deepEqual(anchorMarqueeIds(targets, [10, 10], [20, 20]), ['b', 'a']);
  assert.deepEqual(anchorMarqueeIds(targets, [20, 20], [10, 10]), ['b', 'a']);
  assert.deepEqual(anchorMarqueeIds(targets, [12, 12], [18, 18]), []);
  assert.deepEqual(anchorMarqueeIds(targets, [12, 12], [18, 18], ['a', 'unknown']), ['a']);
  assert.deepEqual(anchorMarqueeIds(targets, [19, 19], [21, 21], ['a', 'unknown']), ['b', 'a']);
  assert.deepEqual(anchorMarqueeIds(targets, [NaN, 0], [20, 20], ['c', 'unknown']), ['c']);
});

test('resolved rotation and nonuniform scale use displayed anchor centers, not raw authored coordinates', () => {
  const f = directMarqueeFixture({ transformed: true }),
    before = serialize(f.doc),
    route = directSelectionSource(f.doc, f.layer.id),
    targets = pointControlTargets(route.path, 1, ['anchor-0']);
  assert.equal(route.supported, true, route.reason);
  assert.deepEqual(route.route, [f.out.id, f.fill.id, f.transform.id, f.curve.id]);
  const a = targets.find((t) => t.key === 'position' && t.pointId === 'anchor-0').position;
  assert.deepEqual(
    anchorMarqueeIds(
      targets,
      a.map((v) => v - 1),
      a.map((v) => v + 1),
    ),
    ['anchor-0'],
  );
  assert.deepEqual(anchorMarqueeIds(targets, [99, 99], [101, 101]), []);
  assert.equal(serialize(f.doc), before);
});

test('affine target positions and handles keep identity under rotation, reflection and nonuniform scale', () => {
  const path = {
      contours: [
        { id: 'c', points: [{ id: 'p', position: [-4, 3], handleA: [6, 0], handleB: [0, 7] }] },
      ],
    },
    transformed = transformDirectPath(path, [0, 2, -3, 0, 100, 50]),
    targets = pointControlTargets(transformed, 2, ['p']);
  assert.deepEqual(transformed.contours[0].points[0].position, [91, 42]);
  assert.deepEqual(anchorMarqueeIds(targets, [90, 41], [92, 43]), ['p']);
  assert.deepEqual(anchorMarqueeIds(targets, [90, 53], [92, 55]), []);
  assert.deepEqual(path.contours[0].points[0].position, [-4, 3]);
});
