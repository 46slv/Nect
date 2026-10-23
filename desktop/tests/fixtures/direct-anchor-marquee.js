import { blankDocument, blankLayer, node, point, connect, parameter } from '../../src/core.js';

export function directMarqueeFixture({ transformed = false, locked = false, driven = false } = {}) {
  const doc = blankDocument(),
    layer = doc.layers[0],
    curve = node('Curve', 40, 40),
    transform = node('Transform', 350, 40),
    fill = node('Fill', 660, 40),
    out = layer.nodes[0];
  doc.name = 'Direct anchor rectangle';
  doc.width = 600;
  doc.height = 500;
  doc.schemaVersion = 49;
  doc.editor.snap = false;
  layer.id = 'anchor-marquee-layer';
  curve.id = 'anchor-marquee-source';
  transform.id = 'anchor-marquee-transform';
  fill.id = 'anchor-marquee-fill';
  out.id = 'anchor-marquee-output';
  layer.output = out.id;
  curve.points = [
    [100, 100],
    [200, 100],
    [210, 200],
    [110, 210],
  ].map((xy, i) => {
    const p = point(...xy);
    p.id = `anchor-${i}`;
    return p;
  });
  curve.points[0].handleB.value = [50, -40];
  curve.points[1].position.locked = locked;
  if (driven) curve.points[1].position.expression = 'value; value';
  curve.params.closed.value = true;
  Object.assign(transform.params, {
    uniformScale: { ...parameter(transform, 'uniformScale'), value: false },
    scaleWidth: { ...parameter(transform, 'scaleWidth'), value: 1.5 },
    position: { ...parameter(transform, 'position'), value: [300, 240] },
  });
  transform.params.anchor.value = [150, 150];
  transform.params.scale.value = 0.7;
  transform.params.rotation.value = 35;
  layer.nodes = [curve, ...(transformed ? [transform] : []), fill, out];
  for (const [i, n] of layer.nodes.entries()) {
    n.position = [40 + i * 310, 40];
    n.collapsed = true;
  }
  connect(layer, curve.id, transformed ? transform.id : fill.id, 'path');
  if (transformed) connect(layer, transform.id, fill.id, 'path');
  connect(layer, fill.id, out.id, 'path');
  // A second eligible source inside some rectangles must never join the set.
  const other = blankLayer('Other path'),
    otherCurve = node('Curve');
  otherCurve.points = [
    [140, 140],
    [160, 140],
    [160, 160],
    [140, 160],
  ].map((p) => point(...p));
  otherCurve.collapsed = true;
  other.nodes.unshift(otherCurve);
  connect(other, otherCurve.id, other.output, 'path');
  doc.layers.push(other);
  return { doc, layer, curve, transform, fill, out, other, otherCurve };
}
