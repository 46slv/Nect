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
import { mirrorDeclaration } from '../src/mirror-node-declaration.js';
import { evaluateMirror } from '../src/mirror-node-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.mirror.v1', { evaluate: evaluateMirror }]]);
function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    mirror = node('Mirror'),
    stroke = node('Stroke');
  source.id = 'source';
  source.rawPath = rawPath;
  mirror.id = 'mirror-node';
  stroke.id = 'stroke';
  layer.nodes.push(source, mirror, stroke);
  connect(layer, source.id, mirror.id, 'path');
  connect(layer, mirror.id, stroke.id, 'path');
  connect(layer, stroke.id, layer.output, 'path');
  return { doc, layer, source, mirror };
}

test('Mirror resolves through the trusted binding and preserves operation interfaces', () => {
  const registered = builtinNodeRegistry.get('Mirror'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Mirror');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateMirror);
  assert.deepEqual(registered.declaration, mirrorDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(mirrorDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Mirror');
  for (const operation of ['left-right', 'up-down', 'angle']) {
    assert.deepEqual(resolved.interface(operation).active, [
      'path',
      'center',
      ...(operation === 'angle' ? ['angle'] : []),
      'keepOriginal',
    ]);
  }
  assert.throws(() => resolved.interface('invalid'), /operation/i);
  const invalid = structuredClone(mirrorDeclaration);
  invalid.parameters[1].default = [0];
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
  const n = node('Mirror');
  assert.equal(n.operation, 'left-right');
  assert.deepEqual(operationOptions.Mirror, ['left-right', 'up-down', 'angle']);
  assert.deepEqual(
    [n.params.path.value, n.params.center.value, n.params.angle.value, n.params.keepOriginal.value],
    [null, [480, 270], 0, false],
  );
  for (const operation of operationOptions.Mirror) {
    n.operation = operation;
    assert.deepEqual(
      activeParameters(n).map(([id]) => id),
      ['path', 'center', ...(operation === 'angle' ? ['angle'] : []), 'keepOriginal'],
    );
  }
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Corner Round');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Path Remap');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Taper');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, mirrorDeclaration.catalog);
});

test('Mirror axes, live copy, point IDs and Text routing match the pre-migration evaluator', () => {
  // Frozen from the production evaluator before migration.
  const cases = [
    [
      'M400 200 L450 250 L500 200',
      'left-right',
      0,
      false,
      'a6df8d76b451a0a68883dd8aab2d45ed57040f990a5a262f6dcd3585c4875f0a',
    ],
    [
      'M400 200 L500 200 L500 300 L400 300 Z',
      'up-down',
      0,
      false,
      '6e215aae12614c968f6f425b61146c1007ebf3de3a07a38d50be0c249b886087',
    ],
    [
      'M400 200 C420 200 420 250 460 250',
      'angle',
      35,
      false,
      '2a783becf07cd8be65ac41128935fc022a259e5d166a82215598ecbb2ea1f920',
    ],
    [
      'M400 200 L450 250 L500 200',
      'left-right',
      0,
      true,
      '22f393fbec79ddc2b55577619bf9f617cfd17e08a7b4934ceffe8790ade254dc',
    ],
  ];
  for (const [raw, operation, angle, keepOriginal, expected] of cases) {
    const { layer, source, mirror } = scene(raw);
    mirror.operation = operation;
    mirror.params.angle.value = angle;
    mirror.params.keepOriginal.value = keepOriginal;
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateMirror(
      operation,
      (key) => {
        reads.push(key);
        return { path: input, center: [480, 270], angle, keepOriginal }[key];
      },
      { nodeId: mirror.id },
    );
    assert.deepEqual(reads, [
      'path',
      'center',
      ...(operation === 'angle' ? ['angle'] : []),
      'keepOriginal',
    ]);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(mirror.id)), expected);
    assert.equal(JSON.stringify(input), before);
  }
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const fonts = {
    default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
  };
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    mirror = node('Mirror');
  text.id = 'text-source';
  text.text = 'I';
  mirror.id = 'mirror-text';
  mirror.params.keepOriginal.value = true;
  layer.nodes.push(text, mirror);
  connect(layer, text.id, mirror.id, 'path');
  const result = evaluate(layer, fonts).run(mirror.id);
  assert.equal(hash(result), '9978925b08791f7a9495bb5f79dcebd61455647a8e7269cd8ae7b449d5d11e1c');
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${mirror.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Mirror/);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(mirror.id), result);
});

test('Mirror skips inactive angle and keeps failure atomic, history, bypass and project reload', () => {
  const { doc, layer, source, mirror } = scene('M400 200 L450 250 L500 200');
  mirror.params.angle.expression = '1/0';
  assert.ok(evaluate(layer).run(mirror.id).contours.length);
  mirror.operation = 'angle';
  assert.throws(() => evaluate(layer).run(mirror.id), /finite/i);
  mirror.operation = 'left-right';
  mirror.params.angle.expression = '';
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: mirror.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(mirror.id);
  const before = serialize(session.snapshot());
  send('set-operation', { operation: 'angle' });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-operation', { operation: 'invalid' }));
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
    restored.layers[0].nodes.find((n) => n.id === mirror.id).params.path.source,
    source.id,
  );
  assert.deepEqual(
    evaluate(restored.layers[0]).run(mirror.id),
    evaluate(restored.layers[0]).run(source.id),
  );
  assert.doesNotMatch(saved, /implementationId|capabilities|legacyDefault/);
});
