import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  blankLayer,
  node,
  evaluate,
  CommandSession,
  serialize,
  deserialize,
} from '../src/core.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';
import { migrateLegacyDocumentToCompositionProject } from '../src/composition-project-model.js';

function fixture(version = 60) {
  const a = blankDocument(),
    b = blankDocument();
  a.layers[0].id = 'art-a';
  b.layers[0].id = 'art-b';
  const sink = node('Value'),
    driver = node('Value'),
    color = node('Color'),
    vector = node('Vector'),
    curve = node('Curve');
  sink.id = 'sink';
  driver.id = 'driver';
  driver.params.value.value = 7;
  color.id = 'color';
  vector.id = 'vector';
  curve.id = 'curve';
  curve.points[0].id = 'point';
  curve.points[0].handleA.value = [3, 4];
  a.layers[0].nodes.push(sink, driver, color, vector, curve);
  const bDriver = node('Value');
  bDriver.id = 'b-driver';
  b.layers[0].nodes.push(bDriver);
  const extra = blankLayer('Extra'),
    source = node('Color'),
    extraValue = node('Value');
  extra.id = 'art-extra';
  source.id = 'source-color';
  extraValue.id = 'extra-value';
  extraValue.params.value.value = 19;
  extra.nodes.push(source, extraValue);
  a.layers.push(extra);
  const project = migrateLegacyDocumentToCompositionProject(a);
  project.schemaVersion = version;
  project.compositionSchemaVersion = version >= 60 ? 2 : 1;
  project.compositions[0].id = 'comp-a';
  project.compositions.push({
    id: 'comp-b',
    name: 'B',
    width: b.width,
    height: b.height,
    layers: b.layers,
    editor: b.editor,
  });
  return project;
}
const send = (session, type, payload = {}) => session.dispatch({ ...session.state, type, payload });
const setExpr = (session, nodeId, key, expression) =>
  send(session, 'set-parameter-expression', { layerId: 'art-a', nodeId, key, expression });
const boundSession = (project) =>
  new CommandSession(createCompositionSessionOwner(project).snapshot());
const promote = (session) =>
  setExpr(session, 'color', 'value', 'ref(colorArtwork, "art-extra", "source-color", "value")');

test('schema 64 unrelated Color clipboard preserves restored schema 60 and 61 with Undo/Redo', () => {
  for (const version of [60, 61]) {
    const session = boundSession(fixture(version));
    promote(session);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['vector'] });
    send(session, 'undo');
    const before = session.snapshot();
    send(session, 'paste-nodes', { layerId: 'art-a' });
    const after = session.snapshot();
    assert.equal(after.schemaVersion, version);
    assert.equal(deserialize(serialize(after)).schemaVersion, version);
    send(session, 'undo');
    assert.deepEqual(session.snapshot(), before);
    send(session, 'redo');
    assert.deepEqual(session.snapshot(), after);
  }
});

test('copied handle and numeric Artwork references require their actual schema 61 and 62', () => {
  for (const [expression, expected, value] of [
    ['ref("curve", "point.handleA", "length")', 61, 5],
    ['ref(artwork, "art-extra", "extra-value", "value")', 62, 19],
  ]) {
    const session = boundSession(fixture(60));
    promote(session);
    setExpr(session, 'sink', 'value', expression);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['sink'] });
    send(session, 'undo');
    send(session, 'undo');
    const id = send(session, 'paste-nodes', { layerId: 'art-a' }).nodeIds[0];
    const doc = session.snapshot();
    assert.equal(doc.schemaVersion, expected);
    assert.equal(
      evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(id, 'value'),
      value,
    );
    assert.equal(deserialize(serialize(doc)).schemaVersion, expected);
  }
});

test('pre-60 destination retains the existing conservative clipboard floor', () => {
  const session = boundSession(fixture(59));
  promote(session);
  send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['vector'] });
  send(session, 'undo');
  send(session, 'paste-nodes', { layerId: 'art-a' });
  assert.equal(session.snapshot().schemaVersion, 62);
});

