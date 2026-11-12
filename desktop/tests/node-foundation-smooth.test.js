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
import { smoothPathDeclaration } from '../src/smooth-path-declaration.js';
import { evaluateSmoothPath } from '../src/smooth-path-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.smooth-path.v1', { evaluate: evaluateSmoothPath }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    smooth = node('Smooth Path'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  smooth.id = 'smooth-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, smooth, stroke);
  connect(layer, source.id, smooth.id, 'path');
  connect(layer, smooth.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, smooth };
}

test('Smooth Path resolves through the trusted binding and retains the existing interface', () => {
  const registered = builtinNodeRegistry.get('Smooth Path'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Smooth Path');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateSmoothPath);
  assert.deepEqual(registered.declaration, smoothPathDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(smoothPathDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Smooth Path');
  assert.equal(resolved.declaration.bypassInput, 'path');
  const invalid = structuredClone(smoothPathDeclaration);
  invalid.parameters[1].default = [0, 0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Smooth Path');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.factor, { type: 'Scalar', value: 0.5, expression: '', locked: false });
  assert.deepEqual(n.params.iterations, {
    type: 'Scalar',
    value: 3,
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
    ['path', 'factor', 'iterations', 'keepEndpoints'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Vector Transform');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Dash Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Corner Round');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Simplify Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, smoothPathDeclaration.catalog);
});

test('Smooth Path retains pre-migration geometry, endpoint settings and zero-factor source IDs', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 L25 0 L50 0 L75 20 L100 20',
      0.5,
      3,
      true,
      'e0eaeb355364d645ea37eade07cf18d066b9e76b9ef18980f90bcbac94be99d8',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      0.5,
      2,
      false,
      '575b7ae211337a291232ee2153ba029ba77b182557587e6bd129ebd788655e49',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      1,
      1,
      true,
      'f826bf07c3a1d03467965fb393936d5a1bb0bc5ba33999220bc58acf21e22c89',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      0,
      3,
      true,
      'c85b55fade9ad6c9b27ceb6d0e05fa6bef10af21d4570626c3891f2807433dab',
    ],
  ];
  for (const [raw, factor, iterations, keepEndpoints, expected] of cases) {
    const { layer, source, smooth } = scene(raw);
    smooth.params.factor.value = factor;
    smooth.params.iterations.value = iterations;
    smooth.params.keepEndpoints.value = keepEndpoints;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateSmoothPath(
      'subtract',
      (key) => {
        reads.push(key);
        return { path: input, factor, iterations, keepEndpoints }[key];
      },
      { nodeId: smooth.id },
    );
    assert.deepEqual(reads, ['path', 'factor', 'iterations', 'keepEndpoints']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(smooth.id)), expected);
    assert.equal(JSON.stringify(input), before);
    if (factor === 0) assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Smooth Path Session preserves atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, smooth } = scene('M0 0 L25 0 L50 0 L75 20 L100 20');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: smooth.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(smooth.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'factor', value: 0.8 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'factor', value: 1.1 }), /factor/i);
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
    restored.layers[0].nodes.find((n) => n.id === smooth.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(smooth.id),
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
    smooth = node('Smooth Path');
  text.id = 'text-source';
  text.text = 'I';
  smooth.id = 'smooth-text';
  smooth.params.factor.value = 0.5;
  layer.nodes.push(text, smooth);
  connect(layer, text.id, smooth.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(smooth.id);
  assert.equal(hash(result), 'd4b0788713235f7a3dde10c343607e69cc8c005a7e29cd7c643de85090379282');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${smooth.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Smooth Path/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(smooth.id), result);
});
