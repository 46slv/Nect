import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  deserialize,
  enableRemapPoint,
  evaluate,
  node,
  outputType,
  serialize,
  setNodeBypassed,
} from '../src/core.js';
import { paintedGroup } from '../src/composition-reference.js';
import { signature } from '../src/path-signature.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { convertToPathDeclaration } from '../src/convert-to-path-declaration.js';
import { evaluateConvertToPath } from '../src/convert-to-path-binding.js';

const expected = {
  path: 'fb6b69d412db432e143eae60bf0ecb4350011a2b36795162c9cee423c27315c0',
  fill: '440d541a912152c3acf1a039df1b8d442fc0e50e570f9cdce08159f0ad6e5f95',
  stroke: 'e91641034fdb23340335c749ded4a01f6b2dc2778f279033fecd8bc08a014385',
  group: 'f70ae0be66b9e916fe3f2358721102f93168fe602d44526351f3481f06122638',
  unstable: '3961f7fc3a8a9fd6a6d1f30cdee0118e34189fc91fe4d732ec8f4f668a9387b6',
  'bypassed-fill': '0726160106645c45d11f48d2a94b3582498e4ac492c039626696167ad32f110b',
};

function fixture(scenario) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    shape = node('Rectangle'),
    converter = node('Convert to Path');
  output.id = 'output';
  layer.output = output.id;
  shape.id = 'source';
  shape.params.center.value = [30, 20];
  shape.params.size.value = [60, 40];
  converter.id = 'convert';
  layer.nodes.push(shape, converter);
  let source = shape;
  if (['fill', 'stroke', 'group', 'bypassed-fill'].includes(scenario)) {
    const paint = node(scenario === 'stroke' ? 'Stroke' : 'Fill');
    paint.id = 'paint';
    layer.nodes.push(paint);
    connect(layer, shape.id, paint.id, 'path');
    if (scenario === 'bypassed-fill') setNodeBypassed(layer, paint.id, true);
    source = paint;
  }
  if (scenario === 'group') {
    doc.schemaVersion = 31;
    const shape2 = node('Rectangle'),
      paint2 = node('Fill'),
      join = node('Join Geometry');
    shape2.id = 'source2';
    shape2.params.center.value = [90, 20];
    shape2.params.size.value = [40, 30];
    paint2.id = 'paint2';
    join.id = 'join';
    layer.nodes.push(shape2, paint2, join);
    connect(layer, shape2.id, paint2.id, 'path');
    connect(layer, source.id, join.id, 'a');
    connect(layer, paint2.id, join.id, 'b');
    source = join;
  }
  if (scenario === 'unstable') {
    const closure = node('Set Closure');
    closure.id = 'closure';
    closure.params.closed.value = false;
    layer.nodes.push(closure);
    connect(layer, shape.id, closure.id, 'path');
    source = closure;
  }
  connect(layer, source.id, converter.id, 'path');
  return { doc, layer, shape, source, converter };
}

function contextFor(type, calls) {
  return {
    cloneEvaluated(value) {
      calls.push('clone');
      return structuredClone(value);
    },
    sourceOutputType(key) {
      calls.push(['type', key]);
      return type;
    },
    identity: {
      provenance: (current) => (current ? 'current-provenance' : 'canonical-provenance'),
      setFingerprint(result, preimage) {
        calls.push(['fingerprint', preimage(false), preimage(true)]);
        result.fingerprint = 'test-fingerprint';
      },
    },
  };
}

test('Convert to Path declaration resolves in an isolated trusted registry', () => {
  const declaration = parseNodeDeclaration(JSON.stringify(convertToPathDeclaration)),
    resolved = createNodeRegistry(
      [declaration],
      new Map([['builtin.convert-to-path.v1', { evaluate: evaluateConvertToPath }]]),
      nectNodeHost,
    ).get('Convert to Path');
  assert.equal(resolved.declaration.resultType, 'Path');
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(resolved.declaration.parameters, [{ id: 'path', type: 'Path', default: null }]);
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', label: '', type: 'Path', legacyDefault: true },
  ]);
  assert.deepEqual(resolved.declaration.catalog, {
    categoryId: 'path',
    labelJa: 'パス変換',
    aliases: ['パス変換'],
    description: 'パス変換',
    creationRoute: 'add',
  });
});

