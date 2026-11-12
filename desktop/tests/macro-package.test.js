import test from 'node:test';
import assert from 'node:assert/strict';
import {
  assertCompatibleMacroDefinition,
  parseMacroDefinition,
  resetMacroOverride,
  resolveMacroInstance,
  setMacroOverride,
  snapshotMacroDefinition,
  snapshotMacroInstance,
} from '../src/macro-package.js';

function definition() {
  return {
    format: 'nect.macro',
    formatVersion: 1,
    id: 'macro-double',
    revision: 1,
    name: 'Double Transform',
    input: { type: 'Path', nodeId: 'local-a', socket: 'path' },
    output: { type: 'Path', nodeId: 'local-b', port: null },
    // Storage order is not graph order.
    nodes: [
      {
        id: 'local-b',
        type: 'Transform',
        params: { anchor: [5, 9], offset: [12, -4], scale: 2, rotation: 10 },
      },
      {
        id: 'local-a',
        type: 'Transform',
        params: { anchor: [0, 0], offset: [0, 0], scale: 1, rotation: 15 },
      },
    ],
    edges: [
      { sourceNodeId: 'local-a', sourcePort: null, targetNodeId: 'local-b', targetSocket: 'path' },
    ],
    parameters: [
      {
        id: 'public-angle',
        label: 'Angle',
        nodeId: 'local-a',
        key: 'rotation',
        type: 'Scalar',
        defaultValue: 30,
      },
      {
        id: 'public-scale',
        label: 'Scale',
        nodeId: 'local-b',
        key: 'scale',
        type: 'Scalar',
        defaultValue: 2,
      },
    ],
  };
}
function instance() {
  return { id: 'instance-1', definitionId: 'macro-double', definitionRevision: 1, overrides: {} };
}

test('A two-Transform Path definition is immutable, serializable and independent of node order', () => {
  const input = definition(),
    snapshot = snapshotMacroDefinition(input),
    parsed = parseMacroDefinition(JSON.stringify(snapshot));
  assert.deepEqual(parsed, snapshot);
  assert.ok(Object.isFrozen(snapshot));
  assert.ok(Object.isFrozen(snapshot.nodes[0].params.anchor));
  input.nodes[0].params.anchor[0] = 999;
  input.parameters[0].defaultValue = 999;
  assert.deepEqual(snapshot.nodes[0].params.anchor, [5, 9]);
  assert.equal(snapshot.parameters[0].defaultValue, 30);
  assert.deepEqual(resolveMacroInstance(snapshot, instance()), [
    { parameterId: 'public-angle', nodeId: 'local-a', key: 'rotation', value: 30 },
    { parameterId: 'public-scale', nodeId: 'local-b', key: 'scale', value: 2 },
  ]);
});

test('Published defaults and per-instance overrides use stable IDs, including explicit zero', () => {
  const def = snapshotMacroDefinition(definition()),
    first = snapshotMacroInstance(instance(), def),
    second = snapshotMacroInstance({ ...instance(), id: 'instance-2' }, def),
    changed = setMacroOverride(def, first, 'public-angle', 0);
  assert.equal(resolveMacroInstance(def, first)[0].value, 30);
  assert.equal(resolveMacroInstance(def, changed)[0].value, 0);
  assert.equal(resolveMacroInstance(def, second)[0].value, 30);
  assert.deepEqual(first.overrides, {});
  assert.deepEqual(changed.overrides, { 'public-angle': 0 });
  const reset = resetMacroOverride(def, changed, 'public-angle');
  assert.deepEqual(reset.overrides, {});
  assert.equal(resolveMacroInstance(def, reset)[0].value, 30);
  assert.deepEqual(changed.overrides, { 'public-angle': 0 });
  assert.throws(() => resetMacroOverride(def, changed, 'missing'), /published/i);
});

test('An instance pins the exact definition revision and does not follow newer defaults', () => {
  const old = snapshotMacroDefinition(definition()),
    pinned = snapshotMacroInstance(instance(), old),
    updated = definition();
  updated.revision = 2;
  updated.name = 'Renamed';
  updated.parameters[0].defaultValue = 75;
  const next = snapshotMacroDefinition(updated);
  assert.equal(assertCompatibleMacroDefinition(old, next), false);
  assert.equal(resolveMacroInstance(old, pinned)[0].value, 30);
  assert.throws(() => resolveMacroInstance(next, pinned), /unpinned/i);
  const later = snapshotMacroInstance(
    { ...instance(), id: 'instance-2', definitionRevision: 2 },
    next,
  );
  assert.equal(resolveMacroInstance(next, later)[0].value, 75);
});

