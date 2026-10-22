import { blankDocument, node, connect } from '../../src/core.js';
export function angledGridFixture(angle = 45, origin = 'top-left') {
  const doc = blankDocument();
  Object.assign(doc, { name: `NECT_ANGLED_GRID_${angle}_${origin}`, width: 600, height: 600 });
  Object.assign(doc.editor, {
    gridAnchor: origin,
    gridVisible: true,
    gridSize: 40,
    gridAngle: 0,
    snap: true,
    snapGuides: false,
    snapArtboard: false,
    snapArtwork: false,
  });
  const o = origin === 'center' ? [300, 300] : [0, 0],
    local = origin === 'center' ? [80, 80] : [240, 120],
    r = (angle * Math.PI) / 180;
  const target = [
    o[0] + local[0] * Math.cos(r) - local[1] * Math.sin(r),
    o[1] + local[0] * Math.sin(r) + local[1] * Math.cos(r),
  ];
  const layer = doc.layers[0];
  layer.id = 'angled-grid-layer';
  const shape = node('Rectangle', 40, 50),
    fill = node('Fill', 350, 50),
    out = node('Output', 660, 50);
  [shape, fill, out].forEach(
    (n, i) => (n.id = ['angled-grid-rectangle', 'angled-grid-fill', 'angled-grid-output'][i]),
  );
  shape.params.center.value = target.map((v, i) => v + (i ? -7 : 8));
  shape.params.size.value = [40, 40];
  layer.nodes = [shape, fill, out];
  layer.output = out.id;
  layer.allowEmptyOutput = false;
  connect(layer, shape.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  return { doc, layer, shape, fill, out, target };
}
