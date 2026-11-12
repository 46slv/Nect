import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluate, node, point } from '../src/core.js';
import { prepareLiteralMacroCandidate } from '../src/macro-execution.js';
import { setMacroOverride } from '../src/macro-package.js';
import { assertMacroPathStep, macroTransformSteps } from '../src/macro-transform-steps.js';
import { transformValues } from '../src/transform-affine.js';

function fixture() {
  const definition = {
      format: 'nect.macro',
      formatVersion: 1,
      id: 'double',
      revision: 2,
      name: 'Two transforms',
      input: { type: 'Path', nodeId: 'a', socket: 'path' },
      output: { type: 'Path', nodeId: 'b', port: null },
      nodes: [
        {
          id: 'b',
          type: 'Transform',
          params: { anchor: [0, 0], offset: [4, -2], scale: 2, rotation: 0 },
        },
        {
          id: 'a',
          type: 'Transform',
          params: { anchor: [0, 0], offset: [10, 20], scale: 1, rotation: 0 },
        },
      ],
      edges: [{ sourceNodeId: 'a', sourcePort: null, targetNodeId: 'b', targetSocket: 'path' }],
      parameters: [
        {
          id: 'angle',
          label: 'Angle',
          nodeId: 'a',
          key: 'rotation',
          type: 'Scalar',
          defaultValue: 90,
        },
      ],
    },
    instance = { id: 'instance', definitionId: 'double', definitionRevision: 2, overrides: {} },
    source = node('Curve');
  source.points = [point(1, 2), point(20, 2), point(20, 5)];
  source.points[0].handleA.value = [-2, 0];
  source.points[0].handleB.value = [3, 1];
  return { definition, instance, source };
}

test('resolved steps match the actual connected Transform chain without mutating inputs', () => {
  const { definition, instance, source } = fixture(),
    before = structuredClone({ definition, instance, source }),
    probe = prepareLiteralMacroCandidate(source, definition, instance),
    frame = evaluate(probe.document.layers[0], {}, {}, probe.document, null, true),
    transforms = [probe.firstNodeId, probe.outputNodeId].map((id) =>
      probe.document.layers[0].nodes.find((item) => item.id === id),
    ),
    paths = [frame.run(probe.sourceNodeId), frame.run(probe.firstNodeId)],
    steps = macroTransformSteps(definition, instance);
  assert.deepEqual(
    steps.map((step) => step.id),
    ['a', 'b'],
  );
  for (const [index, step] of steps.entries()) {
    const actual = transforms[index];
    assert.deepEqual(
      step.matrix,
      transformValues(actual, (key) => actual.params[key].value).matrix,
    );
    const pathBefore = structuredClone(paths[index]);
    assert.doesNotThrow(() => assertMacroPathStep(paths[index], step.matrix));
    assert.deepEqual(paths[index], pathBefore);
  }
  assert.deepEqual({ definition, instance, source }, before);
});

test('explicit zero overrides resolve separately for each pinned instance', () => {
  const { definition, instance } = fixture(),
    changed = setMacroOverride(definition, instance, 'angle', 0),
    original = macroTransformSteps(definition, instance),
    edited = macroTransformSteps(definition, changed);
  assert.notDeepEqual(original[0].matrix, edited[0].matrix);
  assert.deepEqual(original[1], edited[1]);
  assert.deepEqual(instance.overrides, {});
  assert.throws(
    () => macroTransformSteps(definition, { ...instance, definitionRevision: 3 }),
    /unpinned|invalid/i,
  );
});

test('each actual Path and handle endpoint must remain finite before a Transform step', () => {
  const identity = [1, 0, 0, 1, 0, 0],
    double = [2, 0, 0, 2, 0, 0],
    path = {
      contours: [
        {
          id: 'contour',
          points: [{ id: 'p', position: [1e308, 0], handleA: [0, 0], handleB: [0, 0] }],
        },
      ],
    },
    before = structuredClone(path);
  assert.doesNotThrow(() => assertMacroPathStep(path, identity));
  assert.throws(() => assertMacroPathStep(path, double), /finite/i);
  assert.deepEqual(path, before);
  path.contours[0].points[0].handleB = [1e308, 0];
  assert.throws(() => assertMacroPathStep(path, identity), /finite/i);
  assert.throws(() => assertMacroPathStep(path, [0, 0, 0, 0, 0, 0]), /finite/i);
  const sparse = Array(6);
  assert.throws(() => assertMacroPathStep(before, sparse), /finite/i);
});
