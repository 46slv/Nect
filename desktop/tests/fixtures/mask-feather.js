import { node, parameter, connect, layer as createLayer } from '../../src/core.js';
import { keyerMaskExample } from '../../scripts/keyer-mask-example.js';

export function featherFixture(
  feather = 0,
  { invert = false, postTransform = null, shapeTransform = false, largeShape = false } = {},
) {
  const doc = keyerMaskExample(false),
    layer = doc.layers[0],
    byId = (id) => layer.nodes.find((n) => n.id === id),
    mask = byId('example-mask'),
    shape = byId('example-mask-shape'),
    out = byId('example-output');
  doc.name = 'Mask feather comparison';
  shape.params.size.value = largeShape ? [1000, 1000] : [40, 40];
  mask.params.invert.value = invert;
  if (feather !== 0) {
    mask.params.feather = { ...parameter(mask, 'feather'), value: feather };
    doc.schemaVersion = 50;
  }
  if (shapeTransform) {
    const t = node('Transform');
    t.id = 'feather-shape-transform';
    t.params.anchor.value = [40, 40];
    t.params.rotation.value = 30;
    t.params.scale.value = 0.7;
    t.params.position = { ...parameter(t, 'position'), value: [40, 40] };
    t.params.uniformScale = { ...parameter(t, 'uniformScale'), value: false };
    t.params.scaleWidth = { ...parameter(t, 'scaleWidth'), value: 1.5 };
    layer.nodes.splice(layer.nodes.indexOf(mask), 0, t);
    connect(layer, shape.id, t.id, 'path');
    connect(layer, t.id, mask.id, 'mask');
  }
  if (postTransform) {
    const t = node('Transform');
    t.id = 'feather-appearance-transform';
    t.params.anchor.value = [0, 0];
    t.params.position = {
      ...parameter(t, 'position'),
      value: postTransform === 'rotate' ? [90, 0] : [10, 10],
    };
    t.params.uniformScale = { ...parameter(t, 'uniformScale'), value: false };
    t.params.scaleWidth = {
      ...parameter(t, 'scaleWidth'),
      value: postTransform === 'rotate' ? 1 : 2,
    };
    t.params.scale.value = 1;
    t.params.rotation.value = postTransform === 'rotate' ? 90 : 0;
    layer.nodes.splice(layer.nodes.indexOf(out), 0, t);
    connect(layer, mask.id, t.id, 'path');
    connect(layer, t.id, out.id, 'path');
    doc.width = postTransform === 'rotate' ? 100 : 180;
    doc.height = 100;
  }
  return {
    doc,
    layer,
    mask,
    shape,
    out,
    image: byId('example-image'),
    keyer: byId('example-keyer'),
  };
}

export function strokeFeatherFixture(feather = 4) {
  const f = featherFixture(feather, { largeShape: true }),
    source = createLayer('Stroke source'),
    rect = node('Rectangle'),
    stroke = node('Stroke'),
    out = node('Output'),
    ref = node('Composition Reference');
  source.id = 'feather-stroke-source';
  rect.id = 'feather-stroke-rectangle';
  stroke.id = 'feather-stroke';
  out.id = 'feather-stroke-output';
  ref.id = 'feather-stroke-reference';
  rect.params.center.value = [40, 40];
  rect.params.size.value = [40, 40];
  stroke.params.width.value = 12;
  stroke.params.color.value = { hex: '#336699', alpha: 0.5 };
  source.nodes = [rect, stroke, out];
  source.output = out.id;
  source.visible = false;
  connect(source, rect.id, stroke.id, 'path');
  connect(source, stroke.id, out.id, 'path');
  ref.compositionReference = { compositionId: source.id };
  f.layer.nodes = [ref, f.shape, f.mask, f.out];
  f.doc.layers.unshift(source);
  connect(f.layer, ref.id, f.mask.id, 'path');
  return { ...f, source, ref, stroke };
}
