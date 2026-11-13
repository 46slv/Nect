import test from 'node:test';
import assert from 'node:assert/strict';
import {
  bypassContract,
  CommandSession,
  definitions,
  deserialize,
  evaluate,
  exportSVG,
  node,
  outputPorts,
  outputType,
  parameter,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { maskDeclaration } from '../src/mask-node-declaration.js';
import { evaluateMask } from '../src/mask-node-binding.js';
import { luminanceFixture } from './fixtures/mask-luminance.js';

test('Mask registration retains its interface, placement and implicit controls', () => {
  const registered = builtinNodeRegistry.get('Mask'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Mask'),
    current = node('Mask'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.ok(registered && manifest);
  assert.deepEqual(registered.declaration, maskDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateMask);
  assert.equal(outputType(current), 'PaintedPath');
  assert.deepEqual(outputPorts(current), [{ id: undefined, label: '', type: 'PaintedPath' }]);
  assert.equal(bypassContract(current).input, 'path');
  for (const item of maskDeclaration.parameters) {
    assert.equal(definitions.Mask[item.id].type, item.type);
    assert.deepEqual(current.params[item.id].value, item.default);
  }
  assert.equal(Object.hasOwn(current.params, 'feather'), false);
  assert.equal(Object.hasOwn(current.params, 'luminance'), false);
  assert.equal(parameter(current, 'feather').value, 0);
  assert.equal(parameter(current, 'luminance').value, false);
  assert.equal(types[types.indexOf('Mask') - 1], 'Keyer');
  assert.equal(types[types.indexOf('Mask') + 1], 'Circle');
  assert.equal(catalog[catalog.indexOf('Mask') - 1], 'Keyer');
  assert.equal(catalog[catalog.indexOf('Mask') + 1], 'Circle');
  const { typeId, ...metadata } = nodeCatalog.find((entry) => entry.typeId === 'Mask');
  assert.equal(typeId, 'Mask');
  assert.deepEqual(metadata, maskDeclaration.catalog);
});

test('binding retains input appearance identity and validates before copying it', () => {
  const source = { kind: 'image', asset: 'image-source', position: [0, 0], size: [40, 20] },
    shape = { kind: 'image', asset: 'image-mask', position: [0, 0], size: [40, 20] },
    original = structuredClone({ source, shape }),
    values = { path: source, mask: shape, invert: true, feather: 2, luminance: true },
    calls = [],
    read = (key) => {
      calls.push(key);
      return values[key];
    },
    context = {
      nodeId: 'mask',
      pathBounds(value) {
        assert.equal(value, source);
        calls.push('bounds');
        return [
          [0, 20, 40],
          [0, 10, 20],
        ];
      },
      identity: {
        copy(from, to) {
          assert.equal(from, source);
          calls.push(['copy', to]);
        },
      },
    };
  const result = evaluateMask(undefined, read, context);
  assert.deepEqual(calls, [
    'path',
    'mask',
    'invert',
    'feather',
    'luminance',
    'bounds',
    ['copy', result.children[0]],
  ]);
  assert.deepEqual(result.children[0], source);
  assert.notEqual(result.children[0], source);
  assert.deepEqual(result.mask.shape, shape);
  assert.equal(result.mask.invert, true);
  assert.equal(result.mask.feather, 2);
  assert.equal(result.mask.luminance, true);
  assert.deepEqual({ source, shape }, original);
  calls.length = 0;
  values.mask = null;
  assert.throws(() => evaluateMask(undefined, read, context), /Connect appearance/);
  assert.deepEqual(calls, ['path', 'mask']);
  calls.length = 0;
  values.mask = shape;
  values.feather = -1;
  assert.throws(() => evaluateMask(undefined, read, context), /Mask Feather/);
  assert.equal(
    calls.some((call) => Array.isArray(call) && call[0] === 'copy'),
    false,
  );
  assert.deepEqual({ source, shape }, original);
});

test('registered Image mask preserves evaluated appearance and bypass history', () => {
  const { doc, layer, mask, keyer, alphaKeyer } = luminanceFixture({
      luminance: true,
      feather: 4,
      keyed: true,
    }),
    original = serialize(doc),
    first = evaluate(layer, {}, doc.imageAssets, doc),
    result = first.run(mask.id),
    warm = evaluate(layer, {}, doc.imageAssets, doc);
  assert.equal(first.error, null);
  assert.deepEqual(result.children[0], first.run(keyer.id));
  assert.deepEqual(result.mask.shape, first.run(alphaKeyer.id));
  assert.equal(result.mask.feather, 4);
  assert.equal(result.mask.luminance, true);
  assert.deepEqual(warm.run(mask.id), result);
  assert.ok(warm.stats.reused > 0);
  assert.equal(serialize(doc), original);

  const session = new CommandSession(doc),
    send = (type, payload = {}) => session.dispatch({ ...session.state, type, payload }),
    beforeSvg = exportSVG(session.snapshot());
  send('set-node-bypassed', { layerId: layer.id, nodeId: mask.id, bypassed: true });
  const bypassed = serialize(session.snapshot()),
    snapshot = session.snapshot(),
    bypassResult = evaluate(snapshot.layers[0], {}, snapshot.imageAssets, snapshot).run(mask.id);
  assert.equal(bypassResult.kind, 'painted-group');
  assert.equal(bypassResult.mask, undefined);
  assert.notEqual(exportSVG(snapshot), beforeSvg);
  send('undo');
  assert.equal(serialize(session.snapshot()), original);
  assert.equal(exportSVG(session.snapshot()), beforeSvg);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.equal(exportSVG(deserialize(bypassed)), exportSVG(session.snapshot()));
});
