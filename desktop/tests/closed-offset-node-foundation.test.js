import test from 'node:test';
import assert from 'node:assert/strict';
import { closedOffsetDeclaration } from '../src/closed-offset-node-declaration.js';
import { evaluateClosedOffset } from '../src/closed-offset-node-binding.js';
import { closedOffsetSettings } from '../src/closed-offset-node-settings.js';
import { signature } from '../src/path-signature.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import {
  activeParameters,
  bypassContract,
  closedOffsetSettings as currentCoreSettings,
  connect,
  definitions,
  document,
  evaluate,
  node,
  operationOptions,
  outputPorts,
} from '../src/core.js';

const resolved = createNodeRegistry(
  [closedOffsetDeclaration],
  new Map([[closedOffsetDeclaration.implementationId, { evaluate: evaluateClosedOffset }]]),
  nectNodeHost,
).get(closedOffsetDeclaration.typeId);

test('Closed Path Offset is registered without changing its current interface or order', () => {
  const n = node('Closed Path Offset'),
    registered = builtinNodeRegistry.get(n.type),
    manifest = builtinManifest.find((row) => row.declaration.typeId === n.type),
    defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.deepEqual(registered.declaration, closedOffsetDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateClosedOffset);
  assert.equal(n.operation, 'miter');
  assert.equal(bypassContract(n).input, 'path');
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.deepEqual(operationOptions[n.type], ['miter', 'bevel', 'round']);
  for (const operation of operationOptions[n.type]) {
    n.operation = operation;
    assert.deepEqual(
      activeParameters(n).map(([key]) => key),
      ['path', 'amount', 'miterLimit'],
    );
  }
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Convert to Path');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Path Remap');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Expand Stroke');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Corner Round');
});

function rectangle(id, x, y, width, height, group) {
  return {
    closed: true,
    id,
    ...(group === undefined ? {} : { group }),
    points: [
      [x, y],
      [x + width, y],
      [x + width, y + height],
      [x, y + height],
    ].map(([px, py], index) => ({
      id: `${id}-p${index}`,
      position: [px, py],
      handleA: [0, 0],
      handleB: [0, 0],
    })),
  };
}

function identityFor(calls) {
  return Object.freeze({
    provenance: (currentEra) => `closed-offset:${currentEra ? 'current' : 'canonical'}`,
    fingerprint: (path, currentEra) =>
      currentEra ? (path.currentFingerprint ?? path.fingerprint) : path.fingerprint,
    setFingerprint: (value, makePreimage) => {
      calls.push({ value, canonical: makePreimage(false), current: makePreimage(true) });
    },
  });
}

function evaluateCandidate(
  source,
  { amount = 10, miterLimit = 4, operation = 'miter', nodeId = 'offset-node' } = {},
) {
  const calls = [],
    read = (key) => ({ path: source, amount, miterLimit })[key],
    value = resolved.evaluate(operation, read, {
      nodeId,
      identity: identityFor(calls),
    });
  return { value, calls };
}

test('Closed Path Offset declaration resolves in an isolated Node Foundation registry', () => {
  assert.equal(resolved.declaration.typeId, 'Closed Path Offset');
  assert.equal(resolved.declaration.revision, 1);
  assert.equal(resolved.declaration.resultType, 'Path');
  assert.equal(resolved.declaration.defaultOperation, 'miter');
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(
    resolved.declaration.parameters.map(({ id, type, default: value }) => [id, type, value]),
    [
      ['path', 'Path', null],
      ['amount', 'Scalar', 10],
      ['miterLimit', 'Scalar', 4],
    ],
  );
  assert.deepEqual(
    resolved.declaration.operations.map(({ id, active }) => [id, active]),
    [
      ['miter', ['path', 'amount', 'miterLimit']],
      ['bevel', ['path', 'amount', 'miterLimit']],
      ['round', ['path', 'amount', 'miterLimit']],
    ],
  );
  assert.deepEqual(resolved.interface('round').outputs, [
    { id: 'default', label: '', type: 'Path', legacyDefault: true },
  ]);
  assert.deepEqual(resolved.declaration.catalog, {
    categoryId: 'path',
    labelJa: '閉じた輪郭',
    aliases: ['閉じた輪郭', 'オフセット'],
    description: '閉じた輪郭 オフセット',
    creationRoute: 'add',
  });
});

test('Closed Path Offset settings preserve authored and evaluated numeric constraints', () => {
  const outcome = (fn) => {
    try {
      return { value: fn() };
    } catch (error) {
      return { error: [error.name, error.code, error.message] };
    }
  };
  for (const amount of [0, 0.0001, -0.0001, 10, -10])
    for (const miterLimit of [1, 4, 100]) {
      const read = (key) => ({ amount, miterLimit })[key];
      assert.deepEqual(closedOffsetSettings(read), { amount, miterLimit });
      assert.deepEqual(
        outcome(() => closedOffsetSettings(read)),
        outcome(() => currentCoreSettings(read)),
      );
    }
  for (const amount of [NaN, Infinity, -Infinity, 0.00001, -0.00001])
    for (const miterLimit of [0, 4, 101]) {
      const read = (key) => ({ amount, miterLimit })[key],
        candidate = outcome(() => closedOffsetSettings(read));
      assert.deepEqual(
        candidate,
        outcome(() => currentCoreSettings(read)),
      );
      assert.equal(candidate.error[1], 'OFFSET_AMOUNT');
    }
  for (const miterLimit of [NaN, Infinity, 0.9999, 100.0001])
    for (const amount of [0, 10]) {
      const read = (key) => ({ amount, miterLimit })[key],
        candidate = outcome(() => closedOffsetSettings(read));
      assert.deepEqual(
        candidate,
        outcome(() => currentCoreSettings(read)),
      );
      assert.equal(candidate.error[1], 'OFFSET_OPTIONS');
    }
});

