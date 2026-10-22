import fs from 'node:fs';
const assets = JSON.parse(
  fs.readFileSync(new URL('../../examples/image-luminance-playground.json', import.meta.url)),
).imageAssets;
export const luminanceTypes = [
  'Halftone',
  'Ordered Dither',
  'Line Screen',
  'Error Diffusion Dither',
  'Luminance Instances',
];
// Fixed node identities let the two released hash recipes be captured independently.
export function remapFixture(core, type = 'Halftone', input = 'vector', route = 'convert') {
  const doc = core.document(),
    layer = doc.layers[0],
    shape = core.node(input === 'image' ? 'Image' : 'Rectangle'),
    fill = core.node('Fill'),
    effect = core.node(type),
    motif = core.node('Rectangle'),
    convert = core.node('Convert to Path'),
    remap = core.node('Path Remap'),
    paint = core.node('Fill'),
    output = core.node('Output');
  doc.schemaVersion = 36;
  for (const [name, item] of Object.entries({
    shape,
    fill,
    effect,
    motif,
    convert,
    remap,
    paint,
    output,
  }))
    item.id = `compat-${name}`;
  if (input === 'image') {
    doc.imageAssets = structuredClone(assets);
    if (route === 'tone-control')
      doc.imageAssets = {
        'tone-mid-gray': {
          name: 'mid-gray.png',
          mime: 'image/png',
          width: 1,
          height: 1,
          data: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4AWNoaGj4DwAFhAKAfr3l1AAAAABJRU5ErkJggg==',
        },
      };
    shape.asset = Object.keys(doc.imageAssets)[0];
    shape.params.position.value = [0, 0];
    shape.params.size.value = [16, 16];
    effect.params.path.source = shape.id;
  } else {
    shape.params.center.value = [8, 8];
    shape.params.size.value = [16, 16];
    fill.params.path.source = shape.id;
    fill.params.color.value = { hex: '#000000', alpha: 0.5 };
    effect.params.path.source = fill.id;
  }
  effect.params.pitch.value = 8;
  if (effect.params.dotSize) effect.params.dotSize.value = 8;
  motif.params.center.value = [0, 0];
  motif.params.size.value = [2, 3];
  if (type === 'Luminance Instances') effect.params.motif.source = motif.id;
  convert.params.path.source = effect.id;
  layer.nodes = [shape, ...(input === 'vector' ? [fill] : []), effect, motif, convert];
  let source = convert;
  const append = (type, key, upstream = source) => {
    const item = core.node(type);
    item.id = `compat-route-${layer.nodes.length}`;
    item.params[key].source = upstream.id;
    layer.nodes.push(item);
    source = item;
    return item;
  };
  if (route === 'transform') append('Transform', 'path').params.offset.value = [7, -2];
  if (route === 'bypass') append('Reverse Path', 'path').bypassed = true;
  if (route === 'closure') append('Set Closure', 'path');
  if (route === 'offset') append('Closed Path Offset', 'path').params.amount.value = 1;
  if (route === 'zero-offset') append('Closed Path Offset', 'path').params.amount.value = 0;
  if (route === 'chain') {
    append('Fill', 'path').params.color.value = { hex: '#000000', alpha: 0.5 };
    append('Halftone', 'path').params.pitch.value = 8;
    append('Convert to Path', 'path');
  }
  if (route === 'instances') {
    const targets = source;
    append('Instance on Points', 'path', motif).params.points.source = targets.id;
  }
  if (route === 'painted-transform') {
    append('Fill', 'path');
    const transform = append('Transform', 'path');
    transform.paintMode = true;
    transform.params.path.type = 'PaintedPath';
    append('Convert to Path', 'path');
  }
  if (route === 'painted-join') {
    append('Fill', 'path');
    const join = append('Join Geometry', 'a');
    join.paintMode = true;
    for (const q of Object.values(join.params)) q.type = 'PaintedPath';
    append('Convert to Path', 'path');
  }
  if (route === 'literal') effect.params.pitch.expression = '8';
  if (route === 'literal-math') effect.params.pitch.expression = 'min(16 / 2, 8) + sin(0)';
  if (route === 'literal-base') effect.params.pitch.expression = 'value';
  if (route === 'expression') {
    const value = core.node('Value');
    value.id = 'compat-expression-source';
    value.params.value.value = 8;
    layer.nodes.push(value);
    effect.params.pitch.expression = core.parameterReferenceExpression(value.id, 'value');
  }
  remap.params.path.source = source.id;
  paint.params.path.source = remap.id;
  output.params.path.source = paint.id;
  layer.nodes.push(remap, paint, output);
  layer.output = output.id;
  return { doc, layer, shape, fill, effect, motif, convert, source, remap, output };
}

export function nestedFingerprintFixture(core, expression = false) {
  const doc = core.document(),
    source = doc.layers[0],
    target = core.layer('Target');
  doc.schemaVersion = 36;
  source.id = 'source';
  target.id = 'target';
  const shape = core.node('Rectangle'),
    fill = core.node('Fill'),
    effect = core.node('Halftone'),
    convert = core.node('Convert to Path'),
    transform = core.node('Transform'),
    closure = core.node('Set Closure'),
    offset = core.node('Closed Path Offset'),
    paint = core.node('Fill'),
    output = core.node('Output'),
    reference = core.node('Composition Reference'),
    paintedTransform = core.node('Transform'),
    paintedJoin = core.node('Join Geometry'),
    targetOutput = core.node('Output'),
    value = core.node('Value');
  [
    shape,
    fill,
    effect,
    convert,
    transform,
    closure,
    offset,
    paint,
    output,
    reference,
    paintedTransform,
    paintedJoin,
    targetOutput,
    value,
  ].forEach((node, i) => {
    node.id = `n${i}`;
  });
  shape.params.center.value = [8, 8];
  shape.params.size.value = [16, 16];
  fill.params.color.value = { hex: '#000000', alpha: 1 };
  transform.params.offset.value = [2, 3];
  effect.params.pitch.value = value.params.value.value = 8;
  if (expression)
    effect.params.pitch.expression = core.parameterReferenceExpression(value.id, 'value');
  offset.params.amount.value = 1;
  source.nodes = [shape, fill, effect, convert, transform, closure, offset, paint, output, value];
  source.output = output.id;
  for (const [from, to] of [
    [shape, fill],
    [fill, effect],
    [effect, convert],
    [convert, transform],
    [transform, closure],
    [closure, offset],
    [offset, paint],
    [paint, output],
  ])
    core.connect(source, from.id, to.id, 'path');
  reference.compositionReference = { compositionId: source.id };
  target.nodes = [reference, paintedTransform, paintedJoin, targetOutput];
  target.output = targetOutput.id;
  core.connect(target, reference.id, paintedTransform.id, 'path');
  core.connect(target, reference.id, paintedJoin.id, 'a');
  core.connect(target, paintedTransform.id, paintedJoin.id, 'b');
  core.connect(target, paintedJoin.id, targetOutput.id, 'path');
  doc.layers.push(target);
  return {
    doc,
    sourceIds: [effect, convert, transform, closure, offset, paint, output].map((n) => n.id),
    targetIds: [reference, paintedTransform, paintedJoin, targetOutput].map((n) => n.id),
  };
}
