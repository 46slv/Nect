import test from 'node:test';
import assert from 'node:assert/strict';
import {
  bypassContract,
  connect,
  definitions,
  document,
  evaluate,
  node,
  outputPorts,
  outputType,
  serialize,
} from '../src/core.js';
import { builtinManifest } from '../src/builtin-node-manifest.js';
import { builtinNodeRegistry } from '../src/builtin-node-registry.js';
import { createNodeRegistry } from '../src/node-foundation.js';
import { nectNodeHost } from '../src/nect-node-host.js';
import { nodeCatalog } from '../src/node-catalog.js';
import { textNodeDeclaration } from '../src/text-node-declaration.js';
import { evaluateText, projectTextOutput } from '../src/text-node-binding.js';

test('Text registration retains its unnamed Path and named Position outputs', () => {
  const current = node('Text'),
    registered = builtinNodeRegistry.get('Text'),
    manifest = builtinManifest.find((row) => row.declaration.typeId === 'Text'),
    types = Object.keys(definitions),
    catalog = nodeCatalog.map((entry) => entry.typeId);
  assert.deepEqual(registered.declaration, textNodeDeclaration);
  assert.equal(manifest.binding.evaluate, evaluateText);
  assert.equal(manifest.binding.project, projectTextOutput);
  assert.equal(current.operation, 'subtract');
  assert.equal(outputType(current), 'Path');
  assert.equal(outputType(current, 'position'), 'Vector2');
  assert.deepEqual(outputPorts(current), [
    { id: undefined, label: 'Path', type: 'Path' },
    { id: 'position', label: 'Position', type: 'Vector2' },
  ]);
  assert.equal(bypassContract(current).supported, false);
  assert.equal(types[types.indexOf('Text') - 1], 'Output');
  assert.equal(catalog[catalog.indexOf('Text') - 1], 'Imported Path');
  const bindings = new Map([
    ['builtin.text.v1', { evaluate: evaluateText, project: projectTextOutput }],
  ]);
  const ambiguous = structuredClone(textNodeDeclaration);
  ambiguous.outputs[1].legacyDefault = true;
  assert.throws(() => createNodeRegistry([ambiguous], bindings, nectNodeHost), /output/i);
  assert.throws(
    () =>
      createNodeRegistry(
        [textNodeDeclaration],
        new Map([['builtin.text.v1', { evaluate: evaluateText }]]),
        nectNodeHost,
      ),
    /projection/i,
  );
});

test('Text Position can drive geometry without loading or generating its font outline', () => {
  const doc = document(),
    layer = doc.layers[0],
    text = node('Text'),
    shape = node('Rectangle'),
    fill = node('Fill'),
    output = node('Output');
  text.text = 'Unavailable font';
  text.font = 'missing-font';
  text.params.position.value = [31, 47];
  layer.nodes = [text, shape, fill, output];
  layer.output = output.id;
  connect(layer, text.id, shape.id, 'center', false, 'position');
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  const before = serialize(doc),
    result = evaluate(layer, {}, doc.imageAssets, doc);
  assert.equal(result.error, null);
  assert.deepEqual(result.run(text.id, 'position'), [31, 47]);
  assert.throws(() => result.run(text.id), /Font is still loading or unavailable/);
  assert.equal(serialize(doc), before);
});
