import { document, node, connect } from '../../src/core.js';

export function compoundFixture(counts = [40, 10, 31, 7]) {
  const doc = document(),
    layer = doc.layers[0],
    path = node('Multi Path', 40, 40),
    fill = node('Fill', 360, 40),
    output = node('Output', 680, 40);
  doc.name = 'Contour point navigation';
  doc.width = 400;
  doc.height = 300;
  const template = node('Curve').points[0];
  const definitions = [
    ['scope-outer', true, [85, 100], 60, 1, 'scope-shape-a'],
    ['scope-hole', true, [85, 100], 24, -1, 'scope-shape-a'],
    ['scope-other', true, [260, 100], 55, 1, 'scope-shape-b'],
    ['scope-open', false, [260, 220], 35, 1, 'scope-shape-c'],
  ];
  path.points = [];
  path.contours = definitions.map(([id, closed, center, radius, direction, group], ci) => {
    const ids = [];
    for (let i = 0; i < counts[ci]; i++) {
      const p = structuredClone(template),
        angle = (direction * i * 2 * Math.PI) / counts[ci];
      p.id = `${id}-p${i}`;
      p.position.value = center.map(
        (v, axis) => v + radius * (axis ? Math.sin(angle) : Math.cos(angle)),
      );
      p.handleA.value = [0, 0];
      p.handleB.value = [0, 0];
      p.handleType = 'corner';
      ids.push(p.id);
      path.points.push(p);
    }
    return { id, closed, group, points: ids };
  });
  // Global storage order deliberately differs from contour traversal.
  path.points.reverse();
  path.units = [
    { id: 'scope-unit-a', contours: ['scope-outer', 'scope-hole'] },
    { id: 'scope-unit-b', contours: ['scope-other', 'scope-open'] },
  ];
  layer.nodes = [path, fill, output];
  layer.output = output.id;
  connect(layer, path.id, fill.id, 'path');
  connect(layer, fill.id, output.id, 'path');
  return { doc, layer, path, fill, output };
}
