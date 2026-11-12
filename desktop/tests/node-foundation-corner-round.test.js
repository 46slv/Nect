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
  document,
  deserialize,
  evaluate,
  layer as makeLayer,
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
import { cornerRoundDeclaration } from '../src/corner-round-declaration.js';
import { evaluateCornerRound } from '../src/corner-round-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.corner-round.v1', { evaluate: evaluateCornerRound }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    corner = node('Corner Round'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  corner.id = 'corner-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, corner, stroke);
  connect(layer, source.id, corner.id, 'path');
  connect(layer, corner.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, corner };
}

test('Corner Round resolves through the trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Corner Round'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Corner Round');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateCornerRound);
  assert.deepEqual(registered.declaration, cornerRoundDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(cornerRoundDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Corner Round');
  assert.equal(resolved.declaration.bypassInput, 'path');
  const invalid = structuredClone(cornerRoundDeclaration);
  invalid.parameters[1].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Corner Round');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.radius, { type: 'Scalar', value: 12, expression: '', locked: false });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'radius'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Resample Path');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Mirror');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Closed Path Offset');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Smooth Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, cornerRoundDeclaration.catalog);
});

test('Corner Round retains pre-migration geometry, identities and zero-radius source IDs', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 C3 0 7 0 10 0 C7 0 3 0 0 0',
      2,
      '4dab070d25660325ba3de3de451c8c7ec79b28396f950a1e69208023b869818a',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      12,
      '1c4a3398410d9b9733e7956cacf049052a1604fc1549e1467ecccbfa3ef59ec0',
    ],
    ['M0 0 L50 0 L50 50', 5, '483b865fe75f782377d36c69358683996657ec11aa3540cac30e4c73f774252e'],
    [
      'M0 0 C20 0 20 50 60 50',
      0,
      'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
    ],
  ];
  for (const [raw, radius, expected] of cases) {
    const { layer, source, corner } = scene(raw);
    corner.params.radius.value = radius;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [],
      warnings = [];
    const actual = evaluateCornerRound(
      'subtract',
      (key) => {
        reads.push(key);
        return key === 'path' ? input : radius;
      },
      { nodeId: corner.id, warn: (message) => warnings.push(`Corner Round: ${message}`) },
    );
    assert.deepEqual(reads, ['path', 'radius']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(corner.id)), expected);
    assert.deepEqual(engine.warnings, warnings);
    assert.equal(JSON.stringify(input), before);
    if (radius === 0) assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('corner warnings keep prior nested order, same-frame behavior, cache, bypass and errors', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    first = node('Corner Round'),
    second = node('Corner Round');
  source.id = 'source';
  source.rawPath = 'M0 0 C3 0 7 0 10 0 C7 0 3 0 0 0';
  first.id = 'round-a';
  first.name = 'Alpha';
  first.params.radius.value = 2;
  second.id = 'round-b';
  second.name = 'Beta';
  second.params.radius.value = 2;
  layer.nodes.push(source, first, second);
  connect(layer, source.id, first.id, 'path');
  connect(layer, first.id, second.id, 'path');
  const warning = (name) =>
    `${name}: 1 corner(s) left unchanged (irregular). Curved corners use conservative local G1 blends.`;
  const run = () => {
    const evaluation = evaluate(layer);
    let error = null;
    try {
      evaluation.run(second.id);
      evaluation.run(second.id);
    } catch (cause) {
      error = cause.message;
    }
    return { warnings: [...evaluation.warnings], error };
  };
  assert.deepEqual(run(), { warnings: [warning('Alpha'), warning('Beta')], error: null });
  assert.deepEqual(run(), { warnings: [warning('Alpha'), warning('Beta')], error: null });
  first.name = 'Renamed';
  assert.deepEqual(run(), { warnings: [warning('Alpha'), warning('Beta')], error: null });
  first.bypassed = true;
  assert.deepEqual(run(), { warnings: [warning('Beta')], error: null });
  first.bypassed = false;
  second.params.radius.value = -1;
  assert.deepEqual(run(), {
    warnings: [warning('Renamed')],
    error: 'Corner Round radius must be from 0 to 100000 px',
  });
  second.params.radius.value = 2;
  assert.deepEqual(run(), { warnings: [warning('Renamed'), warning('Beta')], error: null });
});

