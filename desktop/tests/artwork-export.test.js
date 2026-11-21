import assert from 'node:assert/strict';
import test from 'node:test';
import { createArtworkExporter } from '../src/artwork-export.js';

function fixture() {
  const document = Object.freeze({ id: 'document', width: 960, height: 540 });
  const fonts = Object.freeze({ reference: 'font' });
  const options = Object.freeze({ unit: 'mm', responsive: false, precision: 3 });
  const pngOptions = Object.freeze({ mode: 'ppi', ppi: 192 });
  const size = Object.freeze({ width: 1920, height: 1080 });
  const blob = new Blob(['png']);
  const calls = [];
  const run = createArtworkExporter({
    exportSVG: (...args) => (calls.push(['svg', ...args]), '<svg/>'),
    pngDimensions: (...args) => (calls.push(['dimensions', ...args]), size),
    rasterizeSVG: async (...args) => (calls.push(['raster', ...args]), blob),
  });
  return { document, fonts, options, pngOptions, size, blob, calls, run };
}

test('SVG passes existing options and reports destination cancel without claiming a write', async () => {
  const { document, fonts, options, calls, run } = fixture();
  const result = await run(
    { document, fonts, format: 'svg', svgOptions: options },
    async (...args) => {
      calls.push(['write', ...args]);
      return false;
    },
  );
  assert.deepEqual(result, { written: false });
  assert.deepEqual(calls, [
    ['svg', document, fonts, options],
    ['write', '<svg/>', 'nect-artwork.svg', 'svg'],
  ]);
});

test('PNG uses unrounded SVG, existing PPI and format-specific destination', async () => {
  const { document, fonts, options, pngOptions, size, blob, calls, run } = fixture();
  const result = await run(
    { document, fonts, format: 'png', svgOptions: options, pngOptions },
    async (...args) => (calls.push(['write', ...args]), true),
  );
  assert.deepEqual(result, { written: true, size });
  assert.deepEqual(calls, [
    ['dimensions', document, pngOptions],
    ['svg', document, fonts],
    ['raster', '<svg/>', size, 192],
    ['write', blob, 'nect-artwork.png', 'png'],
  ]);
});

test('unsupported formats and render errors never start a write', async () => {
  const { document, fonts, run } = fixture();
  const write = () => assert.fail('must not write');
  await assert.rejects(run({ document, fonts, format: 'json' }, write), /Unsupported/);
  const fail = createArtworkExporter({
    exportSVG: () => {
      throw Error('render failed');
    },
  });
  await assert.rejects(fail({ document, fonts, format: 'svg' }, write), /render failed/);
});

test('omitted PNG sizing uses renderer defaults, no PPI, and preserves cancel', async () => {
  const { document, fonts, size, calls, blob, run } = fixture();
  const result = await run(
    { document, fonts, format: 'png' },
    async (...args) => (calls.push(['write', ...args]), false),
  );
  assert.deepEqual(result, { written: false, size });
  assert.deepEqual(calls, [
    ['dimensions', document, undefined],
    ['svg', document, fonts],
    ['raster', '<svg/>', size, null],
    ['write', blob, 'nect-artwork.png', 'png'],
  ]);
});

test('PNG raster or destination failure rejects without a success result', async () => {
  const { document, fonts, pngOptions, run } = fixture();
  const request = { document, fonts, format: 'png', pngOptions };
  await assert.rejects(
    run(request, async () => {
      throw Error('destination failed');
    }),
    /destination failed/,
  );
  const fail = createArtworkExporter({
    pngDimensions: () => ({ width: 1, height: 1 }),
    exportSVG: () => '<svg/>',
    rasterizeSVG: async () => {
      throw Error('raster failed');
    },
  });
  await assert.rejects(
    fail(request, () => assert.fail('must not write')),
    /raster failed/,
  );
});
