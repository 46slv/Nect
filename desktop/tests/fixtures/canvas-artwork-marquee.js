import { blankDocument, blankLayer, node, point, connect } from '../../src/core.js';
export function marqueeFixture() {
  const doc = blankDocument();
  doc.width = 400;
  doc.height = 240;
  doc.schemaVersion = 33;
  const make = (name, type, center) => {
    const layer = blankLayer(name),
      shape = node(type, 40, 50),
      fill = node('Fill', 350, 50);
    shape.collapsed = fill.collapsed = true;
    const out = layer.nodes[0];
    out.position = [660, 50];
    out.collapsed = true;
    layer.nodes = [shape, fill, out];
    if (shape.params.center) shape.params.center.value = center;
    if (type === 'Rectangle') shape.params.size.value = [20, 20];
    if (type === 'Circle') shape.params.radius.value = 10;
    if (type === 'Star') {
      shape.params.outerRadius.value = 20;
      shape.params.innerRadius.value = 10;
    }
    connect(layer, shape.id, fill.id, 'path');
    connect(layer, fill.id, out.id, 'path');
    return { layer, shape, fill, out };
  };
  const a = make('A', 'Rectangle', [50, 50]),
    b = make('Locked B', 'Circle', [115, 50]),
    path = make('Owned path', 'Curve'),
    hidden = make('Hidden source', 'Rectangle', [260, 50]),
    generated = make('Generated star', 'Star', [200, 150]);
  path.shape.points = [point(160, 40), point(180, 40), point(180, 60), point(160, 60)];
  b.layer.locked = true;
  hidden.layer.visible = false;
  const reference = blankLayer('Reference'),
    ref = node('Composition Reference', 40, 50);
  ref.collapsed = true;
  ref.compositionReference = { compositionId: hidden.layer.id };
  reference.nodes.unshift(ref);
  connect(reference, ref.id, reference.output, 'path');
  doc.layers = [a.layer, b.layer, path.layer, hidden.layer, generated.layer, reference];
  return { doc, a, b, path, hidden, generated, reference, ref };
}
