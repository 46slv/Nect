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
  operationOptions,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { bendPointsDeclaration } from '../src/bend-points-declaration.js';
import { evaluateBendPoints } from '../src/bend-points-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.bend-points.v1', { evaluate: evaluateBendPoints }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    bend = node('Bend Points'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  bend.id = 'bend-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, bend, stroke);
  connect(layer, source.id, bend.id, 'path');
  connect(layer, bend.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, bend };
}

test('Bend resolves through the trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Bend Points'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Bend Points');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateBendPoints);
  assert.deepEqual(registered.declaration, bendPointsDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(bendPointsDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Bend Points');
  assert.equal(resolved.declaration.bypassInput, 'path');
  for (const axis of ['horizontal', 'vertical'])
    assert.deepEqual(resolved.interface(axis).active, ['path', 'angle']);
  assert.throws(() => resolved.interface('diagonal'), /operation/i);
  const invalid = structuredClone(bendPointsDeclaration);
  invalid.parameters[1].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Bend Points');
  assert.equal(n.operation, 'horizontal');
  assert.deepEqual(operationOptions[n.type], ['horizontal', 'vertical']);
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.angle, { type: 'Scalar', value: 0, expression: '', locked: false });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'angle'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Taper');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Taper');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Twist Points');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, bendPointsDeclaration.catalog);
});

test('Bend retains pre-migration geometry, identities and zero-angle source IDs', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 L25 0 L50 0 L75 20 L100 20',
      45,
      'horizontal',
      'ab3095d97a065e8cfd61c9c37f5d10c21530dd1bc5033fb5f103a68821f4be94',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      -45,
      'vertical',
      '5dd3ffb7f91d0f62b09072324f4e769197c9b967bc13fcfd12cb96b7a2fdc5a7',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      60,
      'horizontal',
      'adeef6ad1c0dd958edc66ef78e87e016abb0cc840605a9f9a804b0538d0b6054',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      0,
      'horizontal',
      'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
    ],
  ];
  for (const [raw, angle, operation, expected] of cases) {
    const { layer, source, bend } = scene(raw);
    bend.params.angle.value = angle;
    bend.operation = operation;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateBendPoints(
      operation,
      (key) => {
        reads.push(key);
        return key === 'path' ? input : angle;
      },
      { nodeId: bend.id },
    );
    assert.deepEqual(reads, ['path', 'angle']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(bend.id)), expected);
    assert.equal(JSON.stringify(input), before);
    if (angle === 0) assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Bend Session preserves atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, bend } = scene('M0 0 L25 0 L50 0 L75 20 L100 20');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: bend.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(bend.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'angle', value: 90 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'angle', value: 181 }), /angle/i);
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
    restored.layers[0].nodes.find((n) => n.id === bend.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(bend.id),
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
    bend = node('Bend Points');
  text.id = 'text-source';
  text.text = 'I';
  bend.id = 'bend-text';
  bend.params.angle.value = 5;
  layer.nodes.push(text, bend);
  connect(layer, text.id, bend.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(bend.id);
  assert.equal(hash(result), '31a32424c319c9c2a236f3340143f8e9f16a6dda9abe247b754cf8b05116a08e');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${bend.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Bend Points/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(bend.id), result);
});