test('schema 63 clipboard source retains its existing transfer behavior', () => {
  const session = boundSession(fixture(61));
  setExpr(session, 'sink', 'value', 'ref(composition, "comp-b", "art-b", "b-driver", "value")');
  send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['vector'] });
  send(session, 'undo');
  send(session, 'paste-nodes', { layerId: 'art-a' });
  assert.equal(session.snapshot().schemaVersion, 63);
});

test('paused missing Curve handle x/y stay missing without unnecessarily promoting schema 60', () => {
  for (const axis of ['x', 'y']) {
    const project = fixture(60),
      layer = project.compositions[0].layers[0];
    const sink = layer.nodes.find((n) => n.id === 'sink');
    sink.params.value.source = 'driver';
    sink.params.value.expression = `ref("deleted", "missing.handleA", "${axis}")`;
    const session = boundSession(project);
    promote(session);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['sink', 'driver'] });
    send(session, 'undo');
    const ids = send(session, 'paste-nodes', { layerId: 'art-a' }).nodeIds;
    const doc = session.snapshot(),
      copy = doc.layers[0].nodes.find(
        (n) => ids.includes(n.id) && n.type === 'Value' && n.params.value.expression,
      );
    assert.equal(doc.schemaVersion, 60);
    assert.equal(copy.params.value.expression, sink.params.value.expression);
    assert.equal(
      evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(copy.id, 'value'),
      7,
    );
    assert.doesNotThrow(() => deserialize(serialize(doc)));
  }
});

test('a paused missing-owner handle length still requires schema 61', () => {
  const session = boundSession(fixture(60));
  promote(session);
  setExpr(session, 'sink', 'value', 'ref("curve", "point.handleA", "length")');
  send(session, 'connect', { layerId: 'art-a', nodeId: 'sink', key: 'value', sourceId: 'driver' });
  send(session, 'delete-node', { layerId: 'art-a', nodeId: 'curve' });
  send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['sink', 'driver'] });
  for (let i = 0; i < 4; i++) send(session, 'undo');
  assert.equal(session.snapshot().schemaVersion, 60);
  send(session, 'delete-node', { layerId: 'art-a', nodeId: 'curve' });
  const ids = send(session, 'paste-nodes', { layerId: 'art-a' }).nodeIds;
  const doc = session.snapshot(),
    copy = doc.layers[0].nodes.find((n) => ids.includes(n.id) && n.params.value?.expression);
  assert.equal(doc.schemaVersion, 61);
  assert.equal(copy.params.value.expression, 'ref("curve", "point.handleA", "length")');
  assert.equal(
    evaluate(doc.layers[0], {}, doc.imageAssets, doc).parameterValue(copy.id, 'value'),
    7,
  );
  assert.doesNotThrow(() => deserialize(serialize(doc)));
});

test('promotion from copied references refuses inactive opaque text and preserves Redo', () => {
  for (const [version, expression] of [
    [60, 'ref("curve", "point.handleA", "length")'],
    [61, 'ref(artwork, "art-extra", "extra-value", "value")'],
  ]) {
    const original = fixture(version),
      session = boundSession(original);
    promote(session);
    setExpr(session, 'sink', 'value', expression);
    send(session, 'copy-nodes', { layerId: 'art-a', nodeIds: ['sink'] });
    send(session, 'undo');
    send(session, 'undo');
    const withOpaque = structuredClone(original),
      layer = withOpaque.compositions[1].layers[0];
    const holder = node('Value');
    holder.id = 'opaque-holder';
    if (version === 60) {
      const curve = node('Curve');
      curve.id = 'legacy-curve';
      layer.nodes.push(curve);
      holder.params.value.expression = 'ref("legacy-curve", "missing.handleA", "x")';
    } else holder.params.value.expression = 'ref(artwork, malformed';
    layer.nodes.push(holder);
    session.setProjectContext(withOpaque, 'comp-a');
    const before = session.snapshot(),
      state = session.state,
      future = session.historyView.future.length;
    assert.throws(() => send(session, 'paste-nodes', { layerId: 'art-a' }), /opaque|unresolved/i);
    assert.deepEqual(session.snapshot(), before);
    assert.deepEqual(session.state, state);
    assert.equal(session.historyView.future.length, future);
    send(session, 'redo');
    assert.equal(session.snapshot().schemaVersion, 64);
  }
});
