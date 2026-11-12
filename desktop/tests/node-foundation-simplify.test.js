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
import { simplifyPathDeclaration } from '../src/simplify-path-declaration.js';
import { evaluateSimplifyPath } from '../src/simplify-path-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const bindings = () => new Map([['builtin.simplify-path.v1', { evaluate: evaluateSimplifyPath }]]);

test('production Simplify Path resolves through the trusted registered binding', () => {
  const registered = builtinNodeRegistry.get('Simplify Path'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Simplify Path');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateSimplifyPath);
  assert.deepEqual(registered.declaration, simplifyPathDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
});

function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    simplify = node('Simplify Path');
  source.id = 'source-path';
  source.rawPath = rawPath;
  simplify.id = 'simplify-node';
  layer.nodes.push(source, simplify);
  connect(layer, source.id, simplify.id, 'path');
  return { doc, layer, source, simplify };
}

test('Simplify declaration resolves the existing interface and bypass contract', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(simplifyPathDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Simplify Path');
  assert.ok(Object.isFrozen(resolved.declaration));
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', type: 'Path', label: '', legacyDefault: true },
  ]);
  const invalid = structuredClone(simplifyPathDeclaration);
  invalid.parameters[1].default = [1, 2];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
});

test('Simplify factory and catalog preserve current defaults and order', () => {
  const n = node('Simplify Path');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.tolerance, { type: 'Scalar', value: 1, expression: '', locked: false });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'tolerance'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Dash Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Smooth Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, simplifyPathDeclaration.catalog);
});

test('Simplify paths and tolerance boundaries retain the pre-migration geometry and identities', () => {
  // Frozen from the production evaluator before this migration, including point IDs and fingerprint.
  const cases = [
    [
      'M0 0 L25 1 L50 0 L75 -1 L100 0',
      [
        '40e0ff11cf069f4651ce6d96975932df9a568a98bd4e15b30c5ffc1c00f37791',
        '45e248c1757c067177383b7039bd6c4d1da5034e7836e38ebe69b2eddd8732a3',
        '45e248c1757c067177383b7039bd6c4d1da5034e7836e38ebe69b2eddd8732a3',
        '45e248c1757c067177383b7039bd6c4d1da5034e7836e38ebe69b2eddd8732a3',
      ],
    ],
    [
      'M0 0 L50 0 L100 0 L100 100 L0 100 Z',
      [
        '70a7819c019dd541ba40c0bd33a6f5c3248d2ccd8e63727f4fba7eb32ce9c13a',
        'ea247ed5cec4424082693307836dda5b8e0f2c66904ddfdc4c6a65032e693254',
        'ea247ed5cec4424082693307836dda5b8e0f2c66904ddfdc4c6a65032e693254',
        '34df8fa3787ebd18c4933d97514705501ec9a3081b27bf5b543bf02d001eb8ad',
      ],
    ],
  ];
  const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
  for (const [rawPath, expected] of cases) {
    for (const [index, tolerance] of [0, 1, 10, 100000].entries()) {
      const { layer, source, simplify } = scene(rawPath);
      simplify.params.tolerance.value = tolerance;
      const engine = evaluate(layer),
        input = engine.run(source.id),
        reads = [];
      const actual = evaluateSimplifyPath(
        'subtract',
        (key) => {
          reads.push(key);
          return key === 'path' ? input : tolerance;
        },
        { nodeId: simplify.id },
      );
      assert.deepEqual(reads, ['path', 'tolerance']);
      assert.equal(hash(actual), expected[index]);
      assert.equal(hash(engine.run(simplify.id)), expected[index]);
    }
  }
  assert.throws(
    () =>
      evaluateSimplifyPath('subtract', (key) => (key === 'path' ? null : 1), {
        nodeId: 'simplify-node',
      }),
    /Path input/,
  );
});

test('Simplify Session retains failed-edit atomicity, bypass, history and current project IDs', () => {
  const { doc, layer, source, simplify } = scene('M0 0 L25 1 L50 0 L75 -1 L100 0');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: simplify.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(simplify.id);
  const before = serialize(session.snapshot());
  send('set-parameter', { key: 'tolerance', value: 5 });
  const edited = serialize(session.snapshot());
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'tolerance', value: -1 }), /tolerance/i);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
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
    restored.layers[0].nodes.find((n) => n.id === simplify.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(simplify.id),
    evaluate(restored.layers[0]).run(source.id),
  );
  assert.doesNotMatch(saved, /implementationId|capabilities|legacyDefault/);
});

test('Simplify retains actual Text character routing, point IDs and fingerprints across reload', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const fonts = {
    default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
  };
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    simplify = node('Simplify Path');
  text.id = 'text-source';
  text.text = 'I';
  simplify.id = 'simplify-text';
  layer.nodes.push(text, simplify);
  connect(layer, text.id, simplify.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(simplify.id);
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${simplify.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Simplify Path/);
  assert.ok(result.fingerprint);
  assert.ok(result.contours.flatMap((contour) => contour.points).every((point) => point.id));
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(layer, fonts).run(simplify.id), result);
  const reopened = deserialize(serialize(doc));
  assert.deepEqual(evaluate(reopened.layers[0], fonts).run(simplify.id), result);
});
