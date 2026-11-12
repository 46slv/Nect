import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  document,
  evaluate,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { setClosureDeclaration } from '../src/set-closure-declaration.js';
import { evaluateSetClosure } from '../src/set-closure-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');

test('Set Closure resolves through its trusted binding without changing its interface', () => {
  const registered = builtinNodeRegistry.get('Set Closure'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Set Closure'),
    parsed = parseNodeDeclaration(JSON.stringify(setClosureDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.set-closure.v1', { evaluate: evaluateSetClosure }]]),
      nectNodeHost,
    ).get('Set Closure'),
    n = node('Set Closure');
  assert.equal(manifest.binding.evaluate, evaluateSetClosure);
  assert.deepEqual(registered.declaration, setClosureDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(n.params.closed, {
    type: 'Boolean',
    value: true,
    expression: '',
    locked: false,
  });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Set Path Start');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Taper');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Set Path Start');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Reverse Path');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, setClosureDeclaration.catalog);
});
function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    curve = node('Curve'),
    closure = node('Set Closure'),
    fill = node('Fill'),
    output = node('Output');
  doc.schemaVersion = 31;
  layer.id = 'layer';
  for (const [item, id] of [
    [curve, 'curve'],
    [closure, 'close'],
    [fill, 'fill'],
    [output, 'out'],
  ])
    item.id = id;
  curve.points.forEach((point, i) => (point.id = `p${i}`));
  layer.nodes = [curve, closure, fill, output];
  layer.output = output.id;
  for (const [from, to] of [
    [curve, closure],
    [closure, fill],
    [fill, output],
  ])
    connect(layer, from.id, to.id, 'path');
  return { doc, layer, curve, closure };
}

test('Set Closure preserves current geometry, stable IDs and fingerprints in both states', () => {
  const { doc, layer, curve, closure } = fixture(),
    before = serialize(doc);
  for (const [closed, expected] of [
    [true, '948e95db36b00cbc5ae290729ee917d504854a92392999f324b35ef54b956571'],
    [false, 'b91b850f9a4182aa7a05c1316b597601a7e1f9d3eec560c4d22e61bb0590d3a3'],
  ]) {
    closure.params.closed.value = closed;
    const e = evaluate(layer),
      source = e.run(curve.id),
      result = e.run(closure.id);
    assert.equal(hash(result), expected);
    assert.equal(result.contours[0].closed, closed);
    assert.notEqual(result.contours[0].points[0].id, source.contours[0].points[0].id);
    assert.deepEqual(evaluate(layer).run(closure.id), result);
  }
  closure.params.closed.value = true;
  assert.equal(serialize(doc), before);
  const missing = blankDocument(),
    nodeWithoutInput = node('Set Closure');
  missing.layers[0].nodes.push(nodeWithoutInput);
  assert.throws(
    () => evaluate(missing.layers[0]).run(nodeWithoutInput.id),
    /Set Closure needs a Path with contours/,
  );
});

test('Set Closure edits are atomic and survive bypass, Undo/Redo and reload', () => {
  const { doc, layer, curve, closure } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: closure.id, ...payload },
      }),
    before = serialize(session.snapshot());
  send('set-parameter', { key: 'closed', value: false });
  const open = serialize(session.snapshot());
  assert.equal(evaluate(session.snapshot().layers[0]).run(closure.id).contours[0].closed, false);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  const rev = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'closed', value: 'invalid' }));
  assert.equal(serialize(session.snapshot()), before);
  assert.equal(session.state.expectedRevision, rev);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), open);
  send('set-node-bypassed', { bypassed: true });
  const bypassed = serialize(session.snapshot()),
    evaluation = evaluate(session.snapshot().layers[0]);
  assert.deepEqual(evaluation.run(closure.id), evaluation.run(curve.id));
  send('undo');
  assert.equal(serialize(session.snapshot()), open);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.equal(serialize(deserialize(bypassed)), bypassed);
});

test('actual Text keeps its routed occurrence and current point identities', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    fonts = {
      default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    closure = node('Set Closure');
  text.id = 'text-source';
  text.text = 'I';
  closure.id = 'closure-text';
  layer.nodes.push(text, closure);
  connect(layer, text.id, closure.id, 'path');
  for (const [closed, expected] of [
    [true, '28e4996ed455ac80ecc95e7bca3ed825855a38d5b6f2f18ac59c1432a7c945a6'],
    [false, '3c1a6d1379d45bc3f4cfada769dc53f3540d0bb953d6e426211a4801c9349dbd'],
  ]) {
    closure.params.closed.value = closed;
    const before = serialize(doc),
      result = evaluate(layer, fonts).run(closure.id);
    assert.equal(hash(result), expected);
    assert.equal(result.characterOccurrences[0].source, text.id);
    assert.equal(result.characterOccurrences[0].route, `${text.id}/${closure.id}`);
    assert.match(result.characterOccurrences[0].reason, /unavailable through Set Closure/);
    assert.deepEqual(evaluate(deserialize(before).layers[0], fonts).run(closure.id), result);
    assert.equal(serialize(doc), before);
  }
});
