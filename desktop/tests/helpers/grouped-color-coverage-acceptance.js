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

function groupedDocument(kind) {
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
      other: measured.other.contribution,
      reference,
      painted: measured.paintedPixelEquivalent,
      alpha,
      unchanged: serialize(doc) === before,
    });
  }
  return results;
}
