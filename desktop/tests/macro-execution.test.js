import test from 'node:test';
import assert from 'node:assert/strict';
import { deserialize, evaluate, node, point, serialize } from '../src/core.js';
import { prepareLiteralMacroCandidate } from '../src/macro-execution.js';
import { setMacroOverride } from '../src/macro-package.js';

function definition() {
  return {
    format: 'nect.macro',
    formatVersion: 1,
    id: 'macro-double',
    revision: 1,
    name: 'Double Transform',
    input: { type: 'Path', nodeId: 'local-a', socket: 'path' },
    output: { type: 'Path', nodeId: 'local-b', port: null },
    nodes: [
      {
        id: 'local-b',
        type: 'Transform',
        params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 0 },
      },
      {
        id: 'local-a',
        type: 'Transform',
        params: { anchor: [0, 0], offset: [10, 20], scale: 1, rotation: 0 },
      },
    ],
    edges: [
      { sourceNodeId: 'local-a', sourcePort: null, targetNodeId: 'local-b', targetSocket: 'path' },
    ],
    parameters: [
      {
        id: 'angle',
        label: 'Angle',
        nodeId: 'local-a',
        key: 'rotation',
        type: 'Scalar',
        defaultValue: 90,
      },
    ],
  };
}
const instance = () => ({
  id: 'instance-1',
  definitionId: 'macro-double',
  definitionRevision: 1,
  overrides: {},
});
function curve() {
  const source = node('Curve');
  source.points = [point(1, 2), point(20, 2), point(20, 5)];
  source.points[0].handleA.value = [-2, 0];
  source.points[0].handleB.value = [3, 1];
  return source;
}
const close = (actual, expected) =>
  assert.ok(Math.abs(actual - expected) < 1e-8, `${actual} != ${expected}`);

test('Detached execution follows the existing Transform evaluator and preserves real Path IDs', () => {
  const source = curve(),
    def = definition(),
    inst = instance(),
    before = structuredClone({ source, def, inst }),
    result = prepareLiteralMacroCandidate(source, def, inst),
    layer = result.document.layers[0],
    output = result.path.contours[0];
  assert.deepEqual({ source, def, inst }, before);
  assert.deepEqual(
    output.points.map((item) => item.id),
    source.points.map((item) => item.id),
  );
  assert.equal(output.closed, true);
  close(output.points[0].position[0], 8);
  close(output.points[0].position[1], 21);
  close(output.points[0].handleA[0], 0);
  close(output.points[0].handleA[1], -2);
  close(output.points[0].handleB[0], -1);
  close(output.points[0].handleB[1], 3);
  const actual = evaluate(layer, {}, {}, result.document, null, true).run(result.outputNodeId);
  assert.deepEqual(actual, result.path);
  const reopened = deserialize(serialize(result.document)),
    reopenedPath = evaluate(reopened.layers[0], {}, {}, reopened, null, true).run(
      result.outputNodeId,
    );
  assert.deepEqual(reopenedPath, result.path);
  result.document.layers[0].nodes.find((item) => item.id === source.id).name = 'Changed copy';
  assert.equal(source.name, before.source.name);
});

test('A literal Rectangle uses the same Path evaluation and retains generated point IDs', () => {
  const source = node('Rectangle'),
    def = definition(),
    inst = instance();
  source.params.center.value = [25, 40];
  source.params.size.value = [18, 12];
  const before = structuredClone({ source, def, inst }),
    result = prepareLiteralMacroCandidate(source, def, inst),
    frame = evaluate(result.document.layers[0], {}, {}, result.document, null, true),
    original = frame.run(result.sourceNodeId),
    output = result.path;
  assert.deepEqual({ source, def, inst }, before);
  assert.equal(output.contours.length, original.contours.length);
  for (const [contourIndex, contour] of original.contours.entries()) {
    const next = output.contours[contourIndex];
    assert.equal(next.id, contour.id);
    for (const [pointIndex, originalPoint] of contour.points.entries()) {
      const transformed = next.points[pointIndex];
      assert.equal(transformed.id, originalPoint.id);
      close(transformed.position[0], -originalPoint.position[1] + 10);
      close(transformed.position[1], originalPoint.position[0] + 20);
    }
  }
  const driven = structuredClone(source);
  driven.params.size.source = 'outside';
  assert.throws(() => prepareLiteralMacroCandidate(driven, def, inst), /driven/i);
});

