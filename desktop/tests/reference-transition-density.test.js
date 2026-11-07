import test, { mock } from 'node:test';
import assert from 'node:assert/strict';
import {
  referenceEditFixture,
  measureReferenceEdits,
} from '../scripts/bench-cross-composition-edit.mjs';
import { parameters, evaluate } from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

test('dense reference fixture has empty point expressions before its terminal live reference', () => {
  const project = referenceEditFixture(128),
    curve = project.compositions[1].layers[0].nodes.find((n) => n.id === 'source-curve'),
    pointParameters = parameters(curve).filter(([key]) => key.startsWith('point-'));
  assert.equal(pointParameters.length, 128 * 3);
  assert.ok(pointParameters.every(([, q]) => q.expression === ''));
  assert.equal(curve.points.at(-1).id, 'point-127');
});

test('dense empty-expression scan preserves foreign terminal point, Undo/Redo and reopen', () => {
  const result = measureReferenceEdits(128, 3);
  assert.equal(result.count, 128);
  assert.equal(result.value, 130);
  assert.equal(result.samples.length, 3);
  assert.ok(result.samples.every((sample) => Number.isFinite(sample) && sample >= 0));
  // Deliberately no wall-clock threshold: semantic correctness is a CI gate,
  // while recorded before/after measurements establish performance separately.
});

test('measurement fixture rejects unbounded inputs without allocating them', () => {
  for (const count of [0, -1, 10001, 1.5, NaN]) assert.throws(() => referenceEditFixture(count));
  for (const samples of [0, 1, 21, 1.5]) assert.throws(() => measureReferenceEdits(1, samples));
});

const send = (owner, type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
const ref =
  'ref(componentComposition, "comp-source", "art-source", "source-curve", "point-127.position", "x")';
const read = (owner) => {
  const doc = owner.snapshot();
  return evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(
    'reference-sink',
    'value',
  );
};
const bind = (owner, expression) =>
  send(owner, 'set-parameter-expression', {
    layerId: 'art-target',
    nodeId: 'reference-sink',
    key: 'value',
    expression,
  });

test('dense empty descriptors cannot hide an invalid inactive dependent or consume Redo', () => {
  const owner = createCompositionSessionOwner(referenceEditFixture(128));
  bind(owner, `1 / (${ref} - 129)`);
  assert.equal(read(owner), -0.5);
  owner.switchComposition('comp-source');
  const target = { layerId: 'art-source', nodeId: 'source-curve', key: 'point-127.position' };
  send(owner, 'set-parameter', { ...target, value: [128, 11] });
  send(owner, 'undo');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  assert.throws(
    () => send(owner, 'set-parameter', { ...target, value: [129, 11] }),
    /finite|zero|divide/i,
  );
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
  send(owner, 'redo');
  owner.switchComposition('comp-target');
  assert.equal(read(owner), -1);
});

test('dense empty descriptors retain missing-point identity protection and Undo restoration', () => {
  const owner = createCompositionSessionOwner(referenceEditFixture(128));
  bind(owner, ref);
  owner.switchComposition('comp-source');
  send(owner, 'delete-point', {
    layerId: 'art-source',
    nodeId: 'source-curve',
    pointId: 'point-127',
  });
  owner.switchComposition('comp-target');
  assert.throws(() => read(owner), /missing/i);
  send(owner, 'undo');
  assert.equal(read(owner), 127);
  send(owner, 'redo');
  assert.throws(() => read(owner), /missing/i);
  owner.switchComposition('comp-source');
  const before = owner.projectSnapshot(),
    state = owner.state,
    history = owner.historyState;
  const random = mock.method(globalThis.crypto, 'randomUUID', () => '127');
  try {
    assert.throws(
      () =>
        send(owner, 'create-point', {
          layerId: 'art-source',
          nodeId: 'source-curve',
          position: [5, 6],
        }),
      /retarget|collid/i,
    );
  } finally {
    random.mock.restore();
  }
  assert.deepEqual(owner.projectSnapshot(), before);
  assert.deepEqual(owner.state, state);
  assert.deepEqual(owner.historyState, history);
});
