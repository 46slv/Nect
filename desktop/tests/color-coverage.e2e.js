// Experimental acceptance: real SVG decoding/canvas alpha, never RGB-to-ID inference.
// Run against the Vite dev server: BASE_URL=... CHROMIUM_PATH=... node tests/color-coverage.e2e.js
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import { chromium } from 'playwright-core';

const output = 'test-results/color-coverage';
await fs.mkdir(output, { recursive: true });
const browser = await chromium.launch({
  executablePath: process.env.CHROMIUM_PATH || '/usr/bin/chromium',
  headless: true,
  args: ['--no-sandbox', '--disable-dev-shm-usage'],
});
const errors = [];
try {
  const page = await browser.newPage();
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const results = await page.evaluate(async () => {
    const { blankDocument, layer, node, connect, exportScene, exportSVG, renderElement } =
      await import('/src/core.js');
    const { colorCoveragePasses } = await import('/src/color-coverage-passes.js');
    const { fixturePaintSlotId } = await import(
      '/tests/helpers/color-coverage-fixture-identity.js'
    );
    const { sampleColorCoverage } = await import('/src/color-coverage-sampling.js');
    const { rasterizeCoverageAlpha } = await import('/src/color-coverage-raster.js');
    const { summarizeCoverageRasters } = await import(
      '/tests/helpers/color-coverage-diagnostics.js'
    );
    const { orderedAngularReferenceSVG, isAngularReferenceInterior } = await import(
      '/tests/helpers/angular-coverage-reference.js'
    );
    const width = 64,
      height = 48,
      pixels = width * height;
    const slots = ['same_hex_a', 'same_hex_b', 'outline', 'background'];
    const raster = async (svg, channel = 'alpha') => {
      const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
      try {
        const image = new Image();
        image.src = url;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const floating = channel === 'premultiplied-red';
        const context = canvas.getContext('2d', {
          willReadFrequently: true,
          ...(floating ? { colorType: 'float16', colorSpace: 'srgb' } : {}),
        });
        const attributes = floating ? context.getContextAttributes?.() : null;
        if (floating && (attributes?.colorType !== 'float16' || attributes.colorSpace !== 'srgb'))
          throw Error('Independent diagnostic requires actual float16 sRGB canvas');
        context.drawImage(image, 0, 0, width, height);
        const data = context.getImageData(
          0,
          0,
          width,
          height,
          ...(floating ? [{ pixelFormat: 'rgba-float16', colorSpace: 'srgb' }] : []),
        );
        const rgba = data.data;
        if (
          floating &&
          (data.pixelFormat !== 'rgba-float16' ||
            data.colorSpace !== 'srgb' ||
            typeof globalThis.Float16Array !== 'function' ||
            !(rgba instanceof globalThis.Float16Array))
        )
          throw Error('Independent diagnostic requires actual rgba-float16 readback');
        return Array.from({ length: pixels }, (_, index) => {
          if (channel === 'reference-rgb') {
            const alpha = rgba[index * 4 + 3] / 255;
            const first = [0, 1, 2].map((component) => (rgba[index * 4 + component] / 255) * alpha);
            return [...first, alpha - first.reduce((sum, mass) => sum + mass, 0)];
          }
          return floating ? rgba[index * 4] * rgba[index * 4 + 3] : rgba[index * 4 + 3] / 255;
        });
      } finally {
        URL.revokeObjectURL(url);
      }
    };
    const cases = [
      { name: 'transparent', rects: [] },
      {
        name: 'same-hex-explicit-ids-and-other',
        rects: [
          { box: [4, 4, 28, 28], fill: 1, id: slots[0] },
          { box: [16, 8, 40, 32], fill: 0.5, id: slots[1] },
          { box: [44, 8, 56, 20], fill: 1 },
        ],
      },
      {
        name: 'clipped-translucent-fill-and-stroke',
        rects: [
          { box: [-8, 8, 24, 40], fill: 0.5, id: slots[0], stroke: 0.5 },
          { box: [16, 16, 48, 40], fill: 0.5, id: slots[1] },
        ],
      },
      {
        name: 'reversed-object-order',
        rects: [
          { box: [16, 8, 40, 32], fill: 0.5, id: slots[1] },
          { box: [4, 4, 28, 28], fill: 1, id: slots[0] },
        ],
      },
    ];
    cases.push(
      {
        name: 'gradient-same-hex-distinct-stop-ids',
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: {
              start: [4, 4],
              end: [60, 4],
              startId: slots[0],
              endId: slots[1],
              startAlpha: 1,
              endAlpha: 1,
            },
          },
        ],
      },
      {
        name: 'gradient-transparent-stop-hue-and-unbound-other',
        slotAlpha: { [slots[0]]: 0 },
        slotHex: { [slots[0]]: '#ff0000' },
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: {
              start: [4, 4],
              end: [60, 4],
              startId: slots[0],
              startAlpha: 0,
              endAlpha: 1,
            },
          },
        ],
      },
      {
        name: 'gradient-padded-diagonal-alpha-stroke-and-occlusion',
        slotAlpha: { [slots[0]]: 0.25, [slots[1]]: 0.75 },
        rects: [
          { box: [4, 4, 60, 44], fill: 1, id: slots[3] },
          {
            box: [-8, 8, 56, 40],
            gradient: {
              start: [20, 16],
              end: [40, 32],
              startId: slots[0],
              endId: slots[1],
              startAlpha: 0.25,
              endAlpha: 0.75,
            },
            stroke: 0.5,
          },
          { box: [32, 12, 60, 36], fill: 0.5, id: slots[2] },
        ],
      },
      {
        name: 'gradient-reversed-pad-same-id-antialiased-geometry',
        analytic: false,
        rects: [
          {
            box: [4, 4, 60, 44],
            rawPath: 'M4.25 4.5L60.75 17.25L18.5 43.75Z',
            gradient: {
              start: [44, 24],
              end: [20, 24],
              startId: slots[0],
              endId: slots[0],
              startAlpha: 1,
              endAlpha: 1,
            },
          },
        ],
      },
    );
    // Independent fixture oracle: no production paint renderer, mask generator,
    // coverage reducer or RGB matching. Forward source-over attenuates accumulated
    // per-ID mass once per paint (the production reducer walks backwards).
    const analyticalCoverage = (layers) => {
      const ids = [...slots, null],
        totals = ids.map(() => 0);
      for (let p = 0; p < pixels; p++) {
        const masses = ids.map(() => 0);
        for (const entry of layers) {
          for (let i = 0; i < masses.length; i++) masses[i] *= 1 - entry.alpha[p];
          const add = (id, amount) => (masses[ids.indexOf(id ?? null)] += amount);
          if (entry.contributions) {
            for (const stop of entry.contributions) add(stop.slotId, stop.alpha[p]);
          } else if (entry.split) {
            add(entry.split.startSlotId, entry.split.startAlpha[p]);
            add(entry.split.endSlotId, entry.alpha[p] - entry.split.startAlpha[p]);
          } else add(entry.slotId, entry.alpha[p]);
        }
        masses.forEach((mass, i) => (totals[i] += mass));
      }
      const total = totals.reduce((sum, mass) => sum + mass, 0);
      const rows = totals.map((contribution, i) => ({
        id: ids[i],
        contribution,
        percent: total ? (contribution / total) * 100 : 0,
      }));
      return { slots: rows.slice(0, -1), other: rows.at(-1) };
    };
    const radialOverlap = [
      { box: [4, 4, 60, 44], fill: 1, id: slots[3] },
      {
        box: [-8, 8, 56, 40],
        gradient: {
          kind: 'radial',
          center: [12.5, 24.5],
          radius: 25,
          startId: slots[0],
          endId: slots[1],
          startAlpha: 0.25,
          endAlpha: 0.75,
        },
        stroke: 0.5,
      },
      { box: [32, 12, 60, 36], fill: 0.5, id: slots[2] },
    ];
    cases.push(
      {
        name: 'radial-center-inside-same-hex-distinct-ids-and-pad',
        radialProbes: true,
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: {
              kind: 'radial',
              center: [32.5, 24.5],
              radius: 14,
              startId: slots[0],
              endId: slots[1],
              startAlpha: 1,
              endAlpha: 1,
            },
          },
        ],
      },
      {
        name: 'radial-center-outside-radius-intersects-path-and-pad',
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: {
              kind: 'radial',
              center: [-4, 24],
              radius: 40,
              startId: slots[0],
              endId: slots[1],
              startAlpha: 1,
              endAlpha: 1,
            },
          },
        ],
      },
      {
        name: 'radial-transparent-center-hue-and-unbound-outer-other',
        slotAlpha: { [slots[0]]: 0 },
        slotHex: { [slots[0]]: '#ff0000' },
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: {
              kind: 'radial',
              center: [32.5, 24.5],
              radius: 40,
              startId: slots[0],
              startAlpha: 0,
              endAlpha: 1,
            },
          },
        ],
      },
      {
        name: 'radial-unequal-alpha-clipping-stroke-and-occlusion',
        slotAlpha: { [slots[0]]: 0.25, [slots[1]]: 0.75 },
        rects: radialOverlap,
      },
      {
        name: 'radial-reversed-overlap-order',
        slotAlpha: { [slots[0]]: 0.25, [slots[1]]: 0.75 },
        rects: [radialOverlap[0], radialOverlap[2], radialOverlap[1]],
      },
      {
        name: 'radial-same-id-antialiased-clipped-triangle',
        rects: [
          {
            box: [-6, 8, 60, 44],
            rawPath: 'M-6.25 8.5L60.75 17.25L18.5 43.75Z',
            // Independent polygon coordinates, used only by the supersampled oracle.
            polygon: [
              [-6.25, 8.5],
              [60.75, 17.25],
              [18.5, 43.75],
            ],
            gradient: {
              kind: 'radial',
              center: [20, 24],
              radius: 18,
              startId: slots[0],
              endId: slots[0],
              startAlpha: 1,
              endAlpha: 1,
            },
          },
        ],
      },
      {
        name: 'radial-minimum-radius-unbound-stops-use-outer-pad',
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: {
              kind: 'radial',
              center: [-100, -100],
              radius: 0.0001,
              startAlpha: 0,
              endAlpha: 1,
            },
          },
        ],
      },
    );
    const radialAAFixture = cases.find(
      (fixture) => fixture.name === 'radial-same-id-antialiased-clipped-triangle',
    );
    cases.push({
      ...radialAAFixture,
      name: 'radial-distinct-ids-antialiased-clipped-triangle',
      rects: radialAAFixture.rects.map((rectangle) => ({
        ...rectangle,
        gradient: { ...rectangle.gradient, endId: slots[1] },
      })),
    });
    // Angular oracle is independent of the production sampler and SVG masks:
    // identify the 720 midpoint wedge from angle/direction, locate its adjacent
    // stops, then allocate premultiplied alpha_i * weight_i. No HEX identity.
    const angularKeys = ['startColor', 'color1', 'color2', 'endColor'];
    const angularMassAt = (g, x, y) => {
      const turn =
        (((((Math.atan2(y - g.center[1], x - g.center[0]) - (g.angle * Math.PI) / 180) *
          (g.clockwise ? 1 : -1)) /
          (2 * Math.PI)) %
          1) +
          1) %
        1;
      const t = (Math.floor(turn * 720) + 0.5) / 720;
      const positions = [0, g.position1, g.position2, 1];
      const right = positions.findIndex((position) => position >= t);
      const left = right - 1,
        fraction = (t - positions[left]) / (positions[right] - positions[left]);
      const masses = [0, 0, 0, 0];
      masses[left] = g.alphas[left] * (1 - fraction);
      masses[right] = g.alphas[right] * fraction;
      return masses;
    };
    const angular = (patch = {}) => ({
      kind: 'angular',
      center: [32.5, 24.5],
      angle: 0,
      clockwise: true,
      position1: 0.17,
      position2: 0.73,
      ids: [...slots],
      alphas: [1, 1, 1, 1],
      ...patch,
    });
    const angularOverlap = [
      { box: [4, 4, 60, 44], fill: 1, id: slots[3] },
      {
        box: [-8, 8, 56, 40],
        gradient: angular({
          center: [12.5, 24.5],
          angle: -47,
          clockwise: false,
          alphas: [0.25, 0.5, 0.75, 1],
        }),
        stroke: 0.75,
      },
      { box: [32, 12, 60, 36], fill: 0.75, id: slots[2] },
    ];
    const angularAlphas = Object.fromEntries(slots.map((id, i) => [id, [0.25, 0.5, 0.75, 1][i]]));
    const triangle = [
      [-3.25, 4.5],
      [60.75, 17.25],
      [18.5, 52.75],
    ];
    cases.push(
      {
        name: 'angular-center-inside-same-hex-four-distinct-ids',
        angularProbes: true,
        slotAlpha: Object.fromEntries(slots.map((id) => [id, 1])),
        rects: [{ box: [4, 4, 60, 44], gradient: angular() }],
      },
      {
        name: 'angular-center-outside-reverse-rotated-nonuniform-stops',
        slotAlpha: Object.fromEntries(slots.map((id) => [id, 1])),
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: angular({
              center: [-20, 18],
              angle: 123,
              clockwise: false,
              position1: 0.03,
              position2: 0.81,
            }),
          },
        ],
      },
      {
        name: 'angular-transparent-stop-zero-contribution-and-unbound-other',
        slotAlpha: { [slots[0]]: 0 },
        slotHex: { [slots[0]]: '#ff0000' },
        rects: [
          {
            box: [4, 4, 60, 44],
            gradient: angular({
              ids: [slots[0], slots[1], null, slots[3]],
              alphas: [0, 1, 1, 1],
              angle: 31,
            }),
          },
        ],
      },
      {
        name: 'angular-unequal-alpha-clipping-stroke-and-occlusion',
        slotAlpha: angularAlphas,
        rects: angularOverlap,
      },
      {
        name: 'angular-reversed-overlap-order',
        slotAlpha: angularAlphas,
        rects: [...angularOverlap].reverse(),
      },
      {
        name: 'angular-same-id-antialiased-clipped-triangle',
        rects: [
          {
            box: [-3.25, 4.5, 60.75, 52.75],
            rawPath: 'M-3.25 4.5L60.75 17.25L18.5 52.75Z',
            polygon: triangle,
            gradient: angular({ ids: Array(4).fill(slots[0]), angle: -179, clockwise: false }),
          },
        ],
      },
      {
        name: 'angular-distinct-ids-antialiased-clipped-triangle',
        slotAlpha: angularAlphas,
        rects: [
          {
            box: [-3.25, 4.5, 60.75, 52.75],
            rawPath: 'M-3.25 4.5L60.75 17.25L18.5 52.75Z',
            polygon: triangle,
            gradient: angular({ alphas: [0.25, 0.5, 0.75, 1], angle: -179, clockwise: false }),
          },
        ],
      },
      {
        name: 'angular-all-transparent',
        slotAlpha: Object.fromEntries(slots.map((id) => [id, 0])),
        rects: [{ box: [4, 4, 60, 44], gradient: angular({ alphas: [0, 0, 0, 0] }) }],
      },
    );
    const receipts = [];
    for (const fixture of cases) {
      const doc = blankDocument();
      doc.schemaVersion = 54;
      doc.colorSlots = slots.map((id) => ({
        id,
        name: id,
        value: {
          hex: fixture.slotHex?.[id] ?? '#336699',
          alpha:
            fixture.slotAlpha?.[id] ??
            (id === slots[2] ? 0.5 : (fixture.rects.find((r) => r.id === id)?.fill ?? 1)),
        },
      }));
      doc.width = width;
      doc.height = height;
      if (fixture.rects.length) doc.layers = [];
      const expectedOrder = [],
        analyticLayers = [];
      for (const [index, rectangle] of fixture.rects.entries()) {
        const l = layer(`Rectangle ${index}`),
          shape = node('Imported Path'),
          fill = node(
            rectangle.gradient?.kind === 'angular'
              ? 'Angular Gradient Fill'
              : rectangle.gradient?.kind === 'radial'
                ? 'Radial Gradient Fill'
                : rectangle.gradient
                  ? 'Linear Gradient Fill'
                  : 'Fill',
          ),
          stroke = node('Stroke'),
          out = node('Output');
        const [x0, y0, x1, y1] = rectangle.box;
        shape.rawPath = rectangle.rawPath ?? `M${x0} ${y0}H${x1}V${y1}H${x0}Z`;
        if (rectangle.gradient) {
          const g = rectangle.gradient;
          if (g.kind === 'angular') {
            for (const key of ['center', 'angle', 'clockwise', 'position1', 'position2'])
              fill.params[key].value = g[key];
          } else if (g.kind === 'radial') {
            fill.params.center.value = g.center;
            fill.params.radius.value = g.radius;
          } else {
            fill.params.start.value = g.start;
            fill.params.end.value = g.end;
          }
          for (const [key, id, alpha] of g.kind === 'angular'
            ? angularKeys.map((key, i) => [key, g.ids[i], g.alphas[i]])
            : [
                ['startColor', g.startId, g.startAlpha],
                ['endColor', g.endId, g.endAlpha],
              ]) {
            fill.params[key].value = { hex: '#0000ff', alpha };
            if (id) fill.params[key].colorSlotId = id;
          }
        } else {
          fill.params.color.value = { hex: '#336699', alpha: rectangle.fill };
          if (rectangle.id) fill.params.color.colorSlotId = rectangle.id;
        }
        l.nodes = [shape, fill];
        connect(l, shape.id, fill.id, 'path');
        let previous = fill;
        if (rectangle.stroke !== undefined) {
          stroke.params.color.value = { hex: '#336699', alpha: rectangle.stroke };
          stroke.params.color.colorSlotId = slots[2];
          stroke.params.width.value = 4;
          l.nodes.push(stroke);
          connect(l, fill.id, stroke.id, 'path');
          previous = stroke;
        }
        l.nodes.push(out);
        l.output = out.id;
        connect(l, previous.id, out.id, 'path');
        doc.layers.push(l);
        for (const paint of rectangle.stroke === undefined ? ['fill'] : ['fill', 'stroke']) {
          const slotId = fixturePaintSlotId(rectangle, paint, slots[2]);
          expectedOrder.push([l.id, paint, slotId]);
          const gradient = paint === 'fill' ? rectangle.gradient : null;
          const interpolationAt = (x, y) => {
            if (gradient.kind === 'radial')
              return Math.min(
                Math.hypot(x - gradient.center[0], y - gradient.center[1]) / gradient.radius,
                1,
              );
            const dx = gradient.end[0] - gradient.start[0],
              dy = gradient.end[1] - gradient.start[1];
            return Math.max(
              0,
              Math.min(
                1,
                ((x - gradient.start[0]) * dx + (y - gradient.start[1]) * dy) / (dx * dx + dy * dy),
              ),
            );
          };
          const interpolation = (p) =>
            interpolationAt((p % width) + 0.5, Math.floor(p / width) + 0.5);
          const alpha = Array.from({ length: pixels }, (_, p) => {
            const x = (p % width) + 0.5,
              y = Math.floor(p / width) + 0.5;
            const inside = (a, b, c, d) => x >= a && x < c && y >= b && y < d;
            return paint === 'fill'
              ? inside(x0, y0, x1, y1)
                ? gradient
                  ? gradient.kind === 'angular'
                    ? angularMassAt(gradient, x, y).reduce((sum, mass) => sum + mass, 0)
                    : gradient.startAlpha * (1 - interpolation(p)) +
                      gradient.endAlpha * interpolation(p)
                  : rectangle.fill
                : 0
              : inside(x0 - 2, y0 - 2, x1 + 2, y1 + 2) && !inside(x0 + 2, y0 + 2, x1 - 2, y1 - 2)
                ? rectangle.stroke
                : 0;
          });
          let startAlpha =
            gradient && gradient.kind !== 'angular'
              ? alpha.map((a, p) => a * (1 - interpolation(p)))
              : null;
          const contributions =
            gradient?.kind === 'angular'
              ? gradient.ids.map((slotId, i) => ({
                  slotId,
                  alpha: alpha.map((a, p) =>
                    a
                      ? angularMassAt(gradient, (p % width) + 0.5, Math.floor(p / width) + 0.5)[i]
                      : 0,
                  ),
                }))
              : null;
          if (rectangle.polygon && paint === 'fill') {
            // 16x16 subpixel integration checks actual AA and artboard clipping,
            // including a curved ramp crossing the polygon edge. No SVG decode.
            const triangle = rectangle.polygon;
            const inside = (x, y) => {
              const sides = triangle.map(([ax, ay], i) => {
                const [bx, by] = triangle[(i + 1) % triangle.length];
                return (bx - ax) * (y - ay) - (by - ay) * (x - ax);
              });
              return sides.every((v) => v >= 0) || sides.every((v) => v <= 0);
            };
            for (let p = 0; p < pixels; p++) {
              let full = 0,
                start = 0;
              const angularMasses = [0, 0, 0, 0];
              for (let sy = 0; sy < 16; sy++)
                for (let sx = 0; sx < 16; sx++) {
                  const x = (p % width) + (sx + 0.5) / 16,
                    y = Math.floor(p / width) + (sy + 0.5) / 16;
                  if (!inside(x, y)) continue;
                  if (contributions) {
                    const masses = angularMassAt(gradient, x, y);
                    masses.forEach((mass, i) => {
                      angularMasses[i] += mass / 256;
                      full += mass / 256;
                    });
                    continue;
                  }
                  const t = interpolationAt(x, y),
                    a = gradient.startAlpha * (1 - t) + gradient.endAlpha * t;
                  full += a / 256;
                  start += (a * (1 - t)) / 256;
                }
              alpha[p] = full;
              if (contributions)
                contributions.forEach((stop, i) => {
                  stop.alpha[p] = angularMasses[i];
                });
              else startAlpha[p] = start;
            }
          }
          analyticLayers.push({
            slotId,
            alpha,
            ...(contributions
              ? { contributions }
              : gradient
                ? {
                    split: {
                      startSlotId: gradient.startId ?? null,
                      endSlotId: gradient.endId ?? null,
                      startAlpha,
                    },
                  }
                : {}),
          });
        }
      }
      const scene = exportScene(doc),
        before = JSON.stringify(scene);
      const passes = colorCoveragePasses(scene, doc);
      const layers = [],
        diagnosticRasterErrors = [],
        productionPrecisionReceipts = [];
      let productionRasterCount = 0;
      for (const pass of passes)
        layers.push({
          slotId: pass.slotId,
          alpha: await raster(pass.svg),
          startAlpha: fixture.radialProbes ? await raster(pass.split.startSvg) : null,
          components: pass.contributions
            ? await Promise.all(
                pass.contributions.map((stop) => raster(stop.svg, stop.channel)),
              ).catch((error) => {
                diagnosticRasterErrors.push(String(error.message));
                return null;
              })
            : null,
        });
      // Retain evidence and continue all fixtures even if attribution fails. The
      // Node assertions below still fail the suite after results.json is saved.
      let measured = null,
        measurementError = null;
      try {
        measured = await sampleColorCoverage(slots, passes, doc, async (svg, size) => {
          try {
            productionRasterCount++;
            const alpha = await rasterizeCoverageAlpha(svg, size);
            if (size.channel === 'premultiplied-red')
              productionPrecisionReceipts.push(alpha.coveragePrecision ?? null);
            return alpha;
          } catch (error) {
            productionPrecisionReceipts.push({
              requestedChannel: size.channel ?? 'alpha',
              error: String(error.message),
            });
            throw error;
          }
        });
      } catch (error) {
        measurementError = String(error?.message ?? error);
      }
      const angularProbeMasks = fixture.angularProbes ? layers[0].components : null;
      const analytic = analyticalCoverage(analyticLayers);
      let orderedReference = null,
        analyticInterior = null,
        orderedReferenceProbes = null;
      if (fixture.angularProbes) {
        const referencePixels = await raster(orderedAngularReferenceSVG(), 'reference-rgb');
        const referenceMasses = [0, 0, 0, 0],
          idealInterior = [0, 0, 0, 0],
          actualInterior = [0, 0, 0, 0];
        for (let pixel = 0; pixel < pixels; pixel++) {
          referencePixels[pixel].forEach((mass, i) => {
            referenceMasses[i] += mass;
          });
          if (!isAngularReferenceInterior(pixel % width, Math.floor(pixel / width))) continue;
          analyticLayers[0].contributions.forEach((stop, i) => {
            idealInterior[i] += stop.alpha[pixel];
          });
          if (angularProbeMasks)
            angularProbeMasks.forEach((mask, i) => {
              actualInterior[i] += mask[pixel];
            });
        }
        const total = referenceMasses.reduce((sum, mass) => sum + mass, 0);
        orderedReference = {
          slots: referenceMasses.map((contribution, i) => ({
            id: slots[i],
            contribution,
            percent: (100 * contribution) / total,
          })),
          other: { id: null, contribution: 0, percent: 0 },
        };
        analyticInterior = {
          expected: idealInterior,
          actual: angularProbeMasks ? actualInterior : null,
          excludedSharedRayPixels: 28,
        };
        orderedReferenceProbes = angularProbeMasks
          ? [
              [32, 24],
              [44, 24],
              [59, 24],
              [44, 25],
              [31, 36],
            ].map(([x, y]) => ({
              x,
              y,
              expected: referencePixels[y * width + x],
              actual: angularProbeMasks.map((mask) => mask[y * width + x]),
            }))
          : null;
      }
      const full = await raster(exportSVG(doc));
      const rendered = await raster(
        `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" overflow="hidden">${scene.map(({ layer: l, value }) => renderElement(value, l)).join('')}</svg>`,
      );
      let maxCompositeError = 0,
        fullMass = 0,
        rendererMaxError = 0;
      for (let p = 0; p < pixels; p++) {
        const composed =
          1 - layers.reduce((remaining, entry) => remaining * (1 - entry.alpha[p]), 1);
        maxCompositeError = Math.max(maxCompositeError, Math.abs(composed - full[p]));
        rendererMaxError = Math.max(rendererMaxError, Math.abs(rendered[p] - full[p]));
        fullMass += full[p];
      }
      receipts.push({
        name: fixture.name,
        measured,
        measurementError,
        productionPrecisionReceipts,
        productionRasterCount,
        expectedRasterCount: passes.reduce(
          (count, pass) => count + 1 + (pass.split ? 1 : 0) + (pass.contributions?.length ?? 0),
          0,
        ),
        expectedContributionRasterCount: passes.reduce(
          (count, pass) => count + (pass.contributions?.length ?? 0),
          0,
        ),
        diagnosticRasterErrors,
        orderedReference,
        analyticInterior,
        orderedReferenceProbes,
        contributionDiagnostics: layers.flatMap((entry, index) =>
          entry.components
            ? [
                {
                  passIndex: index,
                  slotIds: passes[index].contributions.map((stop) => stop.slotId),
                  ...summarizeCoverageRasters(entry.alpha, entry.components, width, height, [
                    [32, 24],
                    [44, 25],
                    [31, 36],
                    [20, 23],
                    [33, 12],
                    [0, 0],
                  ]),
                },
              ]
            : [],
        ),
        angularProbes:
          fixture.angularProbes && angularProbeMasks
            ? [
                [44, 25],
                [31, 36],
                [20, 23],
                [33, 12],
              ].map(([x, y]) => ({
                x,
                y,
                expected: angularMassAt(fixture.rects[0].gradient, x + 0.5, y + 0.5),
                actual: angularProbeMasks.map((mask) => mask[y * width + x]),
              }))
            : null,
        radialProbes: fixture.radialProbes
          ? [
              [32, 24],
              [38, 24],
              [32, 30],
              [46, 24],
              [55, 24],
            ].map(([x, y]) => ({
              x,
              y,
              alpha: layers[0].alpha[y * width + x],
              startAlpha: layers[0].startAlpha[y * width + x],
            }))
          : null,
        analytic: fixture.analytic === false ? null : analytic,
        fullMass,
        maxCompositeError,
        rendererMaxError,
        expectedOrder,
        actualOrder: passes.map((p) => [p.layerId, p.paint, p.slotId]),
        sceneUnchanged: JSON.stringify(scene) === before,
        originAlpha: full[0],
        passCount: passes.length,
        antialiasedPixels: full.filter((alpha) => alpha > 0 && alpha < 1).length,
      });
    }
    return receipts;
  });
  await fs.writeFile(
    `${output}/results.json`,
    JSON.stringify(
      {
        passed: false,
        route:
          'production exportScene/renderElement/exportSVG → browser SVG Image → canvas RGBA alpha',
        results,
        errors,
      },
      null,
      2,
    ),
  );
  for (const result of results) {
    const context = result.name;
    assert.equal(result.measurementError, null, `${context}: ${result.measurementError}`);
    assert.deepEqual(
      result.actualOrder,
      result.expectedOrder,
      `${context}: fill then stroke, document order`,
    );
    assert.equal(result.sceneUnchanged, true, `${context}: scene mutation`);
    assert.equal(result.originAlpha, 0, `${context}: transparent background`);
    assert.ok(result.rendererMaxError <= 1 / 255, `${context}: exportSVG/renderElement mismatch`);
    // SVG 8-bit alpha quantization may differ after source-over rounding.
    assert.ok(result.maxCompositeError <= 2 / 255, `${context}: per-pixel compositing mismatch`);
    assert.ok(
      Math.abs(result.measured.paintedPixelEquivalent - result.fullMass) <= (64 * 48 * 2) / 255,
      `${context}: full exported SVG alpha mass mismatch`,
    );
    for (const [index, row] of [...result.measured.slots, result.measured.other].entries()) {
      if (!result.analytic) continue;
      const expectedSource = result.orderedReference ?? result.analytic;
      const expected = [...expectedSource.slots, expectedSource.other][index];
      assert.ok(
        Math.abs(row.contribution - expected.contribution) <= (64 * 48 * 2) / 255,
        `${context}: analytic contribution mismatch for ${row.id}`,
      );
      assert.ok(
        Math.abs(row.percent - expected.percent) <= 1,
        `${context}: analytic percentage mismatch`,
      );
    }
    if (result.analyticInterior)
      result.analyticInterior.expected.forEach((mass, i) =>
        assert.ok(
          Math.abs(mass - result.analyticInterior.actual[i]) <= (64 * 48 * 2) / 255,
          `${context}: strict ideal-angle interior contribution ${i}`,
        ),
      );
    for (const probe of result.orderedReferenceProbes ?? [])
      probe.expected.forEach((mass, i) =>
        assert.ok(
          Math.abs(mass - probe.actual[i]) <= 2 / 255,
          `${context}: ordered primitive probe ${probe.x},${probe.y} stop ${i}`,
        ),
      );
    assert.equal(
      result.productionPrecisionReceipts.length,
      result.expectedContributionRasterCount,
      `${context}: precision evidence covers every angular contribution raster`,
    );
    for (const precision of result.productionPrecisionReceipts) {
      assert.equal(precision?.contextColorType, 'float16', `${context}: actual canvas precision`);
      assert.equal(precision.contextColorSpace, 'srgb');
      assert.equal(precision.readbackColorSpace, 'srgb');
      assert.equal(precision.pixelFormat, 'rgba-float16', `${context}: actual readback precision`);
      assert.equal(precision.arrayType, 'Float16Array', `${context}: actual typed array`);
      assert.equal(precision.bytesPerElement, 2, `${context}: actual channel storage`);
    }
    assert.equal(
      result.measured.slots.reduce((sum, row) => sum + row.percent, result.measured.other.percent),
      result.fullMass ? 100 : 0,
      `${context}: integer percentages`,
    );
  }
  const angularInside = results.find(
    (r) => r.name === 'angular-center-inside-same-hex-four-distinct-ids',
  );
  for (const probe of angularInside.angularProbes)
    probe.expected.forEach((mass, i) =>
      assert.ok(
        Math.abs(mass - probe.actual[i]) <= 2 / 255,
        `angular midpoint wedge probe ${probe.x},${probe.y} stop ${i}`,
      ),
    );
  assert.ok(
    angularInside.measured.slots.every((row) => row.contribution > 0),
    'angular equal HEX preserves four distinct identities',
  );
  const angularTransparent = results.find(
    (r) => r.name === 'angular-transparent-stop-zero-contribution-and-unbound-other',
  ).measured;
  assert.equal(
    angularTransparent.slots[0].contribution,
    0,
    'premultiplied transparent angular stop supplies no color',
  );
  assert.ok(angularTransparent.other.contribution > 0, 'unbound angular stop stays Other');
  assert.notEqual(
    results.find((r) => r.name === 'angular-unequal-alpha-clipping-stroke-and-occlusion').measured
      .slots[0].contribution,
    results.find((r) => r.name === 'angular-reversed-overlap-order').measured.slots[0].contribution,
    'angular paint order and stroke occlude only once',
  );
  const angularAA = results.find((r) => r.name === 'angular-same-id-antialiased-clipped-triangle');
  assert.ok(angularAA.antialiasedPixels > 0, 'angular clipped triangle exercises actual AA');
  assert.equal(
    angularAA.measured.slots[0].percent,
    100,
    'four same-ID masses conserve actual paint',
  );
  assert.equal(
    results.find((r) => r.name === 'angular-all-transparent').measured.paintedPixelEquivalent,
    0,
  );
  const radialInside = results.find(
    (r) => r.name === 'radial-center-inside-same-hex-distinct-ids-and-pad',
  );
  const probeExpected = [1, 1 - 6 / 14, 1 - 6 / 14, 0, 0];
  radialInside.radialProbes.forEach((probe, i) => {
    assert.equal(probe.alpha, 1, 'radial paint is fully opaque inside and outside circle');
    assert.ok(
      Math.abs(probe.startAlpha - probeExpected[i]) <= 2 / 255,
      `radial circular-distance/pad probe ${i}`,
    );
  });
  assert.ok(
    radialInside.measured.slots[0].contribution > 0 &&
      radialInside.measured.slots[1].contribution > 0,
    'radial equal HEX never merges distinct stable stop IDs',
  );
  const radialTransparent = results.find(
    (r) => r.name === 'radial-transparent-center-hue-and-unbound-outer-other',
  ).measured;
  assert.ok(
    radialTransparent.slots[0].contribution > 0 && radialTransparent.other.contribution > 0,
    'transparent center still supplies interpolated hue; unbound outer stop stays Other',
  );
  assert.notEqual(
    results.find((r) => r.name === 'radial-unequal-alpha-clipping-stroke-and-occlusion').measured
      .slots[0].contribution,
    results.find((r) => r.name === 'radial-reversed-overlap-order').measured.slots[0].contribution,
    'radial contribution respects overlap and fill-before-stroke order',
  );
  for (const name of [
    'gradient-reversed-pad-same-id-antialiased-geometry',
    'radial-same-id-antialiased-clipped-triangle',
    'radial-minimum-radius-unbound-stops-use-outer-pad',
  ]) {
    const receipt = results.find((result) => result.name === name);
    assert.equal(receipt.productionRasterCount, 1, `${name}: only full paint is decoded`);
    assert.equal(receipt.expectedRasterCount, 1);
  }
  const radialAA = results.find((r) => r.name === 'radial-same-id-antialiased-clipped-triangle');
  assert.ok(radialAA.antialiasedPixels > 0, 'radial fixture exercises actual antialiasing');
  assert.equal(
    radialAA.measured.slots[0].percent,
    100,
    'same radial ID receives both masses including AA',
  );
  const radialDistinctAA = results.find(
    (r) => r.name === 'radial-distinct-ids-antialiased-clipped-triangle',
  );
  assert.ok(
    radialDistinctAA.antialiasedPixels > 0 &&
      radialDistinctAA.measured.slots[0].contribution > 0 &&
      radialDistinctAA.measured.slots[1].contribution > 0,
    'distinct radial stop masses survive clipped antialiased edges',
  );
  assert.equal(
    results.find((r) => r.name === 'radial-minimum-radius-unbound-stops-use-outer-pad').measured
      .other.percent,
    100,
    'missing radial IDs remain Other even at the minimum-radius pad',
  );
  const transparentStop = results.find(
    (r) => r.name === 'gradient-transparent-stop-hue-and-unbound-other',
  ).measured;
  assert.ok(
    Math.abs(transparentStop.slots[0].percent - 33) <= 1,
    'non-premultiplied interpolation still attributes mixed pixels to transparent stop',
  );
  assert.ok(Math.abs(transparentStop.other.percent - 67) <= 1, 'unbound end stop stays Other');
  const antialiasedFixture = results.find(
    (r) => r.name === 'gradient-reversed-pad-same-id-antialiased-geometry',
  );
  assert.ok(
    antialiasedFixture.antialiasedPixels > 0,
    'triangle exercises real partial-alpha edges',
  );
  const antialiased = antialiasedFixture.measured;
  assert.equal(
    antialiased.slots[0].percent,
    100,
    'same ID receives both stop masses including antialiased edges',
  );
  const same = results.find((r) => r.name === 'same-hex-explicit-ids-and-other').measured;
  assert.ok(
    same.slots[0].contribution > 0 && same.slots[1].contribution > 0 && same.other.contribution > 0,
    'equal HEX must not merge distinct explicit IDs or infer an ID for unbound paint',
  );
  assert.notEqual(
    same.slots[0].contribution,
    results.find((r) => r.name === 'reversed-object-order').measured.slots[0].contribution,
    'changing paint order must change visible contribution',
  );
  assert.deepEqual(errors, []);
  await fs.writeFile(
    `${output}/results.json`,
    JSON.stringify(
      { passed: true, route: 'production SVG to browser canvas alpha', results, errors },
      null,
      2,
    ),
  );
  console.log(
    `Color coverage raster acceptance passed (${results.length} fixtures); ${output}/results.json`,
  );
} finally {
  await browser.close();
}
