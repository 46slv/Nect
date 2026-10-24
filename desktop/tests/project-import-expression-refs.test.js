import test from 'node:test';
import assert from 'node:assert/strict';
import { blankDocument, node, evaluate, serialize, CommandSession } from '../src/core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  validateCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';
import { prepareProjectCompositionsImport } from '../src/composition-project-import.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

function project(prefix, nodes) {
  const doc = blankDocument();
  doc.schemaVersion = Math.max(doc.schemaVersion, 44);
  doc.name = prefix;
  const layer = doc.layers[0];
  layer.id = `${prefix}-layer`;
  const output = node('Output');
  output.id = `${prefix}-output`;
  layer.nodes = [...nodes, output];
  layer.output = output.id;
  const result = migrateLegacyDocumentToCompositionProject(doc);
  result.compositions[0].id = `${prefix}-composition`;
  return result;
}

function value(id, amount = 42) {
  const result = node('Value');
  result.id = id;
  result.params.value.value = amount;
  return result;
}

function fixture(collision = true) {
  const driver = value('driver');
  const circle = node('Circle');
  circle.id = 'circle';
  circle.params.radius.expression = 'ref("driver","value")';
  const sink = value('sink', 0);
  sink.params.value.expression = 'ref("driver","value")';
  return {
    current: project('destination', [value(collision ? 'driver' : 'unrelated', 999)]),
    source: project('source', [driver, circle, sink]),
  };
}

function freeze(value) {
  if (value && typeof value === 'object') {
    Object.values(value).forEach(freeze);
    Object.freeze(value);
  }
  return value;
}

test('project composition import retargets Circle expression after a node ID collision', () => {
  const { current, source } = fixture();
  const beforeCurrent = JSON.stringify(current),
    beforeSource = JSON.stringify(source);
  freeze(current);
  freeze(source);
  const { project: imported } = prepareProjectCompositionsImport(current, source);
  const layer = imported.compositions[1].layers[0];
  assert.equal(layer.nodes[0].id, 'driver-import-1');
  assert.equal(layer.nodes[1].params.radius.expression, 'ref("driver-import-1","value")');
  assert.ok(layer.nodes.some((n) => n.id === 'driver-import-1'));
  assert.equal(evaluate(layer).run('sink'), 42);
  assert.equal(evaluate(layer).parameterValue('circle', 'radius'), 42);
  assert.doesNotThrow(() => evaluate(layer).run('circle'));
  assert.equal(validateCompositionProject(imported), imported);
  assert.equal(JSON.stringify(current), beforeCurrent);
  assert.equal(JSON.stringify(source), beforeSource);
});

test('noncolliding import preserves expression bytes and destination expressions', () => {
  const { current, source } = fixture(false);
  current.compositions[0].layers[0].nodes[0].params.value.expression = 'ref("driver","value")';
  const before = structuredClone(current);
  const { project: imported } = prepareProjectCompositionsImport(freeze(current), freeze(source));
  assert.equal(
    imported.compositions[1].layers[0].nodes[1].params.radius.expression,
    'ref("driver","value")',
  );
  assert.deepEqual(imported.compositions[0], before.compositions[0]);
  assert.equal(evaluate(imported.compositions[1].layers[0]).run('sink'), 42);
});

test('only same-layer imported nodes qualify; cross-layer, external and non-node IDs stay byte-exact', () => {
  const ids = [
    'foreign',
    'only-destination',
    'outer-target',
    'layer-target',
    'point-target',
    'not-selected',
  ];
  const current = project(
    'destination',
    ids.map((id) => value(id)),
  );
  const source = fixture().source;
  const comp = source.compositions[0],
    layer = comp.layers[0];
  comp.id = 'outer-target';
  layer.id = 'layer-target';
  const curve = node('Curve');
  curve.id = 'curve';
  curve.points[0].id = 'point-target';
  layer.nodes.push(curve);
  comp.layers.push({ ...structuredClone(layer), id: 'other-layer', nodes: [value('foreign')] });
  const unselected = structuredClone(comp);
  unselected.id = 'unselected-composition';
  unselected.layers = [
    { ...structuredClone(layer), id: 'unselected-layer', nodes: [value('not-selected')] },
  ];
  source.compositions.push(unselected);
  const original = ids.map((id) => `ref("${id}","value")`).join(' + ');
  layer.nodes[1].params.radius.expression = original;
  const { project: imported } = prepareProjectCompositionsImport(freeze(current), freeze(source), [
    'outer-target',
  ]);
  const copied = imported.compositions[1];
  assert.equal(copied.id, 'outer-target-import-1');
  assert.equal(copied.layers[0].id, 'layer-target-import-1');
  assert.equal(copied.layers[1].nodes[0].id, 'foreign-import-1');
  assert.equal(
    copied.layers[0].nodes.find((n) => n.id === 'curve').points[0].id,
    'point-target-import-1',
  );
  assert.equal(copied.layers[0].nodes[1].params.radius.expression, original);
  assert.equal(imported.compositions.length, 2, 'unselected source Composition stays absent');
});

