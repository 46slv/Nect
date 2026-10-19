import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { importSVG } from '../src/svg-import.js';
import { evaluate, pathArea, exportSVG } from '../src/core.js';
const Parser = new Window().DOMParser;

const cssPixelsForMillimetres = (value) => (value * 96) / 25.4;
const close = (actual, expected) =>
  assert.ok(Math.abs(actual - expected) <= 1e-9 * Math.max(1, Math.abs(expected)));

test('physical SVG root sizes import as 96 CSS px/in and round-trip through mm and cm', () => {
  const original = importSVG(
      '<svg width="100mm" height="50mm"><rect width="96" height="48" fill="#123456"/></svg>',
      Parser,
    ),
    exportedMm = exportSVG(original, {}, { unit: 'mm' }),
    roundtrip = importSVG(exportedMm, Parser),
    exportedCm = exportSVG(original, {}, { unit: 'cm' }),
    roundtripCm = importSVG(exportedCm, Parser),
    cmRoot = importSVG('<svg width="10cm" height="5cm"><rect width="1" height="1"/></svg>', Parser);

  close(original.width, cssPixelsForMillimetres(100));
  close(original.height, cssPixelsForMillimetres(50));
  assert.match(
    exportedMm,
    /^<svg xmlns="http:\/\/www\.w3\.org\/2000\/svg" width="100mm" height="50mm" viewBox="0 0 /,
  );
  assert.match(
    exportedCm,
    /^<svg xmlns="http:\/\/www\.w3\.org\/2000\/svg" width="10cm" height="5cm" viewBox="0 0 /,
  );
  close(roundtrip.width, original.width);
  close(roundtrip.height, original.height);
  close(roundtripCm.width, original.width);
  close(roundtripCm.height, original.height);
  close(cmRoot.width, original.width);
  close(cmRoot.height, original.height);
  for (const width of ['1in', '72pt', '6pc', '101.6q']) {
    const absolute = importSVG(
      `<svg width="${width}" height="1in"><rect width="1" height="1"/></svg>`,
      Parser,
    );
    close(absolute.width, 96);
    close(absolute.height, 96);
  }
});

test('zero-origin physical SVG viewBox accepts matching user units and default preserveAspectRatio', () => {
  const width = cssPixelsForMillimetres(100),
    height = cssPixelsForMillimetres(50),
    imported = importSVG(
      `<svg width="100mm" height="50mm" viewBox="0 0 ${width} ${height}" preserveAspectRatio="xMidYMid meet"><rect width="96" height="48" fill="#123456"/></svg>`,
      Parser,
    ),
    exported = exportSVG(imported, {}, { unit: 'mm' });

  close(imported.width, width);
  close(imported.height, height);
  assert.match(
    exported,
    /^<svg xmlns="http:\/\/www\.w3\.org\/2000\/svg" width="100mm" height="50mm" viewBox="0 0 /,
  );
});

test('SVG importer rejects viewport transforms it cannot preserve', () => {
  assert.throws(
    () =>
      importSVG(
        '<svg width="100mm" height="50mm" viewBox="10 20 377.9527559055 188.9763779528"><rect x="10" y="20" width="96" height="48"/></svg>',
        Parser,
      ),
    /zero-origin SVG viewBox/,
  );
  for (const [viewBox, preserveAspectRatio] of [
    ['200 100', 'xMidYMid meet'],
    ['200 150', 'xMidYMid meet'],
    ['200 150', 'none'],
  ])
    assert.throws(
      () =>
        importSVG(
          `<svg width="100mm" height="50mm" viewBox="0 0 ${viewBox}" preserveAspectRatio="${preserveAspectRatio}"><rect width="200" height="100"/></svg>`,
          Parser,
        ),
      /viewBox scaling is not supported/,
    );
});

test('fill none without a stroke remains transparent instead of acquiring the layer default fill', () => {
  const d = importSVG('<svg><rect width="10" height="10" fill="none"/></svg>', Parser);
  assert.equal(evaluate(d.layers[0]).path.fill.alpha, 0);
  assert.match(exportSVG(d), /fill-opacity="0"/);
});
test('SVG import roundtrips compound holes through Remap and headless export', () => {
  let d = importSVG(
    '<svg viewBox="0 0 200 200"><path fill="#336699" d="M0 0H100V100H0Z M20 20V80H80V20Z"/></svg>',
    Parser,
  );
  assert.equal(d.layers.length, 1);
  assert.ok(Math.abs(pathArea(evaluate(d.layers[0]).path) - 6400) < 0.001);
  assert.match(exportSVG(d), /#336699/);
});
test('SVG supported primitives become editable source nodes', () => {
  let d = importSVG(
    '<svg width="300" height="200"><g fill="#123"><circle cx="50" cy="60" r="20"/><rect x="100" y="20" width="30" height="40"/></g></svg>',
    Parser,
  );
  assert.equal(d.layers.length, 2);
  assert.equal(d.layers[0].nodes[0].type, 'Circle');
  assert.deepEqual(d.layers[1].nodes[0].params.center.value, [115, 40]);
});
test('SVG import refuses executable/external content and unsupported rendering rather than losing it', () => {
  for (const body of [
    '<script>alert(1)</script>',
    '<image href="https://example.com/a"/>',
    '<path d="M0 0L1 1" transform="scale(2)"/>',
    '<rect width="10" height="10" onload="alert(1)"/>',
    '<path fill-rule="evenodd" d="M0 0L1 1"/>',
    '<rect width="10" height="10" style="fill:red"/>',
  ])
    assert.throws(() => importSVG(`<svg>${body}</svg>`, Parser));
});
test('SVG refuses invisible content and nested coordinate systems instead of silently changing appearance', () => {
  for (const body of [
    '<rect display="none" width="10" height="10"/>',
    '<svg x="10"><circle r="5"/></svg>',
    '<g visibility="hidden"><circle r="5"/></g>',
  ])
    assert.throws(() => importSVG(`<svg>${body}</svg>`, Parser));
});
test('the editor imports its own SVG export with layer names and equivalent geometry', () => {
  const original = importSVG(
    '<svg viewBox="0 0 200 200"><circle id="Disc" cx="70" cy="80" r="30" fill="#123456"/></svg>',
    Parser,
  );
  const exported = exportSVG(original),
    roundtrip = importSVG(exported, Parser);
  assert.equal(roundtrip.layers[0].name, 'Disc');
  assert.ok(
    Math.abs(
      pathArea(evaluate(roundtrip.layers[0]).path) - pathArea(evaluate(original.layers[0]).path),
    ) < 0.01,
  );
});
