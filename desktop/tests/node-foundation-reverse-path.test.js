import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  outputType,
  outputPorts,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import { REVERSE_PATH } from '../src/builtin-node-registry.js';
import { evaluateReversePath } from '../src/reverse-path-binding.js';
import { reversePathGeometry } from '../src/reverse-path-geometry.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { reversePathDeclaration } from '../src/reverse-path-declaration.js';
import { nectNodeHost } from '../src/nect-node-host.js';

test('Reverse Path uses registered production path with Session and unchanged authored representation', () => {
  const doc = document(),
    layer = doc.layers[0],
    curve = node('Curve'),
    reverse = node('Reverse Path');
  curve.id = 'curve-test';
  reverse.id = 'reverse-test';
  layer.nodes.push(curve, reverse);
  assert.equal(reverse.operation, 'subtract');
  assert.deepEqual(Object.keys(reverse.params), ['path']);
  assert.equal(reverse.params.path.value, null);
  assert.deepEqual(outputPorts(reverse), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(reverse, 'x'), undefined);
  assert.equal(outputType({ ...reverse, paintMode: true }), 'PaintedPath');
  assert.throws(() => evaluate(layer).run(reverse.id), /Connect a Path input/);
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({ ...session.state, type, payload: { layerId: layer.id, ...payload } });
  send('connect', { sourceId: curve.id, nodeId: reverse.id, key: 'path' });
  const saved = serialize(session.snapshot()),
    sourceBefore = JSON.stringify(
      session.snapshot().layers[0].nodes.find((n) => n.id === curve.id),
    );
  const current = session.snapshot().layers[0],
    first = evaluate(current).run(reverse.id),
    second = evaluate(current).run(reverse.id);
  assert.deepEqual(first, second);
  assert.equal(first.unstable, true);
  assert.ok(first.fingerprint);
  assert.equal(JSON.stringify(current.nodes.find((n) => n.id === curve.id)), sourceBefore);
  assert.deepEqual(evaluate(deserialize(saved).layers[0]).run(reverse.id), first);
  send('set-node-bypassed', { nodeId: reverse.id, bypassed: true });
  assert.deepEqual(
    evaluate(session.snapshot().layers[0]).run(reverse.id),
    evaluate(session.snapshot().layers[0]).run(curve.id),
  );
  send('undo');
  assert.equal(serialize(session.snapshot()), saved);
  send('redo');
  assert.equal(session.snapshot().layers[0].nodes.find((n) => n.id === reverse.id).bypassed, true);
  assert.doesNotMatch(saved, /implementationId|resultType|capabilities/);
});

test('trusted reverse binding preserves route metadata, prior reason, shallow aliases and null/empty distinction', () => {
  const shared = { tag: 'shared' },
    matrix = [1, 0, 0, 1, 0, 0];
  const source = {
    meta: shared,
    characterOccurrences: [
      { route: 'text', matrix, reason: null },
      { route: 'prior', matrix, reason: 'existing' },
    ],
    contours: [
      {
        id: 'c',
        closed: true,
        group: 'g',
        meta: shared,
        points: [
          { id: 'a', position: [0, 0], handleA: [1, 2], handleB: [3, 4], meta: shared },
          { id: 'b', position: [5, 6], handleA: [7, 8], handleB: [9, 10] },
        ],
      },
    ],
  };
  const before = JSON.stringify(source),
    context = Object.freeze({ nodeId: 'rev-test' });
  const result = evaluateReversePath('subtract', () => source, context);
  assert.equal(JSON.stringify(source), before);
  assert.equal(result.meta, shared);
  assert.equal(result.contours[0].meta, shared);
  assert.notEqual(result.contours[0].points[0].position, source.contours[0].points[0].position);
  assert.deepEqual(result.contours[0].points[0].handleA, [3, 4]);
  assert.equal(result.characterOccurrences[0].route, 'text/rev-test');
  assert.equal(result.characterOccurrences[0].matrix, matrix);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Reverse Path/);
  assert.equal(result.characterOccurrences[1].reason, 'existing');
  assert.equal(result.unstable, true);
  assert.ok(result.fingerprint);
  assert.deepEqual(reversePathGeometry({ contours: [] }).contours, []);
  assert.throws(() => evaluateReversePath('subtract', () => null, context), /Connect a Path input/);
  assert.throws(
    () =>
      reversePathGeometry({
        contours: [
          {
            id: 'c',
            closed: false,
            points: [{ id: 'a', position: [NaN, 0], handleA: [0, 0], handleB: [0, 0] }],
          },
        ],
      }),
    /finite/,
  );
});

test('closed Path registration remains fail-closed for unavailable implementation, type and capabilities', () => {
  const declaration = structuredClone(reversePathDeclaration),
    bindings = new Map([['builtin.reverse-path.v1', { evaluate: evaluateReversePath }]]);
  const registry = createNodeRegistry([declaration], bindings, nectNodeHost);
  assert.equal(registry.get('Reverse Path').declaration.parameters[0].default, null);
  assert.throws(() => registry.get('Reverse Path').project('unknown', {}), /output/);
  declaration.implementationId = 'not-trusted';
  assert.throws(() => createNodeRegistry([declaration], bindings, nectNodeHost), /Unavailable/);
  const unknown = structuredClone(reversePathDeclaration);
  unknown.parameters[0].type = 'Unknown';
  assert.throws(() => createNodeRegistry([unknown], bindings, nectNodeHost), /Unsupported/);
  const capability = structuredClone(reversePathDeclaration);
  capability.capabilities = ['unknown'];
  assert.throws(() => createNodeRegistry([capability], bindings, nectNodeHost), /Unsupported/);
  assert.ok(Object.isFrozen(REVERSE_PATH));
});
