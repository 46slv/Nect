import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, connect, evaluate, node } from '../src/core.js';
import { captureMacroDefinition } from '../src/macro-capture.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';
import { planMacroInsertion } from '../src/macro-insert-plan.js';

function request() {
  const first = node('Transform'),
    second = node('Transform'),
    layer = { nodes: [first, second] };
  connect(layer, first.id, second.id, 'path');
  first.params.scale.value = 2;
  second.params.rotation.value = 30;
  return {
    id: 'macro-1',
    name: 'Example',
    nodes: [second, first],
    parameters: [
      { id: 'size', label: 'Size', nodeId: first.id, key: 'scale' },
      { id: 'turn', label: 'Turn', nodeId: second.id, key: 'rotation' },
    ],
  };
}

test('captures topology, independent internal IDs, authored literals and published defaults', () => {
  const input = request(),
    before = structuredClone(input),
    captured = captureMacroDefinition(input),
    definition = captured.definition;
  assert.deepEqual(captured.sourceIds, [input.nodes[1].id, input.nodes[0].id]);
  assert.deepEqual(
    definition.nodes.map((item) => item.id),
    ['step-1', 'step-2'],
  );
  assert.deepEqual(
    definition.parameters.map((item) => [item.id, item.nodeId, item.key, item.defaultValue]),
    [
      ['size', 'step-1', 'scale', 2],
      ['turn', 'step-2', 'rotation', 30],
    ],
  );
  assert.deepEqual(input, before);
  input.nodes[1].params.scale.value = 99;
  assert.equal(definition.nodes[0].params.scale, 2);
});

test('an upstream Path connection becomes the one Macro input without capturing its owner', () => {
  const input = request();
  input.nodes[1].params.path.source = 'source-outside';
  const { definition } = captureMacroDefinition(input);
  assert.deepEqual(definition.input, { type: 'Path', nodeId: 'step-1', socket: 'path' });
  assert.equal(JSON.stringify(definition).includes('source-outside'), false);
});

test('disconnected, cyclic, duplicated or non-Transform selections are rejected', () => {
  for (const mutate of [
    (x) => delete x.nodes[0].params.path.source,
    (x) => (x.nodes[1].params.path.source = x.nodes[0].id),
    (x) => (x.nodes[1].params.path.source = x.nodes[1].id),
    (x) => (x.nodes[1].params.path.source = 17),
    (x) => (x.nodes[1].id = x.nodes[0].id),
    (x) => (x.nodes[0].type = 'Circle'),
  ]) {
    const input = request();
    mutate(input);
    const before = structuredClone(input);
    assert.throws(() => captureMacroDefinition(input));
    assert.deepEqual(input, before);
  }
});

test('driven, bypassed, locked, non-Path and extended Transforms are refused', () => {
  for (const mutate of [
    (x) => (x.nodes[0].params.scale.expression = '2'),
    (x) => (x.nodes[0].params.scale.source = 'driver'),
    (x) => (x.nodes[0].params.path.sourcePort = 'other'),
    (x) => (x.nodes[0].bypassed = true),
    (x) => (x.nodes[0].params.rotation.locked = true),
    (x) => (x.nodes[0].paintMode = true),
    (x) => (x.nodes[0].params.path.type = 'PaintedPath'),
    (x) => (x.nodes[0].params.scale.type = 'Vector2'),
    (x) => (x.nodes[0].params.scale.futureMode = 'unknown'),
    (x) => (x.nodes[0].params.skew = { type: 'Scalar', value: 1, expression: '', locked: false }),
  ]) {
    const input = request();
    mutate(input);
    const before = structuredClone(input);
    assert.throws(() => captureMacroDefinition(input));
    assert.deepEqual(input, before);
  }
});

test('published IDs, scalar targets and defaults are validated without coercion', () => {
  for (const mutate of [
    (x) => (x.parameters = []),
    (x) => (x.parameters[0].nodeId = 'missing'),
    (x) => (x.parameters[0].key = 'offset'),
    (x) => (x.parameters[1].id = x.parameters[0].id),
    (x) => (x.parameters[1] = { ...x.parameters[0], id: 'other' }),
    (x) => (x.nodes[1].params.scale.value = Infinity),
  ]) {
    const input = request();
    mutate(input);
    assert.throws(() => captureMacroDefinition(input));
  }
});

test('accessors and hidden data are rejected before a getter can run', () => {
  const input = request();
  let calls = 0;
  Object.defineProperty(input.nodes[0].params.rotation, 'value', {
    enumerable: true,
    get() {
      calls++;
      return 4;
    },
  });
  assert.throws(() => captureMacroDefinition(input), /accessor|data/i);
  assert.equal(calls, 0);
});

test('a captured definition can be registered and applied through the real Session', () => {
  const definition = captureMacroDefinition(request()).definition,
    doc = blankDocument(),
    layer = doc.layers[0],
    rectangle = node('Rectangle'),
    terminal = node('Transform'),
    fill = node('Fill'),
    output = node('Output');
  layer.nodes = [rectangle, terminal, fill, output];
  layer.output = output.id;
  connect(layer, rectangle.id, terminal.id, 'path');
  connect(layer, terminal.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc)),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  send('register-macro-definition', { definition });
  const plan = planMacroInsertion(owner.snapshot(), layer.id, {
    definitionId: definition.id,
    revision: definition.revision,
  });
  assert.equal(plan.supported, true, plan.reason);
  const result = send('insert-macro', {
    expectedProjectRevision: owner.projectRevision,
    target: plan.target,
  });
  const inserted = owner.snapshot(),
    instance = inserted.layers[0].nodes.find((item) => item.id === result.nodeId),
    firstPath = evaluate(inserted.layers[0], {}, {}, inserted).path;
  assert.equal(instance.macro.definitionId, definition.id);
  send('set-macro-override', {
    layerId: layer.id,
    nodeId: instance.id,
    parameterId: 'size',
    value: 1,
  });
  const edited = owner.snapshot(),
    secondPath = evaluate(edited.layers[0], {}, {}, edited).path;
  assert.notDeepEqual(secondPath.contours[0].points, firstPath.contours[0].points);
  send('undo');
  assert.deepEqual(
    owner.snapshot().layers[0].nodes.find((item) => item.id === instance.id).macro.overrides,
    {},
  );
});
