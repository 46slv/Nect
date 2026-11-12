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
  enableRemapPoint,
  evaluate,
  node,
  outputPorts,
  outputType,
  remapFingerprintMatches,
  serialize,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { textToPathDeclaration } from '../src/text-to-path-declaration.js';
import { evaluateTextToPath } from '../src/text-to-path-binding.js';
import { createNodeRegistry, parseNodeDeclaration } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';

const canonical = 'sha256:fdac2e16e2d57ac4881efb8fcc0aeb286f9d831627631abcba06bcdc49b8eff7';
const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');

test('Text to Path resolves through its trusted binding without changing its interface', () => {
  const registered = builtinNodeRegistry.get('Text to Path'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Text to Path'),
    parsed = parseNodeDeclaration(JSON.stringify(textToPathDeclaration)),
    resolved = createNodeRegistry(
      [parsed],
      new Map([['builtin.text-to-path.v1', { evaluate: evaluateTextToPath }]]),
      nectNodeHost,
    ).get('Text to Path'),
    n = node('Text to Path');
  assert.equal(manifest.binding.evaluate, evaluateTextToPath);
  assert.deepEqual(registered.declaration, textToPathDeclaration);
  assert.ok(Object.isFrozen(registered.declaration));
  assert.equal(resolved.declaration.bypassInput, 'path');
  assert.deepEqual(n.params.path, { type: 'Path', value: null, expression: '', locked: false });
  assert.deepEqual(outputPorts(n), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(outputType(n), 'Path');
  assert.equal(bypassContract(n).input, 'path');
  const defs = Object.keys(definitions),
    catalog = nodeCatalog.map(({ typeId }) => typeId);
  assert.equal(defs[defs.indexOf(n.type) - 1], 'Text');
  assert.equal(defs[defs.indexOf(n.type) + 1], 'Imported Path');
  assert.equal(catalog[catalog.indexOf(n.type) - 1], 'Convert to Path');
  assert.equal(catalog[catalog.indexOf(n.type) + 1], 'Expand Stroke');
  const { typeId, ...metadata } = nodeCatalog.find((item) => item.typeId === n.type);
  assert.equal(typeId, n.type);
  assert.deepEqual(metadata, textToPathDeclaration.catalog);
});

function fixture() {
  const doc = document(),
    layer = doc.layers[0],
    curve = node('Curve'),
    closure = node('Set Closure'),
    outline = node('Text to Path'),
    fill = node('Fill'),
    output = node('Output');
  doc.schemaVersion = 31;
  layer.id = 'layer';
  for (const [item, id] of [
    [curve, 'curve'],
    [closure, 'close'],
    [outline, 'outline'],
    [fill, 'fill'],
    [output, 'out'],
  ])
    item.id = id;
  curve.points.forEach((point, i) => (point.id = `p${i}`));
  layer.nodes = [curve, closure, outline, fill, output];
  layer.output = output.id;
  for (const [from, to] of [
    [curve, closure],
    [closure, outline],
    [outline, fill],
    [fill, output],
  ])
    connect(layer, from.id, to.id, 'path');
  return { doc, layer, curve, closure, outline };
}

test('Text to Path preserves current authored identity through clone and cache', () => {
  const { doc, layer, closure, outline } = fixture(),
    before = serialize(doc),
    first = evaluate(layer),
    source = first.run(closure.id),
    copied = first.run(outline.id),
    second = evaluate(layer),
    cached = second.run(outline.id);
  assert.notEqual(copied, source);
  assert.deepEqual(copied, source);
  assert.equal(hash(copied), '948e95db36b00cbc5ae290729ee917d504854a92392999f324b35ef54b956571');
  for (const value of [source, copied, cached]) {
    assert.equal(value.fingerprint, canonical);
    assert.equal(remapFingerprintMatches(value, canonical), true);
  }
  assert.deepEqual(first.stats, { computed: 5, reused: 0 });
  assert.deepEqual(second.stats, { computed: 0, reused: 5 });
  copied.contours[0].points[0].position[0] += 5;
  assert.notDeepEqual(copied.contours, source.contours);
  assert.equal(serialize(doc), before);
});

test('Text to Path keeps empty and missing input behavior and Session history', () => {
  const emptyDoc = blankDocument(),
    emptyLayer = emptyDoc.layers[0],
    missing = node('Text to Path'),
    empty = node('Multi Path');
  emptyLayer.nodes.push(missing, empty);
  assert.throws(() => evaluate(emptyLayer).run(missing.id), /Connect a Path input/);
  missing.bypassed = true;
  assert.throws(() => evaluate(emptyLayer).run(missing.id), /Connect path to bypass Text to Path/);
  missing.bypassed = false;
  connect(emptyLayer, empty.id, missing.id, 'path');
  assert.deepEqual(evaluate(emptyLayer).run(missing.id), {
    contours: [],
    unstable: true,
    fingerprint: '[]',
  });

  const { doc, layer, outline } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: outline.id, ...payload },
      }),
    before = serialize(session.snapshot());
  send('set-node-bypassed', { bypassed: true });
  const bypassed = serialize(session.snapshot());
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.equal(serialize(deserialize(bypassed)), bypassed);
  const reopened = deserialize(bypassed);
  const result = evaluate(reopened.layers[0]).run(outline.id);
  assert.equal(remapFingerprintMatches(result, canonical), true);
});

test('a current Path Remap edit survives save, reopen, continued editing and history', () => {
  const { doc, layer, outline } = fixture(),
    remap = node('Path Remap'),
    fill = layer.nodes.find((item) => item.id === 'fill');
  remap.id = 'remap';
  layer.nodes.splice(layer.nodes.indexOf(fill), 0, remap);
  connect(layer, outline.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  const initial = evaluate(layer).run(outline.id),
    point = initial.contours[0].points[0],
    initialPosition = [...point.position];
  enableRemapPoint(remap, initial, point.id).params.position.value = [3, 5];
  const reopened = deserialize(serialize(doc)),
    session = new CommandSession(reopened),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: remap.id, ...payload },
      }),
    position = () => {
      const result = evaluate(session.snapshot().layers[0]).run(remap.id);
      assert.notEqual(result.remapSuspended, true);
      return result.contours[0].points.find((item) => item.id === point.id).position;
    };
  assert.deepEqual(position(), [initialPosition[0] + 3, initialPosition[1] + 5]);
  send('translate-points', { pointIds: [point.id], delta: [2, -1] });
  const edited = serialize(session.snapshot());
  assert.deepEqual(position(), [initialPosition[0] + 5, initialPosition[1] + 4]);
  send('undo');
  assert.deepEqual(position(), [initialPosition[0] + 3, initialPosition[1] + 5]);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  assert.deepEqual(position(), [initialPosition[0] + 5, initialPosition[1] + 4]);
  assert.equal(serialize(deserialize(edited)), edited);
});

test('actual Text passes through without altering character route or source identities', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    fonts = {
      default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    outline = node('Text to Path');
  text.id = 'text-source';
  text.text = 'I';
  outline.id = 'outline';
  layer.nodes.push(text, outline);
  connect(layer, text.id, outline.id, 'path');
  const before = serialize(doc),
    engine = evaluate(layer, fonts),
    source = engine.run(text.id),
    result = engine.run(outline.id);
  assert.notEqual(result, source);
  assert.deepEqual(result, source);
  assert.equal(result.characterOccurrences.length, 1);
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].reason, null);
  assert.deepEqual(evaluate(deserialize(before).layers[0], fonts).run(outline.id), result);
  assert.equal(serialize(doc), before);
});
