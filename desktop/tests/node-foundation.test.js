import test from 'node:test';
import assert from 'node:assert/strict';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { mathDeclaration } from '../src/math-node-declaration.js';
import { evaluateMath } from '../src/math-node-binding.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { builtinNodeRegistry, MATH_NODE } from '../src/builtin-node-registry.js';
import {
  CommandSession,
  bypassContract,
  setNodeBypassed,
  setOperation,
  document,
  node,
  definitions,
  activeParameters,
  outputPorts,
  outputType,
  serialize,
  deserialize,
  evaluate,
} from '../src/core.js';
import { nodePickerItems } from '../src/node-catalog.js';

const bindings = () => new Map([['builtin.math.v1', { evaluate: evaluateMath }]]);
const copy = () => structuredClone(mathDeclaration);

test('data declaration rejects executable, ambiguous and unsupported inputs without running getters', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(mathDeclaration));
  assert.equal(parsed.typeId, 'Math');
  assert.ok(Object.isFrozen(parsed.parameters));
  assert.throws(() => parseNodeDeclaration('x'.repeat(262145)), /Invalid/);
  assert.throws(() => parseNodeDeclaration('{broken'), SyntaxError);
  let invoked = false;
  const getter = copy();
  Object.defineProperty(getter, 'unexpected', {
    enumerable: true,
    get() {
      invoked = true;
      return 1;
    },
  });
  assert.throws(() => createNodeRegistry([getter], bindings(), nectNodeHost), /accessor|data/);
  assert.equal(invoked, false);
  for (const bad of [() => {}, new Date(), NaN, undefined]) {
    const item = copy();
    item.extra = bad;
    assert.throws(() => createNodeRegistry([item], bindings(), nectNodeHost), /data|plain/);
  }
  const duplicate = copy();
  duplicate.operations.push({ ...duplicate.operations[0] });
  assert.throws(() => createNodeRegistry([duplicate], bindings(), nectNodeHost), /Duplicate/);
  assert.throws(() => createNodeRegistry([copy(), copy()], bindings(), nectNodeHost), /Duplicate/);
  const capability = copy();
  capability.capabilities = ['unknown.semantic'];
  assert.throws(() => createNodeRegistry([capability], bindings(), nectNodeHost), /Unsupported/);
  const unavailable = copy();
  unavailable.implementationId = 'untrusted.script';
  assert.throws(() => createNodeRegistry([unavailable], bindings(), nectNodeHost), /Unavailable/);
  const badPort = copy();
  badPort.outputs[0].type = 'Mystery';
  assert.throws(() => createNodeRegistry([badPort], bindings(), nectNodeHost), /Unsupported/);
});

test('trusted type and host adapters are required and fail closed', () => {
  assert.throws(() => createNodeRegistry([copy()], bindings()), /host adapters/);
  const unknown = copy();
  unknown.parameters[0].type = 'Unknown';
  assert.throws(() => createNodeRegistry([unknown], bindings(), nectNodeHost), /Unsupported/);
  const badDefault = copy();
  badDefault.parameters[0].default = 'bad';
  assert.throws(() => createNodeRegistry([badDefault], bindings(), nectNodeHost), /default/);
  const badOutput = copy();
  badOutput.outputs[0].legacyDefault = false;
  assert.throws(() => createNodeRegistry([badOutput], bindings(), nectNodeHost), /output/i);
  const host = { types: new Map(), validateOutputs: () => true };
  assert.throws(() => createNodeRegistry([copy()], bindings(), host), /Unsupported/);
});

test('kernel resolves an injected data type without Nect legacy port knowledge', () => {
  const declaration = copy();
  declaration.parameters.forEach((parameter) => {
    parameter.type = 'Synthetic';
    parameter.default = 'valid';
  });
  declaration.resultType = 'Synthetic';
  declaration.outputs = [{ id: 'result', type: 'Synthetic' }];
  const adapter = {
    validateDefault: (value) => value === 'valid',
    validateOutput: (output) => output.id === 'result',
  };
  const host = { types: new Map([['Synthetic', adapter]]), validateOutputs: () => true };
  const resolved = createNodeRegistry([declaration], bindings(), host).get('Math');
  host.types.clear();
  declaration.outputs[0].id = 'changed';
  assert.equal(resolved.declaration.outputs[0].id, 'result');
  assert.equal(
    resolved.evaluate('abs', () => -7),
    7,
  );
});

