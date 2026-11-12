import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  document,
  evaluate,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { nullPositionDeclaration } from '../src/null-position-declaration.js';
import { evaluateNullPosition } from '../src/null-position-binding.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const bindings = () => new Map([['builtin.null-position.v1', { evaluate: evaluateNullPosition }]]);

test('Null uses a frozen production declaration and the trusted position binding', () => {
  const registered = builtinNodeRegistry.get('Null'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Null');
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, nullPositionDeclaration);
  assert.ok(Object.isFrozen(registered.declaration.parameters[0].default));
  assert.equal(manifest.binding.evaluate, evaluateNullPosition);
  const parsed = parseNodeDeclaration(JSON.stringify(nullPositionDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Null');
  const reads = [],
    position = [13, -7];
  assert.equal(
    resolved.evaluate('subtract', (key) => {
      reads.push(key);
      return position;
    }),
    position,
  );
  assert.deepEqual(reads, ['position']);
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', type: 'Vector2', label: 'Position', legacyDefault: true },
  ]);
  for (const value of [[0], [0, 1, 2], [0, '1'], null]) {
    const invalid = structuredClone(nullPositionDeclaration);
    invalid.parameters[0].default = value;
    assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  }
});

test('Null preserves the Position label, unnamed output, defaults and catalog order', () => {
  const current = node('Null'),
    other = node('Null');
  assert.equal(current.name, 'Null');
  assert.equal(current.operation, 'subtract');
  assert.deepEqual(current.params.position, {
    type: 'Vector2',
    value: [300, 250],
    expression: '',
    locked: false,
  });
  assert.deepEqual(
    activeParameters(current).map(([id]) => id),
    ['position'],
  );
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: 'Position', type: 'Vector2' }]);
  for (const port of ['default', 'output', 'position'])
    assert.equal(outputType(current, port), undefined);
  assert.equal(bypassContract(current).supported, false);
  current.params.position.value[0] = -1;
  assert.deepEqual(other.params.position.value, [300, 250]);
  assert.deepEqual(definitions.Null.position.value, [300, 250]);
  const definitionsOrder = Object.keys(definitions),
    catalogOrder = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(definitionsOrder[definitionsOrder.indexOf('Null') - 1], 'Vector');
  assert.equal(catalogOrder[catalogOrder.indexOf('Null') - 1], 'Toggle');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === 'Null');
  assert.equal(typeId, 'Null');
  assert.deepEqual(metadata, nullPositionDeclaration.catalog);
});

test('Null Session preserves source priority, rejection atomicity, Undo/Redo and reload', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Null'),
    target = node('Null'),
    wrong = node('Value');
  doc.schemaVersion = 26;
  source.params.position.value = [12, 34];
  target.params.position.value = [80, 90];
  target.params.position.expression = 'value * 2; value * 3';
  layer.nodes.push(source, target, wrong);
  connect(layer, source.id, target.id, 'position');
  const session = new CommandSession(doc);
  const dispatch = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: target.id, key: 'position', ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(target.id);
  const connected = serialize(session.snapshot());
  assert.deepEqual(run(), [12, 34]);
  dispatch('disconnect');
  const disconnected = serialize(session.snapshot());
  assert.deepEqual(run(), [160, 270]);
  assert.equal(session.historyView.past.length, 1);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), connected);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  for (const [type, payload] of [
    ['connect', { sourceId: wrong.id }],
    ['set-parameter', { value: [1] }],
  ]) {
    assert.throws(() => dispatch(type, payload), /connect|Vector2/i);
    assert.equal(serialize(session.snapshot()), connected);
    assert.equal(session.state.expectedRevision, revision);
    assert.equal(JSON.stringify(session.historyView), history);
  }
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), disconnected);
  dispatch('set-parameter-expression', { expression: '' });
  assert.deepEqual(run(), [80, 90]);
  const before = serialize(session.snapshot());
  dispatch('set-parameter', { value: [5, 8] });
  const changed = serialize(session.snapshot());
  assert.deepEqual(run(), [5, 8]);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), before);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), changed);
  const reloaded = deserialize(changed);
  assert.equal(serialize(reloaded), changed);
  assert.deepEqual(evaluate(reloaded.layers[0]).run(target.id), [5, 8]);
  assert.doesNotMatch(changed, /implementationId|capabilities|legacyDefault/);
});

test('Null stable source identity survives Composition project Undo/Redo and reopen', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Null'),
    target = node('Null');
  layer.nodes.push(source, target);
  connect(layer, source.id, target.id, 'position');
  const session = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const before = serializeCompositionProject(session.projectSnapshot());
  session.dispatch({
    ...session.state,
    type: 'set-parameter',
    payload: { layerId: layer.id, nodeId: source.id, key: 'position', value: [44, 55] },
  });
  const after = serializeCompositionProject(session.projectSnapshot());
  session.dispatch({ ...session.state, type: 'undo' });
  assert.equal(serializeCompositionProject(session.projectSnapshot()), before);
  session.dispatch({ ...session.state, type: 'redo' });
  assert.equal(serializeCompositionProject(session.projectSnapshot()), after);
  const reopened = deserializeCompositionProject(after),
    reloaded = activeCompositionDocument(reopened, reopened.compositions[0].id),
    reloadedLayer = reloaded.layers.find((item) => item.id === layer.id);
  assert.equal(serializeCompositionProject(reopened), after);
  assert.equal(
    reloadedLayer.nodes.find((item) => item.id === target.id).params.position.source,
    source.id,
  );
  assert.deepEqual(evaluate(reloadedLayer).run(target.id), [44, 55]);
  assert.doesNotMatch(after, /implementationId|capabilities|legacyDefault/);
});
