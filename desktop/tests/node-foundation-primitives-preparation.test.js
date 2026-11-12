import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { blankDocument, evaluate, node } from '../src/core.js';
import { evaluateCircle } from '../src/circle-node-binding.js';
import { evaluateEllipse } from '../src/ellipse-node-binding.js';
import { evaluateRectangle } from '../src/rectangle-node-binding.js';

const bindings = {
  Circle: evaluateCircle,
  Ellipse: evaluateEllipse,
  Rectangle: evaluateRectangle,
};
const baselineHashes = {
  Circle: '6dc7778870e6eab8c1a345f86018592fcf4f968e7717d950b03ef413389f1c87',
  Ellipse: '86f3c017ef100636b7979f930e8d48f79f39ebf3a8b5be85132f85af9a870f29',
  Rectangle: 'e7c9b7a06af268f39c9b5e00b71f72f87112d7a9cb4bb765b746f9772d8a5ee8',
};

for (const type of Object.keys(bindings)) {
  test(`${type} binding matches current geometry for signed and degenerate dimensions`, () => {
    const dimensions =
      type === 'Circle'
        ? [-20, 0, 1, 80, 10000]
        : [
            [-20, 50],
            [0, 50],
            [50, 0],
            [1, 1],
            [80, 120],
          ];
    const outputs = [];
    for (const center of [
      [0, 0],
      [-100, 50],
      [350, 250],
    ]) {
      for (const value of dimensions) {
        const doc = blankDocument(),
          layer = doc.layers[0],
          shape = node(type);
        shape.id = `geometry-${type}`;
        shape.params.center.value = center;
        shape.params[type === 'Circle' ? 'radius' : 'size'].value = value;
        layer.nodes.push(shape);
        const before = JSON.stringify(shape),
          expected = evaluate(layer).run(shape.id),
          actual = bindings[type](undefined, (key) => shape.params[key].value, {
            nodeId: shape.id,
          });
        assert.deepEqual(actual, expected);
        assert.equal(JSON.stringify(shape), before);
        outputs.push(actual);
      }
    }
    assert.equal(
      createHash('sha256').update(JSON.stringify(outputs)).digest('hex'),
      baselineHashes[type],
    );
  });
}
