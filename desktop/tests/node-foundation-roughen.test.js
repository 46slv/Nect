import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import {
  activeParameters,
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  evaluate,
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
import { roughenDeclaration } from '../src/roughen-node-declaration.js';
import { evaluateRoughen } from '../src/roughen-node-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.roughen.v1', { evaluate: evaluateRoughen }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    roughen = node('Roughen'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  roughen.id = 'roughen-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, roughen, stroke);
  connect(layer, source.id, roughen.id, 'path');
  connect(layer, roughen.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, roughen };
}

test('production Roughen resolves through the trusted binding and retains its interface', () => {
  const registered = builtinNodeRegistry.get('Roughen'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Roughen');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateRoughen);
  assert.deepEqual(registered.declaration, roughenDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(roughenDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Roughen');
  assert.ok(Object.isFrozen(resolved.declaration));
  assert.equal(resolved.declaration.bypassInput, 'path');
  const invalid = structuredClone(roughenDeclaration);
  invalid.parameters[1].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Roughen');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.amount, { type: 'Scalar', value: 3, expression: '', locked: false });
  assert.deepEqual(n.params.seed, {
    type: 'Scalar',
    value: 1,
    expression: '',
    locked: false,
  });
  assert.deepEqual(n.params.keepEndpoints, {
    type: 'Boolean',
    value: true,
    expression: '',
    locked: false,
  });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'amount', 'seed', 'keepEndpoints'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Subdivide Path');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Resample Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Pucker & Bloat');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Boolean');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, roughenDeclaration.catalog);
});

test('Roughen retains pre-migration geometry, endpoint settings and zero-amount source IDs', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 L25 0 L50 0 L75 20 L100 20',
      3,
      1,
      true,
      'dddb933da715a3d020b5a90583f8efef9b46c1d232d88e76147ef48854cabc26',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      5,
      42,
      false,
      'a40ce392c59ecf2a4ab001c95bbd7758efdf94a75de18f81233f8281a2ca854b',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      10,
      0,
      false,
      '038d0f2cf38a7855f0b266476aac5d6ac221aeb68bd51dcdc6d8548fd0302c54',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      0,
      1,
      true,
      'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
    ],
  ];
  for (const [raw, amount, seed, keepEndpoints, expected] of cases) {
    const { layer, source, roughen } = scene(raw);
    roughen.params.amount.value = amount;
    roughen.params.seed.value = seed;
    roughen.params.keepEndpoints.value = keepEndpoints;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateRoughen(
      'subtract',
      (key) => {
        reads.push(key);
        return { path: input, amount, seed, keepEndpoints }[key];
      },
      { nodeId: roughen.id },
    );
    assert.deepEqual(reads, ['path', 'amount', 'seed', 'keepEndpoints']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(roughen.id)), expected);
    assert.equal(JSON.stringify(input), before);
    if (amount === 0) assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Roughen Session preserves atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, roughen } = scene('M0 0 L25 0 L50 0 L75 20 L100 20');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: roughen.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(roughen.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'amount', value: 4.5 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'amount', value: -1 }), /amount/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  assert.deepEqual(run(), editedResult);
  send('set-node-bypassed', { bypassed: true });
  const bypassed = serialize(session.snapshot());
  assert.deepEqual(run(), evaluate(session.snapshot().layers[0]).run(source.id));
  send('undo');
  assert.equal(serialize(session.snapshot()), edited);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.equal(serialize(deserialize(bypassed)), bypassed);
  const project = migrateLegacyDocumentToCompositionProject(session.snapshot()),
    owner = createCompositionSessionOwner(project),
    saved = serializeCompositionProject(owner.projectSnapshot()),
    reopened = deserializeCompositionProject(saved),
    restored = activeCompositionDocument(reopened, reopened.compositions[0].id);
  assert.equal(serializeCompositionProject(reopened), saved);
  assert.equal(
    restored.layers[0].nodes.find((n) => n.id === roughen.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(roughen.id),
    evaluate(restored.layers[0]).run(source.id),
  );
  assert.doesNotMatch(saved, /implementationId|capabilities|legacyDefault/);
});

test('actual Text route and point identities match pre-migration output across reload', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const fonts = {
    default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
  };
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    roughen = node('Roughen');
  text.id = 'text-source';
  text.text = 'I';
  roughen.id = 'roughen-text';
  roughen.params.amount.value = 3;
  layer.nodes.push(text, roughen);
  connect(layer, text.id, roughen.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(roughen.id);
  assert.equal(hash(result), 'e5ae22c6e24a0e7bd92abec0c06ae36642e1975e66d04679394913c0f1d02461');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${roughen.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Roughen/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(roughen.id), result);
});