test('zero offset preserves exact source identity after validating operation and settings', () => {
  const source = { contours: [{ closed: false, points: [] }] },
    { value, calls } = evaluateCandidate(source, { amount: 0 });
  assert.equal(value, source);
  assert.equal(calls.length, 0);
  assert.throws(
    () =>
      evaluateClosedOffset('square', (key) => ({ path: source, amount: 0, miterLimit: 4 })[key], {
        nodeId: 'offset-node',
        identity: identityFor([]),
      }),
    { code: 'OFFSET_OPTIONS' },
  );
  assert.throws(() => evaluateCandidate(source, { amount: 0, miterLimit: 0 }), {
    code: 'OFFSET_OPTIONS',
  });
});

test('nonzero offset keeps stable derived point IDs and both Remap fingerprint eras', () => {
  const source = {
      fingerprint: 'source-fingerprint',
      contours: [rectangle('source-contour', 0, 0, 40, 30)],
    },
    before = JSON.stringify(source),
    first = evaluateCandidate(source, { amount: 3, operation: 'bevel' }),
    repeated = evaluateCandidate(source, { amount: 3, operation: 'bevel' }),
    changed = evaluateCandidate(source, { amount: 4, operation: 'bevel' }),
    ids = (value) => value.contours.flatMap((contour) => contour.points.map(({ id }) => id));

  assert.equal(JSON.stringify(source), before);
  assert.equal(first.value.unstable, true);
  assert.deepEqual(ids(first.value), ids(repeated.value));
  assert.ok(
    ids(first.value).every((id) => !source.contours[0].points.some((point) => point.id === id)),
  );
  assert.equal(first.calls.length, 1);
  assert.equal(repeated.calls.length, 1);
  assert.equal(changed.calls.length, 1);
  assert.deepEqual(first.calls[0].canonical, [
    'closed-offset:canonical',
    3,
    'bevel',
    4,
    'source-fingerprint',
    source.contours,
    signature(first.value),
  ]);
  assert.deepEqual(first.calls[0].current, [
    'closed-offset:current',
    3,
    'bevel',
    4,
    'source-fingerprint',
    source.contours,
    signature(first.value),
  ]);
  assert.deepEqual(first.calls[0].canonical, repeated.calls[0].canonical);
  assert.notDeepEqual(first.calls[0].canonical, changed.calls[0].canonical);
});

test('grouped paths offset each group independently and preserve distinct group identity', () => {
  const source = {
      fingerprint: 'grouped-source-fingerprint',
      contours: [
        rectangle('left', 0, 0, 20, 20, 'left-group'),
        rectangle('right', 10, 0, 20, 20, 'right-group'),
      ],
    },
    before = JSON.stringify(source),
    { value, calls } = evaluateCandidate(source, { amount: 2, nodeId: 'group-offset' });

  assert.equal(JSON.stringify(source), before);
  assert.equal(value.contours.length, 2);
  assert.deepEqual(
    value.contours.map(({ group }) => group),
    ['group-offset:g0', 'group-offset:g1'],
  );
  assert.equal(calls.length, 1);
  assert.deepEqual(calls[0].canonical[5], source.contours);
  assert.equal(calls[0].canonical[6], signature(value));
});

test('candidate geometry and derived point IDs match the current evaluator for every join', () => {
  for (const operation of ['miter', 'bevel', 'round']) {
    const layer = document().layers[0],
      sourceNode = node('Rectangle'),
      offsetNode = node('Closed Path Offset'),
      fill = node('Fill'),
      output = node('Output');
    sourceNode.id = 'closed-offset-source';
    offsetNode.id = 'closed-offset-node';
    sourceNode.params.center.value = [24, 18];
    sourceNode.params.size.value = [40, 30];
    offsetNode.operation = operation;
    offsetNode.params.amount.value = 5;
    offsetNode.params.miterLimit.value = 2;
    layer.nodes = [sourceNode, offsetNode, fill, output];
    layer.output = output.id;
    connect(layer, sourceNode.id, offsetNode.id, 'path');
    connect(layer, offsetNode.id, fill.id, 'path');
    connect(layer, fill.id, output.id, 'path');

    const current = evaluate(layer);
    assert.equal(current.error, null);
    const candidate = evaluateCandidate(current.run(sourceNode.id), {
      amount: 5,
      miterLimit: 2,
      operation,
      nodeId: offsetNode.id,
    });
    assert.deepEqual(candidate.value.contours, current.run(offsetNode.id).contours);
    assert.deepEqual(
      candidate.value.characterOccurrences,
      current.run(offsetNode.id).characterOccurrences,
    );
    assert.equal(candidate.value.unstable, current.run(offsetNode.id).unstable);
  }
});
