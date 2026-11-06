import { blankDocument, node } from '../../src/core.js';

// Fixed IDs and values make this a portable legacy output oracle.
export function legacyLuminanceFixture() {
  const doc = blankDocument(),
    layer = doc.layers[0],
    shape = node('Rectangle'),
    paint = node('Fill'),
    points = node('Luminance to Points'),
    motif = node('Curve'),
    instances = node('Instance on Points'),
    fill = node('Fill'),
    output = node('Output');
  doc.schemaVersion = 36;
  layer.id = 'legacy-field-layer';
  layer.name = 'Legacy field fixture';
  for (const [item, id] of [
    [shape, 'legacy-shape'],
    [paint, 'legacy-paint'],
    [points, 'legacy-points'],
    [motif, 'legacy-motif'],
    [instances, 'legacy-instances'],
    [fill, 'legacy-fill'],
    [output, 'legacy-output'],
  ])
    item.id = id;
  motif.points.forEach((point, index) => (point.id = `legacy-motif-${index}`));
  shape.params.center.value = [32, 32];
  shape.params.size.value = [64, 64];
  paint.params.path.source = shape.id;
  paint.params.color.value = { hex: '#000000', alpha: 0.5 };
  points.params.path.source = paint.id;
  points.params.pitch.value = 16;
  points.params.minScale.value = 0.25;
  points.params.maxScale.value = 1.25;
  instances.params.path.source = motif.id;
  instances.params.points.source = points.id;
  fill.params.path.source = instances.id;
  output.params.path.source = fill.id;
  layer.nodes = [shape, paint, points, motif, instances, fill, output];
  layer.output = output.id;
  return { doc, layer, points, instances };
}
