import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { importSVG } from '../src/svg-import.js';
import { evaluate, pathArea, pathBounds, exportSVG, serialize, deserialize } from '../src/core.js';
const Parser = new Window().DOMParser;

const cssPixelsForMillimetres = (value) => (value * 96) / 25.4;
const close = (actual, expected) =>
  assert.ok(Math.abs(actual - expected) <= 1e-9 * Math.max(1, Math.abs(expected)));
const viewportMatrix = (doc) => {
  const layer = doc.layers[0],
    transform = layer.nodes.find((n) => n.name === 'SVG viewport mapping');
  assert.ok(transform, 'expected a non-identity SVG viewport Transform');
  return evaluate(layer).run(transform.id).matrix;
};
const closeMatrix = (actual, expected) =>
  actual.forEach((value, index) => close(value, expected[index]));

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

test('SVG viewBox maps non-zero origins and physical root sizes through Transform', () => {
  const width = cssPixelsForMillimetres(100),
    height = cssPixelsForMillimetres(50),
    imported = importSVG(
      '<svg width="100mm" height="50mm" viewBox="10 20 100 50"><rect x="10" y="20" width="100" height="50" fill="#123456"/></svg>',
      Parser,
    ),
    scale = width / 100;

  close(imported.width, width);
  close(imported.height, height);
  closeMatrix(viewportMatrix(imported), [scale, 0, 0, scale, -10 * scale, -20 * scale]);
  assert.equal(imported.layers[0].nodes.filter((n) => n.name === 'SVG viewport mapping').length, 1);
  const exported = exportSVG(imported);
  assert.match(exported, /transform="matrix\(/);
  assert.equal(exportSVG(deserialize(serialize(imported))), exported);
});

test('SVG viewBox default meet and none produce independent expected viewport matrices', () => {
  const markup = (preserve = '') =>
      `<svg width="100" height="100" viewBox="0 0 200 100"${preserve ? ` preserveAspectRatio="${preserve}"` : ''}><rect width="200" height="100"/></svg>`,
    meet = importSVG(markup(), Parser),
    none = importSVG(markup('none'), Parser);

  closeMatrix(viewportMatrix(meet), [0.5, 0, 0, 0.5, 0, 25]);
  closeMatrix(viewportMatrix(none), [0.5, 0, 0, 1, 0, 0]);
});

test('SVG percentage rectangle dimensions resolve in viewBox user space before mapping', () => {
  const imported = importSVG(
      '<svg width="100mm" height="50mm" viewBox="10 20 100 50"><rect x="10" y="20" width="50%" height="50%"/></svg>',
      Parser,
    ),
    source = imported.layers[0].nodes.find((n) => n.type === 'Rectangle'),
    geometry = evaluate(imported.layers[0]).run(source.id);

  assert.deepEqual(pathBounds(geometry), [
    [10, 35, 60],
    [20, 32.5, 45],
  ]);
});

test('SVG viewport Transform carries strokes and user-space gradients as one painted group', () => {
  const imported = importSVG(
    '<svg width="100mm" height="50mm" viewBox="10 20 100 50"><defs><linearGradient id="ramp" gradientUnits="userSpaceOnUse" x1="10" y1="20" x2="110" y2="70" spreadMethod="pad" color-interpolation="sRGB"><stop offset="0" stop-color="#ff0000"/><stop offset="1" stop-color="#0000ff"/></linearGradient></defs><path d="M10 20H110V70H10Z" fill="url(#ramp)" stroke="#123456" stroke-width="2"/></svg>',
    Parser,
  );
  const matrix = viewportMatrix(imported),
    exported = exportSVG(imported);

  close(matrix[0], cssPixelsForMillimetres(100) / 100);
  close(matrix[3], cssPixelsForMillimetres(50) / 50);
  close(matrix[4], -10 * matrix[0]);
  close(matrix[5], -20 * matrix[3]);
  assert.match(exported, /transform="matrix\(/);
  assert.match(exported, /stroke-width="2"/);
  assert.match(exported, /x1="10"/);
  assert.match(exported, /x2="110"/);
});

test('SVG importer rejects preserveAspectRatio mappings and clipping it cannot preserve', () => {
  for (const preserveAspectRatio of [
    'xMinYMin',
    'xMaxYMax meet',
    'xMidYMid slice',
    'xMinYMid slice',
  ])
    assert.throws(
      () =>
        importSVG(
          `<svg width="100" height="100" viewBox="0 0 200 100" preserveAspectRatio="${preserveAspectRatio}"><rect width="200" height="100"/></svg>`,
          Parser,
        ),
      /Unsupported SVG preserveAspectRatio/,
    );
  assert.throws(
    () =>
      importSVG('<svg><clipPath id="clip"><rect width="10" height="10"/></clipPath></svg>', Parser),
    /Unsupported SVG element: clipPath/,
  );
  assert.throws(
    () => importSVG('<svg><rect width="10" height="10" clip-path="url(#clip)"/></svg>', Parser),
    /Unsupported SVG attribute: clip-path/,
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
