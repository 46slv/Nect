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
import { expandStrokeDeclaration } from '../src/expand-stroke-declaration.js';
import { evaluateExpandStroke } from '../src/expand-stroke-binding.js';
import { parseFont } from '../src/fonts.js';
import {
  activeCompositionDocument,
  deserializeCompositionProject,
  migrateLegacyDocumentToCompositionProject,
  serializeCompositionProject,
} from '../src/composition-project-model.js';
import { createCompositionSessionOwner } from '../src/composition-session-owner-core.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const bindings = () => new Map([['builtin.expand-stroke.v1', { evaluate: evaluateExpandStroke }]]);

function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    path = node('Imported Path'),
    stroke = node('Stroke'),
    expand = node('Expand Stroke'),
    fill = node('Fill');
  path.id = 'source';
  path.rawPath = rawPath;
  stroke.id = 'stroke';
  stroke.params.width.value = 10;
  expand.id = 'expand';
  fill.id = 'fill';
  layer.nodes.push(path, stroke, expand, fill);
  connect(layer, path.id, stroke.id, 'path');
  connect(layer, stroke.id, expand.id, 'path');
  connect(layer, expand.id, fill.id, 'path');
  connect(layer, fill.id, layer.output, 'path');
  return { doc, layer, path, stroke, expand };
}

test('production Expand Stroke resolves through the trusted registered binding', () => {
  const registered = builtinNodeRegistry.get('Expand Stroke'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Expand Stroke');
  assert.ok(registered && manifest);
  assert.equal(manifest.binding.evaluate, evaluateExpandStroke);
  assert.deepEqual(registered.declaration, expandStrokeDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  const parsed = parseNodeDeclaration(JSON.stringify(expandStrokeDeclaration));
  const resolved = createNodeRegistry([parsed], bindings(), nectNodeHost).get('Expand Stroke');
  assert.equal(resolved.declaration.bypassInput, undefined);
  assert.deepEqual(resolved.interface().outputs, [
    { id: 'default', type: 'Path', label: '', legacyDefault: true },
  ]);
  const invalid = structuredClone(expandStrokeDeclaration);
  invalid.parameters[0].default = {};
  assert.throws(() => createNodeRegistry([invalid], bindings(), nectNodeHost), /default/);
});

test('Expand Stroke factory, catalog and bypass retain the existing interface', () => {
  const n = node('Expand Stroke');
  assert.deepEqual(n.params.path, {
    type: 'PaintedPath',
    value: null,
    expression: '',
    locked: false,
  });
  for (const key of ['startWidthMultiplier', 'endWidthMultiplier'])
    assert.deepEqual(n.params[key], { type: 'Scalar', value: 1, expression: '', locked: false });
  assert.deepEqual(
    activeParameters(n).map(([id]) => id),
    ['path', 'startWidthMultiplier', 'endWidthMultiplier'],
  );
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  for (const port of ['default', 'output', 'path']) assert.equal(outputType(n, port), undefined);
  assert.equal(bypassContract(n).supported, false);
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Simplify Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Text to Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, expandStrokeDeclaration.catalog);
});

test('open, closed and curved outlines retain pre-migration geometry and point IDs', () => {
  // Frozen from the production evaluator before migration, including IDs and fingerprints.
  const cases = [
    [
      'M0 0 L50 0 L100 40',
      0.5,
      2,
      '57fd08c01f679dae6945c6d73ac9ede4ce89f0436784eb157a795a65b366c204',
    ],
    [
      'M0 0 L100 0 L100 100 L0 100 Z',
      1,
      1,
      'c5a4a4f8524db25ad991c3c9aef3531c6f049c06f968774dc52e4f1f8ff8f7b9',
    ],
    [
      'M0 0 C20 0 20 50 60 50',
      1.5,
      0.75,
      'f7dccb604946da77b958c6619d2877dfd253ca460a20368e412565c74a60545a',
    ],
  ];
  for (const [raw, start, end, expected] of cases) {
    const { layer, stroke, expand } = scene(raw);
    expand.params.startWidthMultiplier.value = start;
    expand.params.endWidthMultiplier.value = end;
    const engine = evaluate(layer),
      input = engine.run(stroke.id),
      before = JSON.stringify(input),
      reads = [];
    const actual = evaluateExpandStroke(
      'subtract',
      (key) => {
        reads.push(key);
        return { path: input, startWidthMultiplier: start, endWidthMultiplier: end }[key];
      },
      { nodeId: expand.id },
    );
    assert.deepEqual(reads, ['path', 'startWidthMultiplier', 'endWidthMultiplier']);
    assert.equal(hash(actual), expected);
    assert.equal(hash(engine.run(expand.id)), expected);
    assert.equal(JSON.stringify(input), before);
    assert.equal(actual.stroke, undefined);
    assert.equal(actual.fill, undefined);
    assert.ok(actual.contours.flatMap((contour) => contour.points).every((point) => point.id));
  }
});

test('Expand Stroke Session keeps atomic failure, undo, redo and project reload', () => {
  const { doc, layer, expand, stroke } = scene('M0 0 L50 0 L100 40');
  const session = new CommandSession(doc);
  const send = (type, payload = {}) =>
    session.dispatch({
      ...session.state,
      type,
      payload: { layerId: layer.id, nodeId: expand.id, ...payload },
    });
  const run = () => evaluate(session.snapshot().layers[0]).run(expand.id);
  const before = serialize(session.snapshot()),
    beforeResult = run();
  send('set-parameter', { key: 'startWidthMultiplier', value: 0.5 });
  const edited = serialize(session.snapshot()),
    editedResult = run();
  assert.notDeepEqual(editedResult.contours, beforeResult.contours);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'startWidthMultiplier', value: 5 }), /0 to 4/);
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  assert.deepEqual(run(), editedResult);
  assert.equal(serialize(deserialize(edited)), edited);
  const project = migrateLegacyDocumentToCompositionProject(session.snapshot()),
    owner = createCompositionSessionOwner(project),
    saved = serializeCompositionProject(owner.projectSnapshot()),
    reopened = deserializeCompositionProject(saved),
    restored = activeCompositionDocument(reopened, reopened.compositions[0].id);
  assert.equal(serializeCompositionProject(reopened), saved);
  assert.equal(
    restored.layers[0].nodes.find((n) => n.id === expand.id).params.path.source,
    stroke.id,
  );
  assert.deepEqual(evaluate(restored.layers[0]).run(expand.id), editedResult);
  assert.doesNotMatch(saved, /implementationId|capabilities|legacyDefault/);
});

test('actual Text routing and expanded point identities match the pre-migration evaluator', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url));
  const fonts = {
    default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
  };
  const doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    stroke = node('Stroke'),
    expand = node('Expand Stroke');
  text.id = 'text-source';
  text.text = 'I';
  stroke.id = 'stroke';
  stroke.params.width.value = 4;
  expand.id = 'expand-text';
  layer.nodes.push(text, stroke, expand);
  connect(layer, text.id, stroke.id, 'path');
  connect(layer, stroke.id, expand.id, 'path');
  const sourceBefore = JSON.stringify(text),
    result = evaluate(layer, fonts).run(expand.id);
  assert.equal(hash(result), '261f3793ea461f754774f2887a080fb0ddc48c08f38f80aceeee35a0f277241a');
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${expand.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Expand Stroke/);
  assert.equal(JSON.stringify(text), sourceBefore);
  assert.deepEqual(evaluate(deserialize(serialize(doc)).layers[0], fonts).run(expand.id), result);
});