for (const scenario of Object.keys(expected)) {
  test(`Convert to Path retains current ${scenario} output, fingerprint and saved Remap`, () => {
    const { doc, layer, source, converter } = fixture(scenario),
      engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      result = engine.run(converter.id),
      sourceType = outputType(source),
      calls = [],
      candidate = evaluateConvertToPath(undefined, () => input, contextFor(sourceType, calls));
    assert.equal(
      createHash('sha256').update(JSON.stringify(result)).digest('hex'),
      expected[scenario],
    );
    assert.equal(JSON.stringify(input), before);
    const expectedGeometry = structuredClone(result),
      candidateGeometry = structuredClone(candidate);
    if (sourceType === 'PaintedPath') {
      delete expectedGeometry.fingerprint;
      delete candidateGeometry.fingerprint;
      const call = calls.find(([kind]) => kind === 'fingerprint');
      assert.equal(call[1][0], 'painted-path-geometry');
      assert.equal(call[1][1], 'canonical-provenance');
      assert.equal(call[2][1], 'current-provenance');
      assert.equal(call[1][2], signature(candidate));
      assert.deepEqual(
        call[1][3],
        candidate.contours.map((contour) => [contour.id, contour.points.map((point) => point.id)]),
      );
    }
    assert.deepEqual(candidateGeometry, expectedGeometry);
    assert.ok(
      calls.some((call) => Array.isArray(call) && call[0] === 'type' && call[1] === 'path'),
    );
    assert.equal(bypassContract(converter, layer).supported, sourceType === 'Path');

    const remap = node('Path Remap');
    remap.id = 'remap';
    layer.nodes.push(remap);
    connect(layer, converter.id, remap.id, 'path');
    const point = result.contours[0].points[0];
    enableRemapPoint(remap, result, point.id).params.position.value = [3, 1];
    const active = evaluate(layer),
      edited = active.run(remap.id),
      saved = serialize(doc),
      reopened = evaluate(deserialize(saved).layers[0]);
    assert.notDeepEqual(edited.contours[0].points[0].position, point.position);
    assert.deepEqual(reopened.run(remap.id), edited);
    assert.deepEqual(active.warnings, []);
    assert.deepEqual(reopened.warnings, []);
  });
}

test('source type, not the value shape, decides whether appearance is removed', () => {
  const source = { contours: [], fill: { hex: '#112233', alpha: 1 }, stroke: { width: 3 } },
    path = evaluateConvertToPath(undefined, () => source, contextFor('Path', [])),
    painted = evaluateConvertToPath(undefined, () => source, contextFor('PaintedPath', []));
  assert.deepEqual(path.fill, source.fill);
  assert.deepEqual(path.stroke, source.stroke);
  assert.equal(Object.hasOwn(painted, 'fill'), false);
  assert.equal(Object.hasOwn(painted, 'stroke'), false);
  assert.ok(Object.hasOwn(source, 'fill'));
  assert.ok(Object.hasOwn(source, 'stroke'));
});

test('missing inputs and masked painted groups fail without changing their source', () => {
  assert.throws(() => evaluateConvertToPath(undefined, () => null, {}), /Connect a Path/);
  const group = paintedGroup([{ contours: [] }]);
  group.mask = {};
  const before = JSON.stringify(group),
    calls = [];
  assert.throws(
    () => evaluateConvertToPath(undefined, () => group, contextFor('PaintedPath', calls)),
    /Masked appearance cannot be converted/,
  );
  assert.equal(JSON.stringify(group), before);
  assert.deepEqual(calls, []);
  const nested = paintedGroup([group]);
  const nestedBefore = JSON.stringify(nested);
  assert.throws(
    () => evaluateConvertToPath(undefined, () => nested, contextFor('PaintedPath', [])),
    /Masked appearance cannot be converted/,
  );
  assert.equal(JSON.stringify(nested), nestedBefore);
});

