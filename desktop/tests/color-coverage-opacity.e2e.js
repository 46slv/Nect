import assert from 'node:assert/strict';
import { chromium } from 'playwright-core';

const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH ||
    (process.platform === 'win32'
      ? 'C:/Program Files/Google/Chrome/Application/chrome.exe'
      : '/usr/bin/chromium'),
  headless: true,
  chromiumSandbox: true,
});
const page = await browser.newPage();
try {
  await page.goto(process.env.BASE_URL || 'http://127.0.0.1:5173');
  await page.waitForFunction(() => window.nect);
  const results = await page.evaluate(async () => {
    const { opacityCoverageDocument } = await import('/tests/helpers/opacity-coverage-fixture.js');
    const { exportScene, exportSVG, serialize, deserialize } = await import('/src/core.js');
    const { colorCoveragePasses } = await import('/src/color-coverage-passes.js');
    const { rasterizeCoverageAlpha } = await import('/src/color-coverage-raster.js');
    const { sampleColorCoverage } = await import('/src/color-coverage-sampling.js');
    async function pixels(svg) {
      const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
      try {
        const image = new Image();
        image.src = url;
        await image.decode();
        const canvas = document.createElement('canvas');
        canvas.width = 64;
        canvas.height = 48;
        const context = canvas.getContext('2d', { willReadFrequently: true });
        context.drawImage(image, 0, 0);
        return context.getImageData(0, 0, 64, 48).data;
      } finally {
        URL.revokeObjectURL(url);
      }
    }
    const cases = [
      { count: 2, start: 25, end: 75, copiesAbove: true },
      { count: 2, start: 25, end: 75, copiesAbove: false },
      { count: 2.5, start: 0, end: 100, copiesAbove: true },
      { count: 3, start: 0, end: 0, copiesAbove: false },
    ];
    const results = [];
    for (const options of cases) {
      const doc = opacityCoverageDocument(options),
        before = serialize(doc),
        scene = exportScene(doc),
        passes = colorCoveragePasses(scene, doc),
        measured = await sampleColorCoverage(['red', 'blue'], passes, doc, rasterizeCoverageAlpha),
        restored = deserialize(before),
        reloaded = await sampleColorCoverage(
          ['red', 'blue'],
          colorCoveragePasses(exportScene(restored), restored),
          restored,
          rasterizeCoverageAlpha,
        ),
        reference = await pixels(exportSVG(doc));
      const mass = [0, 0];
      for (let i = 0; i < reference.length; i += 4) {
        const alpha = reference[i + 3] / 255;
        mass[0] += (reference[i] / 255) * alpha;
        mass[1] += (reference[i + 2] / 255) * alpha;
      }
      results.push({
        options,
        measured: measured.slots.map((row) => row.contribution),
        reloaded: reloaded.slots.map((row) => row.contribution),
        reference: mass,
        other: measured.other.contribution,
        unchanged: serialize(doc) === before,
        groups: scene.flatMap((entry) => entry.value.contours.map((c) => c.repeaterOpacity ?? 1)),
        hole: Array.from(reference.slice((14 * 64 + 14) * 4, (14 * 64 + 14) * 4 + 4)),
      });
    }
    return results;
  });
  for (const result of results) {
    assert.equal(result.unchanged, true);
    assert.ok(result.groups.some((opacity) => opacity !== 1));
    assert.equal(result.other, 0);
    assert.deepEqual(result.reloaded, result.measured);
    for (let i = 0; i < 2; i++)
      assert.ok(
        Math.abs(result.measured[i] - result.reference[i]) < 6,
        `${JSON.stringify(result.options)} slot ${i}: ${result.measured[i]} vs ${result.reference[i]}`,
      );
  }
  assert.ok(results[0].measured[0] > 0 && results[0].measured[1] > 0);
  assert.ok(results[0].hole[0] < 4 && results[0].hole[2] > 250 && results[0].hole[3] > 0);
  assert.notDeepEqual(results[0].groups, results[1].groups);
  assert.equal(results[3].measured[0], 0);
  console.log('PASS solid repeater opacity, source-over backdrop, holes and fractional copies');
} finally {
  await browser.close();
}
