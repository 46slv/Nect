import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createHash } from 'node:crypto';
import {
  activeParameters,
  blankDocument,
  CommandSession,
  connect,
  deserialize,
  evaluate,
  node,
  serialize,
} from '../src/core.js';
import { parseFont } from '../src/fonts.js';
import { evaluateResamplePath } from '../src/resample-path-binding.js';

const hash = (value) => createHash('sha256').update(JSON.stringify(value)).digest('hex');
const cases = [
  [
    'M0 0 L100 0 L100 100',
    'count',
    5,
    24,
    '83540f9ceae99f77ec8f06c5e99dc0c0720d7973db3fbdc3ac6c6eef06042b6f',
  ],
  [
    'M0 0 L100 0 L100 100',
    'spacing',
    32,
    50,
    '83540f9ceae99f77ec8f06c5e99dc0c0720d7973db3fbdc3ac6c6eef06042b6f',
  ],
  [
    'M0 0 L100 0 L100 100 L0 100 Z',
    'count',
    8,
    24,
    'a474398931d7a6a735ac0f26c0c3237c11416540149eabff69209a63766eaa16',
  ],
  [
    'M0 0 L100 0 L100 100 L0 100 Z',
    'spacing',
    32,
    45,
    '71a26366793c802017c922bd10eedfb31beb847ab69f710613750a953f22083c',
  ],
  [
    'M0 0 C20 0 20 50 60 50',
    'count',
    7,
    24,
    '67df56afe71ca46d6946d16a545e1ced7d1f15b76bad7b9c4430ca231e95fc86',
  ],
  ['M0 0 L0 0', 'count', 4, 24, 'c530b4d871605ec2e622dbd1cfa2e1fec2d57472a25e726b7529405b93cc3ca7'],
];

for (const [raw, operation, count, spacing, expected] of cases)
  test(`Resample ${operation} preserves geometry and IDs for ${raw}`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      source = node('Imported Path'),
      effect = node('Resample Path');
    source.id = 'source';
    source.rawPath = raw;
    effect.id = 'resample';
    effect.operation = operation;
    effect.params.count.value = count;
    effect.params.spacing.value = spacing;
    layer.nodes.push(source, effect);
    connect(layer, source.id, effect.id, 'path');
    const engine = evaluate(layer),
      input = engine.run(source.id),
      before = JSON.stringify(input),
      reads = [],
      bound = evaluateResamplePath(
        operation,
        (key) => {
          reads.push(key);
          return key === 'path' ? input : effect.params[key].value;
        },
        { nodeId: effect.id },
      );
    assert.deepEqual(reads, ['path', operation]);
    assert.equal(hash(bound), expected);
    assert.deepEqual(engine.run(effect.id), bound);
    assert.equal(JSON.stringify(input), before);
    const saved = serialize(doc);
    assert.deepEqual(evaluate(deserialize(saved).layers[0]).run(effect.id), bound);
  });

test('Resample skips inactive expressions and keeps active input projection', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    effect = node('Resample Path');
  source.rawPath = 'M0 0 L100 0';
  layer.nodes.push(source, effect);
  connect(layer, source.id, effect.id, 'path');
  effect.operation = 'count';
  effect.params.spacing.expression = '1/0';
  assert.deepEqual(
    activeParameters(effect).map(([key]) => key),
    ['path', 'count'],
  );
  assert.equal(evaluate(layer).run(effect.id).contours[0].points.length, 32);
  effect.params.spacing.expression = '';
  effect.operation = 'spacing';
  effect.params.count.expression = '1/0';
  assert.deepEqual(
    activeParameters(effect).map(([key]) => key),
    ['path', 'spacing'],
  );
  assert.ok(evaluate(layer).run(effect.id).contours[0].points.length > 0);
});

test('Resample Session preserves mode, invalid-edit atomicity, bypass and history', () => {
  const doc = blankDocument(),
    layer = doc.layers[0],
    source = node('Imported Path'),
    effect = node('Resample Path');
  source.rawPath = 'M0 0 L100 0 L100 100 L0 100 Z';
  layer.nodes.push(source, effect);
  connect(layer, source.id, effect.id, 'path');
  const session = new CommandSession(doc),
    send = (type, payload = {}) =>
      session.dispatch({
        ...session.state,
        type,
        payload: { layerId: layer.id, nodeId: effect.id, ...payload },
      }),
    before = serialize(session.snapshot());
  send('set-operation', { operation: 'spacing' });
  send('set-parameter', { key: 'spacing', value: 50 });
  const edited = serialize(session.snapshot()),
    expected = evaluate(session.snapshot().layers[0]).run(effect.id);
  send('undo');
  const prior = serialize(session.snapshot()),
    revision = session.state.expectedRevision,
    history = JSON.stringify(session.historyView);
  assert.throws(() => send('set-parameter', { key: 'spacing', value: 0 }));
  assert.equal(serialize(session.snapshot()), prior);
  assert.equal(session.state.expectedRevision, revision);
  assert.equal(JSON.stringify(session.historyView), history);
  send('redo');
  assert.equal(serialize(session.snapshot()), edited);
  assert.deepEqual(evaluate(session.snapshot().layers[0]).run(effect.id), expected);
  send('set-node-bypassed', { bypassed: true });
  const bypassed = serialize(session.snapshot()),
    evaluation = evaluate(session.snapshot().layers[0]);
  assert.deepEqual(evaluation.run(effect.id), evaluation.run(source.id));
  send('undo');
  assert.equal(serialize(session.snapshot()), edited);
  send('redo');
  assert.equal(serialize(session.snapshot()), bypassed);
  assert.equal(serialize(deserialize(bypassed)), bypassed);
  assert.notEqual(before, edited);
});

test('Resample retains actual Text routing and generated point identities', () => {
  const bytes = fs.readFileSync(new URL('../public/fonts/DejaVuSans.ttf', import.meta.url)),
    fonts = {
      default: parseFont(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength)),
    },
    doc = blankDocument(),
    layer = doc.layers[0],
    text = node('Text'),
    effect = node('Resample Path');
  text.id = 'text-source';
  text.text = 'I';
  effect.id = 'resample-text';
  effect.operation = 'count';
  effect.params.count.value = 6;
  layer.nodes.push(text, effect);
  connect(layer, text.id, effect.id, 'path');
  const saved = serialize(doc),
    result = evaluate(layer, fonts).run(effect.id);
  assert.equal(hash(result), '03a76bacb75a12e26df5edddf4f4a6c9073b296f84ef5dcd94c0ae0ecce479ce');
  assert.equal(result.characterOccurrences[0].source, text.id);
  assert.equal(result.characterOccurrences[0].route, `${text.id}/${effect.id}`);
  assert.match(result.characterOccurrences[0].reason, /unavailable through Resample Path/);
  assert.deepEqual(evaluate(deserialize(saved).layers[0], fonts).run(effect.id), result);
});
