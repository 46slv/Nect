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
import { wavePointsDeclaration } from '../src/wave-points-declaration.js';
import { evaluateWavePoints } from '../src/wave-points-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.wave-points.v1', { evaluate: evaluateWavePoints }]]);

function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    wave = node('Wave Points'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  wave.id = 'wave-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, wave, stroke);
  connect(layer, source.id, wave.id, 'path');
  connect(layer, wave.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, wave };
}

test('production Wave Points resolves through the trusted registered binding', () => {
  const registered = builtinNodeRegistry.get('Wave Points'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Wave Points');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateWavePoints);
  assert.deepEqual(registered.declaration, wavePointsDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(wavePointsDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Wave Points');
  assert.equal(resolved.declaration.bypassInput, 'path');
  for (const direction of ['vertical', 'horizontal']) {
    assert.deepEqual(resolved.interface(direction).outputs, [
      { id: 'default', type: 'Path', label: '', legacyDefault: true },
    ]);
    assert.deepEqual(resolved.interface(direction).active, [
      'path',
      'amplitude',
      'wavelength',
      'phase',
      'keepEndpoints',
    ]);
  }
  assert.throws(() => resolved.interface('diagonal'), /operation/i);
  const invalid = structuredClone(wavePointsDeclaration);
  invalid.parameters[4].default = 1;
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
});

test('Wave factory, operation, catalog and bypass retain the existing interface', () => {
  const n = node('Wave Points');
  assert.equal(n.operation, 'vertical');
  assert.deepEqual(operationOptions[n.type], ['vertical', 'horizontal']);
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  for (const [key, value] of [
    ['amplitude', 20],
    ['wavelength', 150],
    ['phase', 0],
  ])
    assert.deepEqual(n.params[key], { type: 'Scalar', value, expression: '', locked: false });
  assert.deepEqual(n.params.keepEndpoints, {
    type: 'Boolean',
    value: false,
    expression: '',
    locked: false,
  });
  for (const direction of operationOptions[n.type]) {
    n.operation = direction;
    assert.deepEqual(
      activeParameters(n).map(([id]) => id),
      ['path', 'amplitude', 'wavelength', 'phase', 'keepEndpoints'],
    );
  }
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Reverse Path');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Pucker & Bloat');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Twist Points');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Pucker & Bloat');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, wavePointsDeclaration.catalog);
});

test('directions, endpoint pinning and zero amplitude keep pre-migration paths and IDs', () => {
  // Frozen from the production evaluator before migration, including IDs and fingerprints.
  const cases = [
    [
      'M0 0 L25 0 L50 0 L75 0 L100 0',
      'vertical',
      10,
      100,
      0,
      false,
      '07e0746421cd72a5f9bd139616b8389942eb1f48769ad9eb864cbe92d8243efa',
    ],
    [
      'M0 0 L0 25 L0 50 L0 75 L0 100',
      'horizontal',
      10,
      100,
      45,
      true,
      '3d44de4f83bc997b11ec690778dc0b01df97b04ecf62bf03784c258db35a8816',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      'vertical',
      20,
      150,
      45,
      true,
      'd230cbc938053018f003d72badec18bc5d96ba92c117456db9338cf7ab957cda',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      'vertical',
      0,
      150,
      0,
      false,
      'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
    ],
  ];
  for (const [raw, operation, amplitude, wavelength, phase, keepEndpoints, expected] of cases) {
    const { layer, source, wave } = scene(raw);
    wave.operation = operation;
    wave.params.amplitude.value = amplitude;
    wave.params.wavelength.value = wavelength;
    wave.params.phase.value = phase;
    wave.params.keepEndpoints.value = keepEndpoints;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateWavePoints(
      operation,
      (key) => {
        reads.push(key);
        return { path: input, amplitude, wavelength, phase, keepEndpoints }[key];
      },
      { nodeId: wave.id },
    );
    assert.deepEqual(reads, ['path', 'amplitude', 'wavelength', 'phase', 'keepEndpoints']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(wave.id)), expected);
    assert.equal(JSON.stringify(input), before);
    if (amplitude === 0)
      assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Wave Session retains atomic refusal, direction, bypass, history and project reload', () => {
  const { doc, layer, source, wave } = scene('M0 0 L25 0 L50 0 L75 0 L100 0');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: wave.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(wave.id);
  const before = serialize(session.snapshot());
  send('set-operation', { operation: 'horizontal' });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'wavelength', value: 0 }), /wavelength/i);
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
    restored.layers[0].nodes.find((n) => n.id === wave.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(wave.id),
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
    wave = node('Wave Points');
  text.id = 'text-source';
  text.text = 'I';
  wave.id = 'wave-text';
  layer.nodes.push(text, wave);
  connect(layer, text.id, wave.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(wave.id);
  assert.equal(hash(result), '6b902f89bc0eb75e96ec9be5c52df57f8838bd697ec9178c487732229d269e7c');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${wave.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Wave Points/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(wave.id), result);
});
