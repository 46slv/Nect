import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  node,
  connect,
  CommandSession,
  evaluate,
  serialize,
  deserialize,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import {
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
  deserializeCompositionProject,
} from '../src/composition-project-model.js';

function fixture() {
  const doc = blankDocument();
  doc.schemaVersion = 44;
  const layer = doc.layers[0],
    driver = node('Value'),
    circle = node('Circle'),
    fill = node('Fill'),
    output = layer.nodes[0];
  driver.id = 'driver';
  driver.params.value.value = 42;
  circle.id = 'circle';
  circle.params.radius.expression = 'ref ( "driver", "value" ) + ref("driver","value")';
  circle.name = 'ref("driver","value")';
  layer.nodes = [driver, circle, fill, output];
  connect(layer, circle.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, driver, circle };
}

test('generic layer duplicate remaps internal expressions and keeps an independent undoable copy', () => {
  const { doc, layer } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    before = serialize(session.snapshot()),
    revision = session.state.expectedRevision;
  const result = send('duplicate-layer', { layerId: layer.id });
  const duplicated = session.snapshot(),
    copy = duplicated.layers.find((l) => l.id === result.layerId),
    [driver, circle] = copy.nodes;
  assert.deepEqual(duplicated.layers[0], doc.layers[0]);
  assert.notEqual(driver.id, 'driver');
  assert.notEqual(circle.id, 'circle');
  assert.equal(circle.name, 'ref("driver","value")');
  assert.equal(
    circle.params.radius.expression,
    `ref ( "${driver.id}", "value" ) + ref("${driver.id}","value")`,
  );
  assert.equal(evaluate(copy).parameterValue(circle.id, 'radius'), 84);
  assert.equal(session.state.expectedRevision, revision + 1);
  const after = serialize(duplicated);
  assert.equal(serialize(deserialize(after)), after);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), after);
  send('set-parameter', { layerId: copy.id, nodeId: driver.id, key: 'value', value: 10 });
  const edited = session.snapshot();
  assert.equal(evaluate(edited.layers[1]).parameterValue(circle.id, 'radius'), 20);
  assert.equal(evaluate(edited.layers[0]).parameterValue('circle', 'radius'), 84);
});

test('typed vector, color and point expressions remap, including dormant connected expressions', () => {
  const { doc, layer } = fixture(),
    vector = node('Vector'),
    color = node('Color'),
    curve = node('Curve'),
    fill = layer.nodes[2];
  vector.id = 'vector';
  vector.params.value.value = [3, 7];
  color.id = 'color';
  color.params.value.value = { hex: '#123456', alpha: 0.5 };
  fill.params.color.expression = 'ref("color","value")';
  curve.points[0].position.expression = 'ref("vector","value","x"); ref("vector","value","y")';
  layer.nodes[1].params.radius.source = 'driver';
  layer.nodes[1].params.radius.expression = 'ref("vector","value","y")';
  layer.nodes.push(vector, color, curve);
  const session = new CommandSession(doc);
  session.dispatch({ ...session.state, type: 'duplicate-layer', payload: { layerId: layer.id } });
  const copy = session.snapshot().layers[1],
    [copiedDriver, copiedCircle, copiedFill, , copiedVector, copiedColor, copiedCurve] = copy.nodes;
  assert.equal(copiedCircle.params.radius.source, copiedDriver.id);
  assert.equal(copiedCircle.params.radius.expression, `ref("${copiedVector.id}","value","y")`);
  assert.equal(evaluate(copy).parameterValue(copiedCircle.id, 'radius'), 42);
  copiedCircle.params.radius.source = null;
  assert.equal(evaluate(copy).parameterValue(copiedCircle.id, 'radius'), 7);
  assert.equal(copiedFill.params.color.expression, `ref("${copiedColor.id}","value")`);
  assert.deepEqual(evaluate(copy).parameterValue(copiedFill.id, 'color'), color.params.value.value);
  assert.equal(
    copiedCurve.points[0].position.expression,
    `ref("${copiedVector.id}","value","x"); ref("${copiedVector.id}","value","y")`,
  );
  assert.doesNotThrow(() => evaluate(copy).run(copiedCurve.id));
});

test('Composition Session duplicate retains original, history and JSON reopen', () => {
  const { doc, layer } = fixture(),
    owner = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc)),
    send = (type, payload = {}) => owner.dispatch({ ...owner.state, type, payload }),
    before = serializeCompositionProject(owner.projectSnapshot());
  send('duplicate-layer', { layerId: layer.id });
  const after = serializeCompositionProject(owner.projectSnapshot());
  send('undo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), before);
  send('redo');
  assert.equal(serializeCompositionProject(owner.projectSnapshot()), after);
  const reopened = createCompositionSessionOwner(deserializeCompositionProject(after)),
    copy = reopened.activeDocument().layers[1];
  assert.equal(evaluate(copy).parameterValue(copy.nodes[1].id, 'radius'), 84);
  assert.equal(serializeCompositionProject(reopened.projectSnapshot()), after);
});

test('locked and missing duplicate targets refuse without changing state or history', () => {
  const { doc, layer } = fixture();
  layer.locked = true;
  const session = new CommandSession(doc),
    before = serialize(session.snapshot()),
    state = session.state;
  for (const id of [layer.id, 'missing-layer']) {
    assert.throws(() =>
      session.dispatch({ ...session.state, type: 'duplicate-layer', payload: { layerId: id } }),
    );
    assert.equal(serialize(session.snapshot()), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.past.length, 0);
  }
});

test('dangling and non-node targets remain unchanged, and metadata is not expression-authored state', () => {
  const { doc, layer, circle } = fixture();
  circle.params.radius.source = 'driver';
  circle.params.radius.expression = `ref("${layer.id}","value") + ref("deleted-node","value")`;
  circle.metadata = { type: 'Scalar', expression: 'ref("driver","value")' };
  const session = new CommandSession(doc);
  session.dispatch({ ...session.state, type: 'duplicate-layer', payload: { layerId: layer.id } });
  const copy = session.snapshot().layers[1].nodes[1];
  assert.equal(copy.params.radius.expression, circle.params.radius.expression);
  assert.deepEqual(copy.metadata, circle.metadata);
});