test('same-name nodes and child Composition references replay the original warning multiplicity', () => {
  const warning =
    'Child: Same: 1 corner(s) left unchanged (irregular). Curved corners use conservative local G1 blends.';
  for (const count of [1, 2]) {
    const doc = document(),
      source = doc.layers[0],
      target = makeLayer('Parent'),
      path = node('Imported Path'),
      corner = node('Corner Round'),
      fill = node('Fill'),
      out = node('Output'),
      a = node('Composition Reference'),
      b = node('Composition Reference'),
      join = node('Join Geometry'),
      dest = node('Output');
    source.name = 'Child';
    path.rawPath = 'M0 0 C3 0 7 0 10 0 C7 0 3 0 0 0';
    corner.name = 'Same';
    corner.params.radius.value = 2;
    source.nodes = [path, corner, fill, out];
    source.output = out.id;
    connect(source, path.id, corner.id, 'path');
    connect(source, corner.id, fill.id, 'path');
    connect(source, fill.id, out.id, 'path');
    a.compositionReference = { compositionId: source.id };
    b.compositionReference = { compositionId: source.id };
    target.nodes = [a, b, join, dest];
    target.output = dest.id;
    connect(target, a.id, join.id, 'a');
    if (count === 2) connect(target, b.id, join.id, 'b');
    connect(target, join.id, dest.id, 'path');
    doc.layers.push(target);
    for (let pass = 0; pass < 2; pass++) {
      const evaluation = evaluate(target, {}, doc.imageAssets, doc);
      assert.equal(evaluation.error, null);
      assert.deepEqual(evaluation.warnings, Array(count).fill(warning));
    }
  }
  const doc = blankDocument(),
    layer = doc.layers[0],
    path = node('Imported Path'),
    a = node('Corner Round'),
    b = node('Corner Round');
  path.id = 'same-source';
  a.id = 'same-a';
  b.id = 'same-b';
  path.rawPath = 'M0 0 C3 0 7 0 10 0 C7 0 3 0 0 0';
  a.name = b.name = 'Same';
  a.params.radius.value = b.params.radius.value = 2;
  layer.nodes.push(path, a, b);
  connect(layer, path.id, a.id, 'path');
  connect(layer, path.id, b.id, 'path');
  const evaluation = evaluate(layer),
    aResult = evaluation.run(a.id),
    bResult = evaluation.run(b.id);
  assert.deepEqual(evaluation.warnings, Array(2).fill(warning.replace('Child: ', '')));
  assert.equal(hash(aResult), '0ae0fcd18d50418ed9929f3189c4c472cd93493afe1851fac507c0618224591d');
  assert.equal(hash(bResult), 'f1f2256b6f9daafb61547f578423263d647dc8c48841c00b515a7deac66936af');
});

test('Corner Round Session preserves atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, corner } = scene('M0 0 L25 0 L50 0 L75 20 L100 20');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: corner.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(corner.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'radius', value: 20 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'radius', value: -1 }), /radius/i);
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
    restored.layers[0].nodes.find((n) => n.id === corner.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(corner.id),
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
    corner = node('Corner Round');
  text.id = 'text-source';
  text.text = 'I';
  corner.id = 'corner-text';
  corner.params.radius.value = 1;
  layer.nodes.push(text, corner);
  connect(layer, text.id, corner.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(corner.id);
  assert.equal(hash(result), 'f9b3a9f3ac43fd028ce42162c3e051e0919af382da18efa3ce6e6b05f30eaad7');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${corner.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Corner Round/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(corner.id), result);
});
