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
import { puckerBloatDeclaration } from '../src/pucker-bloat-declaration.js';
import { evaluatePuckerBloat } from '../src/pucker-bloat-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.pucker-bloat.v1', { evaluate: evaluatePuckerBloat }]]);

function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    effect = node('Pucker & Bloat'),
    fill = node('Fill');
  source.id = 'source';
  source.rawPath = rawPath;
  effect.id = 'effect';
  fill.id = 'fill';
  layer.nodes.push(source, effect, fill);
  connect(layer, source.id, effect.id, 'path');
  connect(layer, effect.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  return { doc, layer, source, effect };
}

test('production Pucker & Bloat uses the trusted registered binding', () => {
  const registered = builtinNodeRegistry.get('Pucker & Bloat'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Pucker & Bloat');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluatePuckerBloat);
  assert.deepEqual(registered.declaration, puckerBloatDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(puckerBloatDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Pucker & Bloat');
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', type: 'Path', label: '', legacyDefault: true },
  ]);
  assert.deepEqual(
    parsed.parameters.map(({ id, default: value }) => [id, value]),
    [
      ['path', null],
      ['amount', 0],
      ['autoCenter', true],
      ['center', [0, 0]],
    ],
  );
  const invalid = structuredClone(puckerBloatDeclaration);
  invalid.parameters[3].default = [0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
});

test('Pucker & Bloat factory, catalog, active inputs and bypass retain the existing interface', () => {
  const n = node('Pucker & Bloat');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.amount, { type: 'Scalar', value: 0, expression: '', locked: false });
  assert.deepEqual(n.params.autoCenter, {
    type: 'Boolean',
    value: true,
    expression: '',
    locked: false,
  });
  assert.deepEqual(n.params.center, {
    type: 'Vector2',
    value: [0, 0],
    expression: '',
    locked: false,
  });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'amount', 'autoCenter'],
  );
  n.params.autoCenter.value = false;
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'amount', 'autoCenter', 'center'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Wave Points');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Wave Points');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, puckerBloatDeclaration.catalog);
});

test('automatic and manual centers retain pre-migration outlines, point IDs and fingerprints', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M0 0 L50 0 L100 40',
      50,
      true,
      [0, 0],
      'f1106bae4e762c00ae18bb213792d3e6d4914f738007ed03f75d1ef6199baf70',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      -50,
      true,
      [0, 0],
      'dc79fd4e6001af9d32b6d0d7bf8938ba90e30c33f700b24748f622fba9767f48',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      25,
      false,
      [20, 60],
      '9bf3e867fcf7ce3298ebdbb2b01711b93ce1f9d553ad49703a7fe640a9f2267e',
    ],
    [
      'M0 0 L100 0 L100 100 Z',
      0,
      true,
      [0, 0],
      '17eea2b1283209c4c828a1884d79639bd72d04341b8029fbaec3a21265af2eca',
    ],
  ];
  for (const [raw, amount, autoCenter, center, expected] of cases) {
    const { layer, source, effect } = scene(raw);
    effect.params.amount.value = amount;
    effect.params.autoCenter.value = autoCenter;
    effect.params.center.value = center;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluatePuckerBloat(
      'subtract',
      (key) => {
        reads.push(key);
        return { path: input, amount, autoCenter, center }[key];
      },
      { nodeId: effect.id },
    );
    assert.deepEqual(reads, ['path', 'autoCenter', 'amount', ...(autoCenter ? [] : ['center'])]);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(effect.id)), expected);
    assert.equal(JSON.stringify(input), before);
    assert.ok(actual.contours.flatMap((contour) => contour.points).every((point) => point.id));
  }
});

test('automatic centering does not evaluate an invalid inactive manual center', () => {
  const { layer, effect } = scene('M0 0 L50 0 L100 40');
  effect.params.center.expression = '[1/0,0]';
  assert.ok(evaluate(layer).run(effect.id).contours.length);
  effect.params.autoCenter.value = false;
  assert.throws(() => evaluate(layer).run(effect.id), /expression/i);
});

test('connected automatic centering selects the same lazy center behavior', () => {
  const { layer, effect } = scene('M0 0 L50 0 L100 40');
  const toggle = node('Toggle');
  toggle.id = 'toggle';
  layer.nodes.push(toggle);
  connect(layer, toggle.id, effect.id, 'autoCenter');
  effect.params.center.expression = '[1/0,0]';
  assert.ok(activeParameters(effect).some(([id]) => id === 'center'));
  assert.ok(evaluate(layer).run(effect.id).contours.length);
  toggle.params.value.value = false;
  assert.throws(() => evaluate(layer).run(effect.id), /expression/i);
});

test('Pucker & Bloat Session retains atomic failure, bypass, history and project reload', () => {
  const { doc, layer, source, effect } = scene('M0 0 L50 0 L100 40');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: effect.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(effect.id);
  send('set-parameter', { key: 'amount', value: 50 });
  send('set-parameter', { key: 'autoCenter', value: false });
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'center', value: [20, 60] });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'amount', value: 101 }), /amount/i);
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
    restored.layers[0].nodes.find((n) => n.id === effect.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(effect.id),
    evaluate(restored.layers[0]).run(source.id),
  );
  assert.doesNotMatch(saved, /implementationId|capabilities|legacyDefault/);
});

test('actual Text route and point identities retain pre-migration output across reload', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const fonts = {
    default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
  };
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    effect = node('Pucker & Bloat');
  text.id = 'text-source';
  text.text = 'I';
  effect.id = 'effect-text';
  effect.params.amount.value = 30;
  layer.nodes.push(text, effect);
  connect(layer, text.id, effect.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(effect.id);
  assert.equal(hash(result), '39f1387be929f2038eccdda4b32f71301848bff46ca399c12f3779ff69d2d121');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${effect.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Pucker & Bloat/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(effect.id), result);
});
