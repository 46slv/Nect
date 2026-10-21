import { blankDocument, node, connect } from '../../src/core.js';
export function graphPerformanceFixture(count) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    prefix = `perf-${count}`;
  doc.name = `Graph performance ${count}`;
  layer.id = `${prefix}-layer`;
  const mathCount = Math.floor((count - 3) / 2),
    valueCount = count - 3 - mathCount;
  const values = Array.from({ length: valueCount }, (_, i) => {
    const n = node('Value');
    n.id = `${prefix}-value-${i}`;
    n.params.value.value = i + 0.25;
    return n;
  });
  const maths = Array.from({ length: mathCount }, (_, i) => {
    const n = node('Math');
    n.id = `${prefix}-math-${i}`;
    return n;
  });
  const shape = node('Circle'),
    fill = node('Fill'),
    out = layer.nodes[0];
  shape.id = `${prefix}-circle`;
  fill.id = `${prefix}-fill`;
  out.id = `${prefix}-output`;
  shape.params.center.value = [200, 120];
  shape.params.radius.value = 24;
  layer.nodes = [...values, ...maths, shape, fill, out];
  layer.output = out.id;
  for (const [i, n] of layer.nodes.entries())
    n.position = [40 + (i % 6) * 350, 50 + Math.floor(i / 6) * 390];
  for (const [i, n] of maths.entries()) {
    connect(layer, values[i % values.length].id, n.id, 'a');
    connect(layer, values[(i + 1) % values.length].id, n.id, 'b');
  }
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  return {
    doc,
    nodeCount: count,
    connectionCount: mathCount * 2 + 2,
    dragNodeId: values[0].id,
    expanded: true,
    images: 0,
    dithers: 0,
    groups: 0,
  };
}
