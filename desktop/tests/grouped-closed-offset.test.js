import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import opentype from 'opentype.js';
import {
  document,
  node,
  connect,
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  enableRemapPoint,
  History,
  clone,
  assertClosedOffsetChange,
  OFFSET_EDITOR_LIMITS,
} from '../src/core.js';
import { offsetGroupedClosedPathDetailed } from '../src/grouped-closed-offset.js';
import { OffsetError } from '../src/closed-offset.js';

const point = (position, i) => ({
  id: `p${i}`,
  position,
  handleA: [0, 0],
  handleB: [0, 0],
});
const rectangle = (x, y, width = 100, height = 60, reverse = false, group) => {
  const positions = [
    [x, y],
    [x + width, y],
    [x + width, y + height],
    [x, y + height],
  ];
  if (reverse) positions.reverse();
  return {
    id: `rect-${x}-${y}`,
    closed: true,
    ...(group === undefined ? {} : { group }),
    points: positions.map(point),
  };
};
const signedArea = (contour) =>
  contour.points.reduce((sum, a, i) => {
    const b = contour.points[(i + 1) % contour.points.length];
    return sum + a.position[0] * b.position[1] - a.position[1] * b.position[0];
  }, 0) / 2;
const allIds = (path) =>
  path.contours.flatMap((contour) => [contour.id, ...contour.points.map((point) => point.id)]);
const deepFreeze = (value) => {
  if (!value || typeof value !== 'object' || Object.isFrozen(value)) return value;
  Object.values(value).forEach(deepFreeze);
  return Object.freeze(value);
};
const isOffsetLimit = (error) => error instanceof OffsetError && error.code === 'OFFSET_LIMIT';

test('grouped Offset preserves per-group holes, overlaps, deterministic IDs and source immutability', () => {
  const source = {
    contours: [
      rectangle(0, 0, 100, 60, false, ''),
      rectangle(30, 20, 40, 20, true), // Untagged contours coalesce with the empty group.
      rectangle(20, 10, 100, 60, false, 'overlap'),
    ],
  };
  const before = clone(source);
  deepFreeze(source);
  const a = offsetGroupedClosedPathDetailed(source, 5, { join: 'miter', prefix: 'offset' });
  const b = offsetGroupedClosedPathDetailed(source, 5, { join: 'miter', prefix: 'offset' });

  assert.deepEqual(source, before);
  assert.deepEqual(a.path, b.path);
  assert.equal(a.path.contours.length, 3);
  assert.equal(new Set(a.path.contours.map((contour) => contour.group)).size, 2);
  assert.equal(new Set(allIds(a.path)).size, allIds(a.path).length);

  const firstGroup = a.path.contours.filter((contour) => contour.group === 'offset:g0');
  assert.equal(firstGroup.length, 2);
  assert.ok(signedArea(firstGroup[0]) * signedArea(firstGroup[1]) < 0, 'hole winding is retained');
  assert.equal(a.path.contours.filter((contour) => contour.group === 'offset:g1').length, 1);
});

test('touching groups remain independent, while invalid topology inside a group still rejects', () => {
  const touching = {
    contours: [rectangle(0, 0, 100, 60, false, 'left'), rectangle(100, 0, 100, 60, false, 'right')],
  };
  const accepted = offsetGroupedClosedPathDetailed(touching, 5, { prefix: 'touching' });
  assert.equal(accepted.path.contours.length, 2);
  assert.deepEqual(
    new Set(accepted.path.contours.map((contour) => contour.group)),
    new Set(['touching:g0', 'touching:g1']),
  );

  const crossing = {
    contours: [
      {
        id: 'bowtie',
        group: 'bad',
        closed: true,
        points: [
          [0, 0],
          [100, 100],
          [0, 100],
          [100, 0],
        ].map(point),
      },
      rectangle(200, 0, 100, 60, false, 'good'),
    ],
  };
  assert.throws(
    () => offsetGroupedClosedPathDetailed(crossing, 5, { prefix: 'crossing' }),
    (error) =>
      error instanceof OffsetError &&
      ['OFFSET_GEOMETRY', 'OFFSET_UNCERTAIN_TOPOLOGY'].includes(error.code),
  );

  assert.throws(
    () =>
      offsetGroupedClosedPathDetailed(
        { contours: [rectangle(0, 0, 100, 60, false, { invalid: true })] },
        5,
      ),
    /groups must be strings/,
  );
});

