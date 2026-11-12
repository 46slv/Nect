import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  blankDocument,
  connect,
  deserialize,
  enableRemapPoint,
  evaluate,
  node,
  serialize,
} from '../src/core.js';

const expected = {
  miter: 'bbdcf166363fc373b3eeb3f3ea18a7ac2947e211d77b6010d928f770c2511562',
  bevel: '854657826ee4ec38fd01ef375f6a582f222a983a78dab3193476727a37239098',
  round: '1c236e326fe7e5d72e81d9e61ca7daf36790c0f2eaaeba9e3fd1f659aa216a18',
  expression: 'bbdcf166363fc373b3eeb3f3ea18a7ac2947e211d77b6010d928f770c2511562',
  connected: 'ad7d9cf4991698d1de99688461c2f9de6fee98ac406d6398eb39622900951a94',
  grouped: 'cd5a535ee615bb9c9b413dbfbcad34c6e20f5d2f3b3f6cd60184db50ab2eb53e',
};

for (const scenario of Object.keys(expected)) {
  test(`Closed Path Offset preserves evaluated fingerprint and Remap for ${scenario}`, () => {
    const doc = blankDocument(),
      layer = doc.layers[0],
      output = layer.nodes[0],
      source = node('Rectangle'),
      offset = node('Closed Path Offset');
    output.id = 'output';
    layer.output = output.id;
    source.id = 'source';
    source.params.center.value = [30, 20];
    source.params.size.value = [60, 40];
    offset.id = 'offset';
    offset.params.amount.value = 5;
    offset.params.miterLimit.value = 2;
    offset.operation = ['miter', 'bevel', 'round'].includes(scenario) ? scenario : 'miter';
    layer.nodes.push(source, offset);
    let input = source;
    if (scenario === 'grouped') {
      const other = node('Rectangle'),
        join = node('Join Geometry');
      other.id = 'other';
      other.params.center.value = [55, 20];
      other.params.size.value = [40, 40];
      join.id = 'join';
      layer.nodes.push(other, join);
      connect(layer, source.id, join.id, 'a');
      connect(layer, other.id, join.id, 'b');
      input = join;
    }
    if (scenario === 'connected') {
      const amount = node('Value');
      amount.id = 'amount';
      amount.params.value.value = 5;
      layer.nodes.push(amount);
      connect(layer, amount.id, offset.id, 'amount');
    }
    if (scenario === 'expression') offset.params.amount.expression = '2 + 3';
    connect(layer, input.id, offset.id, 'path');
    const result = evaluate(layer).run(offset.id);
    assert.equal(
      createHash('sha256').update(JSON.stringify(result)).digest('hex'),
      expected[scenario],
    );

    const remap = node('Path Remap');
    remap.id = 'remap';
    layer.nodes.push(remap);
    connect(layer, offset.id, remap.id, 'path');
    enableRemapPoint(remap, result, result.contours[0].points[0].id).params.position.value = [3, 1];
    const active = evaluate(layer),
      edited = active.run(remap.id),
      saved = serialize(doc),
      reopened = evaluate(deserialize(saved).layers[0]);
    assert.deepEqual(reopened.run(remap.id), edited);
    assert.deepEqual(active.warnings, []);
    assert.deepEqual(reopened.warnings, []);
  });
}
