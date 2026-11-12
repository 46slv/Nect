import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import { blankDocument, connect, deserialize, evaluate, node, serialize } from '../src/core.js';
import { parseNodeDeclaration } from '../src/node-foundation.js';
import { puckerBloatDeclaration } from '../src/pucker-bloat-declaration.js';
import { evaluatePuckerBloat } from '../src/pucker-bloat-binding.js';
import { parseFont } from '../src/fonts.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');

function scene(rawPath) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    effect = node('Pucker & Bloat');
  source.id = 'source';
  source.rawPath = rawPath;
  effect.id = 'effect';
  layer.nodes.push(source, effect);
  connect(layer, source.id, effect.id, 'path');
  return { doc, layer, source, effect };
}

test('Pucker & Bloat declaration captures existing defaults and pass-through bypass', () => {
  const parsed = parseNodeDeclaration(JSON.stringify(puckerBloatDeclaration));
  assert.ok(Object.isFrozen(parsed));
  assert.equal(parsed.bypassInput, 'path');
  assert.deepEqual(parsed.outputs, [
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