test('every occurrence retains whitespace, parameter key, vector component and connected priority', () => {
  for (const whitespace of ['', ' ', '\t', '\r\n', '\u00a0']) {
    const { current, source } = fixture();
    const l = source.compositions[0].layers[0];
    const vector = node('Vector');
    vector.id = 'vector';
    vector.params.value.value = [3, 7];
    const vectorConsumer = node('Vector');
    vectorConsumer.id = 'vector-consumer';
    l.nodes.push(vector, vectorConsumer);
    current.compositions[0].layers[0].nodes.push(value('vector'), value('driver-import-1'));
    const original = `ref${whitespace}(\n "driver" , "value" ) + ref("driver","value"); ref ("vector", "value", "y")`;
    vectorConsumer.params.value.expression = original;
    // A connected field keeps a dormant expression for a later disconnect.
    l.nodes.find((n) => n.id === 'sink').params.value.source = 'driver';
    l.nodes.find((n) => n.id === 'sink').params.value.expression =
      `ref${whitespace}("vector","value","x")`;
    const copied = prepareProjectCompositionsImport(freeze(current), freeze(source)).project
      .compositions[1].layers[0];
    const copiedVector = copied.nodes.find((n) => n.id === 'vector-consumer');
    assert.equal(
      copiedVector.params.value.expression,
      original
        .replaceAll('"driver"', '"driver-import-2"')
        .replaceAll('"vector"', '"vector-import-1"'),
    );
    const copiedSink = copied.nodes.find((n) => n.id === 'sink');
    assert.equal(copiedSink.params.value.source, 'driver-import-2');
    assert.equal(
      copiedSink.params.value.expression,
      `ref${whitespace}("vector-import-1","value","x")`,
    );
    assert.equal(
      evaluate(copied).run('sink'),
      42,
      'connection still overrides the dormant expression',
    );
    assert.deepEqual(evaluate(copied).run('vector-consumer'), [84, 7]);
    copiedSink.params.value.source = null;
    assert.equal(
      evaluate(copied).run('sink'),
      3,
      'disconnect restores the retained remapped expression',
    );
  }
});

test('Color, point and remap-edit inputs follow renamed nodes while text/metadata do not', () => {
  const { current, source } = fixture();
  const layer = source.compositions[0].layers[0];
  const color = node('Color');
  color.id = 'color';
  color.params.value.value = { hex: '#123456', alpha: 0.5 };
  const fill = node('Fill');
  fill.id = 'fill';
  fill.params.path.source = 'circle';
  fill.params.color.expression = 'ref("color","value")';
  current.compositions[0].layers[0].nodes.push(value('color'));
  layer.nodes.push(color, fill);
  const curve = node('Curve');
  curve.id = 'curve';
  const expression = 'ref ("driver", "value")';
  curve.points[0].position.expression = `${expression}; ${expression}`;
  curve.edits = [
    {
      pointId: 'pending-edit',
      params: {
        position: { type: 'Vector2', expression: `${expression}; ${expression}`, value: [0, 0] },
      },
    },
  ];
  curve.params.savedText = { type: 'String', value: expression, expression };
  curve.name = expression;
  curve.metadata = { type: 'Scalar', expression };
  layer.nodes.push(curve);
  const copiedLayer = prepareProjectCompositionsImport(freeze(current), freeze(source)).project
    .compositions[1].layers[0];
  const copied = copiedLayer.nodes.find((n) => n.id === 'curve');
  const expected = expression.replace('"driver"', '"driver-import-1"');
  assert.equal(copied.points[0].position.expression, `${expected}; ${expected}`);
  assert.equal(copied.edits[0].params.position.expression, `${expected}; ${expected}`);
  assert.equal(
    copiedLayer.nodes.find((n) => n.id === 'fill').params.color.expression,
    'ref("color-import-1","value")',
  );
  assert.deepEqual(evaluate(copiedLayer).parameterValue('fill', 'color'), {
    hex: '#123456',
    alpha: 0.5,
  });
  assert.deepEqual(copied.params.savedText, curve.params.savedText);
  assert.deepEqual(copied.metadata, curve.metadata);
  assert.equal(copied.name, expression);
});