test('resolved registry snapshots are immutable and use one Math projection', () => {
  const input = copy(),
    trusted = bindings(),
    registry = createNodeRegistry([input], trusted, nectNodeHost),
    resolved = registry.get('Math');
  input.parameters[0].default = 99;
  trusted.set('builtin.math.v1', { evaluate: () => 999 });
  assert.equal(
    resolved.evaluate('abs', () => -3),
    3,
    'binding is captured in the snapshot',
  );
  assert.equal(resolved.declaration.parameters[0].default, 0);
  assert.ok(Object.isFrozen(resolved));
  assert.ok(Object.isFrozen(resolved.declaration.parameters[0]));
  assert.throws(() => {
    resolved.declaration.parameters[0].default = 4;
  }, TypeError);
  assert.deepEqual(registry.types(), ['Math']);
  assert.equal(builtinNodeRegistry.get('Math'), MATH_NODE);
  const m = node('Math');
  assert.deepEqual(
    Object.keys(m.params),
    MATH_NODE.declaration.parameters.map((item) => item.id),
  );
  assert.equal(m.operation, MATH_NODE.declaration.defaultOperation);
  assert.equal(outputType(m), 'Scalar');
  assert.deepEqual(outputPorts(m), [{ id: undefined, label: '', type: 'Scalar' }]);
  assert.equal(outputType(m, 'default'), undefined, 'legacy named-port rejection remains');
  assert.equal(outputPorts({ ...m, paintMode: true })[0].type, 'PaintedPath');
  assert.equal(bypassContract(m).input, MATH_NODE.declaration.bypassInput);
  const layer = { nodes: [m] };
  setNodeBypassed(layer, m.id, true);
  setOperation(layer, m.id, 'abs');
  assert.equal(m.bypassed, true);
  assert.deepEqual(
    activeParameters({ ...m, operation: 'clamp' }).map(([id]) => id),
    ['a', 'min', 'max'],
  );
  const catalog = nodePickerItems(definitions).find((item) => item.typeId === 'Math');
  assert.equal(catalog.categoryId, MATH_NODE.declaration.catalog.categoryId);
  assert.equal(catalog.description, MATH_NODE.declaration.catalog.description);
  const reads = [];
  assert.equal(
    MATH_NODE.evaluate('abs', (id) => {
      reads.push(id);
      return -4;
    }),
    4,
  );
  assert.deepEqual(reads, ['a']);
  assert.throws(
    () =>
      MATH_NODE.evaluate('unknown', () => {
        throw Error('read happened');
      }),
    /Invalid operation/,
  );
});

test('Math Session operation, undo, redo and serialization keep authored format', () => {
  const doc = document(),
    layer = doc.layers[0],
    math = node('Math');
  layer.nodes.push(math);
  const session = new CommandSession(doc);
  const initial = serialize(session.snapshot());
  const revision = session.state.expectedRevision;
  assert.throws(
    () =>
      session.dispatch({
        ...session.state,
        type: 'set-operation',
        payload: { layerId: layer.id, nodeId: math.id, operation: 'unknown' },
      }),
    /Invalid/,
  );
  assert.equal(serialize(session.snapshot()), initial);
  assert.equal(session.state.expectedRevision, revision);
  session.dispatch({
    ...session.state,
    type: 'set-operation',
    payload: { layerId: layer.id, nodeId: math.id, operation: 'abs' },
  });
  const changed = serialize(session.snapshot());
  assert.equal(session.snapshot().layers[0].nodes.find((n) => n.id === math.id).operation, 'abs');
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serialize(session.snapshot()), initial);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.equal(serialize(session.snapshot()), changed);
  assert.equal(serialize(deserialize(changed)), changed);
  assert.equal(evaluate(session.snapshot().layers[0]).run(math.id), 0);
  assert.doesNotMatch(changed, /implementationId|legacyDefault|capabilities/);
});
