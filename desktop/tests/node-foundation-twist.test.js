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
import { twistPointsDeclaration } from '../src/twist-points-declaration.js';
import { evaluateTwistPoints } from '../src/twist-points-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.twist-points.v1', { evaluate: evaluateTwistPoints }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    twist = node('Twist Points'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  twist.id = 'twist-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, twist, stroke);
  connect(layer, source.id, twist.id, 'path');
  connect(layer, twist.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, twist };
}

test('Twist resolves through the trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Twist Points'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Twist Points');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateTwistPoints);
  assert.deepEqual(registered.declaration, twistPointsDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(twistPointsDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Twist Points');
  assert.equal(resolved.declaration.bypassInput, 'path');
  const invalid = structuredClone(twistPointsDeclaration);
  invalid.parameters[1].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Twist Points');
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
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Bend Points');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Bend Points');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Wave Points');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, twistPointsDeclaration.catalog);
});

test('Twist retains pre-migration geometry, identities and zero-angle source IDs', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 L25 0 L50 0 L75 20 L100 20',
      90,
      '018f45b99dc98b2a79487f19a13bb31b70e529bbb4f30528868f41d1da09df96',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      -90,
      'd1c6bcd111bcbc251f13acde41a28796c0993a349a32c359ee3cb0a4596c7fb2',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      180,
      '7d30dc7256cfbedca7acdc496427fa174aa4d0ce7963557a48eb2891955f4ea5',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      0,
      'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
    ],
  ];
  for (const [raw, angle, expected] of cases) {
    const { layer, source, twist } = scene(raw);
    twist.params.angle.value = angle;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateTwistPoints(
      'subtract',
      (key) => {
        reads.push(key);
        return key === 'path' ? input : angle;
      },
      { nodeId: twist.id },
    );
    assert.deepEqual(reads, ['path', 'angle']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(twist.id)), expected);
    assert.equal(JSON.stringify(input), before);
    if (angle === 0) assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Twist Session preserves atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, twist } = scene('M0 0 L25 0 L50 0 L75 20 L100 20');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: twist.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(twist.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'angle', value: 90 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'angle', value: 361 }), /angle/i);
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
    restored.layers[0].nodes.find((n) => n.id === twist.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(twist.id),
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
    twist = node('Twist Points');
  text.id = 'text-source';
  text.text = 'I';
  twist.id = 'twist-text';
  twist.params.angle.value = 45;
  layer.nodes.push(text, twist);
  connect(layer, text.id, twist.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(twist.id);
  assert.equal(hash(result), 'c199950ab7ae7e6daa765bb64526ba6acd3b23cc6579a55e54e1b6d957a1aa32');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${twist.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Twist Points/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(twist.id), result);
});
