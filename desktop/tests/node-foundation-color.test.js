import test from 'node:test';
import assert from 'node:assert/strict';
import {
  activeParameters,
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  evaluate,
  exportScene,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { colorDeclaration } from '../src/color-node-declaration.js';
import { evaluateValueSource } from '../src/value-source-binding.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const bindings = () => new Map([['builtin.read-value.v1', { evaluate: evaluateValueSource }]]);

test('production Color resolves through the shared registered value binding', () => {
  const registered = builtinNodeRegistry.get('Color'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Color');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateValueSource);
  assert.deepEqual(registered.declaration, colorDeclaration);
  assert.ok(Object.isFrozen(registered.declaration.parameters[0].default));
});
const initial = { hex: '#aBcD09', alpha: 0.6 },
  changed = { hex: '#1234aB', alpha: 0.3 };

function slotFixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    source = node('Color'),
    relay = node('Color'),
    fill = node('Fill'),
    wrong = node('Value');
  doc.schemaVersion = 54;
  doc.colorSlots = [{ id: 'brand', name: 'Brand', value: initial }];
  source.params.value.colorSlotId = 'brand';
  layer.nodes.push(shape, source, relay, fill, wrong);
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, source.id, relay.id, 'value');
  connect(layer, relay.id, fill.id, 'color');
  connect(layer, fill.id, layer.output, 'path');
  return { doc, layer, source, relay, fill, wrong };
}

test('Color declaration captures the existing default and rejects invalid literal defaults', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(colorDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Color');
  assert.ok(Object.isFrozen(resolved.declaration.parameters[0].default));
  assert.equal(resolved.interface().resultType, 'Color');
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', type: 'Color', label: '', legacyDefault: true },
  ]);
  const reads = [];
  assert.equal(
    resolved.evaluate('subtract', (key) => {
      reads.push(key);
      return initial;
    }),
    initial,
  );
  assert.deepEqual(reads, ['value']);
  for (const value of [
    null,
    '#123456',
    [0, 0, 0, 1],
    { hex: 'red', alpha: 1 },
    { hex: '#123456', alpha: -1 },
    { hex: '#123456', alpha: 1.1 },
    { hex: '#123456', alpha: '1' },
    { hex: '#123456', alpha: 1, extra: true },
  ]) {
    const invalid = structuredClone(colorDeclaration);
    invalid.parameters[0].default = value;
    assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  }
});

test('Color factory, default socket and catalog keep current product behavior', () => {
  const current = node('Color'),
    other = node('Color');
  assert.equal(current.name, 'Color');
  assert.equal(current.operation, 'subtract');
  assert.deepEqual(current.params.value, {
    type: 'Color',
    value: { hex: '#36434e', alpha: 1 },
    expression: '',
    locked: false,
  });
  assert.deepEqual(
    activeParameters(current).map(([id]) => id),
    ['value'],
  );
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Color' }]);
  for (const port of ['default', 'output', 'value'])
    assert.equal(outputType(current, port), undefined);
  assert.equal(bypassContract(current).supported, false);
  current.params.value.value.alpha = 0;
  assert.equal(other.params.value.value.alpha, 1);
  assert.equal(definitions.Color.value.value.alpha, 1);
  const definitionsOrder = Object.keys(definitions),
    catalogOrder = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(definitionsOrder[definitionsOrder.indexOf('Color') - 1], 'Toggle');
  assert.equal(catalogOrder[catalogOrder.indexOf('Color') - 1], 'Luminance Instances');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === 'Color');
  assert.equal(typeId, 'Color');
  assert.deepEqual(metadata, colorDeclaration.catalog);
});

test('direct Color edits preserve exact values, atomic failure and one-step history', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Color'),
    target = node('Color');
  layer.nodes.push(source, target);
  connect(layer, source.id, target.id, 'value');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: source.id, key: 'value', ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(target.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { value: initial });
  const after = serialize(session.snapshot());
  assert.deepEqual(run(), initial);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { value: { hex: '#bad', alpha: 1 } }), /Color/);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), after);
  assert.deepEqual(evaluate(deserialize(after).layers[0]).run(target.id), initial);
});

test('live slot through Color source chain retains paint attribution, Undo/Redo and project reload', () => {
  const { doc, layer, source, relay, wrong } = slotFixture();
  const session = createCompositionSessionOwner(migrateLegacyDocumentToCompositionProject(doc));
  const send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload });
  const project = () => serializeCompositionProject(session.projectSnapshot());
  const inspect = () => {
    const d = session.snapshot(),
      l = d.layers[0],
      result = evaluate(l, {}, d.imageAssets, d);
    return {
      source: result.run(source.id),
      relay: result.run(relay.id),
      fill: result.path.fill,
      provenance: exportScene(d)[0].paintProvenance.fill,
    };
  };
  const before = project();
  assert.deepEqual(inspect(), {
    source: initial,
    relay: initial,
    fill: initial,
    provenance: 'brand',
  });
  send('set-color-slot', { slotId: 'brand', value: changed });
  const after = project();
  assert.deepEqual(inspect(), {
    source: changed,
    relay: changed,
    fill: changed,
    provenance: 'brand',
  });
  send('undo');
  assert.equal(project(), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(
    () =>
      send('connect', { layerId: layer.id, nodeId: relay.id, key: 'value', sourceId: wrong.id }),
    /connect/i,
  );
  assert.equal(project(), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(project(), after);
  const reopened = deserializeCompositionProject(after),
    reloaded = activeCompositionDocument(reopened, reopened.compositions[0].id),
    reloadedLayer = reloaded.layers.find((item) => item.id === layer.id);
  assert.equal(serializeCompositionProject(reopened), after);
  assert.equal(
    reloadedLayer.nodes.find((item) => item.id === source.id).params.value.colorSlotId,
    'brand',
  );
  assert.equal(
    reloadedLayer.nodes.find((item) => item.id === relay.id).params.value.source,
    source.id,
  );
  assert.deepEqual(evaluate(reloadedLayer, {}, reloaded.imageAssets, reloaded).path.fill, changed);
  assert.equal(exportScene(reloaded)[0].paintProvenance.fill, 'brand');
  assert.doesNotMatch(after, /implementationId|capabilities|legacyDefault/);
});

test('a shared slot change invalidates cached Color outputs and downstream paint', () => {
  const { doc, layer } = slotFixture();
  const first = evaluate(layer, {}, doc.imageAssets, doc),
    reused = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(first.path.fill, initial);
  assert.equal(reused.stats.computed, 0);
  assert.ok(reused.stats.reused > 0);
  doc.colorSlots[0].value = changed;
  const next = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(next.path.fill, changed);
  assert.ok(next.stats.computed > 0);
  assert.deepEqual(first.path.fill, initial);
  assert.equal(exportScene(doc)[0].paintProvenance.fill, 'brand');
});
