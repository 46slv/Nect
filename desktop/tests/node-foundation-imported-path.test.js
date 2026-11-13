import test from 'node:test';
import assert from 'node:assert/strict';
import {
  blankDocument,
  bypassContract,
  CommandSession,
  connect,
  definitions,
  deserialize,
  evaluate,
  exportSVG,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { importedPathDeclaration } from '../src/imported-path-node-declaration.js';
import { evaluateImportedPath } from '../src/imported-path-node-binding.js';

function fixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    output = layer.nodes[0],
    source = node('Imported Path'),
    remap = node('Path Remap'),
    fill = node('Fill');
  doc.schemaVersion = 54;
  source.id = 'import-fixed';
  source.rawPath = 'M0 0C20 -10 30 10 40 0L40 40H0Z M10 10L10 20L20 20L20 10Z M70 0L90 15';
  layer.nodes = [source, remap, fill, output];
  connect(layer, source.id, remap.id, 'path');
  connect(layer, remap.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, source, remap };
}

test('Imported Path registration preserves its source interface and catalog placement', () => {
  const registered = builtinNodeRegistry.get('Imported Path'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Imported Path'),
    current = node('Imported Path'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, importedPathDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateImportedPath);
  assert.deepEqual(current.params, {});
  assert.equal(current.rawPath, '');
  assert.equal(outputType(current), 'Path');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'Path' }]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(types[types.indexOf('Imported Path') - 1], 'Text to Path');
  assert.equal(types[types.indexOf('Imported Path') + 1], 'Ordered Dither Image');
  assert.equal(catalog[catalog.indexOf('Imported Path') - 1], 'Multi Path');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Imported Path');
  assert.equal(typeId, 'Imported Path');
  assert.deepEqual(metadata, importedPathDeclaration.catalog);
});

test('imported curves, holes and open contours keep their stable output and fingerprint', () => {
  const { doc, layer, source } = fixture(),
    before = serialize(doc),
    evaluation = evaluate(layer, {}, doc.imageAssets, doc),
    path = evaluation.run(source.id);
  assert.equal(evaluation.error, null);
  assert.deepEqual(
    path.contours.map((contour) => [contour.id, contour.closed, contour.points.length]),
    [
      ['import-fixed:c0', true, 4],
      ['import-fixed:c1', true, 4],
      ['import-fixed:c2', false, 2],
    ],
  );
  assert.deepEqual(path.contours[0].points[0], {
    id: 'import-fixed:c0:p0',
    position: [0, 0],
    handleA: [0, 0],
    handleB: [20, -10],
  });
  assert.equal(path.unstable, true);
  assert.equal(
    path.fingerprint,
    '[[true,[[[0,0],[0,0],[20,-10]],[[40,0],[-10,10],[0,0]],[[40,40],[0,0],[0,0]],[[0,40],[0,0],[0,0]]]],[true,[[[10,10],[0,0],[0,0]],[[10,20],[0,0],[0,0]],[[20,20],[0,0],[0,0]],[[20,10],[0,0],[0,0]]]],[false,[[[70,0],[0,0],[0,0]],[[90,15],[0,0],[0,0]]]]]',
  );
  assert.equal(serialize(doc), before);
  const warm = evaluate(layer, {}, doc.imageAssets, doc);
  assert.deepEqual(warm.run(source.id), path);
  assert.ok(warm.stats.reused > 0);
});

test('Remap edits survive history and reload, then suspend if the imported geometry changes', () => {
  const { doc, layer, source, remap } = fixture(),
    session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    path = (snapshot) =>
      evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot).run(remap.id),
    before = serialize(session.snapshot()),
    id = 'import-fixed:c0:p0';
  send('translate-points', { layerId: layer.id, nodeId: remap.id, pointIds: [id], delta: [3, 4] });
  const edited = serialize(session.snapshot());
  assert.deepEqual(path(session.snapshot()).contours[0].points[0].position, [3, 4]);
  send('undo');
  assert.equal(serialize(session.snapshot()), before);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  const reopened = deserialize(edited);
  assert.equal(exportSVG(reopened), exportSVG(session.snapshot()));
  assert.deepEqual(path(reopened).contours[0].points[0].position, [3, 4]);
  const changed = reopened.layers[0],
    imported = changed.nodes.find((item) => item.id === source.id);
  imported.rawPath = imported.rawPath.replace('40 40', '45 40');
  const evaluated = evaluate(changed, {}, reopened.imageAssets, reopened);
  assert.match(evaluated.warnings.join(' '), /suspended/i);
  assert.deepEqual(evaluated.run(remap.id).contours[0].points[0].position, [0, 0]);
});

test('empty imported paths remain valid and malformed path data remains rejected', () => {
  const { doc, layer, source } = fixture();
  source.rawPath = '';
  const empty = evaluate(layer, {}, doc.imageAssets, doc);
  assert.equal(empty.error, null);
  assert.deepEqual(empty.run(source.id).contours, []);
  assert.equal(empty.run(source.id).fingerprint, '[]');
  source.rawPath = 'M0 0 <script>';
  const invalid = evaluate(layer, {}, doc.imageAssets, doc);
  assert.match(String(invalid.error), /Unsupported SVG path data/);
  assert.throws(() => deserialize(serialize(doc)), /Unsupported SVG path data/);
});