test('A valid source may omit unused rawPath, but a nonempty one is refused', () => {
  for (const source of [curve(), node('Rectangle')]) {
    delete source.rawPath;
    const before = structuredClone(source);
    const result = prepareLiteralMacroCandidate(source, definition(), instance());
    assert.ok(result.path.contours.length);
    assert.deepEqual(source, before);
    source.rawPath = 'M 0 0 L 1 1';
    assert.throws(() => prepareLiteralMacroCandidate(source, definition(), instance()));
  }
});

test('An override creates a new detached evaluation without mutating either instance or source', () => {
  const source = curve(),
    def = definition(),
    first = instance(),
    second = setMacroOverride(def, first, 'angle', 0),
    rotated = prepareLiteralMacroCandidate(source, def, first),
    unrotated = prepareLiteralMacroCandidate(source, def, second);
  close(rotated.path.contours[0].points[0].position[0], 8);
  close(unrotated.path.contours[0].points[0].position[0], 11);
  close(unrotated.path.contours[0].points[0].position[1], 22);
  assert.deepEqual(first.overrides, {});
  assert.deepEqual(second.overrides, { angle: 0 });
  assert.notEqual(rotated.firstNodeId, unrotated.firstNodeId);
  assert.deepEqual(
    rotated.path.contours[0].points.map((item) => item.id),
    unrotated.path.contours[0].points.map((item) => item.id),
  );
});

test('A finite Transform matrix that would overflow the connected Path is refused atomically', () => {
  const source = curve(),
    def = definition(),
    inst = instance();
  source.points[0].position.value = [1e308, 0];
  def.parameters[0].key = 'scale';
  def.parameters[0].defaultValue = 2;
  const before = structuredClone({ source, def, inst });
  assert.throws(() => prepareLiteralMacroCandidate(source, def, inst), /finite coordinates/i);
  assert.deepEqual({ source, def, inst }, before);
  const safe = setMacroOverride(def, inst, 'angle', 0);
  const result = prepareLiteralMacroCandidate(source, def, safe);
  assert.equal(result.path.contours[0].points[0].position[0], 10);
});

test('An overflow from a handle endpoint or from the second Transform is refused', () => {
  {
    const source = curve();
    source.points[0].position.value = [1e308, 0];
    source.points[0].handleB.value = [1e308, 0];
    assert.throws(() => prepareLiteralMacroCandidate(source, definition(), instance()), /finite/i);
  }
  {
    const source = curve(),
      def = definition();
    source.points[0].position.value = [1e308, 0];
    def.parameters[0].defaultValue = 0;
    def.nodes.find((item) => item.id === 'local-b').params.scale = 2;
    assert.throws(() => prepareLiteralMacroCandidate(source, def, instance()), /finite/i);
  }
});

test('Driven, external and unsupported sources are rejected without changing source or instance', () => {
  for (const edit of [
    (source) => (source.params.closed.expression = 'true'),
    (source) => (source.points[0].position.source = 'foreign-node'),
    (source) => (source.reference = { nodeId: 'outside' }),
    (source) => (source.asset = 'image-outside'),
    (source) => (source.type = 'Image'),
    (source) => (source.bypassed = true),
  ]) {
    const source = curve(),
      def = definition(),
      inst = instance();
    edit(source);
    const before = structuredClone({ source, def, inst });
    assert.throws(() => prepareLiteralMacroCandidate(source, def, inst));
    assert.deepEqual({ source, def, inst }, before);
  }
});

test('Source accessors are refused without being invoked', () => {
  const source = curve();
  let called = false;
  Object.defineProperty(source, 'name', {
    enumerable: true,
    get() {
      called = true;
      return 'unsafe';
    },
  });
  assert.throws(
    () => prepareLiteralMacroCandidate(source, definition(), instance()),
    /plain data/i,
  );
  assert.equal(called, false);
});

test('A mismatched definition pin cannot produce a detached candidate', () => {
  const source = curve(),
    def = definition(),
    inst = { ...instance(), definitionRevision: 2 },
    before = structuredClone({ source, def, inst });
  assert.throws(() => prepareLiteralMacroCandidate(source, def, inst), /unpinned/i);
  assert.deepEqual({ source, def, inst }, before);
});
