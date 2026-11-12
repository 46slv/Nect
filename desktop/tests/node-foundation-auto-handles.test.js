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
import { autoHandlesDeclaration } from '../src/auto-handles-declaration.js';
import { evaluateAutoHandles } from '../src/auto-handles-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.auto-handles.v1', { evaluate: evaluateAutoHandles }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    auto = node('Auto Handles'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  auto.id = 'auto-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, auto, stroke);
  connect(layer, source.id, auto.id, 'path');
  connect(layer, auto.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, auto };
}

test('Auto Handles resolves through the trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Auto Handles'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Auto Handles');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateAutoHandles);
  assert.deepEqual(registered.declaration, autoHandlesDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(autoHandlesDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Auto Handles');
  assert.equal(resolved.declaration.bypassInput, 'path');
  const invalid = structuredClone(autoHandlesDeclaration);
  invalid.parameters[1].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Auto Handles');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.strength, { type: 'Scalar', value: 1, expression: '', locked: false });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'strength'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Polar Transform');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Reverse Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Resample Path');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Dash Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, autoHandlesDeclaration.catalog);
});

test('Auto Handles retains pre-migration geometry and generated IDs including zero strength', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 L25 0 L50 0 L75 20 L100 20',
      1,
      'ede2f2850259ea331b22b21ca6cf7b274ed879a3a0ec8432d4b266026834c862',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      0.5,
      'aa844700f0ff925c2484e7630ec7b129efb3372ef4ca60079acc666db5c84784',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      1,
      'c7493bff2aa13bbf269c70399cd5f61626ebb85afc72be7ed0858832cfe57cba',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      0,
      'd1dcefdb5aaa3ec4d5c628fe1f7119c753732d2bace0ecbde7834a55f512141b',
    ],
  ];
  for (const [raw, strength, expected] of cases) {
    const { layer, source, auto } = scene(raw);
    auto.params.strength.value = strength;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateAutoHandles(
      'subtract',
      (key) => {
        reads.push(key);
        return key === 'path' ? input : strength;
      },
      { nodeId: auto.id },
    );
    assert.deepEqual(reads, ['path', 'strength']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(auto.id)), expected);
    assert.equal(JSON.stringify(input), before);
    if (strength === 0)
      assert.notEqual(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Auto Handles Session preserves atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, auto } = scene('M0 0 L25 0 L50 0 L75 20 L100 20');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: auto.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(auto.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'strength', value: 0.5 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'strength', value: 1.1 }), /strength/i);
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
    restored.layers[0].nodes.find((n) => n.id === auto.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(auto.id),
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
    auto = node('Auto Handles');
  text.id = 'text-source';
  text.text = 'I';
  auto.id = 'auto-text';
  auto.params.strength.value = 0.5;
  layer.nodes.push(text, auto);
  connect(layer, text.id, auto.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(auto.id);
  assert.equal(hash(result), 'bda0d5e641eeffe6813fda3cf2cae195563dbffc811e4be8beee732d509da7b7');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${auto.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Auto Handles/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(auto.id), result);
});
