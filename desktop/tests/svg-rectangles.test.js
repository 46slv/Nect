import test from 'node:test';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { importSVG } from '../src/svg-import.js';
import {
  evaluate,
  exportSVG,
  serialize,
  deserialize,
  CommandSession,
  blankDocument,
} from '../src/core.js';
import { readSVGSource } from '../src/svg-tree.js';
const Parser = new Window().DOMParser;
export function syntheticGrid() {
  return `<svg width="1200" height="1200" viewBox="0 0 40 40"><rect width="100%" height="100%" fill="#ffffff"/><g fill="#111111">${Array.from({ length: 900 }, (_, i) => `<rect x="${(i % 30) + 4}" y="${Math.floor(i / 30) + 4}" width="0.8" height="0.8" rx="0.35"/>`).join('')}</g></svg>`;
}
test('percentage rectangle background, rounded modules and opaque batches import without per-module layers', () => {
  const svg = syntheticGrid(),
    d = importSVG(svg, Parser);
  assert.ok(d.layers.length >= 2 && d.layers.length < 12);
  assert.equal(d.width, 40);
  assert.equal(d.layers[0].nodes[0].params.size.value[0], 40);
  const views = d.layers.slice(1).map((l) => evaluate(l));
  for (const r of views) assert.equal(r.error, null);
  assert.equal(
    views.reduce((sum, r) => sum + r.path.contours.length, 0),
    900,
  );
  for (const l of d.layers.slice(1)) {
    assert.equal(l.nodes.length, 4);
    assert.ok(l.nodes[0].rawPath.length < 100000);
  }
  const output = exportSVG(d);
  assert.equal(exportSVG(deserialize(serialize(d))), output);
  const s = new CommandSession(blankDocument());
  s.dispatch({
    ...s.state,
    type: 'add-svg-part',
    payload: {
      layerId: s.snapshot().layers[0].id,
      source: readSVGSource(svg, Parser),
      name: 'synthetic-grid.svg',
    },
  });
  assert.equal(s.snapshot().layers.length, 2);
  assert.equal(evaluate(s.snapshot().layers[0], {}, {}, s.snapshot()).error, null);
});
test('rounded rectangle copies the omitted radius, clamps and rejects negative radii', () => {
  for (const radii of ['rx="20"', 'ry="20"', 'rx="20" ry="5"']) {
    const d = importSVG(`<svg><rect x="10" y="20" width="10" height="6" ${radii}/></svg>`, Parser);
    assert.equal(d.layers[0].nodes[0].type, 'Imported Path');
    assert.equal(evaluate(d.layers[0]).error, null);
  }
  assert.throws(
    () => importSVG('<svg><rect width="10" height="10" rx="-1"/></svg>', Parser),
    /radius/,
  );
});
test('rectangle batching never merges transparent or alternating paints or arbitrary opposite-winding paths', () => {
  for (const body of [
    '<rect width="10" height="10" fill-opacity="0.5"/><rect width="10" height="10" fill-opacity="0.5"/>',
    '<rect width="10" height="10" fill="#ff0000"/><rect width="10" height="10" fill="#0000ff"/>',
    '<path d="M0 0H10V10H0Z"/><path d="M0 0V10H10V0Z"/>',
    '<g data-layer="one"><rect width="10" height="10"/></g><g data-layer="two"><rect width="10" height="10"/></g>',
    '<rect id="one" width="10" height="10"/><rect id="two" width="10" height="10"/>',
  ])
    assert.equal(importSVG(`<svg>${body}</svg>`, Parser).layers.length, 2);
});