test('grouped Offset pre-sums inputs and shares every editor work budget across groups', () => {
  const oneGroup = { contours: [rectangle(0, 0, 100, 60, false, 'one')] };
  const twoGroups = {
    contours: [rectangle(0, 0, 100, 60, false, 'one'), rectangle(150, 0, 100, 60, false, 'two')],
  };
  const one = offsetGroupedClosedPathDetailed(oneGroup, 5, { prefix: 'one' });
  const both = offsetGroupedClosedPathDetailed(twoGroups, 5, { prefix: 'both' });

  assert.equal(both.diagnostics.groups, 2);
  for (const key of [
    'flattenedVertices',
    'subdivisionNodes',
    'topologyPairChecks',
    'estimatedJoinVertices',
    'outputVertices',
  ]) {
    const limitKey = key === 'topologyPairChecks' ? 'pairChecks' : key;
    assert.ok(both.diagnostics[key] <= OFFSET_EDITOR_LIMITS[limitKey], key);
    assert.ok(one.diagnostics[key] > 0, key);
    assert.throws(
      () =>
        offsetGroupedClosedPathDetailed(
          twoGroups,
          5,
          { prefix: 'limited' },
          {
            [limitKey]: one.diagnostics[key],
          },
        ),
      isOffsetLimit,
      `${key} must not reset for the second group`,
    );
  }

  assert.throws(
    () => offsetGroupedClosedPathDetailed(twoGroups, 5, {}, { contours: 1 }),
    isOffsetLimit,
  );
  assert.throws(
    () => offsetGroupedClosedPathDetailed(twoGroups, 5, {}, { sourcePoints: 7 }),
    isOffsetLimit,
  );

  const fullThenEmpty = {
    contours: [rectangle(0, 0, 100, 60, false, 'large'), rectangle(200, 0, 4, 4, false, 'small')],
  };
  assert.throws(
    () => offsetGroupedClosedPathDetailed(fullThenEmpty, -5, {}, { outputVertices: 4 }),
    isOffsetLimit,
    'an exhausted output budget rejects the next group even if its erosion would be empty',
  );

  const eroded = offsetGroupedClosedPathDetailed(twoGroups, -100, { prefix: 'eroded' });
  assert.deepEqual(eroded.path.contours, []);
  assert.equal(eroded.diagnostics.outputVertices, 0);
});

function groupedGraph() {
  const doc = document(),
    layer = doc.layers[0],
    first = node('Rectangle'),
    second = node('Rectangle'),
    join = node('Join Geometry'),
    offset = node('Closed Path Offset'),
    remap = node('Path Remap'),
    fill = node('Fill'),
    output = node('Output');
  first.params.center.value = [50, 30];
  first.params.size.value = [100, 60];
  second.params.center.value = [95, 30];
  second.params.size.value = [100, 60];
  layer.nodes = [first, second, join, offset, remap, fill, output];
  layer.output = output.id;
  connect(layer, first.id, join.id, 'a');
  connect(layer, second.id, join.id, 'b');
  connect(layer, join.id, offset.id, 'path');
  connect(layer, offset.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, first, second, join, offset, remap, fill };
}

test('Join groups offset independently through evaluation, Remap provenance, serialization and history', () => {
  const { doc, layer, first, join, offset, remap } = groupedGraph();
  const evaluated = evaluate(layer);
  assert.equal(evaluated.error, null);
  assert.equal(evaluated.run(join.id).contours.length, 2);
  const joined = evaluated.run(offset.id);
  assert.equal(joined.contours.length, 2, 'overlapping input groups do not union');
  assert.equal(new Set(joined.contours.map((contour) => contour.group)).size, 2);

  const roundtrip = deserialize(serialize(doc));
  const restored = evaluate(roundtrip.layers[0]);
  assert.equal(restored.error, null);
  assert.deepEqual(restored.run(offset.id).contours, joined.contours);
  assert.equal(exportSVG(roundtrip), exportSVG(doc));

  enableRemapPoint(remap, joined, joined.contours[0].points[0].id).params.position.value = [4, 0];
  assert.equal(evaluate(layer).warnings.length, 0);
  const old = clone(layer);
  first.params.center.value[0] += 1;
  assert.match(evaluate(layer).warnings.join(' '), /suspended/);
  assert.match(evaluate(deserialize(serialize(doc)).layers[0]).warnings.join(' '), /suspended/);
  Object.assign(layer, old);

  const historyDoc = clone(doc),
    history = new History(historyDoc),
    live = history.doc.layers[0],
    initial = serialize(history.doc),
    previous = clone(live);
  history.apply(() => {
    live.nodes.find((item) => item.id === offset.id).params.amount.value = 5;
    assertClosedOffsetChange(live, previous);
  });
  assert.equal(history.past.length, 1);
  history.undo();
  assert.equal(serialize(history.doc), initial);
  history.redo();
  assert.equal(history.past.length, 1);

  const beforeFailure = serialize(history.doc),
    priorFailure = clone(history.doc.layers[0]),
    priorHistory = history.past.length;
  assert.throws(
    () =>
      history.apply(() => {
        history.doc.layers[0].nodes.find((item) => item.id === first.id).params.size.value = [
          1e9, 60,
        ];
        assertClosedOffsetChange(history.doc.layers[0], priorFailure);
      }),
    /safe integer envelope/,
  );
  assert.equal(serialize(history.doc), beforeFailure);
  assert.equal(history.past.length, priorHistory);
});

test('Text stays one ungrouped global CompoundPath with a single provenance route', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const font = opentype.parse(
    bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
  );
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    offset = node('Closed Path Offset'),
    fill = node('Fill'),
    output = node('Output');
  text.text = 'II';
  layer.nodes = [text, offset, fill, output];
  layer.output = output.id;
  connect(layer, text.id, offset.id, 'path');
  connect(layer, offset.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');

  const evaluated = evaluate(layer, { default: font });
  assert.equal(evaluated.error, null);
  const source = evaluated.run(text.id),
    result = evaluated.run(offset.id);
  assert.equal(source.contours.length, 2);
  assert.ok(source.contours.every((contour) => contour.group === undefined));
  assert.ok(result.contours.every((contour) => contour.group === undefined));
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${offset.id}`);
});
