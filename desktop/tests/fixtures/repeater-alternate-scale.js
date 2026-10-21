import { document, node, connect, point } from '../../src/core.js';

// The source anchor is the motif center, not the radial orbit center. In the
// radial seed M(anchor) follows a 150-unit orbit around (480,270).
export function alternateRepeaterFixture(kind = 'linear') {
  const doc = document();
  doc.schemaVersion = 37;
  doc.name = `Repeater alternate scale ${kind}`;
  doc.width = 960;
  doc.height = 540;
  const layer = doc.layers[0];
  layer.id = 'alternate-layer';
  const source = node('Curve', 40, 40),
    repeat = node('Geometry Repeater', 340, 40),
    fill = node('Fill', 640, 40),
    out = node('Output', 940, 40);
  [source, repeat, fill, out].forEach((n, i) => (n.id = `alternate-${i}`));
  const anchor = kind === 'radial' ? [480, 120] : [100, 270];
  source.params.closed.value = true;
  source.points = [
    [-8, -13, [0, 4], [3, -2]],
    [8, -13, [-3, -2], [0, 4]],
    [8, 13, [0, -4], [-3, 2]],
    [-8, 13, [3, 2], [0, -4]],
  ].map(([x, y, handleA, handleB], i) => {
    const p = point(anchor[0] + x, anchor[1] + y);
    p.id = `alternate-source-p${i}`;
    p.handleA.value = handleA;
    p.handleB.value = handleB;
    p.handleType = 'corner';
    return p;
  });
  repeat.operation = 'ae-2d';
  repeat.params.count.value = 24;
  repeat.params.anchor.value = [...anchor];
  repeat.params.position.value =
    kind === 'radial'
      ? [150 * Math.sin(Math.PI / 12), 150 * (1 - Math.cos(Math.PI / 12))]
      : [32, 0];
  repeat.params.rotation.value = kind === 'radial' ? 15 : 0;
  repeat.params.offset.value = kind === 'radial' ? 1 : -1;
  repeat.params.startOpacity.value = 45;
  repeat.params.endOpacity.value = 95;
  fill.params.color.value = { hex: '#285d8f', alpha: 0.8 };
  layer.nodes = [source, repeat, fill, out];
  layer.output = out.id;
  for (const [a, b] of [
    [source, repeat],
    [repeat, fill],
    [fill, out],
  ])
    connect(layer, a.id, b.id, 'path');
  return { doc, layer, source, repeat, fill, out, anchor };
}
