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
    const { measureColorCoverage } = await import('/src/color-coverage.js');
    const width = 64,
      height = 48,
      pixels = width * height;
    const slots = ['same_hex_a', 'same_hex_b', 'outline'];
    const raster = async (svg) => {
      const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
      try {
        const image = new Image();
        image.src = url;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const context = canvas.getContext('2d', { willReadFrequently: true });
        context.drawImage(image, 0, 0, width, height);
        const rgba = context.getImageData(0, 0, width, height).data;
        return Array.from({ length: pixels }, (_, index) => rgba[index * 4 + 3] / 255);
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
    const receipts = [];
    for (const fixture of cases) {
      const doc = blankDocument();
      doc.width = width;
      doc.height = height;
      if (fixture.rects.length) doc.layers = [];
      const bindings = new Map(),
        expectedOrder = [],
        analyticLayers = [];
      for (const [index, rectangle] of fixture.rects.entries()) {
        const l = layer(`Rectangle ${index}`),
          shape = node('Imported Path'),
          fill = node('Fill'),
          stroke = node('Stroke'),
          out = node('Output');
        const [x0, y0, x1, y1] = rectangle.box;
        shape.rawPath = `M${x0} ${y0}H${x1}V${y1}H${x0}Z`;
        fill.params.color.value = { hex: '#336699', alpha: rectangle.fill };
        l.nodes = [shape, fill];
        connect(l, shape.id, fill.id, 'path');
        let previous = fill;
        if (rectangle.stroke !== undefined) {
          stroke.params.color.value = { hex: '#336699', alpha: rectangle.stroke };
          stroke.params.width.value = 4;
          l.nodes.push(stroke);
          connect(l, fill.id, stroke.id, 'path');
          previous = stroke;
        }
        l.nodes.push(out);
        l.output = out.id;
        connect(l, previous.id, out.id, 'path');
        doc.layers.push(l);
        bindings.set(l.id, { fill: rectangle.id, stroke: slots[2] });
        for (const paint of rectangle.stroke === undefined ? ['fill'] : ['fill', 'stroke']) {
          const slotId = paint === 'fill' ? (rectangle.id ?? null) : slots[2];
          expectedOrder.push([l.id, paint, slotId]);
          const alpha = Array.from({ length: pixels }, (_, p) => {
            const x = (p % width) + 0.5,
              y = Math.floor(p / width) + 0.5;
            const inside = (a, b, c, d) => x >= a && x < c && y >= b && y < d;
            return paint === 'fill'
              ? inside(x0, y0, x1, y1)
                ? rectangle.fill
                : 0
              : inside(x0 - 2, y0 - 2, x1 + 2, y1 + 2) && !inside(x0 + 2, y0 + 2, x1 - 2, y1 - 2)
                ? rectangle.stroke
                : 0;
          });
          analyticLayers.push({ slotId, alpha });
        }
      }
      const scene = exportScene(doc),
        before = JSON.stringify(scene);
      const passes = colorCoveragePasses(scene, doc, bindings);
      const layers = [];
      for (const pass of passes)
        layers.push({ slotId: pass.slotId, alpha: await raster(pass.svg) });
      const measured = measureColorCoverage(slots, layers, pixels);
      const analytic = measureColorCoverage(slots, analyticLayers, pixels);
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
        analytic,
        fullMass,
        maxCompositeError,
        rendererMaxError,
        expectedOrder,
        actualOrder: passes.map((p) => [p.layerId, p.paint, p.slotId]),
        sceneUnchanged: JSON.stringify(scene) === before,
        originAlpha: full[0],
        passCount: passes.length,
      });
    }
    return receipts;
  });
  await fs.writeFile(
    `${output}/results.json`,
    JSON.stringify(
      {
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
      const expected = [...result.analytic.slots, result.analytic.other][index];
      assert.ok(
        Math.abs(row.contribution - expected.contribution) <= (64 * 48 * 2) / 255,
        `${context}: analytic contribution mismatch for ${row.id}`,
      );
      assert.ok(
        Math.abs(row.percent - expected.percent) <= 1,
        `${context}: analytic percentage mismatch`,
      );
    }
    assert.equal(
      result.measured.slots.reduce((sum, row) => sum + row.percent, result.measured.other.percent),
      result.fullMass ? 100 : 0,
      `${context}: integer percentages`,
    );
  }
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
