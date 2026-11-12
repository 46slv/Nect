import { blankDocument, node, connect, exportScene, exportSVG, serialize } from '../../src/core.js';
import { colorCoveragePasses } from '../../src/color-coverage-passes.js';
import { sampleColorCoverage } from '../../src/color-coverage-sampling.js';
import { rasterizeCoverageAlpha } from '../../src/color-coverage-raster.js';

// Distinct RGB primaries let the browser's production rendering serve as an
// independent per-slot source-over oracle, including overlap and antialiasing.
async function rgba(svg, width, height) {
  const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
  try {
    const image = new Image();
    image.src = url;
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext('2d', { willReadFrequently: true });
    context.drawImage(image, 0, 0);
    return context.getImageData(0, 0, width, height).data;
  } finally {
    URL.revokeObjectURL(url);
  }
}

export function groupedDocument(kind) {
  const doc = blankDocument();
  doc.width = 64;
  doc.height = 48;
  doc.schemaVersion = 54;
  doc.colorSlots = [
    { id: 'fill_red', name: 'Fill', value: { hex: '#ff0000', alpha: 0.5 } },
    { id: 'stroke_blue', name: 'Stroke', value: { hex: '#0000ff', alpha: 0.75 } },
  ];
  const layer = doc.layers[0],
    output = layer.nodes[0],
    source = node('Imported Path'),
    fill = node('Fill'),
    stroke = node('Stroke');
  // Two contours in one group form a hole; splitting them would fill the hole.
  source.rawPath = 'M4 4H40V36H4Z M12 12V24H20V12Z';
  fill.params.color.colorSlotId = 'fill_red';
  stroke.params.color.colorSlotId = 'stroke_blue';
  stroke.params.width.value = 6;
  let grouped;
  if (kind === 'join') {
    const second = node('Imported Path'),
      join = node('Join Geometry');
    second.rawPath = 'M24 8H56V40H24Z';
    layer.nodes = [source, second, join, fill, stroke, output];
    connect(layer, source.id, join.id, 'a');
    connect(layer, second.id, join.id, 'b');
    grouped = join;
  } else {
    const repeat = node('Geometry Repeater');
    repeat.operation = 'ae-2d';
    repeat.params.count.value = 2;
    repeat.params.position.value = [20, 4];
    layer.nodes = [source, repeat, fill, stroke, output];
    connect(layer, source.id, repeat.id, 'path');
    grouped = repeat;
  }
  connect(layer, grouped.id, fill.id, 'path');
  connect(layer, fill.id, stroke.id, 'path');
  connect(layer, stroke.id, output.id, 'path');
  return doc;
}

export async function checkGroupedColorCoverage() {
  const results = [];
  for (const kind of ['join', 'repeater']) {
    const doc = groupedDocument(kind);
    const before = serialize(doc);
    const scene = exportScene(doc);
    const passes = colorCoveragePasses(scene, doc);
    const measured = await sampleColorCoverage(
      ['fill_red', 'stroke_blue'],
      passes,
      doc,
      rasterizeCoverageAlpha,
    );
    const pixels = await rgba(exportSVG(doc), doc.width, doc.height);
    const reference = [0, 0];
    let alpha = 0;
    for (let index = 0; index < pixels.length; index += 4) {
      const coverage = pixels[index + 3] / 255;
      reference[0] += (pixels[index] / 255) * coverage;
      reference[1] += (pixels[index + 2] / 255) * coverage;
      alpha += coverage;
    }
    results.push({
      kind,
      order: passes.map((pass) => [pass.paint, pass.slotId]),
      measured: measured.slots.map((slot) => slot.contribution),
      percentages: measured.slots.map((slot) => slot.percent),
      other: measured.other.contribution,
      reference,
      painted: measured.paintedPixelEquivalent,
      alpha,
      unchanged: serialize(doc) === before,
    });
  }
  return results;
}

export function groupedGradientDocument(kind, gradientType) {
  const doc = groupedDocument(kind),
    layer = doc.layers[0],
    previous = layer.nodes.find((n) => n.type === 'Fill'),
    stroke = layer.nodes.find((n) => n.type === 'Stroke'),
    gradient = node(gradientType === 'linear' ? 'Linear Gradient Fill' : 'Radial Gradient Fill');
  doc.colorSlots = [
    { id: 'start_red', name: 'Start', value: { hex: '#ff0000', alpha: 0.25 } },
    { id: 'end_green', name: 'End', value: { hex: '#00ff00', alpha: 0.75 } },
    { id: 'stroke_blue', name: 'Stroke', value: { hex: '#0000ff', alpha: 0.6 } },
  ];
  gradient.params.startColor.colorSlotId = 'start_red';
  gradient.params.endColor.colorSlotId = 'end_green';
  if (gradientType === 'linear') {
    gradient.params.start.value = [0, 0];
    gradient.params.end.value = [64, 0];
  } else {
    gradient.params.center.value = [32, 24];
    gradient.params.radius.value = 30;
  }
  layer.nodes.push(gradient);
  connect(layer, previous.params.path.source, gradient.id, 'path');
  connect(layer, gradient.id, stroke.id, 'path');
  layer.nodes.splice(layer.nodes.indexOf(previous), 1);
  return doc;
}

export async function checkGroupedGradientCoverage() {
  const results = [];
  for (const [kind, gradientType] of [
    ['join', 'linear'],
    ['join', 'radial'],
    ['repeater', 'linear'],
    ['repeater', 'radial'],
  ]) {
    const doc = groupedGradientDocument(kind, gradientType),
      before = serialize(doc),
      scene = exportScene(doc),
      passes = colorCoveragePasses(scene, doc),
      measured = await sampleColorCoverage(
        ['start_red', 'end_green', 'stroke_blue'],
        passes,
        doc,
        rasterizeCoverageAlpha,
      ),
      pixels = await rgba(exportSVG(doc), doc.width, doc.height),
      reference = [0, 0, 0];
    const wrongOrder = await sampleColorCoverage(
      ['start_red', 'end_green', 'stroke_blue'],
      [passes[0], passes[2], passes[1], passes[3]],
      doc,
      rasterizeCoverageAlpha,
    );
    let alpha = 0;
    for (let index = 0; index < pixels.length; index += 4) {
      const coverage = pixels[index + 3] / 255;
      for (let channel = 0; channel < 3; channel++)
        reference[channel] += (pixels[index + channel] / 255) * coverage;
      alpha += coverage;
    }
    results.push({
      kind,
      gradientType,
      order: passes.map((pass) => ({
        paint: pass.paint,
        slotId: pass.slotId,
        ...(pass.split ? { start: pass.split.startSlotId, end: pass.split.endSlotId } : {}),
      })),
      measured: measured.slots.map((slot) => slot.contribution),
      wrongOrder: wrongOrder.slots.map((slot) => slot.contribution),
      percentages: measured.slots.map((slot) => slot.percent),
      other: measured.other.contribution,
      reference,
      painted: measured.paintedPixelEquivalent,
      alpha,
      unchanged: serialize(doc) === before,
    });
  }
  return results;
}