test('unsupported quoted ID syntax and non-string expressions are not reinterpreted', () => {
  for (const expression of [
    "ref('driver','value')",
    'ref("dri\\"ver","value")',
    'xref("driver","value")',
    'ref("driver-long","value")',
    '',
    null,
    12,
  ]) {
    const { current, source } = fixture();
    source.compositions[0].layers[0].nodes[1].params.radius.expression = expression;
    const copied = prepareProjectCompositionsImport(freeze(current), freeze(source)).project
      .compositions[1].layers[0];
    assert.equal(copied.nodes[1].params.radius.expression, expression);
  }
});

test('accepted descriptor aliases cannot rewrite metadata or another layer reference', () => {
  const { current, source } = fixture();
  const layer = source.compositions[0].layers[0];
  const circle = layer.nodes.find((n) => n.id === 'circle');
  circle.metadata = circle.params.radius;
  const external = value('external');
  external.params.value = circle.params.radius;
  source.compositions[0].layers.push({ id: 'alias-layer', name: 'Other layer', nodes: [external] });
  source.compositions[0].payload = Object.assign(Object.create(null), { negativeZero: -0 });
  Object.defineProperty(source.compositions[0].payload, '__proto__', {
    value: 'literal',
    enumerable: true,
  });
  validateCompositionProject(source);
  const { project: imported } = prepareProjectCompositionsImport(freeze(current), freeze(source));
  const copied = imported.compositions[1].layers[0].nodes.find((n) => n.id === 'circle');
  assert.equal(copied.params.radius.expression, 'ref("driver-import-1","value")');
  assert.equal(copied.metadata.expression, 'ref("driver","value")');
  assert.equal(
    imported.compositions[1].layers[1].nodes[0].params.value.expression,
    'ref("driver","value")',
  );
  assert.equal(circle.metadata.expression, 'ref("driver","value")');
  assert.ok(Object.is(imported.compositions[1].payload.negativeZero, -0));
  assert.equal(Object.hasOwn(imported.compositions[1].payload, '__proto__'), true);
  assert.equal(imported.compositions[1].payload.__proto__, 'literal');
  assert.equal(Object.getPrototypeOf(imported.compositions[1].payload), Object.prototype);
});

test('real Composition Session import evaluates correctly and is one Undo/Redo transaction', () => {
  const { current, source } = fixture();
  const sourceBytes = JSON.stringify(source);
  const owner = createCompositionSessionOwner(current);
  const before = serializeCompositionProject(owner.projectSnapshot());
  const revision = owner.projectRevision,
    past = owner.historyView.past.length;
  const send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload });
  const result = send('import-project-compositions', { sourceProject: freeze(source) });
  assert.equal(result.changed, true);
  assert.equal(owner.projectRevision, revision + 1);
  assert.equal(owner.historyView.past.length, past + 1);
  const after = serializeCompositionProject(owner.projectSnapshot());
  assert.equal(evaluate(owner.activeDocument().layers[0]).run('sink'), 42);
  assert.equal(evaluate(owner.activeDocument().layers[0]).parameterValue('circle', 'radius'), 42);
  assert.doesNotThrow(() => evaluate(owner.activeDocument().layers[0]).run('circle'));
  assert.equal(JSON.stringify(source), sourceBytes);
  send('undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send('redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  owner.switchComposition(result.importedCompositionIds[0]);
  assert.equal(evaluate(owner.activeDocument().layers[0]).run('sink'), 42);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after), {
    activeCompositionId: result.importedCompositionIds[0],
  });
  assert.equal(evaluate(reopened.activeDocument().layers[0]).run('sink'), 42);
});

test('legacy composition import shares the same remap without changing its transaction boundary', () => {
  const { current, source } = fixture();
  const legacy = (project) => {
    const { compositions, compositionSchemaVersion, ...shared } = project;
    const { width, height, layers, editor } = compositions[0];
    return { ...shared, width, height, layers, ...(editor ? { editor } : {}) };
  };
  const currentDoc = legacy(current),
    sourceDoc = legacy(source);
  const session = new CommandSession(currentDoc);
  const before = serialize(session.snapshot());
  const sourceText = serialize(sourceDoc);
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  send('import-composition', { sourceText, compositionId: 'source-layer' });
  const after = serialize(session.snapshot());
  const layer = session.snapshot().layers.find((l) => l.id === 'source-layer');
  assert.equal(layer.nodes[1].params.radius.expression, 'ref("driver-import-1","value")');
  assert.equal(evaluate(layer).run('sink'), 42);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), after);
  assert.equal(serialize(sourceDoc), sourceText);
});
