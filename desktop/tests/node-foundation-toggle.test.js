import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  definitions,
  activeParameters,
  outputPorts,
  outputType,
  bypassContract,
  connect,
  evaluate,
  serialize,
  deserialize,
  CommandSession,
} from '../src/core.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { toggleDeclaration } from '../src/toggle-node-declaration.js';
import { evaluateValueSource } from '../src/value-source-binding.js';
import { nodeCatalog } from '../src/node-catalog.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const bindings = () => new Map([['builtin.read-value.v1', { evaluate: evaluateValueSource }]]);

test('Boolean type and Toggle declaration resolve as immutable trusted data', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(toggleDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Toggle');
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', type: 'Boolean', label: '', legacyDefault: true },
  ]);
  assert.equal(resolved.interface().resultType, 'Boolean');
  assert.ok(Object.isFrozen(resolved.declaration.parameters[0]));
  const reads = [];
  assert.equal(
    resolved.evaluate('subtract', (key) => {
      reads.push(key);
      return false;
    }),
    false,
  );
  assert.deepEqual(reads, ['value']);
  for (const value of [0, 1, 'false', null, [], {}]) {
    const invalid = structuredClone(toggleDeclaration);
    invalid.parameters[0].default = value;
    assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  }
});

test('Toggle factory, output socket and catalog preserve the current product defaults', () => {
  const toggle = node('Toggle'),
    other = node('Toggle');
  assert.equal(toggle.name, 'Toggle');
  assert.equal(toggle.operation, 'subtract');
  assert.deepEqual(toggle.params.value, {
    type: 'Boolean',
    value: true,
    expression: '',
    locked: false,
  });
  assert.deepEqual(
    activeParameters(toggle).map(([id]) => id),
    ['value'],
  );
  assert.deepEqual(outputPorts(toggle), [{ id: undefined, label: '', type: 'Boolean' }]);
  for (const port of ['default', 'output', 'value'])
    assert.equal(outputType(toggle, port), undefined);
  assert.equal(bypassContract(toggle).supported, false);
  toggle.params.value.value = false;
  assert.equal(other.params.value.value, true);
  assert.equal(definitions.Toggle.value.value, true);
  const definitionsOrder = Object.keys(definitions),
    catalogOrder = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(definitionsOrder[definitionsOrder.indexOf('Toggle') - 1], 'Combine Vector');
  assert.equal(catalogOrder[catalogOrder.indexOf('Toggle') - 1], 'Combine Vector');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === 'Toggle');
  assert.equal(typeId, 'Toggle');
  assert.deepEqual(metadata, toggleDeclaration.catalog);
});

test('Toggle Session preserves connection, atomic rejection, undo, redo and document reload', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Toggle'),
    target = node('Toggle'),
    wrong = node('Value');
  source.params.value.value = false;
  layer.nodes.push(source, target, wrong);
  connect(layer, source.id, target.id, 'value');
  const session = new CommandSession(doc);
  const dispatch = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: target.id, key: 'value', ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(target.id);
  const connected = serialize(session.snapshot());
  assert.equal(run(), false);
  dispatch('disconnect');
  const disconnected = serialize(session.snapshot());
  assert.equal(run(), true);
  assert.equal(session.historyView.past.length, 1);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), connected);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  for (const [type, payload] of [
    ['connect', { sourceId: wrong.id }],
    ['set-parameter', { value: 1 }],
  ]) {
    assert.throws(() => dispatch(type, payload), /connect|Boolean/i);
    assert.equal(serialize(session.snapshot()), connected);
    assert.equal(session.state.expectedRevision, revision);
    assert.equal(JSON.stringify(session.historyView), history);
  }
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), disconnected);
  dispatch('set-parameter', { value: false });
  const changed = serialize(session.snapshot());
  assert.equal(run(), false);
  dispatch('undo');
  assert.equal(serialize(session.snapshot()), disconnected);
  dispatch('redo');
  assert.equal(serialize(session.snapshot()), changed);
  const loaded = deserialize(changed);
  assert.equal(serialize(loaded), changed);
  assert.equal(evaluate(loaded.layers[0]).run(target.id), false);
  assert.doesNotMatch(changed, /implementationId|capabilities|legacyDefault/);
});

test('Toggle keeps stable connection IDs through the current Composition project format', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Toggle'),
    target = node('Toggle');
  layer.nodes.push(source, target);
  connect(layer, source.id, target.id, 'value');
  const project = migrateLegacyDocumentToCompositionProject(doc),
    session = createCompositionSessionOwner(project);
  const before = serializeCompositionProject(session.projectSnapshot());
  session.dispatch({
    ...session.state,
    type: 'set-parameter',
    payload: { layerId: layer.id, nodeId: source.id, key: 'value', value: false },
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
    reloadedLayer.nodes.find((item) => item.id === target.id).params.value.source,
    source.id,
  );
  assert.equal(evaluate(reloadedLayer).run(target.id), false);
  assert.doesNotMatch(after, /implementationId|capabilities|legacyDefault/);
});

test('Toggle dependency cache reuses and invalidates Boolean results', () => {
  const doc = document(),
    layer = doc.layers[0],
    source = node('Toggle'),
    target = node('Toggle');
  source.params.value.value = false;
  layer.nodes = [source, target];
  layer.output = target.id;
  connect(layer, source.id, target.id, 'value');
  const first = evaluate(layer);
  assert.equal(first.path, false);
  assert.deepEqual(first.stats, { computed: 2, reused: 0 });
  const reused = evaluate(layer);
  assert.equal(reused.path, false);
  assert.deepEqual(reused.stats, { computed: 0, reused: 2 });
  source.params.value.value = true;
  const changed = evaluate(layer);
  assert.equal(changed.path, true);
  assert.deepEqual(changed.stats, { computed: 2, reused: 0 });
  assert.equal(first.path, false);
});
