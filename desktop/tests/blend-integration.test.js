import test from 'node:test';
import assert from 'node:assert/strict';
import {
  document,
  node,
  connect,
  evaluate,
  serialize,
  deserialize,
  exportSVG,
} from '../src/core.js';
test('Blend is available through node graph, serializes and exports colored geometry', () => {
  const doc = document(),
    l = doc.layers[0];
  const a = node('Rectangle'),
    b = node('Rectangle'),
    fa = node('Fill'),
    fb = node('Fill'),
    blend = node('Blend'),
    out = node('Output');
  a.params.center.value = [10, 10];
  b.params.center.value = [110, 10];
  fa.params.color.value = { hex: '#000000', alpha: 1 };
  fb.params.color.value = { hex: '#ffffff', alpha: 1 };
  blend.params.steps.value = 1;
  l.nodes = [a, b, fa, fb, blend, out];
  l.output = out.id;
  connect(l, a.id, fa.id, 'path');
  connect(l, b.id, fb.id, 'path');
  connect(l, fa.id, blend.id, 'a');
  connect(l, fb.id, blend.id, 'b');
  connect(l, blend.id, out.id, 'path');
  const result = evaluate(l, {}, doc.imageAssets, doc);
  assert.equal(result.error, null);
  assert.equal(result.path.children.length, 3);
  const svg = exportSVG(doc);
  assert.match(svg, /#bcbcbc/i);
  assert.equal(exportSVG(deserialize(serialize(doc))), svg);
});
