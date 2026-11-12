import {
  blankDocument,
  node,
  connect,
  deserialize,
  exportScene,
  exportSVG,
  renderElement,
  serialize,
} from '../../src/core.js';
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

async function premultipliedRgba(svg, width, height) {
  const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
  try {
    const image = new Image();
    image.src = url;
    await image.decode();
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext('2d', {
      willReadFrequently: true,
      colorType: 'float16',
      colorSpace: 'srgb',
    });
    const attributes = context.getContextAttributes?.();
    if (attributes?.colorType !== 'float16' || attributes.colorSpace !== 'srgb')
      throw Error('Independent RGB oracle requires an sRGB float16 canvas');
    context.drawImage(image, 0, 0);
    const data = context.getImageData(0, 0, width, height, {
      pixelFormat: 'rgba-float16',
      colorSpace: 'srgb',
    });
    if (
      data.pixelFormat !== 'rgba-float16' ||
      data.colorSpace !== 'srgb' ||
      typeof globalThis.Float16Array !== 'function' ||
      !(data.data instanceof globalThis.Float16Array)
    )
      throw Error('Independent RGB oracle requires actual rgba-float16 readback');
    const result = new Float32Array(width * height * 4);
    for (let index = 0; index < result.length; index += 4) {
      const alpha = data.data[index + 3];
      for (let channel = 0; channel < 3; channel++)
        result[index + channel] = data.data[index + channel] * alpha;
      result[index + 3] = alpha - result[index] - result[index + 1] - result[index + 2];
    }
    return result;
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

const angularStops = ['startColor', 'color1', 'color2', 'endColor'];
const angularIds = ['stop_red', 'stop_green', 'stop_blue', 'stop_black'];
const angularColors = ['#ff0000', '#00ff00', '#0000ff', '#000000'];
const angularAlpha = [0.25, 0.5, 0.75, 1];

export function groupedAngularDocument(
  kind,
  { equalHex = false, unbound = false, sameId = false, zeroStop = false } = {},
) {
  const doc = groupedDocument(kind),
    layer = doc.layers[0],
    previous = layer.nodes.find((n) => n.type === 'Fill'),
    stroke = layer.nodes.find((n) => n.type === 'Stroke'),
    sources = layer.nodes.filter((n) => n.type === 'Imported Path'),
    gradient = node('Angular Gradient Fill');
  // Fractional boundaries exercise AA. The opposite-winding inner contour is a hole.
  sources[0].rawPath = 'M3.25 4.5L42.75 9.25L33.25 42.5L4.5 34Z M12.5 12.5V24.5H21.5V12.5Z';
  if (sources[1]) sources[1].rawPath = 'M21.25 7.75L60.5 11.5L55.75 44.25L24.5 37.5Z';
  doc.colorSlots = angularIds.map((id, index) => ({
    id,
    name: id,
    value: {
      hex: equalHex ? '#789abc' : angularColors[index],
      alpha: zeroStop && index === 0 ? 0 : angularAlpha[index],
    },
  }));
  doc.colorSlots.push({
    id: 'stroke_black',
    name: 'Stroke',
    value: { hex: '#000000', alpha: 0.6 },
  });
  angularStops.forEach((key, index) => {
    gradient.params[key].colorSlotId = sameId ? angularIds[0] : angularIds[index];
  });
  if (unbound) {
    delete gradient.params.color1.colorSlotId;
    gradient.params.color1.value = {
      hex: equalHex ? '#789abc' : angularColors[1],
      alpha: angularAlpha[1],
    };
  }
  gradient.params.center.value = [8.5, 24.5];
  gradient.params.angle.value = -47;
  gradient.params.clockwise.value = false;
  gradient.params.position1.value = 0.17;
  gradient.params.position2.value = 0.73;
  stroke.params.color.colorSlotId = 'stroke_black';
  layer.nodes.push(gradient);
  connect(layer, previous.params.path.source, gradient.id, 'path');
  connect(layer, gradient.id, stroke.id, 'path');
  layer.nodes.splice(layer.nodes.indexOf(previous), 1);
  return doc;
}

export async function checkGroupedAngularCoverage() {
  const results = [];
  for (const options of [
    { kind: 'join' },
    { kind: 'repeater' },
    { kind: 'join', equalHex: true, unbound: true },
    { kind: 'join', sameId: true },
    { kind: 'join', zeroStop: true },
  ]) {
    const { kind, ...variant } = options,
      doc = groupedAngularDocument(kind, variant),
      before = serialize(doc),
      scene = exportScene(doc),
      passes = colorCoveragePasses(scene, doc),
      ids = [...angularIds, 'stroke_black'],
      captured = [],
      measured = await sampleColorCoverage(ids, passes, doc, async (svg, size) => {
        const decoded = await rasterizeCoverageAlpha(svg, size);
        captured.push(Float32Array.from(decoded));
        return decoded;
      }),
      wrongOrder = await sampleColorCoverage(
        ids,
        [passes[0], passes[2], passes[1], passes[3]],
        doc,
        rasterizeCoverageAlpha,
      );
    // The independent RGB oracle uses the same geometry and alpha but distinct
    // primaries; black contains the fourth stop and black stroke. It does not
    // infer stop IDs from the original scene's display colors.
    const referenceDoc = deserialize(before);
    for (let index = 0; index < angularIds.length; index++)
      referenceDoc.colorSlots.find((slot) => slot.id === angularIds[index]).value.hex =
        angularColors[index];
    if (variant.unbound)
      referenceDoc.layers[0].nodes.find(
        (n) => n.type === 'Angular Gradient Fill',
      ).params.color1.value.hex = angularColors[1];
    const referenceSvg = exportSVG(referenceDoc),
      pixels = await rgba(referenceSvg, doc.width, doc.height),
      referencePixels = await premultipliedRgba(referenceSvg, doc.width, doc.height),
      reference = [0, 0, 0, 0];
    let alpha = 0;
    for (let index = 0; index < pixels.length; index += 4) {
      const coverage = pixels[index + 3] / 255;
      const rgb = [0, 1, 2].map((channel) => (pixels[index + channel] / 255) * coverage);
      for (let channel = 0; channel < 3; channel++) reference[channel] += rgb[channel];
      reference[3] += coverage - rgb[0] - rgb[1] - rgb[2];
      alpha += coverage;
    }
    const full = await rasterizeCoverageAlpha(exportSVG(doc), doc),
      composite = new Float32Array(full.length);
    const pixelMasses = Array.from({ length: 4 }, () => new Float32Array(full.length));
    let capturedIndex = 0;
    for (const pass of passes) {
      const paint = captured[capturedIndex++],
        bases = pass.contributions?.map(() => captured[capturedIndex++]);
      for (let index = 0; index < paint.length; index++) {
        composite[index] = paint[index] + composite[index] * (1 - paint[index]);
        for (const mass of pixelMasses) mass[index] *= 1 - paint[index];
        if (bases) {
          const sum = bases.reduce((total, basis) => total + basis[index], 0);
          for (let stop = 0; stop < bases.length; stop++)
            pixelMasses[stop][index] += sum > 0 ? (bases[stop][index] / sum) * paint[index] : 0;
        } else pixelMasses[pass.paint === 'fill' ? 0 : 3][index] += paint[index];
      }
    }
    // Solid opaque rendering isolates the shape's AA from gradient alpha.
    const geometry = {
        ...scene[0].value,
        fill: { hex: '#ffffff', alpha: 1 },
        stroke: undefined,
      },
      geometrySvg = `<svg xmlns="http://www.w3.org/2000/svg" width="${doc.width}" height="${doc.height}" viewBox="0 0 ${doc.width} ${doc.height}">${renderElement(geometry, scene[0].layer)}</svg>`,
      geometryAlpha = await rasterizeCoverageAlpha(geometrySvg, doc);
    let maxCompositeError = 0,
      maxAAError = 0,
      maxStopPixelError = 0,
      maxStopAAError = 0,
      antialiasedPixels = 0;
    for (let index = 0; index < full.length; index++) {
      const error = Math.abs(full[index] - composite[index]);
      maxCompositeError = Math.max(maxCompositeError, error);
      const stopError = Math.max(
        ...pixelMasses.map((mass, stop) =>
          Math.abs(mass[index] - referencePixels[index * 4 + stop]),
        ),
      );
      maxStopPixelError = Math.max(maxStopPixelError, stopError);
      if (geometryAlpha[index] > 0 && geometryAlpha[index] < 1) {
        antialiasedPixels++;
        maxAAError = Math.max(maxAAError, error);
        maxStopAAError = Math.max(maxStopAAError, stopError);
      }
    }
    const first = scene[0].value.contours[0].group,
      firstValue = {
        ...scene[0].value,
        contours: scene[0].value.contours.filter((contour) => contour.group === first),
      },
      flatValue = {
        ...firstValue,
        contours: firstValue.contours.map(({ group: _group, ...contour }) => contour),
      },
      oneGroup = [{ ...scene[0], value: firstValue }],
      flat = [{ ...scene[0], value: flatValue }];
    const oneGroupSvg = `<svg xmlns="http://www.w3.org/2000/svg" width="${doc.width}" height="${doc.height}" viewBox="0 0 ${doc.width} ${doc.height}">${renderElement(firstValue, scene[0].layer)}</svg>`,
      flatSvg = `<svg xmlns="http://www.w3.org/2000/svg" width="${doc.width}" height="${doc.height}" viewBox="0 0 ${doc.width} ${doc.height}">${renderElement(flatValue, scene[0].layer)}</svg>`,
      oneGroupAlpha = await rasterizeCoverageAlpha(oneGroupSvg, doc),
      flatAlpha = await rasterizeCoverageAlpha(flatSvg, doc);
    let singleGroupMaxError = 0;
    for (let index = 0; index < oneGroupAlpha.length; index++)
      singleGroupMaxError = Math.max(
        singleGroupMaxError,
        Math.abs(oneGroupAlpha[index] - flatAlpha[index]),
      );
    results.push({
      ...options,
      order: passes.map((pass) => ({
        paint: pass.paint,
        slotId: pass.slotId,
        stops: pass.contributions?.map((stop) => stop.slotId),
      })),
      measured: measured.slots.map((slot) => slot.contribution),
      wrongOrder: wrongOrder.slots.map((slot) => slot.contribution),
      percentages: measured.slots.map((slot) => slot.percent),
      other: measured.other.contribution,
      reference,
      painted: measured.paintedPixelEquivalent,
      alpha,
      maxCompositeError,
      maxAAError,
      maxStopPixelError,
      maxStopAAError,
      antialiasedPixels,
      holeAlpha: geometryAlpha[18 * doc.width + 16],
      oneGroupMatchesFlat:
        JSON.stringify(colorCoveragePasses(oneGroup, doc)) ===
        JSON.stringify(colorCoveragePasses(flat, doc)),
      singleGroupMaxError,
      unchanged: serialize(doc) === before,
    });
  }
  return results;
}
