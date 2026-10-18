import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { chromium } from 'playwright-core';
import { Window } from 'happy-dom';
import { importSVG } from '../src/svg-import.js';
import { exportSVG } from '../src/core.js';
const Parser = new Window().DOMParser;
const source = `<svg xmlns="http://www.w3.org/2000/svg" width="220" height="220" viewBox="0 0 220 220"><rect width="100%" height="100%" fill="#ffffff"/><g fill="#000000">${Array.from({ length: 64 }, (_, i) => `<rect x="${10.3 + (i % 8) * 25}" y="${10.3 + Math.floor(i / 8) * 25}" width="25" height="25" rx="10"/>`).join('')}</g></svg>`;
const converted = exportSVG(importSVG(source, Parser)),
  parsed = new Parser().parseFromString(converted, 'image/svg+xml'),
  paths = [...parsed.querySelectorAll('path')],
  black = paths.filter((p) => p.getAttribute('fill') === '#000000');
assert.ok(black.length >= 4);
const merged = parsed.createElementNS('http://www.w3.org/2000/svg', 'path');
merged.setAttribute('fill', '#000000');
merged.setAttribute('d', black.map((p) => p.getAttribute('d')).join(' '));
black[0].parentElement.append(merged);
black.forEach((p) => p.remove());
const oldMerged = parsed.documentElement.outerHTML;
let shapeId = 0;
// Named rectangles remain separate paints. This isolates batching from Skia's
// different native rounded-rect and cubic-path antialiasing implementations.
const independentPaths = exportSVG(
  importSVG(
    source.replaceAll('<rect ', () => `<rect id="shape-${shapeId++}" `),
    Parser,
  ),
);
const browser = await chromium.launch({
  executablePath:
    process.env.CHROMIUM_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless: true,
});
try {
  const page = await browser.newPage();
  const result = await page.evaluate(
    async ({ source, converted, oldMerged, independentPaths }) => {
      const raster = async (svg) => {
        const img = new Image();
        img.src = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
        await img.decode();
        const c = document.createElement('canvas');
        c.width = c.height = 220;
        const x = c.getContext('2d');
        x.drawImage(img, 0, 0);
        return { rgba: x.getImageData(0, 0, 220, 220).data, png: c.toDataURL().split(',')[1] };
      };
      const a = await raster(independentPaths),
        b = await raster(converted),
        c = await raster(oldMerged),
        d = await raster(source);
      const diff = (other) => {
        let sum = 0,
          max = 0;
        for (let i = 0; i < a.rgba.length; i++) {
          const d = Math.abs(a.rgba[i] - other.rgba[i]);
          sum += d;
          max = Math.max(max, d);
        }
        return { mean: sum / a.rgba.length, max };
      };
      return {
        separated: diff(b),
        touchingCompound: diff(c),
        nativeRectVersusIndependentPaths: diff(d),
        pngs: [a.png, b.png, c.png],
      };
    },
    { source, converted, oldMerged, independentPaths },
  );
  await mkdir('test-results/svg-coverage', { recursive: true });
  for (const [i, name] of ['original', 'separated', 'old-compound'].entries())
    await writeFile(`test-results/svg-coverage/${name}.png`, Buffer.from(result.pngs[i], 'base64'));
  delete result.pngs;
  console.log(JSON.stringify(result));
  assert.ok(result.touchingCompound.max > 10, 'Fixture must expose independent edge coverage');
  // Skia can choose different AA paths even for disjoint compound contours.
  // This frozen fixture checks measured improvement, not universal pixel parity.
  assert.ok(result.separated.mean < result.touchingCompound.mean * 0.8);
  assert.ok(result.separated.max < result.touchingCompound.max);
  await writeFile('test-results/svg-coverage/receipt.json', JSON.stringify(result, null, 2));
} finally {
  await browser.close();
}
