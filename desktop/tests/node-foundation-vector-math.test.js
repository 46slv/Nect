import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  outputType,
  outputPorts,
  activeParameters,
  evaluate,
  serialize,
  deserialize,
  CommandSession,
  setReference,
  setNodeBypassed,
} from '../src/core.js';
import { VECTOR_MATH } from '../src/builtin-node-registry.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { vectorMathDeclaration } from '../src/vector-math-declaration.js';
import { evaluateVectorMath } from '../src/vector-math-binding.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('registered Vector Math interfaces and trusted evaluation preserve lazy semantics', () => {
  const n = node('Vector Math');
  assert.equal(n.operation, 'add');
  assert.deepEqual(n.params.a.value, [0, 0]);
  assert.deepEqual(n.params.b.value, [0, 0]);
  assert.equal(n.params.scale.value, 1);
  for (const op of VECTOR_MATH.declaration.operations) {
    n.operation = op.id;
    assert.deepEqual(
      activeParameters(n).map(([id]) => id),
      op.active,
    );
    assert.equal(outputType(n), op.resultType);
    assert.equal(outputPorts(n)[0].type, op.resultType);
    assert.equal(outputType(n, 'default'), undefined);
    assert.ok(Object.isFrozen(VECTOR_MATH.interface(op.id)));
  }
  const reads = [];
  assert.deepEqual(
    VECTOR_MATH.evaluate('normalize', (id) => {
      reads.push(id);
      return [1e308, 1e308];
    }),
    [1 / Math.hypot(1, 1), 1 / Math.hypot(1, 1)],
  );
  assert.deepEqual(reads, ['a']);
  assert.deepEqual(
    VECTOR_MATH.evaluate('normalize', () => [0, 0]),
    [0, 0],
  );
  assert.deepEqual(
    VECTOR_MATH.evaluate('divide', (id) => (id === 'a' ? [4, 8] : [0, 2])),
    [0, 4],
  );
  assert.throws(() => VECTOR_MATH.interface('unknown'), /operation/);
  assert.throws(
    () =>
      VECTOR_MATH.evaluate('unknown', () => {
        throw Error('must not read');
      }),
    /operation/,
  );
  const layer = document().layers[0];
  layer.nodes.push(n);
  n.operation = 'length';
  n.params.a.value = [3, 4];
  assert.equal(evaluate(layer).run(n.id), 5);
});

test('dynamic declaration fails closed and Session roundtrips operation', () => {
  const bindings = new Map([['builtin.vector-math.v1', { evaluate: evaluateVectorMath }]]);
  const bad = structuredClone(vectorMathDeclaration);
  bad.operations[0].resultType = 'Unknown';
  assert.throws(() => createNodeRegistry([bad], bindings, nectNodeHost), /Unsupported/);
  const conflict = structuredClone(vectorMathDeclaration);
  conflict.resultType = 'Scalar';
  assert.throws(() => createNodeRegistry([conflict], bindings, nectNodeHost), /dynamic|result/i);
  const doc = document(),
    layer = doc.layers[0],
    vm = node('Vector Math');
  layer.nodes.push(vm);
  const session = new CommandSession(doc),
    before = serialize(session.snapshot());
  const revision = session.state.expectedRevision;
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-operation',
        payload: { layerId: layer.id, nodeId: vm.id, operation: 'bogus' },
      }),
    /Invalid/,
  );
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(serialize(session.snapshot()), before);
  session.dispatch({
    ...session.state,
    type: 'set-operation',
    payload: { layerId: layer.id, nodeId: vm.id, operation: 'length' },
  });
  const after = serialize(session.snapshot());
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.equal(serialize(session.snapshot()), after);
  assert.equal(serialize(deserialize(after)), after);
  assert.doesNotMatch(after, /implementationId|resultType|capabilities/);
});

test('Reference and bypass reject changed output type without disturbing Session redo', () => {
  for (const kind of ['reference', 'bypass']) {
    const doc = document(),
      layer = doc.layers[0],
      vm = node('Vector Math');
    layer.nodes.push(vm);
    if (kind === 'reference') {
      const ref = node('Reference');
      layer.nodes.push(ref);
      setReference(layer, ref.id, { nodeId: vm.id, portId: 'output' });
    } else setNodeBypassed(layer, vm.id, true);
    const session = new CommandSession(doc);
    const dispatch = (type, payload = {}) =>
      session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
    dispatch('set-operation', { nodeId: vm.id, operation: 'scale' });
    dispatch('undo');
    const before = serialize(session.snapshot()),
      history = JSON.stringify(session.historyView),
      revision = session.state.expectedRevision;
    assert.throws(
      () => dispatch('set-operation', { nodeId: vm.id, operation: 'length' }),
      /break|Restore/,
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.equal(JSON.stringify(session.historyView), history);
    assert.equal(session.state.expectedRevision, revision);
    dispatch('redo');
    assert.equal(session.snapshot().layers[0].nodes.find((n) => n.id === vm.id).operation, 'scale');
  }
});