for (const nested of [false, true]) {
  test(`current evaluator rejects ${nested ? 'nested' : 'direct'} masked appearance`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      shape = node('Rectangle'),
      fill = node('Fill'),
      mask = node('Mask'),
      converter = node('Convert to Path'),
      join = node('Join Geometry');
    doc.schemaVersion = 31;
    layer.nodes.push(shape, fill, mask, converter, join);
    connect(layer, shape.id, fill.id, 'path');
    connect(layer, fill.id, mask.id, 'path');
    connect(layer, shape.id, mask.id, 'mask');
    if (nested) {
      connect(layer, mask.id, join.id, 'a');
      connect(layer, fill.id, join.id, 'b');
    }
    connect(layer, nested ? join.id : mask.id, converter.id, 'path');
    const before = serialize(doc);
    assert.throws(() => evaluate(layer).run(converter.id), /Masked appearance cannot be converted/);
    assert.equal(serialize(doc), before);
  });
}

test('painted input cannot be bypassed and a failed command preserves history', () => {
  const { doc, layer, converter } = fixture('fill'),
    session = new CommandSession(doc),
    before = serialize(session.snapshot()),
    revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() =>
    session.dispatch({
      ...session.state,
      type: 'set-node-bypassed',
      payload: { layerId: layer.id, nodeId: converter.id, bypassed: true },
    }),
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
});

function typeSwitchFixture(bypassed) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    paint = node('Fill'),
    transform = node('Transform'),
    converter = node('Convert to Path'),
    remap = node('Path Remap');
  doc.schemaVersion = 31;
  for (const [item, id] of [
    [shape, 'shape'],
    [paint, 'paint'],
    [transform, 'transform'],
    [converter, 'convert'],
    [remap, 'remap'],
  ]) {
    item.id = id;
    layer.nodes.push(item);
  }
  connect(layer, shape.id, paint.id, 'path');
  connect(layer, shape.id, transform.id, 'path');
  connect(layer, transform.id, converter.id, 'path');
  connect(layer, converter.id, remap.id, 'path');
  const input = evaluate(layer).run(converter.id);
  enableRemapPoint(remap, input, input.contours[0].points[0].id).params.position.value = [3, 1];
  if (bypassed) setNodeBypassed(layer, converter.id, true);
  return { doc, layer, shape, paint, transform, remap };
}

test('upstream Path to PaintedPath switch is atomic while Convert to Path is bypassed', () => {
  const { doc, layer, shape, paint, transform } = typeSwitchFixture(true),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('set-parameter', { layerId: layer.id, nodeId: shape.id, key: 'size', value: [80, 50] });
  send('undo');
  const before = serialize(session.snapshot()),
    revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () =>
      send('connect', { layerId: layer.id, nodeId: transform.id, key: 'path', sourceId: paint.id }),
    /Cannot bypass Convert to Path/,
  );
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.deepEqual(
    session.snapshot().layers[0].nodes.find((item) => item.id === shape.id).params.size.value,
    [80, 50],
  );
});

test('upstream Path to PaintedPath switch suspends Remap and Undo restores it', () => {
  const { doc, layer, paint, transform, remap } = typeSwitchFixture(false),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    before = serialize(session.snapshot()),
    active = evaluate(session.snapshot().layers[0]),
    expected = active.run(remap.id);
  assert.deepEqual(active.warnings, []);
  send('connect', { layerId: layer.id, nodeId: transform.id, key: 'path', sourceId: paint.id });
  const switched = session.snapshot(),
    changed = evaluate(switched.layers[0]),
    saved = serialize(switched),
    reopened = evaluate(deserialize(saved).layers[0]);
  changed.run(remap.id);
  reopened.run(remap.id);
  assert.match(changed.warnings.join(' '), /suspended/i);
  assert.match(reopened.warnings.join(' '), /suspended/i);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const restored = evaluate(session.snapshot().layers[0]);
  assert.deepEqual(restored.run(remap.id), expected);
  assert.deepEqual(restored.warnings, []);
  send('redo');
  assert.equal(serialize(session.snapshot()), saved);
  const redone = evaluate(session.snapshot().layers[0]);
  redone.run(remap.id);
  assert.match(redone.warnings.join(' '), /suspended/i);
});