test('Same ID and revision with different content is rejected; object key order is not identity', () => {
  const first = snapshotMacroDefinition(definition()),
    reordered = JSON.parse(JSON.stringify(first)),
    second = {
      parameters: reordered.parameters,
      edges: reordered.edges,
      nodes: reordered.nodes,
      output: reordered.output,
      input: reordered.input,
      name: reordered.name,
      revision: reordered.revision,
      id: reordered.id,
      formatVersion: reordered.formatVersion,
      format: reordered.format,
    };
  assert.equal(assertCompatibleMacroDefinition(first, second), true);
  for (const edit of [
    (d) => (d.name = 'Different'),
    (d) => (d.parameters[0].defaultValue = 31),
    (d) => (d.nodes[0].params.offset[0] = 13),
    (d) => d.parameters.reverse(),
  ]) {
    const conflict = definition();
    edit(conflict);
    assert.throws(() => assertCompatibleMacroDefinition(first, conflict), /conflict/i);
  }
  const different = definition();
  different.id = 'other';
  assert.equal(assertCompatibleMacroDefinition(first, different), false);
});

test('Disconnected, ambiguous, non-Path and unknown graph structures are refused', () => {
  for (const edit of [
    (d) => (d.nodes[1].id = d.nodes[0].id),
    (d) => (d.nodes[0].type = 'Image'),
    (d) => (d.input.type = 'PaintedPath'),
    (d) => (d.input.nodeId = 'outside'),
    (d) => (d.output.nodeId = d.input.nodeId),
    (d) => (d.output.port = 'other'),
    (d) => (d.edges[0].sourceNodeId = 'outside'),
    (d) => (d.edges[0].targetNodeId = 'local-a'),
    (d) => (d.edges[0].sourcePort = 'other'),
    (d) => (d.edges[0].targetSocket = 'other'),
    (d) => d.edges.push({ ...d.edges[0] }),
    (d) => d.nodes.pop(),
  ]) {
    const invalid = definition();
    edit(invalid);
    const before = structuredClone(invalid);
    assert.throws(() => snapshotMacroDefinition(invalid));
    assert.deepEqual(invalid, before);
  }
});

test('External resources, expressions, references and unsupported node data are not silently dropped', () => {
  for (const edit of [
    (d) => (d.resources = ['external']),
    (d) => (d.nodes[0].imageAsset = 'external'),
    (d) => (d.nodes[0].params.rotation = { expression: 'ref(artwork,...)', value: 1 }),
    (d) => (d.nodes[0].params.source = 'outside'),
    (d) => (d.nodes[0].bypassed = true),
    (d) => (d.nodes[0].params.anchor = [1, ,]),
    (d) => (d.nodes[0].params.scale = Infinity),
    (d) => (d.input.resource = 'outside'),
  ]) {
    const invalid = definition();
    edit(invalid);
    assert.throws(() => snapshotMacroDefinition(invalid));
  }
});

test('Public parameters require unique stable IDs and distinct existing Scalar targets', () => {
  for (const edit of [
    (d) => (d.parameters[1].id = d.parameters[0].id),
    (d) => (d.parameters[1].nodeId = 'outside'),
    (d) => (d.parameters[1].key = 'anchor'),
    (d) => (d.parameters[1].type = 'Vector2'),
    (d) => (d.parameters[1].defaultValue = NaN),
    (d) => (d.parameters[1].label = '  '),
    (d) => (d.parameters[1] = { ...d.parameters[0], id: 'another-id' }),
    (d) => (d.parameters = []),
  ]) {
    const invalid = definition();
    edit(invalid);
    assert.throws(() => snapshotMacroDefinition(invalid));
  }
  const repeatedLabels = definition();
  repeatedLabels.parameters[1].label = repeatedLabels.parameters[0].label;
  assert.equal(snapshotMacroDefinition(repeatedLabels).parameters.length, 2);
});

test('Unknown, nonfinite or foreign overrides fail without changing either input', () => {
  const def = definition(),
    base = instance();
  for (const [key, value] of [
    ['missing', 1],
    ['public-angle', null],
    ['public-angle', NaN],
    ['public-angle', Infinity],
    ['public-angle', '1'],
  ]) {
    const before = structuredClone(base);
    assert.throws(() => setMacroOverride(def, base, key, value));
    assert.deepEqual(base, before);
  }
  assert.throws(() => snapshotMacroInstance({ ...base, definitionId: 'other' }, def), /unpinned/i);
  assert.throws(() => snapshotMacroInstance({ ...base, definitionRevision: 2 }, def), /unpinned/i);
});

test('Accessor, hidden, cyclic and non-data input is rejected without invoking accessors', () => {
  let called = false;
  const withGetter = definition();
  Object.defineProperty(withGetter, 'name', {
    enumerable: true,
    get() {
      called = true;
      return 'unsafe';
    },
  });
  assert.throws(() => snapshotMacroDefinition(withGetter));
  assert.equal(called, false);
  const hidden = definition();
  Object.defineProperty(hidden, 'extra', { value: 1 });
  assert.throws(() => snapshotMacroDefinition(hidden));
  const cycle = definition();
  cycle.extra = cycle;
  assert.throws(() => snapshotMacroDefinition(cycle));
  assert.throws(() => parseMacroDefinition('{invalid'));
});
