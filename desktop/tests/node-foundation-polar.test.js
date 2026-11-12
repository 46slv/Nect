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
import { polarTransformDeclaration } from '../src/polar-transform-declaration.js';
import { evaluatePolarTransform } from '../src/polar-transform-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () =>
  new Map([['builtin.polar-transform.v1', { evaluate: evaluatePolarTransform }]]);
const keys = ['path', 'origin', 'center', 'width', 'height', 'radius', 'angle', 'inverse'];
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    polar = node('Polar Transform'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  polar.id = 'polar-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, polar, stroke);
  connect(layer, source.id, polar.id, 'path');
  connect(layer, polar.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, polar };
}

test('Polar Transform resolves through the trusted binding and retains its interface', () => {
  const registered = builtinNodeRegistry.get('Polar Transform'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Polar Transform');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluatePolarTransform);
  assert.deepEqual(registered.declaration, polarTransformDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(polarTransformDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Polar Transform');
  assert.equal(resolved.declaration.bypassInput, 'path');
  const invalid = structuredClone(polarTransformDeclaration);
  invalid.parameters[1].default = [0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Polar Transform');
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    keys,
  );
  assert.deepEqual(
    keys.map((id) => n.params[id].value),
    [null, [0, 0], [480, 270], 960, 540, 270, -90, false],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Twist Points');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Auto Handles');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Angular Gradient Fill');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Luminance to Points');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, polarTransformDeclaration.catalog);
});

test('forward and inverse transforms retain pre-migration geometry and source IDs', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 100 L100 200 L300 300',
      false,
      'fdca6a221eee9ac731b944a0becff0b83f4dcb32a9b0cb3fe03b4727d111590c',
    ],
    [
      'M0 100 L100 100 L100 200 L0 200 Z',
      false,
      'dbad418ff249e9adc7b7dd2b66bcddd52eb5aa632d6ba77b4955eb044da2e033',
    ],
    [
      'M500 300 L520 320 L540 340',
      true,
      '172f3d150e47a40aafeee4e991eba41e3b956212ce2f3925541c0b84706c5146',
    ],
    [
      'M0 100 C20 100 20 150 60 150',
      false,
      '9c61de0e2139957ce04e606716feed5125d5f65b7be91c27f307440d69b42e4c',
    ],
  ];
  for (const [raw, inverse, expected] of cases) {
    const { layer, source, polar } = scene(raw);
    polar.params.inverse.value = inverse;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluatePolarTransform(
      'subtract',
      (key) => {
        reads.push(key);
        return key === 'path' ? input : polar.params[key].value;
      },
      { nodeId: polar.id },
    );
    assert.deepEqual(reads, keys);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(polar.id)), expected);
    assert.equal(JSON.stringify(input), before);
    assert.equal(actual.contours[0].points[0].id, input.contours[0].points[0].id);
  }
});

test('Polar Session keeps atomic refusal, bypass, history and Composition reload', () => {
  const { doc, layer, source, polar } = scene('M0 100 L100 200 L300 300');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: polar.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(polar.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'radius', value: 300 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'radius', value: 0 }));
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
    restored.layers[0].nodes.find((n) => n.id === polar.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(polar.id),
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
    polar = node('Polar Transform');
  text.id = 'text-source';
  text.text = 'I';
  polar.id = 'polar-text';
  layer.nodes.push(text, polar);
  connect(layer, text.id, polar.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(polar.id);
  assert.equal(hash(result), '897b0ac0271c084b71c9ebedc5e59e1ba9a47ed2b1f4e3c27dbdf6da20bcc951');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${polar.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Polar Transform/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(polar.id), result);
});
